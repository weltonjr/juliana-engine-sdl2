#pragma once

#include "terrain/Terrain.h"
#include "package/MaterialDef.h"
#include "core/Types.h"
#include <box2d/box2d.h>
#include <vector>
#include <unordered_map>

class PhysicsWorld;
class DefinitionRegistry;

struct DynamicBody {
    b2Body* body      = nullptr;
    int     origin_x  = 0;     // world-pixel position of the body origin at creation
    int     origin_y  = 0;

    // Cells that were removed from the terrain to form this body.
    // Positions are relative to (origin_x, origin_y).
    struct BodyCell {
        int8_t rx = 0, ry = 0;  // relative offset (pixels)
        Cell    cell;            // original cell data
    };
    std::vector<BodyCell> cells;

    bool settled = false;       // true when the body has come to rest and cells are re-planted
};

// DynamicBodyManager — detects disconnected solid groups after terrain modifications,
// converts them into Box2D rigid bodies, and re-plants them when they land.
class DynamicBodyManager {
public:
    DynamicBodyManager(PhysicsWorld& world, const DefinitionRegistry& registry);
    ~DynamicBodyManager();

    // Scan for newly disconnected solid groups in the given region.
    // Call after any bulk terrain modification (explosion, dig, etc.).
    void ScanForFloatingGroups(Terrain& terrain, int rx, int ry, int rw, int rh);

    // Called every frame: step active bodies and re-plant settled ones.
    void Update(Terrain& terrain, float dt);

    int ActiveBodyCount() const { return static_cast<int>(bodies_.size()); }

private:
    // Flood-fill from the terrain boundaries to find all "anchored" solid cells.
    // Returns a bit-array (indexed by y*w+x) where true = anchored.
    void BuildAnchorMap(const Terrain& terrain, int rx, int ry, int rw, int rh,
                        std::vector<bool>& anchored) const;

    // Extract one connected component starting from (sx,sy) and erase from terrain.
    DynamicBody* CreateBodyFromGroup(Terrain& terrain,
                                     const std::vector<std::pair<int,int>>& cells);

    // Write body cells back to terrain (called when body settles).
    void ReplantBody(Terrain& terrain, DynamicBody& db);

    PhysicsWorld&            world_;
    const DefinitionRegistry& registry_;
    std::vector<DynamicBody> bodies_;

    // Fast solid lookup
    std::vector<bool> solid_lut_;
};
