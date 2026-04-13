#include "core/Engine.h"
#include "core/EngineLog.h"
#include "game/GameLoader.h"
#include "input/InputAction.h"
#include <SDL_image.h>
#include <cmath>
#include <cstdio>
#include <array>

Engine::Engine() = default;

Engine::~Engine() {
    for (auto& [path, tex] : sprite_cache_) {
        if (tex) SDL_DestroyTexture(tex);
    }
    sprite_cache_.clear();
}

// ─── Init ─────────────────────────────────────────────────────────────────────

void Engine::Init(const std::string& game_path) {
    // 1. Parse game definition
    game_def_ = GameLoader::Load(game_path);

    int win_w = game_def_.window_width;
    int win_h = game_def_.window_height;
    std::string title = game_def_.name.empty() ? "Juliana Engine" : game_def_.name;

    // 2. Window + input
    window_ = std::make_unique<Window>(title.c_str(), win_w, win_h);
    input_  = std::make_unique<InputManager>(1);

    // 3. Default camera for when simulation starts
    cameras_.push_back(std::make_unique<Camera>(win_w, win_h, 2.0f));

    // 4. UI system + log console
    ui_system_   = std::make_unique<UISystem>(window_->GetRenderer());
    log_console_ = std::make_unique<LogConsole>(window_->GetRenderer());
    if (!game_def_.skin_path.empty())
        ui_system_->LoadSkin(game_def_.Resolve(game_def_.skin_path));
    ui_system_->LoadFont(
        game_def_.font_path.empty() ? "" : game_def_.Resolve(game_def_.font_path),
        game_def_.font_size);

    // Wire text input mode (SDL_StartTextInput/StopTextInput) to UI focus events
    ui_system_->SetTextInputCallback([this](bool on) {
        if (on) input_->StartTextInput();
        else    input_->StopTextInput();
    });

    // 5. Register built-in materials/backgrounds, then load content packages
    registry_.RegisterBuiltins();

    if (!game_def_.packages.empty()) {
        PackageLoader loader(registry_);
        for (const auto& pkg : game_def_.packages)
            loader.LoadAll(pkg);

        char buf0[128];
        std::snprintf(buf0, sizeof(buf0), "Loaded %d materials, %d backgrounds",
                      registry_.GetMaterialCount(), registry_.GetBackgroundCount());
        EngineLog::Log(buf0);
    }

    // 6. Lua state + startup script
    lua_state_ = std::make_unique<LuaState>(*this, *ui_system_);
    if (!game_def_.startup_script.empty()) {
        lua_state_->RunScript(game_def_.Resolve(game_def_.startup_script), game_path);
    }

    // 7. Game loop (always last)
    game_loop_ = std::make_unique<GameLoop>(60);

    char buf1[256];
    std::snprintf(buf1, sizeof(buf1), "Engine initialized: window %dx%d, game '%s'",
                  win_w, win_h, game_def_.name.c_str());
    EngineLog::Log(buf1);
}

// ─── InitSimulation ───────────────────────────────────────────────────────────
// Called on demand (e.g. from Lua via engine.load_scenario) to boot the
// terrain/entity/physics subsystems. Not used by the map editor menu.

