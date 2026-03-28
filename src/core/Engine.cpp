#include "core/Engine.h"
#include "core/FixedPoint.h"
#include <cassert>
#include <cstdio>

Engine::Engine() = default;
Engine::~Engine() = default;

void Engine::Init() {
    // Verify FixedPoint sanity
    {
        auto a = Fixed::FromInt(3);
        auto b = Fixed::FromInt(4);
        assert((a * b) == Fixed::FromInt(12));
        auto c = Fixed::FromFloat(1.5f);
        float cf = c.ToFloat();
        assert(cf > 1.49f && cf < 1.51f);
        std::printf("FixedPoint sanity checks passed.\n");
    }

    window_ = std::make_unique<Window>("Aeterium", WINDOW_WIDTH, WINDOW_HEIGHT);
    input_ = std::make_unique<InputSystem>();
    camera_ = std::make_unique<Camera>(WINDOW_WIDTH, WINDOW_HEIGHT, 2.0f);

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
    physics_ = std::make_unique<PhysicsSystem>(registry_);
    terrain_sim_ = std::make_unique<TerrainSimulator>(registry_);

    // Find spawn position (TODO: temporary, need to be moved to a system resposible for creating objects on the map, and pass the scenario start objects)
    int spawn_x, spawn_y;
    if (scenario && !scenario->players.empty()) {
        auto positions = MapGenerator::FindSpawnPositions(*terrain_, registry_, scenario->players);
        spawn_x = positions[0].x;
        spawn_y = positions[0].y;
    } else {
        spawn_x = terrain_->GetWidth() / 2;
        spawn_y = MapGenerator::FindSurfaceY(*terrain_, spawn_x, registry_) - 21;
    }

    player_entity_id_ = entity_manager_->Spawn(
        "base:Character", //TODO: need to pull the type from the scenario
        Fixed::FromInt(spawn_x),
        Fixed::FromInt(spawn_y)
    );

    // Center camera on player
    float cam_x = static_cast<float>(spawn_x) - camera_->GetViewWorldWidth() / 2.0f;
    float cam_y = static_cast<float>(spawn_y) - camera_->GetViewWorldHeight() / 2.0f;
    camera_->SetPosition(cam_x, cam_y);
    camera_->ClampToBounds(terrain_->GetWidth(), terrain_->GetHeight());

    debug_ui_ = std::make_unique<DebugUI>(window_->GetRenderer());
    game_loop_ = std::make_unique<GameLoop>(60);

    std::printf("Engine initialized: %dx%d terrain, %dx%d window\n", 
        TERRAIN_WIDTH, TERRAIN_HEIGHT, WINDOW_WIDTH, WINDOW_HEIGHT);
}

void Engine::Run() {
    game_loop_->Run(*this);
}

void Engine::UpdatePlayerControl(Entity& entity, double dt) {
    float walk_speed = 120.0f;
    float jump_vel = -280.0f;

    // Read from definition properties
    if (entity.definition) {
        auto it = entity.definition->properties.find("walk_speed");
        if (it != entity.definition->properties.end()) walk_speed = it->second;
        it = entity.definition->properties.find("jump_velocity");
        if (it != entity.definition->properties.end()) jump_vel = it->second;
    }

    bool move_left = input_->IsKeyDown(SDL_SCANCODE_A);
    bool move_right = input_->IsKeyDown(SDL_SCANCODE_D);
    bool jump = input_->IsJustPressed(SDL_SCANCODE_W) || input_->IsJustPressed(SDL_SCANCODE_SPACE);

    // Horizontal movement
    if (move_left && !move_right) {
        entity.vel_x = Fixed::FromFloat(-walk_speed);
        entity.facing = -1;
        if (entity.on_ground && entity.current_action != "Jump") {
            entity.current_action = "Walk";
        }
    } else if (move_right && !move_left) {
        entity.vel_x = Fixed::FromFloat(walk_speed);
        entity.facing = 1;
        if (entity.on_ground && entity.current_action != "Jump") {
            entity.current_action = "Walk";
        }
    } else {
        // Apply friction when no input
        if (entity.on_ground) {
            entity.vel_x = Fixed::Zero();
            if (entity.current_action == "Walk") {
                entity.current_action = "Idle";
            }
        } else {
            // Air drag
            entity.vel_x = Fixed::FromRaw(entity.vel_x.raw * 95 / 100);
        }
    }

    // Jump
    if (jump && entity.on_ground) {
        entity.vel_y = Fixed::FromFloat(jump_vel);
        entity.on_ground = false;
        entity.current_action = "Jump";
    }

    // Dig input: Q+S = dig down, C+direction = dig horizontal
    bool dig_down = input_->IsKeyDown(SDL_SCANCODE_Q) && input_->IsKeyDown(SDL_SCANCODE_S);
    bool dig_horiz = input_->IsKeyDown(SDL_SCANCODE_C) && (move_left || move_right);

    if ((dig_down || dig_horiz) && entity.on_ground && entity.current_action != "Jump") {
        entity.current_action = "Dig";
        entity.vel_x = Fixed::Zero();
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
        if (entity.vel_y > Fixed::Zero() && entity.current_action != "Fall") {
            entity.current_action = "Fall";
        }
    } else if (entity.current_action == "Fall" || entity.current_action == "Jump") {
        entity.current_action = "Idle";
    }
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
                // Check for length-based completion
                // For now just loop
            }
        }
    });
}

