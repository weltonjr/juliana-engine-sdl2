-- Main menu screen for the map editor.
-- Extracted from main.lua to keep startup clean.

local layout = require("util/layout")

local M = {}

function M.show(editor_screen)
    local WIN_W = layout.WIN_W
    local WIN_H = layout.WIN_H

    local screen = engine.ui.create_screen("main_menu")

    -- Centered panel: 500×300
    local panel = screen:add_frame(
        (WIN_W - 500) / 2, (WIN_H - 300) / 2,
        500, 300)

    panel:add_label("MAP EDITOR", 175, 30)

    local btn_new  = panel:add_button("New Map",  150, 100, 200, 36)
    local btn_load = panel:add_button("Load Map", 150, 155, 200, 36)
    local btn_exit = panel:add_button("Exit",     150, 210, 200, 36)

    btn_new:on_click(function()
        -- Push the editor screen on top (main_menu stays below on the stack)
        editor_screen.open_new()
    end)

    btn_load:on_click(function()
        local FileDialog = require("menus/file_dialog")
        FileDialog.show("packages", function(path)
            editor_screen.load_from_path(path)
        end)
    end)

    btn_exit:on_click(function()
        engine.quit()
    end)

    engine.ui.show_screen(screen)
end

return M
