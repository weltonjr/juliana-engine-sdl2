-- main_menu.lua — Title screen with Deathmatch / Options / Exit.

local M = {}

local SCREEN_W, SCREEN_H = 1280, 720
local PANEL_W,  PANEL_H  = 420,  300

local BTN_W, BTN_H = 220, 38

-- Vertical positions of the three menu buttons within the panel
local BTN_Y_DEATHMATCH = 105
local BTN_Y_OPTIONS    = 158
local BTN_Y_EXIT       = 211

-- Horizontal centre of buttons within the panel
local function panel_btn_x()
    return PANEL_W // 2 - BTN_W // 2
end

function M.show()
    local screen = engine.ui.create_screen("main_menu")

    local panel_x = math.floor((SCREEN_W - PANEL_W) / 2)
    local panel_y = math.floor((SCREEN_H - PANEL_H) / 2)
    local panel   = screen:add_frame(panel_x, panel_y, PANEL_W, PANEL_H)
    local btn_x   = panel_btn_x()

    panel:add_label("EUROPA FIGHTERS",       PANEL_W // 2 - 90, 28)
    panel:add_label("ICE TUNNEL DEATHMATCH", PANEL_W // 2 - 96, 52)

    local btn_dm = panel:add_button("Deathmatch", btn_x, BTN_Y_DEATHMATCH, BTN_W, BTN_H)
    btn_dm.on_click = function()
        engine.ui.pop_screen()
        local ShipSelect = require("ship_select")
        ShipSelect.show()
    end

    local btn_opts = panel:add_button("Options", btn_x, BTN_Y_OPTIONS, BTN_W, BTN_H)
    btn_opts.on_click = function()
        engine.log("Options — not yet implemented")
    end

    local btn_quit = panel:add_button("Exit", btn_x, BTN_Y_EXIT, BTN_W, BTN_H)
    btn_quit.on_click = function()
        engine.quit()
    end

    engine.ui.show_screen(screen)
end

return M