void Engine::InitSimulation(const std::string& scenario_path) {
    auto scenario = ScenarioLoader::LoadFromFile(scenario_path);
    terrain_ = std::make_unique<Terrain>(
        MapGenerator::GenerateFromScenario(*scenario, registry_)
    );

    terrain_renderer_ = std::make_unique<TerrainRenderer>(
        window_->GetRenderer(), *terrain_, &registry_
    );
    terrain_renderer_->FullRebuild();

    entity_manager_ = std::make_unique<EntityManager>(registry_);
    physics_        = std::make_unique<PhysicsSystem>(registry_);
    terrain_sim_    = std::make_unique<TerrainSimulator>(registry_);

    // Load action maps for objects with animations
    for (auto& [qid, obj_ptr] : registry_.GetAllObjects()) {
        if (!obj_ptr->animations_path.empty()) {
            ActionMap am;
            if (am.LoadFromFile(obj_ptr->animations_path))
                action_maps_[qid] = std::move(am);
        }
    }

    // Spawn initial entities from scenario definition
    int spawn_x, spawn_y;
    if (!scenario->players.empty()) {
        auto positions = MapGenerator::FindSpawnPositions(*terrain_, registry_, scenario->players);
        spawn_x = positions[0].x;
        spawn_y = positions[0].y;
    } else {
        spawn_x = terrain_->GetWidth() / 2;
        spawn_y = MapGenerator::FindSurfaceY(*terrain_, spawn_x, registry_) - 21;
    }

    entity_manager_->Spawn("base:Character",
                           static_cast<float>(spawn_x),
                           static_cast<float>(spawn_y));

    entity_manager_->ForEach([&](const Entity& e) {
        if (e.definition && e.definition->player_controllable)
            controllable_entities_.push_back(e.id);
    });
    active_char_index_ = 0;

    float cx = static_cast<float>(spawn_x) - cameras_[0]->GetViewWorldWidth()  / 2.0f;
    float cy = static_cast<float>(spawn_y) - cameras_[0]->GetViewWorldHeight() / 2.0f;
    cameras_[0]->SetPosition(cx, cy);
    cameras_[0]->ClampToBounds(terrain_->GetWidth(), terrain_->GetHeight());

    debug_ui_ = std::make_unique<DebugUI>(window_->GetRenderer());
    sim_running_ = true;

    char buf_sim[128];
    std::snprintf(buf_sim, sizeof(buf_sim), "Simulation initialized: %dx%d terrain",
                  terrain_->GetWidth(), terrain_->GetHeight());
    EngineLog::Log(buf_sim);
}

// ─── Generic terrain generation (no entities/physics) ────────────────────────

void Engine::GenerateTerrain(const ScenarioDef& scenario) {
    uint32_t seed_used = 0;
    terrain_ = std::make_unique<Terrain>(
        MapGenerator::GenerateFromScenario(scenario, registry_, &seed_used)
    );
    last_terrain_seed_ = seed_used;

    // Apply cell overrides (from map editor manual edits)
    for (auto& ov : scenario.overrides) {
        if (!ov.material_id.empty()) {
            const auto* mat = registry_.GetMaterial(ov.material_id);
            if (mat) terrain_->SetMaterial(ov.x, ov.y, mat->runtime_id);
        }
        if (!ov.background_id.empty()) {
            const auto* bg = registry_.GetBackground(ov.background_id);
            if (bg) terrain_->SetBackground(ov.x, ov.y, bg->runtime_id);
        }
    }

    terrain_renderer_ = std::make_unique<TerrainRenderer>(
        window_->GetRenderer(), *terrain_, &registry_
    );
    terrain_renderer_->FullRebuild();

    // Centre camera on the terrain
    if (!cameras_.empty()) {
        float cx = (terrain_->GetWidth()  - cameras_[0]->GetViewWorldWidth())  / 2.0f;
        float cy = (terrain_->GetHeight() - cameras_[0]->GetViewWorldHeight()) / 2.0f;
        cameras_[0]->SetPosition(cx, cy);
        cameras_[0]->ClampToBounds(terrain_->GetWidth(), terrain_->GetHeight());
    }

    char buf[128];
    std::snprintf(buf, sizeof(buf), "Terrain generated: %dx%d cells",
                  terrain_->GetWidth(), terrain_->GetHeight());
    EngineLog::Log(buf);
}

void Engine::UnloadTerrain() {
    terrain_renderer_.reset();
    terrain_.reset();
}

void Engine::SetTerrainCell(int x, int y,
                             const std::string& mat_id, const std::string& bg_id) {
    if (!terrain_ || !terrain_->InBounds(x, y)) return;
    if (!mat_id.empty()) {
        const auto* m = registry_.GetMaterial(mat_id);
        if (m) terrain_->SetMaterial(x, y, m->runtime_id);
    }
    if (!bg_id.empty()) {
        const auto* b = registry_.GetBackground(bg_id);
        if (b) terrain_->SetBackground(x, y, b->runtime_id);
    }
    if (terrain_renderer_) terrain_renderer_->UpdateRegion(x, y, 1, 1);
}