void Engine::RenderEntities(SDL_Renderer* renderer, double alpha) {
    entity_manager_->ForEach([&](const Entity& entity) {
        // Interpolate position
        float x = entity.prev_pos_x.ToFloat() + (entity.pos_x.ToFloat() - entity.prev_pos_x.ToFloat()) * static_cast<float>(alpha);
        float y = entity.prev_pos_y.ToFloat() + (entity.pos_y.ToFloat() - entity.prev_pos_y.ToFloat()) * static_cast<float>(alpha);

        int sx, sy;
        camera_->WorldToScreen(x, y, sx, sy);

        float scale = camera_->GetScale();
        SDL_Rect dst = {
            sx, sy,
            static_cast<int>(entity.width * scale),
            static_cast<int>(entity.height * scale)
        };

        // Color based on action
        if (entity.current_action == "Dig") {
            SDL_SetRenderDrawColor(renderer, 220, 120, 50, 255); // orange
        } else if (entity.current_action == "Walk") {
            SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255); // green
        } else if (entity.current_action == "Jump" || entity.current_action == "Fall") {
            SDL_SetRenderDrawColor(renderer, 100, 100, 255, 255); // blue
        } else {
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); // white/gray
        }
        SDL_RenderFillRect(renderer, &dst);

        // Draw a direction indicator
        int indicator_w = static_cast<int>(3 * scale);
        int indicator_h = static_cast<int>(3 * scale);
        int indicator_x = entity.facing > 0 ?
            dst.x + dst.w - indicator_w : dst.x;
        int indicator_y = dst.y + static_cast<int>(4 * scale);
        SDL_Rect eye = {indicator_x, indicator_y, indicator_w, indicator_h};
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_RenderFillRect(renderer, &eye);
    });
}

void Engine::SimTick(double dt) {
    input_->PollEvents();

    // Player control
    Entity* player = entity_manager_->GetEntity(player_entity_id_);
    if (player) {
        UpdatePlayerControl(*player, dt);
    }

    // Process digging
    if (player && player->current_action == "Dig") {
        player->dig_timer++;
        // Dig every 4 ticks (hardness affects this in the future)
        if (player->dig_timer >= 4) {
            player->dig_timer = 0;

            // Dig center: offset from entity center in dig direction
            int cx = player->pos_x.ToInt() + player->width / 2 + player->dig_dir_x * (player->width / 2 + player->dig_radius / 2);
            int cy = player->pos_y.ToInt() + player->height / 2 + player->dig_dir_y * (player->height / 2 + player->dig_radius / 2);

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
    physics_->Update(*entity_manager_, *terrain_, Fixed::FromFloat(static_cast<float>(dt)));

    // Advance animations
    AdvanceActions(dt);

    // Camera follows player
    if (player) {
        float px = player->pos_x.ToFloat();
        float py = player->pos_y.ToFloat();
        float target_x = px - camera_->GetViewWorldWidth() / 2.0f;
        float target_y = py - camera_->GetViewWorldHeight() / 2.0f;

        // Smooth follow
        float cam_x = camera_->GetX() + (target_x - camera_->GetX()) * 0.1f;
        float cam_y = camera_->GetY() + (target_y - camera_->GetY()) * 0.1f;
        camera_->SetPosition(cam_x, cam_y);
        camera_->ClampToBounds(terrain_->GetWidth(), terrain_->GetHeight());
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
                      *camera_, *terrain_, registry_, player);

    entity_manager_->ProcessQueues();
}

void Engine::Render(double alpha) {
    SDL_Renderer* r = window_->GetRenderer();
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    terrain_renderer_->Render(r, *camera_);
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
