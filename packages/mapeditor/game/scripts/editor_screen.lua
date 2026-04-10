-- Map editor screen.
-- Shown when clicking "New Map" or "Load Map" from the main menu.
--
-- Layout:
--   [Top menu bar — 24px full width]
--   [Edit toolbar 48px left | Terrain viewport | Properties panel 290px right]

local layout        = require("util/layout")
local MenuBar       = require("menus/menu_bar")
local ScenarioPanel = require("panels/scenario_panel")
local EditToolbar   = require("panels/edit_toolbar")
local FileDialog    = require("menus/file_dialog")
local AboutDialog   = require("menus/about_dialog")

local WIN_W    = layout.WIN_W
local WIN_H    = layout.WIN_H
local MENU_H   = layout.MENU_H
local PANEL_W  = layout.PANEL_W
local TOOLBAR_W= layout.TOOLBAR_W

local M = {}

-- Internal state — one instance while the editor is open
local state = {
    current_path   = nil,    -- nil = new, string = loaded from file
    scenario_tbl   = nil,    -- last saved/loaded scenario table
    overrides      = {},     -- {x,y,material_id,background_id} applied post-gen
    panel_visible  = true,
    panel_handle   = nil,
    toolbar_handle = nil,
    menu_bar_ctrl  = nil,
    -- Camera drag
    drag_prev_x    = nil,
    drag_prev_y    = nil,
    -- Seed lock
    seed_locked    = false,
    locked_seed    = nil,
    -- Rect tool
    rect_start     = nil,
}

-- ─── Reset transient state ────────────────────────────────────────────────────

local function reset_state()
    state.seed_locked  = false
    state.locked_seed  = nil
    state.rect_start   = nil
    state.drag_prev_x  = nil
    state.drag_prev_y  = nil
end

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
    local W, H = 480, 120
    local X = (WIN_W - W) / 2
    local Y = (WIN_H - H) / 2

    local screen = engine.ui.create_screen("save_dialog")
    local bg     = screen:add_frame(0, 0, WIN_W, WIN_H)
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
    -- If seed is locked, force the fixed seed regardless of panel value
    if state.seed_locked and state.locked_seed then
        map_config.seed = state.locked_seed
    end
    map_config.overrides = state.overrides
    engine.terrain.generate(map_config)
    engine.log("Terrain generated: " ..
        tostring(map_config.width) .. "x" .. tostring(map_config.height))
end

-- ─── Painting helpers ─────────────────────────────────────────────────────────

local function screen_to_world(sx, sy)
    local z  = engine.camera.get_zoom()
    local wx = engine.camera.get_x() + sx / z
    local wy = engine.camera.get_y() + sy / z
    return math.floor(wx), math.floor(wy)
end