std::pair<std::string, std::string> Engine::GetTerrainCell(int x, int y) const {
    if (!terrain_ || !terrain_->InBounds(x, y)) return {"", ""};
    auto cell = terrain_->GetCell(x, y);
    const auto* m = registry_.GetMaterialByRuntimeID(cell.material_id);
    const auto* b = registry_.GetBackgroundByRuntimeID(cell.background_id);
    return {
        m ? m->qualified_id : "",
        b ? b->qualified_id : ""
    };
}

const InputSystem&  Engine::GetRaw()   const { return input_->GetRaw(); }
const InputManager& Engine::GetInput() const { return *input_; }

// ─── Stats ────────────────────────────────────────────────────────────────────

int Engine::GetFPS() const {
    return game_loop_ ? game_loop_->GetActualFPS() : 0;
}

int Engine::GetNonAirCellCount() const {
    if (!terrain_) return 0;
    const auto* air = registry_.GetMaterial("base:Air");
    MaterialID air_id = air ? air->runtime_id : 0;
    int count = 0;
    for (int y = 0; y < terrain_->GetHeight(); ++y)
        for (int x = 0; x < terrain_->GetWidth(); ++x)
            if (terrain_->GetCell(x, y).material_id != air_id) ++count;
    return count;
}

// ─── Bresenham line tracer ────────────────────────────────────────────────────

std::vector<std::pair<int,int>> Engine::TraceLine(int x0, int y0, int x1, int y1) const {
    std::vector<std::pair<int,int>> pts;
    int dx =  std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        pts.push_back({x0, y0});
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
    return pts;
}

// ─── PaintLine ───────────────────────────────────────────────────────────────

std::vector<int> Engine::PaintLine(int x0, int y0, int x1, int y1,
                                    const std::string& mat_id, const std::string& bg_id,
                                    int brush_size) {
    if (!terrain_) return {};

    // Resolve IDs once
    MaterialID   m_id   = 0;
    BackgroundID b_id   = 0;
    bool         has_mat = false, has_bg = false;

    if (!mat_id.empty()) {
        const auto* m = registry_.GetMaterial(mat_id);
        if (m) { m_id = m->runtime_id; has_mat = true; }
    }
    if (!bg_id.empty()) {
        const auto* b = registry_.GetBackground(bg_id);
        if (b) { b_id = b->runtime_id; has_bg = true; }
    }

    int half  = brush_size / 2;
    int W     = terrain_->GetWidth();
    int H     = terrain_->GetHeight();

    // Bounding box of entire stroke (for single UpdateRegion call)
    int bb_x0 = INT_MAX, bb_y0 = INT_MAX, bb_x1 = INT_MIN, bb_y1 = INT_MIN;

    std::vector<int> cells; // flat [x, y, x, y, ...]

    // Bresenham walk
    int dx =  std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int cx = x0, cy = y0;
    for (;;) {
        // Paint brush square around (cx, cy)
        for (int bx = cx - half; bx <= cx - half + brush_size - 1; ++bx) {
            for (int by = cy - half; by <= cy - half + brush_size - 1; ++by) {
                if (bx < 0 || by < 0 || bx >= W || by >= H) continue;
                if (has_mat) terrain_->SetMaterial(bx, by, m_id);
                if (has_bg)  terrain_->SetBackground(bx, by, b_id);
                cells.push_back(bx);
                cells.push_back(by);
                if (bx < bb_x0) bb_x0 = bx;
                if (by < bb_y0) bb_y0 = by;
                if (bx > bb_x1) bb_x1 = bx;
                if (by > bb_y1) bb_y1 = by;
            }
        }
        if (cx == x1 && cy == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; cx += sx; }
        if (e2 <= dx) { err += dx; cy += sy; }
    }

    // Single renderer update for the whole stroke
    if (terrain_renderer_ && bb_x0 != INT_MAX) {
        terrain_renderer_->UpdateRegion(bb_x0, bb_y0,
                                        bb_x1 - bb_x0 + 1,
                                        bb_y1 - bb_y0 + 1);
    }

    return cells;
}

// ─── Editor world-space markers ───────────────────────────────────────────────

void Engine::SetEditorMarkers(std::vector<WorldMarker> markers) {
    editor_markers_ = std::move(markers);
}

