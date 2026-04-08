#include "core/Engine.h"
#include "core/EngineLog.h"
#include "game/GameLoader.h"
#include "input/InputAction.h"
#include <cmath>
#include <cstdio>

Engine::Engine() = default;
Engine::~Engine() = default;

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

    // 5. Load content packages declared by the game definition
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

    if (sim_running_) {
        terrain_renderer_->Render(r, *cameras_[0]);
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
