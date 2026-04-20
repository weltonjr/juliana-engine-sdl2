-- About dialog for the map editor.

local layout  = require("util/layout")
local widgets = require("util/widgets")

local M = {}

function M.show()
    local W, H = 360, 180
    local X, Y = layout.center(W, H)

    local screen = engine.ui.create_screen("about_dialog")
    local bg  = widgets.frame(screen, { x = 0, y = 0, w = layout.WIN_W, h = layout.WIN_H })
    local dlg = widgets.frame(bg,     { x = X, y = Y, w = W,            h = H })

    widgets.label(dlg, { text = "Juliana Engine — Map Editor",         x = 20, y = 20 })
    widgets.label(dlg, { text = "A generic 2D terrain editor.",        x = 20, y = 46 })
    widgets.label(dlg, { text = "Use the properties panel to configure", x = 20, y = 66 })
    widgets.label(dlg, { text = "map generation and click Generate.",  x = 20, y = 82 })

    widgets.button(dlg, {
        text = "OK", x = W / 2 - 40, y = H - 44, w = 80, h = 30,
        on_click = function() engine.ui.pop_screen() end,
    })

    engine.ui.show_screen(screen)
end

return M
