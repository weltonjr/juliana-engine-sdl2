-- Main menu screen for the map editor.
-- Extracted from main.lua to keep startup clean.

local layout  = require("util/layout")
local widgets = require("util/widgets")

local M = {}

function M.show(editor_screen)
    local screen = engine.ui.create_screen("main_menu")

    local PW, PH = 500, 300
    local px, py = layout.center(PW, PH)
    local panel  = widgets.frame(screen, { x = px, y = py, w = PW, h = PH })

    widgets.label(panel, { text = "MAP EDITOR", x = 175, y = 30 })

    widgets.button(panel, {
        text = "New Map", x = 150, y = 100, w = 200, h = 36,
        on_click = function() editor_screen.open_new() end,
    })
    widgets.button(panel, {
        text = "Load Map", x = 150, y = 155, w = 200, h = 36,
        on_click = function()
            local FileDialog = require("menus/file_dialog")
            FileDialog.show("packages", function(path)
                editor_screen.load_from_path(path)
            end)
        end,
    })
    widgets.button(panel, {
        text = "Exit", x = 150, y = 210, w = 200, h = 36,
        on_click = function() engine.quit() end,
    })

    engine.ui.show_screen(screen)
end

return M
