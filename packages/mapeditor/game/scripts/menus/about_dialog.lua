-- About dialog for the map editor.

local layout = require("util/layout")

local M = {}

function M.show()
    local WIN_W = layout.WIN_W
    local WIN_H = layout.WIN_H
    local W, H  = 360, 180
    local X = (WIN_W - W) / 2
    local Y = (WIN_H - H) / 2

    local screen = engine.ui.create_screen("about_dialog")
    local bg     = screen:add_frame(0, 0, WIN_W, WIN_H)
    local dlg    = bg:add_frame(X, Y, W, H)

    dlg:add_label("Juliana Engine — Map Editor", 20, 20)
    dlg:add_label("A generic 2D terrain editor.", 20, 46)
    dlg:add_label("Use the properties panel to configure", 20, 66)
    dlg:add_label("map generation and click Generate.", 20, 82)

    local ok = dlg:add_button("OK", W/2 - 40, H - 44, 80, 30)
    ok:on_click(function()
        engine.ui.pop_screen()
    end)

    engine.ui.show_screen(screen)
end

return M
