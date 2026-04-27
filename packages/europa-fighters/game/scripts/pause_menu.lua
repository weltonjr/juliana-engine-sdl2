-- pause_menu.lua — Overlay shown when the player presses ESC during a match.

local M = {}

local SCREEN_W, SCREEN_H = 1280, 720
local PANEL_W,  PANEL_H  = 300,  210

local BTN_W, BTN_H   = 200, 38
local BTN_Y_RESUME   = 68
local BTN_Y_QUIT     = 124

function M.show()
    engine.sim.pause()

    local screen = engine.ui.create_screen("pause")

    local panel_x = math.floor((SCREEN_W - PANEL_W) / 2)
    local panel_y = math.floor((SCREEN_H - PANEL_H) / 2)
    local panel   = screen:add_frame(panel_x, panel_y, PANEL_W, PANEL_H)
    local btn_x   = PANEL_W // 2 - BTN_W // 2

    panel:add_label("PAUSED", PANEL_W // 2 - 30, 22)

    local btn_resume = panel:add_button("Resume", btn_x, BTN_Y_RESUME, BTN_W, BTN_H)
    btn_resume.on_click = function()
        engine.ui.pop_screen()
        engine.sim.resume()
    end

    local btn_quit = panel:add_button("Quit to Menu", btn_x, BTN_Y_QUIT, BTN_W, BTN_H)
    btn_quit.on_click = function()
        engine.ui.pop_screen()  -- pause screen
        engine.ui.pop_screen()  -- hud screen
        engine.terrain.unload()
        local GameState = require("game_state")
        GameState.reset()
        engine.sim.resume()
        local MainMenu = require("main_menu")
        MainMenu.show()
    end

    engine.ui.show_screen(screen)
end

return M
