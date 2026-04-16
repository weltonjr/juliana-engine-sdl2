#include "terrain/DynamicBodyManager.h"
#include "physics/PhysicsWorld.h"
#include "package/DefinitionRegistry.h"
#include "package/MaterialDef.h"
#include <algorithm>
#include <queue>
#include <cstring>
#include <cstdio>

static constexpr float PIXELS_TO_METERS = 1.0f / 32.0f;
static constexpr float METERS_TO_PIXELS = 32.0f;

// Maximum fragment size (pixel count). Larger groups are left as static terrain.
static constexpr int MAX_FRAGMENT_CELLS = 256;
// Minimum fragment size — single stray pixels are ignored.
static constexpr int MIN_FRAGMENT_CELLS = 2;

DynamicBodyManager::DynamicBodyManager(PhysicsWorld& world, const DefinitionRegistry& registry)
    : world_(world), registry_(registry)
{
    solid_lut_.resize(256, false);
    for (int i = 0; i < 256; i++) {
        auto* mat = registry.GetMaterialByRuntimeID(static_cast<MaterialID>(i));
        if (mat && mat->state == MaterialState::Solid)
            solid_lut_[i] = true;
    }
}

DynamicBodyManager::~DynamicBodyManager() {
    for (auto& db : bodies_) {
        if (db.body) world_.DestroyBody(db.body);
    }
}

// ── Anchor map ────────────────────────────────────────────────────────────────

void DynamicBodyManager::BuildAnchorMap(
    const Terrain& terrain, int /*rx*/, int /*ry*/, int /*rw*/, int /*rh*/,
    std::vector<bool>& anchored) const
{
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();
    int total = w * h;

    static const int DX[] = {1, -1, 0,  0};
    static const int DY[] = {0,  0, 1, -1};

    // ── Phase 1: flood "outside" (non-solid only) from boundary non-solid cells.
    // This identifies all non-solid cells reachable from the map edge.
    std::vector<bool> outside(total, false);
    std::queue<int> q1;

    for (int x = 0; x < w; x++) {
        for (int y : {0, h - 1}) {
            int idx = y * w + x;
            Cell c = terrain.GetCell(x, y);
            if (!solid_lut_[c.material_id] && !outside[idx]) {
                outside[idx] = true;
                q1.push(idx);
            }
        }
    }
    for (int y = 1; y < h - 1; y++) {
        for (int x : {0, w - 1}) {
            int idx = y * w + x;
            Cell c = terrain.GetCell(x, y);
            if (!solid_lut_[c.material_id] && !outside[idx]) {
                outside[idx] = true;
                q1.push(idx);
            }
        }
    }

    while (!q1.empty()) {
        int idx = q1.front(); q1.pop();
        int cx = idx % w, cy = idx / w;
        for (int d = 0; d < 4; d++) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            int nidx = ny * w + nx;
            if (outside[nidx]) continue;
            Cell nc = terrain.GetCell(nx, ny);
            if (solid_lut_[nc.material_id]) continue;  // do NOT enter solids
            outside[nidx] = true;
            q1.push(nidx);
        }
    }

    // ── Phase 2: anchor solids — seed any solid that touches outside[] or the
    //    map edge, then propagate solid-to-solid only.
    anchored.assign(total, false);
    std::queue<int> q2;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;
            Cell c = terrain.GetCell(x, y);
            if (!solid_lut_[c.material_id]) continue;

            // Seed condition: on map edge OR any 4-neighbor is outside[]
            bool seed = (x == 0 || x == w - 1 || y == 0 || y == h - 1);
            if (!seed) {
                for (int d = 0; d < 4 && !seed; d++) {
                    int nx = x + DX[d], ny = y + DY[d];
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h && outside[ny * w + nx])
                        seed = true;
                }
            }
            if (seed && !anchored[idx]) {
                anchored[idx] = true;
                q2.push(idx);
            }
        }
    }

    while (!q2.empty()) {
        int idx = q2.front(); q2.pop();
        int cx = idx % w, cy = idx / w;
        for (int d = 0; d < 4; d++) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            int nidx = ny * w + nx;
            if (anchored[nidx]) continue;
            Cell nc = terrain.GetCell(nx, ny);
            if (!solid_lut_[nc.material_id]) continue;  // solid-only propagation
            anchored[nidx] = true;
            q2.push(nidx);
        }
    }
}

// ── Group extraction ──────────────────────────────────────────────────────────

