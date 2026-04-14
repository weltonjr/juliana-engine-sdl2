-- Stats dialog for the map editor.
-- Shows FPS, terrain dimensions, painted cell count, entity count, and current seed.
--
-- Usage:
--   local StatsDialog = require("menus/stats_dialog")
--   StatsDialog.show(state)   -- state = editor_screen state table

local layout = require("util/layout")

local WIN_W  = layout.WIN_W
local WIN_H  = layout.WIN_H

local M = {}

function M.show(state)
    local W, H = 320, 230
    local X = (WIN_W - W) / 2
    local Y = (WIN_H - H) / 2

    -- Collect stats at open time (solid cell count is expensive — call once)
    local fps         = engine.get_fps()
    local tw          = engine.terrain.get_width()
    local th          = engine.terrain.get_height()
    local total_cells = tw * th
    local solid_cells = engine.terrain.is_loaded() and engine.get_solid_cell_count() or 0
    local ent_count   = state.entities and #state.entities or 0
    local seed        = state.locked_seed
                        or (state.panel_handle and state.panel_handle.get_config().seed)
                        or 0

    local screen = engine.ui.create_screen("stats_dialog")
    local bg     = screen:add_frame(0, 0, WIN_W, WIN_H)
    local dlg    = bg:add_frame(X, Y, W, H)

    local function row(label, value, y)
        dlg:add_label(label, 12, y)
        dlg:add_label(tostring(value), 160, y)
    end

    dlg:add_label("[ Map Statistics ]", 12, 10)

    local y = 36
    local ROW = 22

    row("FPS:",             fps,                                       y) y = y + ROW
    row("Map size:",        tw .. " × " .. th .. " cells",             y) y = y + ROW
    row("Total cells:",     string.format("%d", total_cells),          y) y = y + ROW
    row("Painted cells:",   string.format("%d", solid_cells),          y) y = y + ROW
    row("Override count:",  state.overrides and #state.overrides or 0, y) y = y + ROW
    row("Entities:",        ent_count,                                 y) y = y + ROW
    row("Seed:",            seed,                                      y) y = y + ROW
    row("Seed locked:",     tostring(state.seed_locked or false),       y) y = y + ROW

    local close_btn = dlg:add_button("Close", W - 80, H - 36, 68, 28)
    close_btn:on_click(function()
        engine.ui.pop_screen()
    end)

    engine.ui.show_screen(screen)
end

return M
