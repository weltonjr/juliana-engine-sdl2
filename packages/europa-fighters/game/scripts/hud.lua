-- hud.lua — In-game overlay: HP, kills, deaths, fire cooldown, respawn notice.

local M = {}

-- X position of the left-side stat labels
local STATS_X = 10
-- Y positions for each stat row (18 px apart)
local HP_Y     = 10
local KILLS_Y  = 28
local DEATHS_Y = 46
local AMMO_Y   = 64
local SHIP_Y   = 82
-- Approximate screen centre for the respawn/death message (1280×720 viewport)
local DEAD_MSG_X = 440
local DEAD_MSG_Y = 340

local TICKS_PER_SECOND = 60

local hp_lbl, kills_lbl, deaths_lbl, dead_lbl, ammo_lbl, ship_lbl

function M.build(screen)
    hp_lbl     = screen:add_label("HP: 100",    STATS_X, HP_Y)
    kills_lbl  = screen:add_label("Kills: 0",   STATS_X, KILLS_Y)
    deaths_lbl = screen:add_label("Deaths: 0",  STATS_X, DEATHS_Y)
    ammo_lbl   = screen:add_label("FIRE READY", STATS_X, AMMO_Y)
    ship_lbl   = screen:add_label("",           STATS_X, SHIP_Y)
    dead_lbl   = screen:add_label("",           DEAD_MSG_X, DEAD_MSG_Y)
end

function M.update(hp, kills, deaths, fire_timer, ship_name)
    if hp_lbl     then hp_lbl.text     = "HP: " .. math.max(0, math.floor(hp)) end
    if kills_lbl  then kills_lbl.text  = "Kills: "  .. kills  end
    if deaths_lbl then deaths_lbl.text = "Deaths: " .. deaths end
    if ship_lbl   then ship_lbl.text   = ship_name or "" end
    if ammo_lbl   then
        ammo_lbl.text = (fire_timer > 0)
            and ("Cooling: " .. fire_timer)
            or  "FIRE READY"
    end
    if dead_lbl then dead_lbl.text = "" end
end

function M.update_dead(timer_ticks)
    local secs = math.ceil(timer_ticks / TICKS_PER_SECOND)
    if dead_lbl  then dead_lbl.text  = "DESTROYED — Respawning in " .. secs .. "s" end
    if hp_lbl    then hp_lbl.text    = "HP: 0" end
    if ammo_lbl  then ammo_lbl.text  = "" end
end

return M
