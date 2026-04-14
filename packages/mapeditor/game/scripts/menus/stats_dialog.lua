-- Stats HUD for the map editor.
-- A small, non-modal panel pinned to the top-right corner showing live stats
-- (FPS, map size, painted cells, entity count, seed, active/inactive chunks).
--
-- Usage:
--   local StatsHUD = require("menus/stats_dialog")
--   state.stats_hud = StatsHUD.build(screen, state)
--   -- in tick_callback: if state.stats_hud then state.stats_hud.update() end
--   -- menu action:      state.stats_hud.toggle()

local layout = require("util/layout")

local WIN_W  = layout.WIN_W
local MENU_H = layout.MENU_H
local PANEL_W = layout.PANEL_W or 260

local M = {}

-- Panel geometry — top-right corner, just below the menu bar, left of the
-- properties panel so it never overlaps. Width/height tuned to the current
-- row layout.
local W, H = 220, 210

function M.build(screen, state)
    -- Anchor: right-aligned, below the menu bar, with an 8 px margin on all sides.
    -- The x is recomputed in update() so the HUD follows the panel's visible state.
    local panel_right = state.panel_visible and (WIN_W - PANEL_W) or WIN_W
    local X = panel_right - W - 8
    local Y = MENU_H + 8

    local frame = screen:add_frame(X, Y, W, H)
    frame.visible = false  -- hidden by default; menu toggles it

    frame:add_label("[ Stats ]", 10, 8)

    local rows = {}
    local function row(label, y)
        frame:add_label(label, 10, y)
        local val = frame:add_label("-", 120, y)
        rows[label] = val
    end

    local ROW, y = 18, 28
    row("FPS:",          y) y = y + ROW
    row("Map:",          y) y = y + ROW
    row("Painted:",      y) y = y + ROW
    row("Overrides:",    y) y = y + ROW
    row("Entities:",     y) y = y + ROW
    row("Seed:",         y) y = y + ROW
    row("Chunks A/T:",   y) y = y + ROW
    row("Sim speed:",    y) y = y + ROW

    local hud = {}

    -- Throttle the expensive solid-cell count; refresh the cheap stats every tick.
    local slow_accum = 0.0
    local solid_cells_cache = 0

    function hud.update(dt)
        if not frame.visible then return end

        -- Keep the HUD anchored to the visible viewport edge
        local right = state.panel_visible and (WIN_W - PANEL_W) or WIN_W
        frame.x = right - W - 8

        rows["FPS:"].text = tostring(engine.get_fps())

        if engine.terrain.is_loaded() then
            local tw = engine.terrain.get_width()
            local th = engine.terrain.get_height()
            rows["Map:"].text = tw .. "x" .. th

            slow_accum = slow_accum + (dt or 0.016)
            if slow_accum >= 0.5 then
                solid_cells_cache = engine.get_solid_cell_count()
                slow_accum = 0.0
            end
            rows["Painted:"].text = tostring(solid_cells_cache)

            local active = engine.sim.get_active_chunks()
            local total  = engine.sim.get_total_chunks()
            rows["Chunks A/T:"].text = active .. " / " .. total
        else
            rows["Map:"].text        = "-"
            rows["Painted:"].text    = "-"
            rows["Chunks A/T:"].text = "-"
        end

        rows["Overrides:"].text = tostring(state.overrides and #state.overrides or 0)
        rows["Entities:"].text  = tostring(state.entities  and #state.entities  or 0)

        local seed = state.locked_seed
                  or (state.panel_handle and state.panel_handle.get_config and
                      state.panel_handle.get_config().seed)
                  or 0
        rows["Seed:"].text = tostring(seed)

        local s = engine.sim.get_time_scale()
        rows["Sim speed:"].text = (s == 0) and "Paused" or (s .. "x")
    end

    function hud.show()   frame.visible = true;  hud.update(0) end
    function hud.hide()   frame.visible = false end
    function hud.toggle()
        frame.visible = not frame.visible
        if frame.visible then hud.update(0) end
    end
    function hud.is_visible() return frame.visible end

    return hud
end

-- Back-compat shim: old call sites used StatsDialog.show(state). Redirect it
-- to a toggle so the View menu entry still works.
function M.show(state)
    if state and state.stats_hud then
        state.stats_hud.toggle()
    end
end

return M
