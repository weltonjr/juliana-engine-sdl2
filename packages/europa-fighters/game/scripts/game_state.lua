-- game_state.lua
-- Centralized match state. Reset between matches via M.reset().

local M = {}

-- Ship chosen on the selection screen
M.player_ship_type = "europa:Interceptor"

-- Live entity ID of the player's ship (0 = not spawned)
M.player_entity_id = 0

-- Score counters
M.kills  = 0
M.deaths = 0

-- Match settings
M.frag_limit          = 10
M.respawn_delay_ticks = 180  -- 3 s at 60 Hz

-- Respawn bookkeeping
M.is_dead       = false
M.respawn_timer = 0

-- Ticks remaining before the player can fire again
M.fire_timer = 0

-- Active projectiles: entity_id → { owner = eid, ptype = int, life = ticks }
-- ptype: 1 = Plasma Cannon, 2 = Rocket Launcher, 3 = Spread Shot
M.projectiles = {}

-- Map dimensions — must match the scenario.json / generate_map() call
M.map_width  = 1280
M.map_height = 720

function M.reset()
    M.player_entity_id = 0
    M.kills            = 0
    M.deaths           = 0
    M.is_dead          = false
    M.respawn_timer    = 0
    M.fire_timer       = 0
    M.projectiles      = {}
end

return M