SDL_Texture* Engine::LoadSprite(const std::string& path) {
    auto it = sprite_cache_.find(path);
    if (it != sprite_cache_.end()) return it->second;

    SDL_Texture* tex = IMG_LoadTexture(window_->GetRenderer(), path.c_str());
    if (!tex) {
        std::fprintf(stderr, "Failed to load sprite '%s': %s\n", path.c_str(), IMG_GetError());
    }
    sprite_cache_[path] = tex;  // cache even if null to avoid repeated load attempts
    return tex;
}

void Engine::DrawWorldMarkers(const std::vector<WorldMarker>& markers) {
    if (cameras_.empty() || !terrain_renderer_) return;
    SDL_Renderer* r = window_->GetRenderer();
    float scale = cameras_[0]->GetScale();
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (const auto& m : markers) {
        int sx, sy;
        cameras_[0]->WorldToScreen(m.wx, m.wy, sx, sy);
        SDL_Rect dst = { sx, sy,
                         static_cast<int>(m.w * scale),
                         static_cast<int>(m.h * scale) };

        // Try to render sprite if one is specified
        SDL_Texture* sprite = nullptr;
        if (!m.sprite_path.empty()) {
            sprite = LoadSprite(m.sprite_path);
        }

        if (sprite) {
            SDL_RenderCopy(r, sprite, nullptr, &dst);
        } else {
            // Fallback: translucent colored rect
            SDL_SetRenderDrawColor(r, m.r, m.g, m.b, 130);
            SDL_RenderFillRect(r, &dst);
            // Solid border
            SDL_SetRenderDrawColor(r, m.r, m.g, m.b, 255);
            SDL_RenderDrawRect(r, &dst);
        }

        // White selection outline
        if (m.selected) {
            SDL_Rect ol = { dst.x - 1, dst.y - 1, dst.w + 2, dst.h + 2 };
            SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
            SDL_RenderDrawRect(r, &ol);
        }
    }
}

// ─── Run ──────────────────────────────────────────────────────────────────────

void Engine::Run() {
    game_loop_->Run(*this);
}

// ─── SimTick ─────────────────────────────────────────────────────────────────

