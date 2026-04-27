-- game_screen.lua — Core gameplay: map generation, ship control, weapons, HUD.
--
-- Architecture overview:
--   start()         → generate map → spawn player → show HUD → register tick
--   tick callback   → handle input → physics forces → fire weapons → tick projectiles
--                   → camera follow → update HUD → check victory

local GameState = require("game_state")
local HUD       = require("hud")

local M = {}

-- ─── Ship catalogue (mirrors TOML definitions for readable Lua lookups) ────────

local SHIP_NAMES = {
    ["europa:Interceptor"] = "Interceptor",
    ["europa:Scout"]       = "Scout",
    ["europa:Bomber"]      = "Bomber",
}

-- ─── Map generation ─────────────────────────────────────────────────────────────

local function generate_map()
    engine.terrain.generate({
        width  = GameState.map_width,
        height = GameState.map_height,
        seed   = 0,
        shape  = "flat",
        shape_params = { surface_level = 0.01, roughness = 0.0 },
        materials = {
            { id = "base:Air",           rule = "above_surface" },
            { id = "europa:IceRock",     rule = "deep",  min_depth = 80, background = "europa:DeepIce" },
            { id = "europa:FrozenCrust", rule = "fill",  background = "europa:IceWall" },
        },
        features = {
            { type = "caves", density = 0.20, min_size = 24, max_size = 60,  zone = "all" },
            { type = "caves", density = 0.07, min_size = 55, max_size = 110, zone = "all" },
            { type = "ore_veins", material = "europa:IceSlush", density = 0.012, vein_radius = 10, zone = "all" },
        },
    })
    engine.sim.set_time_scale(1)
end

-- ─── Spawn helper ─────────────────────────────────────────────────────────────

-- Probe random positions until one is open air, with a minimum clearance radius.
local function find_open_spawn(W, H, clearance)
    clearance = clearance or 16
    for _ = 1, 300 do
        local x = math.random(clearance + 10, W - clearance - 10)
        local y = math.random(clearance + 10, H - clearance - 10)
        -- Check a small cross of cells to ensure enough room for the ship
        local ok = true
        for dy = -clearance // 2, clearance // 2, clearance // 4 do
            for dx = -clearance // 2, clearance // 2, clearance // 4 do
                local cell = engine.terrain.get_cell(x + dx, y + dy)
                if not cell or cell.material_id ~= "base:Air" then
                    ok = false
                    break
                end
            end
            if not ok then break end
        end
        if ok then return x, y end
    end
    -- Fallback: map centre (tunnels should have air there after generation)
    return W // 2, H // 2
end

local function spawn_player()
    local sx, sy = find_open_spawn(GameState.map_width, GameState.map_height, 14)
    local eid = engine.entity.spawn(GameState.player_ship_type, sx, sy)
    if eid == 0 then
        engine.log("ERROR: failed to spawn " .. GameState.player_ship_type)
        return
    end

    GameState.player_entity_id = eid
    GameState.is_dead           = false
    GameState.fire_timer        = 0

    -- Centre camera on the ship
    local x, y = engine.entity.get_position(eid)
    local zoom = engine.camera.get_zoom()
    engine.camera.set_position(x - (GameState.map_width  / zoom) * 0.5,
                               y - (GameState.map_height / zoom) * 0.5)

    engine.log("Spawned " .. GameState.player_ship_type .. " at " .. sx .. "," .. sy)
end

-- ─── Ship control ─────────────────────────────────────────────────────────────
--
-- Thrust forces are applied in world space using the ship's current angle.
-- W = forward along nose, S = reverse, A/D = lateral strafe, Q/E = rotate.
-- A soft speed cap scales down thrust when approaching max_speed.

local function apply_controls(eid)
    local thrust_fwd = engine.entity.get_property(eid, "thrust_forward") or 3000
    local thrust_lat = engine.entity.get_property(eid, "thrust_lateral") or 1500
    local ang_thrust = engine.entity.get_property(eid, "angular_thrust")  or 1200
    local max_speed  = engine.entity.get_property(eid, "max_speed")       or 300

    local angle = engine.entity.get_angle(eid)
    local cos_a = math.cos(angle)
    local sin_a = math.sin(angle)

    local fx, fy = 0, 0

    if engine.input.is_key_down(engine.key.W) then
        fx = fx + cos_a * thrust_fwd
        fy = fy + sin_a * thrust_fwd
    end
    if engine.input.is_key_down(engine.key.S) then
        fx = fx - cos_a * thrust_fwd * 0.55
        fy = fy - sin_a * thrust_fwd * 0.55
    end
    -- Strafe: perpendicular to forward direction
    if engine.input.is_key_down(engine.key.A) then
        fx = fx + sin_a * thrust_lat
        fy = fy - cos_a * thrust_lat
    end
    if engine.input.is_key_down(engine.key.D) then
        fx = fx - sin_a * thrust_lat
        fy = fy + cos_a * thrust_lat
    end

    if fx ~= 0 or fy ~= 0 then
        -- Soft speed cap: reduce thrust when already near max_speed
        local vx, vy = engine.entity.get_velocity(eid)
        local speed2 = vx * vx + vy * vy
        local scale = 1.0
        if speed2 > max_speed * max_speed then
            scale = (max_speed / math.sqrt(speed2)) * 0.4
        end
        engine.entity.apply_force(eid, fx * scale, fy * scale)
    end

    -- Rotation torque
    if engine.input.is_key_down(engine.key.Q) then
        engine.entity.apply_torque(eid, -ang_thrust)
    end
    if engine.input.is_key_down(engine.key.E) then
        engine.entity.apply_torque(eid, ang_thrust)
    end
