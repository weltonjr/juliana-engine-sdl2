-- ship_select.lua — Choose your ship before entering the match.

local GameState = require("game_state")

local M = {}

local SCREEN_W, SCREEN_H = 1280, 720
local PANEL_W,  PANEL_H  = 540,  380

local BTN_W, BTN_H   = 210, 38
local SHIP_ROW_H      = 82   -- vertical stride between ship rows
local FIRST_SHIP_Y    = 80   -- top of the first ship button within the panel
local DESC_OFFSET_Y   = 44   -- label sits below its ship button by this many pixels
local BACK_BTN_Y      = PANEL_H - 54

-- Ship catalogue: display name, qualified def ID, short stats blurb.
local SHIPS = {
    {
        id   = "europa:Interceptor",
        name = "Interceptor",
        desc = "Speed: ★★★★  HP: ★★  Weapon: Plasma Cannon",
    },
    {
        id   = "europa:Scout",
        name = "Scout",
        desc = "Speed: ★★★★★ HP: ★★★ Weapon: Spread Shot",
    },
    {
        id   = "europa:Bomber",
        name = "Bomber",
        desc = "Speed: ★★     HP: ★★★★★ Weapon: Rocket Launcher",
    },
}

function M.show()
    local screen = engine.ui.create_screen("ship_select")

    local panel_x = math.floor((SCREEN_W - PANEL_W) / 2)
    local panel_y = math.floor((SCREEN_H - PANEL_H) / 2)
    local panel   = screen:add_frame(panel_x, panel_y, PANEL_W, PANEL_H)
    local btn_x   = PANEL_W // 2 - BTN_W // 2

    panel:add_label("SELECT YOUR SHIP", PANEL_W // 2 - 84, 22)
    panel:add_label("WASD = thrust   Q/E = rotate   SPACE = fire   ESC = pause", 24, 48)

    for i, ship in ipairs(SHIPS) do
        local row_y = FIRST_SHIP_Y + (i - 1) * SHIP_ROW_H
        local btn   = panel:add_button(ship.name, btn_x, row_y, BTN_W, BTN_H)
        panel:add_label(ship.desc, 26, row_y + DESC_OFFSET_Y)

        -- Capture ship_id by value so the closure sees the correct id after the loop ends.
        local ship_id = ship.id
        btn.on_click = function()
            GameState.player_ship_type = ship_id
            engine.ui.pop_screen()
            local GameScreen = require("game_screen")
            GameScreen.start()
        end
    end

    local btn_back = panel:add_button("Back", btn_x, BACK_BTN_Y, BTN_W, BTN_H)
    btn_back.on_click = function()
        engine.ui.pop_screen()
        local MainMenu = require("main_menu")
        MainMenu.show()
    end

    engine.ui.show_screen(screen)
end

return M