void Engine::SimTick(double dt) {
    input_->PollEvents();

    // Toggle log console with backtick
    if (input_->GetRaw().IsJustPressed(SDL_SCANCODE_GRAVE))
        log_console_visible_ = !log_console_visible_;

    // Route mouse events to the UI system (menus, HUD, etc.)
    ui_system_->HandleMouseMove(input_->GetMouseX(), input_->GetMouseY());
    if (input_->IsMouseJustPressed())
        ui_system_->HandleMouseDown(input_->GetMouseX(), input_->GetMouseY());
    if (input_->IsMouseJustReleased())
        ui_system_->HandleMouseUp(input_->GetMouseX(), input_->GetMouseY());

    // Route keyboard events to focused input elements
    static constexpr std::array<SDL_Scancode, 4> INPUT_KEYS = {
        SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_ESCAPE
    };
    for (auto sc : INPUT_KEYS) {
        if (input_->GetRaw().IsJustPressed(sc))
            ui_system_->HandleKeyDown(sc);
    }
    ui_system_->HandleTextInput(input_->GetTextInput());

    // Per-tick Lua callback (editor camera, shortcuts, etc.)
    if (tick_callback_) tick_callback_(dt);

    if (!sim_running_) return;

    // ── Gameplay simulation (only when a scenario is loaded) ──────────────────

    // Character cycling (keys 1 / 3)
    if (!controllable_entities_.empty()) {
        int n    = static_cast<int>(controllable_entities_.size());
        bool prev = input_->IsJustPressed(0, InputAction::PrevCharacter);
        bool next = input_->IsJustPressed(0, InputAction::NextCharacter);
        if (prev) active_char_index_ = (active_char_index_ - 1 + n) % n;
        if (next) active_char_index_ = (active_char_index_ + 1) % n;
        if ((prev || next) && !cameras_.empty()) {
            Entity* nc = entity_manager_->GetEntity(controllable_entities_[active_char_index_]);
            if (nc) {
                float sx = nc->pos_x - cameras_[0]->GetViewWorldWidth()  / 2.0f;
                float sy = nc->pos_y - cameras_[0]->GetViewWorldHeight() / 2.0f;
                cameras_[0]->SetPosition(sx, sy);
                cameras_[0]->ClampToBounds(terrain_->GetWidth(), terrain_->GetHeight());
            }
        }
    }

    Entity* player = nullptr;
    if (!controllable_entities_.empty())
        player = entity_manager_->GetEntity(controllable_entities_[active_char_index_]);

    if (player) UpdatePlayerControl(*player, dt);

    // Digging
    if (player && player->current_action == "Dig") {
        player->dig_timer++;
        if (player->dig_timer >= 4) {
            player->dig_timer = 0;

            int cx = static_cast<int>(player->pos_x) + player->width  / 2
                     + player->dig_dir_x * (player->width  / 2 + player->dig_radius / 2);
            int cy = static_cast<int>(player->pos_y) + player->height / 2
                     + player->dig_dir_y * (player->height / 2 + player->dig_radius / 2);

            const auto* air_mat = registry_.GetMaterial("base:Air");
            if (air_mat) {
                int dug = terrain_->DigCircle(cx, cy, player->dig_radius, air_mat->runtime_id);
                if (dug > 0) {
                    int drx = cx - player->dig_radius - 1;
                    int dry = cy - player->dig_radius - 1;
                    int drw = player->dig_radius * 2 + 3;
                    int drh = player->dig_radius * 2 + 3;
                    terrain_renderer_->UpdateRegion(drx, dry, drw, drh);
                    terrain_sim_->NotifyModified(drx, dry, drw, drh);
                }
            }
        }
    }

    physics_->Update(*entity_manager_, *terrain_, static_cast<float>(dt));
    AdvanceActions(dt);

    if (player) UpdateCameraFollow(*cameras_[0], *player);

    terrain_sim_->Update(*terrain_);
    if (terrain_sim_->HasChanges()) {
        for (auto& rect : terrain_sim_->GetDirtyRects())
            terrain_renderer_->UpdateRegion(rect.x, rect.y, rect.w, rect.h);
    }

    debug_ui_->Update(input_->GetMouseX(), input_->GetMouseY(),
                      *cameras_[0], *terrain_, registry_, player);

    entity_manager_->ProcessQueues();
}

// ─── Render ───────────────────────────────────────────────────────────────────

void Engine::Render(double alpha) {
    SDL_Renderer* r = window_->GetRenderer();
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    // Render terrain whenever it is loaded (game simulation or editor preview)
    if (terrain_renderer_) {
        terrain_renderer_->Render(r, *cameras_[0]);
    }
    // Editor entity markers rendered between terrain and UI
    if (!sim_running_ && !editor_markers_.empty()) {
        DrawWorldMarkers(editor_markers_);
    }

    if (sim_running_) {
        RenderEntities(r, alpha);
        debug_ui_->Render(r);
    }

    ui_system_->Render();

    if (log_console_visible_)
        log_console_->Render(r);

    SDL_RenderPresent(r);
}

// ─── ShouldQuit ──────────────────────────────────────────────────────────────

bool Engine::ShouldQuit() const {
    return input_->ShouldQuit() || quit_requested_;
}

void Engine::SetWindowTitle(const char* title) {
    SDL_SetWindowTitle(window_->GetWindow(), title);
}

// ─── Player control (simulation only) ────────────────────────────────────────