end

-- ─── Weapon firing ────────────────────────────────────────────────────────────
--
-- weapon_type 1 = Plasma Cannon  (PlasmaShot)
-- weapon_type 2 = Rocket Launcher (Rocket)
-- weapon_type 3 = Spread Shot    (three SpreadShots in a fan)

local function try_fire(eid)
    if GameState.fire_timer > 0 then return end

    local weapon_type = engine.entity.get_property(eid, "weapon_type") or 1
    local fire_rate   = engine.entity.get_property(eid, "fire_rate_ticks") or 10
    local angle       = engine.entity.get_angle(eid)
    local cos_a       = math.cos(angle)
    local sin_a       = math.sin(angle)
    local px, py      = engine.entity.get_position(eid)
    local vx, vy      = engine.entity.get_velocity(eid)

    -- Muzzle is 15 px ahead of the entity centre along its nose
    local mx = px + cos_a * 15
    local my = py + sin_a * 15

    if weapon_type == 1 then
        -- Plasma Cannon: single fast projectile, inherits a fraction of ship velocity
        local sid = engine.entity.spawn("europa:PlasmaShot", mx, my)
        if sid ~= 0 then
            local spd = engine.entity.get_property(sid, "speed") or 620
            engine.entity.set_velocity(sid, cos_a * spd + vx * 0.25,
                                            sin_a * spd + vy * 0.25)
            engine.entity.set_property(sid, "owner_id", eid)
            local ttl = engine.entity.get_property(sid, "lifetime_ticks") or 120
            GameState.projectiles[sid] = { owner = eid, ptype = 1, life = ttl }
        end

    elseif weapon_type == 2 then
        -- Rocket Launcher: slower, big explosion on impact
        local rid = engine.entity.spawn("europa:Rocket", mx, my)
        if rid ~= 0 then
            local spd = engine.entity.get_property(rid, "speed") or 390
            engine.entity.set_velocity(rid, cos_a * spd, sin_a * spd)
            engine.entity.set_property(rid, "owner_id", eid)
            local ttl = engine.entity.get_property(rid, "lifetime_ticks") or 200
            GameState.projectiles[rid] = { owner = eid, ptype = 2, life = ttl }
        end

    elseif weapon_type == 3 then
        -- Spread Shot: three pellets fanning out ±0.28 rad (~16°)
        local fan_angles = { angle - 0.28, angle, angle + 0.28 }
        for _, a in ipairs(fan_angles) do
            local sid = engine.entity.spawn("europa:SpreadShot", mx, my)
            if sid ~= 0 then
                local spd = engine.entity.get_property(sid, "speed") or 480
                engine.entity.set_velocity(sid, math.cos(a) * spd, math.sin(a) * spd)
                engine.entity.set_property(sid, "owner_id", eid)
                local ttl = engine.entity.get_property(sid, "lifetime_ticks") or 80
                GameState.projectiles[sid] = { owner = eid, ptype = 3, life = ttl }
            end
        end
    end

    GameState.fire_timer = fire_rate
end

-- ─── Projectile tick ──────────────────────────────────────────────────────────
--
-- Each active projectile is polled every tick:
--   1. Decrement lifetime; destroy if expired.
--   2. Check if the current cell is solid (terrain hit).
--   3. Check spatial proximity to ship entities (direct hit).

-- Ship definition IDs for quick identity checks
local SHIP_DEFS = {
    ["europa:Interceptor"] = true,
    ["europa:Scout"]       = true,
    ["europa:Bomber"]      = true,
}