local function upsert_override(x, y, mat_id, bg_id)
    for _, ov in ipairs(state.overrides) do
        if ov.x == x and ov.y == y then
            ov.material_id   = mat_id
            ov.background_id = bg_id
            return
        end
    end
    state.overrides[#state.overrides + 1] =
        { x = x, y = y, material_id = mat_id, background_id = bg_id }
end

local function lock_seed()
    if state.seed_locked then return end
    -- Ask the engine for the actual seed used by the last generate() call.
    -- When the user left the seed field at 0, MapGenerator picked a random
    -- seed internally; engine.terrain.get_last_seed() returns that exact value
    -- so future regenerations reproduce the identical base terrain.
    local seed = engine.terrain.get_last_seed()
    if seed == 0 then seed = 1 end   -- safety: shouldn't happen after generate
    state.seed_locked = true
    state.locked_seed = seed
    if state.panel_handle then
        state.panel_handle.set_seed(seed)
        state.panel_handle.set_seed_locked(true)
    end
    engine.log("Seed locked at " .. tostring(seed))
end

-- ─── Tick callback ────────────────────────────────────────────────────────────

local function register_tick()
    local ZSTEP = layout.ZOOM_STEP
    local ZMIN  = layout.ZOOM_MIN
    local ZMAX  = layout.ZOOM_MAX

    engine.set_tick_callback(function(dt)
        local mx = engine.input.mouse_x()
        local my = engine.input.mouse_y()

        -- ── Right-click drag: camera pan ──────────────────────────────────────
        local rmb = engine.input.mouse_button(3)
        if rmb then
            if state.drag_prev_x then
                local dx = mx - state.drag_prev_x
                local dy = my - state.drag_prev_y
                local z  = engine.camera.get_zoom()
                engine.camera.move(-dx / z, -dy / z)
            end
            state.drag_prev_x, state.drag_prev_y = mx, my
        else
            state.drag_prev_x, state.drag_prev_y = nil, nil
        end

        -- ── Scroll wheel zoom ─────────────────────────────────────────────────
        local scroll = engine.input.scroll_y()
        if scroll ~= 0 then
            local z = engine.camera.get_zoom()
            z = math.max(ZMIN, math.min(ZMAX, z + scroll * ZSTEP))
            engine.camera.set_zoom(z)
        end

        -- ── Ctrl+S save ───────────────────────────────────────────────────────
        local ctrl = engine.input.is_key_down(engine.key.LCTRL)
                  or engine.input.is_key_down(engine.key.RCTRL)
        if ctrl and engine.input.is_key_just_pressed(engine.key.S) then
            if state.current_path then do_save(state.current_path)
            else save_dialog() end
        end

        -- ── Paint tools ───────────────────────────────────────────────────────
        if not engine.terrain.is_loaded() then return end

        local panel_right = WIN_W - (state.panel_visible and PANEL_W or 0)
        local in_viewport = (mx > TOOLBAR_W and mx < panel_right and my > MENU_H)
        if not in_viewport then return end

        local tool   = state.toolbar_handle and state.toolbar_handle.get_tool() or "brush"
        local mat    = state.toolbar_handle and state.toolbar_handle.get_tool() ~= "eraser"
                       and state.toolbar_handle.get_material() or nil
        local bsize  = state.toolbar_handle and state.toolbar_handle.get_brush_size() or 1
        local half   = math.floor(bsize / 2)

        local lmb       = engine.input.mouse_button(1)
        local lmb_press = engine.input.mouse_just_pressed(1)
        local lmb_rel   = engine.input.mouse_just_released(1)

        -- Helper: paint a bsize×bsize square centred on (cx, cy)
        local function paint_brush(cx, cy, mat_id, bg_id)
            for dx = -half, bsize - 1 - half do
                for dy = -half, bsize - 1 - half do
                    local rx, ry = cx + dx, cy + dy
                    engine.terrain.set_cell(rx, ry, mat_id, bg_id)
                    upsert_override(rx, ry, mat_id, bg_id)
                end
            end
        end

        if tool == "brush" and lmb and mat then
            local wx, wy = screen_to_world(mx, my)
            paint_brush(wx, wy, mat.id, "")
            lock_seed()

        elseif tool == "eraser" and lmb then
            local wx, wy = screen_to_world(mx, my)
            local bg_id  = state.toolbar_handle and state.toolbar_handle.get_background()
                           or "base:Sky"
            paint_brush(wx, wy, "base:Air", bg_id)
            lock_seed()

        elseif tool == "rect" then
            if lmb_press then
                local wx, wy = screen_to_world(mx, my)
                state.rect_start = { x = wx, y = wy }
            elseif lmb_rel and state.rect_start and mat then
                local wx, wy = screen_to_world(mx, my)
                local x0 = math.min(state.rect_start.x, wx)
                local x1 = math.max(state.rect_start.x, wx)
                local y0 = math.min(state.rect_start.y, wy)
                local y1 = math.max(state.rect_start.y, wy)
                for rx = x0, x1 do
                    for ry = y0, y1 do
                        engine.terrain.set_cell(rx, ry, mat.id, "")
                        upsert_override(rx, ry, mat.id, "")
                    end
                end
                engine.log("Rect: " .. mat.name .. " " ..
                    (x1-x0+1) .. "x" .. (y1-y0+1))
                lock_seed()
                state.rect_start = nil
            elseif not lmb then
                -- Cancel rect if button released outside viewport
                state.rect_start = nil
            end
        end
    end)
end

-- ─── Build editor screen (shared by open_new and open_new_with_data) ──────────

local function build_editor_screen(initial_tbl)
    local screen = engine.ui.create_screen("editor")

    -- Edit toolbar (left side)
    local toolbar_handle = EditToolbar.build(screen)
    state.toolbar_handle = toolbar_handle

    -- Properties panel (right side)
    local panel_frame = screen:add_frame(WIN_W - PANEL_W, MENU_H, PANEL_W, WIN_H - MENU_H)
    local panel_handle = ScenarioPanel.build(panel_frame, function(map_config)
        generate(map_config)
    end)
    state.panel_handle = panel_handle

    if initial_tbl then
        panel_handle.set_from_scenario(initial_tbl)
    end

    -- Menu bar (built last — renders on top)
    local actions = {
        on_new = function()
            engine.terrain.unload()
            engine.ui.pop_screen()
            M.open_new()
        end,
        on_open = function()
            FileDialog.show("packages", function(path)
                M.load_from_path(path)
            end)
        end,
        on_save = function()
            if state.current_path then do_save(state.current_path)
            else save_dialog() end
        end,
        on_save_as  = save_dialog,
        on_quit = function()
            engine.terrain.unload()
            engine.ui.pop_screen()
        end,
        on_undo = function() engine.log("Undo — not yet implemented") end,
        on_redo = function() engine.log("Redo — not yet implemented") end,
        toggle_panel = function()
            state.panel_visible = not state.panel_visible
            panel_frame.visible = state.panel_visible
        end,
        on_about = function() AboutDialog.show() end,
    }

    state.menu_bar_ctrl = MenuBar.build(screen, actions)
    engine.ui.show_screen(screen)
    register_tick()

    return panel_handle
end

-- ─── Open new map ─────────────────────────────────────────────────────────────

function M.open_new()
    state.current_path  = nil
    state.scenario_tbl  = nil
    state.overrides     = {}
    state.panel_visible = true
    reset_state()

    local panel_handle = build_editor_screen(nil)
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
    reset_state()

    -- If the editor is already open, update panel + regenerate in place
    if state.panel_handle then
        state.panel_handle.set_from_scenario(tbl)
        -- Re-lock seed if the loaded map had overrides
        if #state.overrides > 0 then
            state.locked_seed = tbl.map and tbl.map.seed or 0
            state.seed_locked = true
            state.panel_handle.set_seed_locked(true)
        end
        generate(state.panel_handle.get_config())
    else
        M.open_new_with_data(tbl)
    end
end

-- ─── Open editor pre-populated with loaded scenario data ──────────────────────

function M.open_new_with_data(tbl)
    state.scenario_tbl  = tbl
    state.overrides     = tbl.overrides or {}
    state.panel_visible = true
    reset_state()

    local panel_handle = build_editor_screen(tbl)

    -- Pre-lock seed if the loaded map already has overrides
    if #state.overrides > 0 then
        state.locked_seed = tbl.map and tbl.map.seed or 0
        state.seed_locked = true
        panel_handle.set_seed_locked(true)
    end

    generate(panel_handle.get_config())
end

return M
