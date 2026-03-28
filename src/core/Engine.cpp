#include "core/Engine.h"
#include "input/InputAction.h"
#include <cmath>
#include <cstdio>

Engine::Engine() = default;
Engine::~Engine() = default;

void Engine::Init() {
    window_ = std::make_unique<Window>("Aeterium", WINDOW_WIDTH, WINDOW_HEIGHT);
    input_  = std::make_unique<InputManager>(1);  // 1 local player slot

    // Camera for player 0 (full viewport). Future splitscreen adds more cameras here.
    cameras_.push_back(std::make_unique<Camera>(WINDOW_WIDTH, WINDOW_HEIGHT, 2.0f));

    // Load packages
    PackageLoader loader(registry_);
    loader.LoadAll("packages");

    // Load action maps for objects that have animations.toml
    for (auto& [qid, obj_ptr] : registry_.GetAllObjects()) {
        if (!obj_ptr->animations_path.empty()) {
            ActionMap am;
            if (am.LoadFromFile(obj_ptr->animations_path)) {
                action_maps_[qid] = std::move(am);
            }
        }
    }

    std::printf("Loaded %d materials, %d backgrounds\n",
        registry_.GetMaterialCount(), registry_.GetBackgroundCount());

    // Load and generate from scenario (try GoldRush first)
    auto scenario = ScenarioLoader::LoadFromFile("packages/base/scenarios/GoldRush/scenario.json");
    terrain_ = std::make_unique<Terrain>(
        MapGenerator::GenerateFromScenario(*scenario, registry_)
    );

    terrain_renderer_ = std::make_unique<TerrainRenderer>(
        window_->GetRenderer(), *terrain_, &registry_
    );
    terrain_renderer_->FullRebuild();

    // Create systems
    entity_manager_ = std::make_unique<EntityManager>(registry_);
    physics_        = std::make_unique<PhysicsSystem>(registry_);
    terrain_sim_    = std::make_unique<TerrainSimulator>(registry_);

    // Find spawn position (TODO: need to be moved to a system responsible for creating objects
    // on the map, and pass the scenario start objects)
    int spawn_x, spawn_y;
    if (scenario && !scenario->players.empty()) {
        auto positions = MapGenerator::FindSpawnPositions(*terrain_, registry_, scenario->players);
        spawn_x = positions[0].x;
        spawn_y = positions[0].y;
    } else {
        spawn_x = terrain_->GetWidth() / 2;
        spawn_y = MapGenerator::FindSurfaceY(*terrain_, spawn_x, registry_) - 21;
    }

    EntityID spawned = entity_manager_->Spawn(
        "base:Character",  // TODO: pull type from scenario
        static_cast<float>(spawn_x),
        static_cast<float>(spawn_y)
    );

    // Collect all player-controllable entities
    entity_manager_->ForEach([&](const Entity& e) {
        if (e.definition && e.definition->player_controllable) {
            controllable_entities_.push_back(e.id);
        }
    });
    active_char_index_ = 0;
    (void)spawned;

    // Center camera on player
    float cam_x = static_cast<float>(spawn_x) - cameras_[0]->GetViewWorldWidth()  / 2.0f;
    float cam_y = static_cast<float>(spawn_y) - cameras_[0]->GetViewWorldHeight() / 2.0f;
    cameras_[0]->SetPosition(cam_x, cam_y);
    cameras_[0]->ClampToBounds(terrain_->GetWidth(), terrain_->GetHeight());

    debug_ui_  = std::make_unique<DebugUI>(window_->GetRenderer());
    game_loop_ = std::make_unique<GameLoop>(60);

    std::printf("Engine initialized: %dx%d terrain, %dx%d window\n",
        TERRAIN_WIDTH, TERRAIN_HEIGHT, WINDOW_WIDTH, WINDOW_HEIGHT);
}

void Engine::Run() {
    game_loop_->Run(*this);
}

