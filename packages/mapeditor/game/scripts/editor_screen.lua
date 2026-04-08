-- Map editor screen.
-- Shown when clicking "New Map" or "Load Map" from the main menu.
--
-- Layout:
--   [Top menu bar — 24px]
--   [Terrain viewport — fills remaining area]
--   [Properties panel — 290px wide, right side, semi-transparent overlay]

local layout       = require("util/layout")
local MenuBar      = require("menus/menu_bar")
local ScenarioPanel= require("panels/scenario_panel")
local FileDialog   = require("menus/file_dialog")
local AboutDialog  = require("menus/about_dialog")

local WIN_W   = layout.WIN_W
local WIN_H   = layout.WIN_H
local MENU_H  = layout.MENU_H
local PANEL_W = layout.PANEL_W

local M = {}

-- Internal state — one instance while the editor is open
local state = {
    current_path   = nil,    -- nil = new, string = loaded from file
    scenario_tbl   = nil,    -- last saved/loaded scenario table
    overrides      = {},     -- {x,y,material_id,background_id} applied post-gen
    panel_visible  = true,
    panel_handle   = nil,
    menu_bar_ctrl  = nil,
}

-- ─── Save helpers ─────────────────────────────────────────────────────────────

local function do_save(path)
    if not path or path == "" then return end
    local tbl = state.panel_handle.get_scenario_table()
    tbl.overrides = state.overrides
    local ok = engine.fs.write_text(path, engine.json.encode(tbl))
    if ok then
        engine.log("Saved: " .. path)
        state.current_path = path
        state.scenario_tbl = tbl
    else
        engine.log("ERROR: could not write " .. path)
    end
end

local function save_dialog()
    -- Simple "input dialog" — for now use a fixed default path
    -- A full save-as dialog with text input would need a modal screen;
    -- we provide one inline here.
    local WIN_W2 = WIN_W
    local WIN_H2 = WIN_H
    local W, H = 480, 120
    local X = (WIN_W2 - W) / 2
    local Y = (WIN_H2 - H) / 2

    local screen = engine.ui.create_screen("save_dialog")
    local bg     = screen:add_frame(0, 0, WIN_W2, WIN_H2)
    local dlg    = bg:add_frame(X, Y, W, H)

    dlg:add_label("Save scenario to path:", 10, 12)
    local inp = dlg:add_input("packages/mapeditor/maps/map1/scenario.json",
                               10, 34, W - 20, 26)
    if state.current_path then inp.value = state.current_path end

    local ok_btn     = dlg:add_button("Save",   W - 110, H - 36, 50, 28)
    local cancel_btn = dlg:add_button("Cancel", W - 56,  H - 36, 50, 28)

    ok_btn:on_click(function()
        local path = inp.value
        engine.ui.pop_screen()
        do_save(path)
    end)
    cancel_btn:on_click(function()
        engine.ui.pop_screen()
    end)

    engine.ui.show_screen(screen)
end

-- ─── Generate terrain ─────────────────────────────────────────────────────────

local function generate(map_config)
    -- Inject current overrides into the config table
    map_config.overrides = state.overrides
    engine.terrain.generate(map_config)
    engine.log("Terrain generated: " ..
        tostring(map_config.width) .. "x" .. tostring(map_config.height))
end

-- ─── Tick callback ────────────────────────────────────────────────────────────

local function register_tick()
    local PAN  = layout.CAM_PAN_SPEED
    local ZSTEP= layout.ZOOM_STEP
    local ZMIN = layout.ZOOM_MIN
    local ZMAX = layout.ZOOM_MAX

    engine.set_tick_callback(function(dt)
        if not engine.terrain.is_loaded() then return end

        -- Camera pan: arrow keys or WASD
        local dx, dy = 0, 0
        if engine.input.is_key_down(engine.key.LEFT)  or engine.input.is_key_down(engine.key.A) then dx = dx - PAN end
        if engine.input.is_key_down(engine.key.RIGHT) or engine.input.is_key_down(engine.key.D) then dx = dx + PAN end
        if engine.input.is_key_down(engine.key.UP)    or engine.input.is_key_down(engine.key.W) then dy = dy - PAN end
        if engine.input.is_key_down(engine.key.DOWN)  or engine.input.is_key_down(engine.key.S) then dy = dy + PAN end
        if dx ~= 0 or dy ~= 0 then engine.camera.move(dx, dy) end

        -- Camera zoom: scroll wheel
        local scroll = engine.input.scroll_y()
        if scroll ~= 0 then
            local z = engine.camera.get_zoom()
            z = math.max(ZMIN, math.min(ZMAX, z + scroll * ZSTEP))
            engine.camera.set_zoom(z)
        end

        -- Ctrl+S to save
        local ctrl = engine.input.is_key_down(engine.key.LCTRL)
                  or engine.input.is_key_down(engine.key.RCTRL)
        if ctrl and engine.input.is_key_just_pressed(engine.key.S) then
            if state.current_path then
                do_save(state.current_path)
            else
                save_dialog()
            end
        end
    end)