local function tick_projectiles()
    local to_remove = {}

    for pid, pdata in pairs(GameState.projectiles) do
        pdata.life = pdata.life - 1

        if pdata.life <= 0 then
            engine.entity.destroy(pid)
            to_remove[#to_remove + 1] = pid
        else
            local px, py = engine.entity.get_position(pid)

            -- Terrain collision check
            local cell = engine.terrain.get_cell(math.floor(px), math.floor(py))
            local hit_terrain = cell and
                cell.material_id ~= "base:Air" and
                cell.material_id ~= "" and
                cell.material_id ~= "europa:Vacuum"

            if hit_terrain then
                if pdata.ptype == 2 then
                    -- Rocket: boom!
                    local rad = engine.entity.get_property(pid, "explosion_radius")   or 36
                    local str = engine.entity.get_property(pid, "explosion_strength") or 14
                    engine.sim.trigger_explosion(math.floor(px), math.floor(py),
                                                 math.floor(rad), math.floor(str))
                end
                engine.entity.destroy(pid)
                to_remove[#to_remove + 1] = pid

            else
                -- Entity hit: find nearby ships (exclusion radius ~10 px)
                local hits = engine.entity.find_entities(px, py, 12)
                local destroyed = false
                for _, hit_id in ipairs(hits) do
                    if hit_id ~= pdata.owner and hit_id ~= pid then
                        local def_id = engine.entity.get_def_id(hit_id)
                        if SHIP_DEFS[def_id] then
                            local dmg = engine.entity.get_property(pid, "damage") or 20
                            if pdata.ptype == 2 then
                                local rad = engine.entity.get_property(pid, "explosion_radius")   or 36
                                local str = engine.entity.get_property(pid, "explosion_strength") or 14
                                engine.sim.trigger_explosion(math.floor(px), math.floor(py),
                                                             math.floor(rad), math.floor(str))
                            end
                            engine.entity.deal_damage(hit_id, dmg)
                            engine.entity.destroy(pid)
                            to_remove[#to_remove + 1] = pid
                            destroyed = true
                            break
                        end
                    end
                end
                if not destroyed and not engine.entity.is_valid(pid) then
                    to_remove[#to_remove + 1] = pid
                end
            end
        end
    end

    for _, pid in ipairs(to_remove) do
        GameState.projectiles[pid] = nil
    end
end

-- ─── Death callback ───────────────────────────────────────────────────────────

local function on_entity_died(eid)
    if eid == GameState.player_entity_id then
        GameState.is_dead           = true
        GameState.player_entity_id  = 0
        GameState.deaths            = GameState.deaths + 1
        GameState.respawn_timer     = GameState.respawn_delay_ticks
        engine.log("Player destroyed. Respawning in 3s.")
    end
end

-- ─── Camera follow ─────────────────────────────────────────────────────────────
--
-- Exponential lerp toward the ship centre at ~12 % per tick for smooth tracking.

local function update_camera(eid)
    local ex, ey = engine.entity.get_position(eid)
    local zoom   = engine.camera.get_zoom()
    local vw     = GameState.map_width  / zoom
    local vh     = GameState.map_height / zoom
    local tx     = ex - vw * 0.5
    local ty     = ey - vh * 0.5
    local cx     = engine.camera.get_x() + (tx - engine.camera.get_x()) * 0.12
    local cy     = engine.camera.get_y() + (ty - engine.camera.get_y()) * 0.12
    engine.camera.set_position(cx, cy)
end

-- ─── Tick loop ────────────────────────────────────────────────────────────────

local function register_tick()
    engine.entity.on_death(on_entity_died)
    local ship_name = SHIP_NAMES[GameState.player_ship_type] or "Ship"

    engine.set_tick_callback(function(_dt)
        -- ESC → pause
        if engine.input.is_key_just_pressed(engine.key.ESCAPE) then
            local Pause = require("pause_menu")
            Pause.show()
            return
        end

        -- ── Respawn phase ───────────────────────────────────────────────────
        if GameState.is_dead then
            GameState.respawn_timer = GameState.respawn_timer - 1
            HUD.update_dead(GameState.respawn_timer)
            if GameState.respawn_timer <= 0 then
                spawn_player()
            end
            return
        end

        local eid = GameState.player_entity_id
        if eid == 0 or not engine.entity.is_valid(eid) then return end

        -- ── Ship controls ───────────────────────────────────────────────────
        apply_controls(eid)

        -- ── Weapon fire ────────────────────────────────────────────────────
        if GameState.fire_timer > 0 then
            GameState.fire_timer = GameState.fire_timer - 1
        end
        if engine.input.is_key_down(engine.key.SPACE) then
            try_fire(eid)
        end

        -- ── Projectile lifecycle ────────────────────────────────────────────
        tick_projectiles()

        -- ── Camera ─────────────────────────────────────────────────────────
        update_camera(eid)

        -- ── HUD update ──────────────────────────────────────────────────────
        local hp = engine.entity.get_property(eid, "hp") or 0
        HUD.update(hp, GameState.kills, GameState.deaths, GameState.fire_timer, ship_name)

        -- ── Victory check ───────────────────────────────────────────────────
        if GameState.kills >= GameState.frag_limit then
            engine.log("VICTORY! " .. GameState.kills .. " frags — returning to menu.")
            engine.terrain.unload()
            GameState.reset()
            engine.ui.pop_screen()  -- hud screen
            local MainMenu = require("main_menu")
            MainMenu.show()
        end
    end)
end

-- ─── Public API ───────────────────────────────────────────────────────────────

function M.start()
    GameState.reset()

    engine.log("Generating Europa Tunnels map…")
    generate_map()

    spawn_player()

    -- Show the HUD overlay screen
    local hud_screen = engine.ui.create_screen("hud")
    HUD.build(hud_screen)
    engine.ui.show_screen(hud_screen)

    -- Set camera zoom: 2× so tunnels fill the viewport nicely
    engine.camera.set_zoom(2.0)

    register_tick()
end

return M