void Engine::UpdatePlayerControl(Entity& entity, double dt) {
    float walk_speed = 120.0f;
    float jump_vel   = -280.0f;

    if (entity.definition) {
        auto it = entity.definition->properties.find("walk_speed");
        if (it != entity.definition->properties.end()) walk_speed = it->second;
        it = entity.definition->properties.find("jump_velocity");
        if (it != entity.definition->properties.end()) jump_vel = it->second;
    }

    float move_x   = input_->GetAxis(0, InputAction::MoveX);
    bool jump      = input_->IsJustPressed(0, InputAction::Jump);
    bool dig_down  = input_->IsPressed(0, InputAction::DigDown);
    bool dig_horiz = input_->IsPressed(0, InputAction::DigHorizontal) && std::abs(move_x) > 0.1f;

    if (move_x < 0.0f) {
        entity.vel_x = move_x * walk_speed;
        entity.facing = -1;
        if (entity.on_ground && entity.current_action != "Jump")
            entity.current_action = "Walk";
    } else if (move_x > 0.0f) {
        entity.vel_x = move_x * walk_speed;
        entity.facing = 1;
        if (entity.on_ground && entity.current_action != "Jump")
            entity.current_action = "Walk";
    } else {
        if (entity.on_ground) {
            entity.vel_x = 0.0f;
            if (entity.current_action == "Walk")
                entity.current_action = "Idle";
        } else {
            entity.vel_x *= 0.95f;
        }
    }

    if (jump && entity.on_ground) {
        entity.vel_y = jump_vel;
        entity.on_ground = false;
        entity.current_action = "Jump";
    }

    if ((dig_down || dig_horiz) && entity.on_ground && entity.current_action != "Jump") {
        entity.current_action = "Dig";
        entity.vel_x = 0.0f;
        if (dig_down) { entity.dig_dir_x = 0;              entity.dig_dir_y = 1; }
        else          { entity.dig_dir_x = entity.facing;  entity.dig_dir_y = 0; }
    } else if (entity.current_action == "Dig" && !dig_down && !dig_horiz) {
        entity.current_action = "Idle";
        entity.dig_timer = 0;
    }

    if (!entity.on_ground) {
        if (entity.vel_y > 0.0f && entity.current_action != "Fall")
            entity.current_action = "Fall";
    } else if (entity.current_action == "Fall" || entity.current_action == "Jump") {
        entity.current_action = "Idle";
    }

    (void)dt;
}

void Engine::UpdateCameraFollow(Camera& cam, const Entity& target) {
    float tx = target.pos_x - cam.GetViewWorldWidth()  / 2.0f;
    float ty = target.pos_y - cam.GetViewWorldHeight() / 2.0f;
    float cx = cam.GetX() + (tx - cam.GetX()) * 0.1f;
    float cy = cam.GetY() + (ty - cam.GetY()) * 0.1f;
    cam.SetPosition(cx, cy);
    cam.ClampToBounds(terrain_->GetWidth(), terrain_->GetHeight());
}

void Engine::AdvanceActions(double /*dt*/) {
    entity_manager_->ForEach([&](Entity& entity) {
        auto map_it = action_maps_.find(entity.definition ? entity.definition->qualified_id : "");
        if (map_it == action_maps_.end()) return;

        const ActionDef* action = map_it->second.GetAction(entity.current_action);
        if (!action) return;

        entity.action_timer++;
        if (entity.action_timer >= action->delay) {
            entity.action_timer = 0;
            entity.action_frame++;
            if (entity.action_frame >= action->frames)
                entity.action_frame = 0;
        }
    });
}

void Engine::RenderEntities(SDL_Renderer* renderer, double alpha) {
    entity_manager_->ForEach([&](const Entity& entity) {
        float x = entity.prev_pos_x + (entity.pos_x - entity.prev_pos_x) * static_cast<float>(alpha);
        float y = entity.prev_pos_y + (entity.pos_y - entity.prev_pos_y) * static_cast<float>(alpha);

        int sx, sy;
        cameras_[0]->WorldToScreen(x, y, sx, sy);

        float scale = cameras_[0]->GetScale();
        SDL_Rect dst = {
            sx, sy,
            static_cast<int>(entity.width  * scale),
            static_cast<int>(entity.height * scale)
        };

        if      (entity.current_action == "Dig")                               SDL_SetRenderDrawColor(renderer, 220, 120,  50, 255);
        else if (entity.current_action == "Walk")                              SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255);
        else if (entity.current_action == "Jump" || entity.current_action == "Fall") SDL_SetRenderDrawColor(renderer, 100, 100, 255, 255);
        else                                                                   SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);

        SDL_RenderFillRect(renderer, &dst);

        int iw = static_cast<int>(3 * scale);
        int ih = static_cast<int>(3 * scale);
        int ix = entity.facing > 0 ? dst.x + dst.w - iw : dst.x;
        int iy = dst.y + static_cast<int>(4 * scale);
        SDL_Rect eye = {ix, iy, iw, ih};
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_RenderFillRect(renderer, &eye);
    });
}