DynamicBody* DynamicBodyManager::CreateBodyFromGroup(
    Terrain& terrain, const std::vector<std::pair<int,int>>& cells)
{
    if (cells.empty()) return nullptr;

    // Compute centroid (body origin = centre of bounding box)
    int min_x = cells[0].first, max_x = cells[0].first;
    int min_y = cells[0].second, max_y = cells[0].second;
    for (auto& [x, y] : cells) {
        min_x = std::min(min_x, x); max_x = std::max(max_x, x);
        min_y = std::min(min_y, y); max_y = std::max(max_y, y);
    }
    int cx = (min_x + max_x) / 2;
    int cy = (min_y + max_y) / 2;

    // Create the Box2D body
    b2BodyDef bd;
    bd.type = b2_dynamicBody;
    bd.position.Set(static_cast<float>(cx) * PIXELS_TO_METERS,
                    static_cast<float>(cy) * PIXELS_TO_METERS);
    bd.linearDamping  = 0.1f;
    bd.angularDamping = 0.5f;
    b2Body* body = world_.CreateBody(bd);

    // Bounding-box fixture (approximate — proper marching-squares contour
    // can be added later for concave shapes).
    float hw = static_cast<float>(max_x - min_x + 1) * 0.5f * PIXELS_TO_METERS;
    float hh = static_cast<float>(max_y - min_y + 1) * 0.5f * PIXELS_TO_METERS;
    b2PolygonShape box;
    box.SetAsBox(hw, hh);

    b2FixtureDef fd;
    fd.shape       = &box;
    fd.density     = 2.0f;
    fd.friction    = 0.6f;
    fd.restitution = 0.1f;
    body->CreateFixture(&fd);

    // Save cell data and erase them from the terrain
    DynamicBody db;
    db.body     = body;
    db.origin_x = cx;
    db.origin_y = cy;

    int w = terrain.GetWidth();
    for (auto& [x, y] : cells) {
        DynamicBody::BodyCell bc;
        bc.rx   = static_cast<int8_t>(x - cx);
        bc.ry   = static_cast<int8_t>(y - cy);
        bc.cell = terrain.GetCell(x, y);
        db.cells.push_back(bc);

        // Replace with Air, preserving background
        Cell air;
        air.material_id  = 0; // base:Air
        air.background_id = bc.cell.background_id;
        terrain.SetCell(x, y, air);
        (void)w;
    }

    bodies_.push_back(std::move(db));
    return &bodies_.back();
}

// ── Scan for floating groups ──────────────────────────────────────────────────

void DynamicBodyManager::ScanForFloatingGroups(
    Terrain& terrain, int rx, int ry, int rw, int rh)
{
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();

    std::vector<bool> anchored;
    BuildAnchorMap(terrain, rx, ry, rw, rh, anchored);

    // Scan for solid cells that are NOT anchored
    int x0 = std::max(0, rx);
    int y0 = std::max(0, ry);
    int x1 = std::min(w - 1, rx + rw - 1);
    int y1 = std::min(h - 1, ry + rh - 1);

    std::vector<bool> visited(w * h, false);

    static const int DX[] = {1, -1, 0,  0};
    static const int DY[] = {0,  0, 1, -1};

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            int idx = y * w + x;
            if (visited[idx] || anchored[idx]) continue;
            Cell cell = terrain.GetCell(x, y);
            if (!solid_lut_[cell.material_id]) continue;

            // BFS: collect the entire unanchored solid component
            std::vector<std::pair<int,int>> group;
            std::queue<int> q;
            q.push(idx);
            visited[idx] = true;

            while (!q.empty()) {
                int cur = q.front(); q.pop();
                int cx2 = cur % w, cy2 = cur / w;
                group.emplace_back(cx2, cy2);
                if (static_cast<int>(group.size()) > MAX_FRAGMENT_CELLS) break;

                for (int d = 0; d < 4; d++) {
                    int nx = cx2 + DX[d], ny = cy2 + DY[d];
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    int nidx = ny * w + nx;
                    if (visited[nidx] || anchored[nidx]) continue;
                    Cell nc = terrain.GetCell(nx, ny);
                    if (!solid_lut_[nc.material_id]) continue;
                    visited[nidx] = true;
                    q.push(nidx);
                }
            }

            if (static_cast<int>(group.size()) > MAX_FRAGMENT_CELLS) continue; // too large
            if (static_cast<int>(group.size()) < MIN_FRAGMENT_CELLS) continue; // too small

            // Mark all group cells visited
            for (auto& [gx, gy] : group) visited[gy * w + gx] = true;

            CreateBodyFromGroup(terrain, group);
        }
    }
}

// ── Per-frame update ──────────────────────────────────────────────────────────

void DynamicBodyManager::ReplantBody(Terrain& terrain, DynamicBody& db) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();

    b2Vec2 pos = db.body->GetPosition();
    float  ang = db.body->GetAngle();
    float  ca  = std::cos(ang);
    float  sa  = std::sin(ang);

    for (auto& bc : db.cells) {
        // Rotate offset by body angle
        float rx = static_cast<float>(bc.rx);
        float ry = static_cast<float>(bc.ry);
        float wrx = rx * ca - ry * sa;
        float wry = rx * sa + ry * ca;

        int wx = static_cast<int>(pos.x * METERS_TO_PIXELS + wrx);
        int wy = static_cast<int>(pos.y * METERS_TO_PIXELS + wry);

        if (wx < 0 || wx >= w || wy < 0 || wy >= h) continue;

        // Only plant if the target is Air
        Cell existing = terrain.GetCell(wx, wy);
        if (existing.material_id == 0) {
            terrain.SetCell(wx, wy, bc.cell);
        }
    }
}

void DynamicBodyManager::Update(Terrain& terrain, float dt) {
    (void)dt; // stepping is handled by PhysicsWorld

    auto it = bodies_.begin();
    while (it != bodies_.end()) {
        DynamicBody& db = *it;

        if (!db.body->IsAwake()) {
            // Body has come to rest — re-plant cells and remove from list
            ReplantBody(terrain, db);
            world_.DestroyBody(db.body);
            db.body = nullptr;
            it = bodies_.erase(it);
        } else {
            ++it;
        }
    }
}
