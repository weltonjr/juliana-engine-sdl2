-- Stats HUD for the map editor.
-- A small, non-modal panel pinned to the top-right corner showing live stats
-- (FPS, map size, painted cells, entity count, seed, active/inactive chunks).
--
-- Usage:
--   local StatsHUD = require("menus/stats_dialog")
--   state.stats_hud = StatsHUD.build(screen, state)
--   -- in tick_callback: if state.stats_hud then state.stats_hud.update() end
--   -- menu action:      state.stats_hud.toggle()

local layout  = require("util/layout")
local widgets = require("util/widgets")

local WIN_W   = layout.WIN_W
local MENU_H  = layout.MENU_H
local PANEL_W = layout.PANEL_W or 260

local M = {}

local W, H = 220, 246

local ROWS = {
    "FPS:", "Map:", "Painted:", "Overrides:", "Entities:",
    "Seed:", "Chunks A/T:", "Sim speed:", "Dyn bodies:", "Viewport T:",
}

function M.build(screen, state)
    local panel_right = state.panel_visible and (WIN_W - PANEL_W) or WIN_W
    local X = panel_right - W - 8
    local Y = MENU_H + 8

    local frame = widgets.frame(screen, { x = X, y = Y, w = W, h = H, visible = false })

    widgets.label(frame, { text = "[ Stats ]", x = 10, y = 8 })

    local next_y = layout.stack_v(28, 18)
    local values = {}
    for _, label in ipairs(ROWS) do
        local y = next_y()
        widgets.label(frame, { text = label, x = 10,  y = y })
        values[label] = widgets.label(frame, { text = "-", x = 120, y = y })
    end

    local hud = {}

    local slow_accum, solid_cells_cache = 0.0, 0

    function hud.update(dt)
        if not frame.visible then return end

        local right = state.panel_visible and (WIN_W - PANEL_W) or WIN_W
        frame.x = right - W - 8

        values["FPS:"].text = tostring(engine.get_fps())

        if engine.terrain.is_loaded() then
            local tw = engine.terrain.get_width()
            local th = engine.terrain.get_height()
            values["Map:"].text = tw .. "x" .. th

            slow_accum = slow_accum + (dt or 0.016)
            if slow_accum >= 0.5 then
                solid_cells_cache = engine.get_solid_cell_count()
                slow_accum = 0.0
            end
            values["Painted:"].text = tostring(solid_cells_cache)

            local active = engine.sim.get_active_chunks()
            local total  = engine.sim.get_total_chunks()
            values["Chunks A/T:"].text = active .. " / " .. total
            values["Dyn bodies:"].text = tostring(engine.sim.dynamic_body_count())

            local cam_x = engine.camera.get_x()
            local cam_y = engine.camera.get_y()
            local vw    = math.floor(tw / engine.camera.get_zoom())
            local vh    = math.floor(th / engine.camera.get_zoom())
            local step  = math.max(1, math.floor(math.sqrt(vw * vh / 400)))
            local sum_t, cnt = 0, 0
            for sy = 0, vh - 1, step do
                for sx = 0, vw - 1, step do
                    local gx = math.floor(cam_x + sx)
                    local gy = math.floor(cam_y + sy)
                    if gx >= 0 and gx < tw and gy >= 0 and gy < th then
                        sum_t = sum_t + engine.sim.get_temperature(gx, gy)
                        cnt = cnt + 1
                    end
                end
            end
            values["Viewport T:"].text = cnt > 0 and string.format("%.1f", sum_t / cnt) or "-"
        else
            values["Map:"].text        = "-"
            values["Painted:"].text    = "-"
            values["Chunks A/T:"].text = "-"
            values["Dyn bodies:"].text = "-"
            values["Viewport T:"].text = "-"
        end

        values["Overrides:"].text = tostring(state.overrides and #state.overrides or 0)
        values["Entities:"].text  = tostring(state.entities  and #state.entities  or 0)

        local seed = state.locked_seed
                  or (state.panel_handle and state.panel_handle.get_config and
                      state.panel_handle.get_config().seed)
                  or 0
        values["Seed:"].text = tostring(seed)

        local s = engine.sim.get_time_scale()
        values["Sim speed:"].text = (s == 0) and "Paused" or (s .. "x")
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
    if state and state.stats_hud then state.stats_hud.toggle() end
end

return M
