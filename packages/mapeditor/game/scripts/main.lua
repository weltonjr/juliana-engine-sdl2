-- Map Editor — startup script
-- Sets up the main menu screen using the engine UI API.
-- Lua defines behavior; layout is configured here with simple API calls.

local function build_main_menu()
    local screen = engine.ui.create_screen("main_menu")

    -- Centered panel: 500×340 at (390, 190) on a 1280×720 window
    local panel = screen:add_frame(390, 190, 500, 340)

    -- Title label (centered manually; x is relative to panel)
    panel:add_label("MAP EDITOR", 160, 30)

    -- Buttons (x, y relative to panel)
    local btn_new    = panel:add_button("New Map",   150, 100, 200, 36)
    local btn_load   = panel:add_button("Load Map",  150, 155, 200, 36)
    local btn_exit   = panel:add_button("Exit",      150, 210, 200, 36)

    btn_new:on_click(function()
        engine.log("New Map clicked — map editor not yet implemented")
    end)

    btn_load:on_click(function()
        engine.log("Load Map clicked — map editor not yet implemented")
    end)

    btn_exit:on_click(function()
        engine.quit()
    end)

    engine.ui.show_screen(screen)
end

build_main_menu()
engine.log("Map Editor ready")