end

-- ─── Open new map ─────────────────────────────────────────────────────────────

function M.open_new()
    state.current_path  = nil
    state.scenario_tbl  = nil
    state.overrides     = {}
    state.panel_visible = true

    local screen = engine.ui.create_screen("editor")

    -- Properties panel (semi-transparent overlay, top-right)
    local panel_frame = screen:add_frame(WIN_W - PANEL_W, MENU_H, PANEL_W, WIN_H - MENU_H)

    local panel_handle = ScenarioPanel.build(panel_frame, function(map_config)
        generate(map_config)
    end)
    state.panel_handle = panel_handle

    -- Menu bar (built last so it renders on top of the panel)
    local actions = {
        on_new     = function()
            engine.terrain.unload()
            engine.ui.pop_screen()
            M.open_new()
        end,
        on_open    = function()
            FileDialog.show("packages", function(path)
                M.load_from_path(path)
            end)
        end,
        on_save    = function()
            if state.current_path then do_save(state.current_path)
            else save_dialog() end
        end,
        on_save_as  = save_dialog,
        on_quit    = function()
            engine.terrain.unload()
            engine.ui.pop_screen()
        end,
        on_undo    = function() engine.log("Undo — not yet implemented") end,
        on_redo    = function() engine.log("Redo — not yet implemented") end,
        toggle_panel = function()
            state.panel_visible = not state.panel_visible
            panel_frame.visible = state.panel_visible
        end,
        on_about   = function() AboutDialog.show() end,
    }

    state.menu_bar_ctrl = MenuBar.build(screen, actions)

    engine.ui.show_screen(screen)
    register_tick()

    -- Auto-generate with defaults on open
    generate(panel_handle.get_config())
end

-- ─── Load from file ───────────────────────────────────────────────────────────

function M.load_from_path(path)
    local text = engine.fs.read_text(path)
    if not text then
        engine.log("ERROR: cannot read " .. tostring(path))
        return
    end
    local tbl = engine.json.decode(text)
    if not tbl then
        engine.log("ERROR: JSON parse failed: " .. tostring(path))
        return
    end

    state.current_path = path
    state.scenario_tbl = tbl
    state.overrides    = tbl.overrides or {}

    -- If we already have the editor screen open, update the panel + regenerate
    if state.panel_handle then
        state.panel_handle.set_from_scenario(tbl)
        generate(state.panel_handle.get_config())
    else
        -- No screen yet — open fresh with the loaded data
        M.open_new_with_data(tbl)
    end
end

-- Open editor pre-populated with loaded scenario data
function M.open_new_with_data(tbl)
    state.current_path  = state.current_path
    state.scenario_tbl  = tbl
    state.overrides     = tbl.overrides or {}
    state.panel_visible = true

    local screen = engine.ui.create_screen("editor")

    local panel_frame = screen:add_frame(WIN_W - PANEL_W, MENU_H, PANEL_W, WIN_H - MENU_H)
    local panel_handle = ScenarioPanel.build(panel_frame, function(map_config)
        generate(map_config)
    end)
    state.panel_handle = panel_handle
    panel_handle.set_from_scenario(tbl)

    local actions = {
        on_new     = function()
            engine.terrain.unload()
            engine.ui.pop_screen()
            M.open_new()
        end,
        on_open    = function()
            FileDialog.show("packages", function(path)
                state.current_path = path
                M.load_from_path(path)
            end)
        end,
        on_save    = function()
            if state.current_path then do_save(state.current_path)
            else save_dialog() end
        end,
        on_save_as  = save_dialog,
        on_quit    = function()
            engine.terrain.unload()
            engine.ui.pop_screen()
        end,
        on_undo    = function() engine.log("Undo — not yet implemented") end,
        on_redo    = function() engine.log("Redo — not yet implemented") end,
        toggle_panel = function()
            state.panel_visible = not state.panel_visible
            panel_frame.visible = state.panel_visible
        end,
        on_about   = function() AboutDialog.show() end,
    }

    state.menu_bar_ctrl = MenuBar.build(screen, actions)

    engine.ui.show_screen(screen)
    register_tick()

    -- Auto-generate from loaded config
    generate(panel_handle.get_config())
end

return M