void Engine::UpdatePlayerControl(Entity& entity, double dt) {
    float walk_speed = 120.0f;
    float jump_vel   = -280.0f;

    if (entity.definition) {
        auto it = entity.definition->properties.find("walk_speed");
        if (it != entity.definition->properties.end()) walk_speed = it->second;
        it = entity.definition->properties.find("jump_velocity");
        if (it != entity.definition->properties.end()) jump_vel = it->second;
    }

    // MoveX is an axis action: -1 = full left, +1 = full right, intermediate for analog gamepad
    float move_x   = input_->GetAxis(0, InputAction::MoveX);
    bool jump      = input_->IsJustPressed(0, InputAction::Jump);
    bool dig_down  = input_->IsPressed(0, InputAction::DigDown);
    bool dig_horiz = input_->IsPressed(0, InputAction::DigHorizontal) && std::abs(move_x) > 0.1f;

    // Horizontal movement — velocity scales with analog stick for gamepad, snaps to ±1 for keyboard
    if (move_x < 0.0f) {
        entity.vel_x = move_x * walk_speed;
        entity.facing = -1;
        if (entity.on_ground && entity.current_action != "Jump") {
            entity.current_action = "Walk";
        }
    } else if (move_x > 0.0f) {
        entity.vel_x = move_x * walk_speed;
        entity.facing = 1;
        if (entity.on_ground && entity.current_action != "Jump") {
            entity.current_action = "Walk";
        }
    } else {
        if (entity.on_ground) {
            entity.vel_x = 0.0f;
            if (entity.current_action == "Walk") {
                entity.current_action = "Idle";
            }
        } else {
            entity.vel_x *= 0.95f;  // air drag
        }
    }

    // Jump
    if (jump && entity.on_ground) {
        entity.vel_y = jump_vel;
        entity.on_ground = false;
        entity.current_action = "Jump";
    }

    // Dig input
    if ((dig_down || dig_horiz) && entity.on_ground && entity.current_action != "Jump") {
        entity.current_action = "Dig";
        entity.vel_x = 0.0f;
        if (dig_down) {
            entity.dig_dir_x = 0;
            entity.dig_dir_y = 1;
        } else {
            entity.dig_dir_x = entity.facing;
            entity.dig_dir_y = 0;
        }
    } else if (entity.current_action == "Dig" && !dig_down && !dig_horiz) {
        entity.current_action = "Idle";
        entity.dig_timer = 0;
    }

    // Action transitions
    if (!entity.on_ground) {
        if (entity.vel_y > 0.0f && entity.current_action != "Fall") {
            entity.current_action = "Fall";
        }
    } else if (entity.current_action == "Fall" || entity.current_action == "Jump") {
        entity.current_action = "Idle";
    }

    (void)dt;
}

void Engine::UpdateCameraFollow(Camera& cam, const Entity& target) {
    float px = target.pos_x;
    float py = target.pos_y;
    float tx = px - cam.GetViewWorldWidth()  / 2.0f;
    float ty = py - cam.GetViewWorldHeight() / 2.0f;
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
            if (entity.action_frame >= action->frames) {
                entity.action_frame = 0;
            }
        }
    });
}

void Engine::RenderEntities(SDL_Renderer* renderer, double alpha) {
    entity_manager_->ForEach([&](const Entity& entity) {
        // Interpolate position between simulation ticks
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

        // Color based on action state
        if (entity.current_action == "Dig") {
            SDL_SetRenderDrawColor(renderer, 220, 120, 50, 255);  // orange
        } else if (entity.current_action == "Walk") {
            SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255); // green
        } else if (entity.current_action == "Jump" || entity.current_action == "Fall") {
            SDL_SetRenderDrawColor(renderer, 100, 100, 255, 255); // blue
        } else {
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); // gray (idle)
        }
        SDL_RenderFillRect(renderer, &dst);

        // Direction indicator (eye)
        int indicator_w = static_cast<int>(3 * scale);
        int indicator_h = static_cast<int>(3 * scale);
        int indicator_x = entity.facing > 0 ?
            dst.x + dst.w - indicator_w : dst.x;
        int indicator_y = dst.y + static_cast<int>(4 * scale);
        SDL_Rect eye = { indicator_x, indicator_y, indicator_w, indicator_h };
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_RenderFillRect(renderer, &eye);
    });
}

void Engine::SimTick(double dt) {
    input_->PollEvents();

    // Character cycling (slot 0 keys: 1 = previous, 3 = next)
    if (!controllable_entities_.empty()) {
        int n = static_cast<int>(controllable_entities_.size());
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

    // Active player control
    Entity* player = nullptr;
    if (!controllable_entities_.empty()) {
        player = entity_manager_->GetEntity(controllable_entities_[active_char_index_]);
    }
    if (player) {
        UpdatePlayerControl(*player, dt);
    }

    // Process digging
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

    // Physics
    physics_->Update(*entity_manager_, *terrain_, static_cast<float>(dt));

    // Advance animations
    AdvanceActions(dt);

    // Camera follows active player
    if (player && !cameras_.empty()) {
        UpdateCameraFollow(*cameras_[0], *player);
    }

    // Terrain simulation (powder/liquid)
    terrain_sim_->Update(*terrain_);
    if (terrain_sim_->HasChanges()) {
        for (auto& rect : terrain_sim_->GetDirtyRects()) {
            terrain_renderer_->UpdateRegion(rect.x, rect.y, rect.w, rect.h);
        }
    }

    // Debug UI update
    debug_ui_->Update(input_->GetMouseX(), input_->GetMouseY(),
                      *cameras_[0], *terrain_, registry_, player);

    entity_manager_->ProcessQueues();
}

void Engine::Render(double alpha) {
    SDL_Renderer* r = window_->GetRenderer();
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    // Future splitscreen: iterate cameras_ and set SDL viewport per camera.
    terrain_renderer_->Render(r, *cameras_[0]);
    RenderEntities(r, alpha);
    debug_ui_->Render(r);

    SDL_RenderPresent(r);
}

bool Engine::ShouldQuit() const {
    return input_->ShouldQuit();
}

void Engine::SetWindowTitle(const char* title) {
    SDL_SetWindowTitle(window_->GetWindow(), title);
}
