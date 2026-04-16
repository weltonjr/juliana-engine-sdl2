-- Map editor screen.
-- Shown when clicking "New Map" or "Load Map" from the main menu.
--
-- Layout:
--   [Top menu bar — 24px full width]
--   [Edit toolbar 96px left | Terrain viewport | Properties panel 290px right]

local layout        = require("util/layout")
local MenuBar       = require("menus/menu_bar")
local ScenarioPanel = require("panels/scenario_panel")
local EditToolbar   = require("panels/edit_toolbar")
local FileDialog    = require("menus/file_dialog")
local AboutDialog   = require("menus/about_dialog")
local StatsDialog   = require("menus/stats_dialog")
local EntityPopup   = require("panels/entity_popup")

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
    overrides_index= {},     -- [x*65536+y] → index in overrides (O(1) upsert)
    entities       = {},     -- {def_id,x,y,size_w,size_h,r,g,b,props} editor placements
    selected_entity_idx = nil,
    panel_visible  = true,
    panel_handle   = nil,
    toolbar_handle = nil,
    menu_bar_ctrl  = nil,
    tooltip_label  = nil,
    -- Camera drag
    drag_prev_x    = nil,
    drag_prev_y    = nil,
    -- Seed lock
    seed_locked    = false,
    locked_seed    = nil,
    -- Rect tool
    rect_start     = nil,
    -- Brush line fill
    last_paint_wx  = nil,
    last_paint_wy  = nil,
    -- Panel lock
    panel_locked   = false,
    panel_frame    = nil,
    -- Pointer tool drag
    ptr_drag_idx    = nil,
    ptr_drag_off_wx = 0,
    ptr_drag_off_wy = 0,
    ptr_drag_moved  = false,
    ptr_start_mx    = 0,
    ptr_start_my    = 0,
}

-- ─── Reset transient state ────────────────────────────────────────────────────

local function reset_state()
    state.seed_locked         = false
    state.locked_seed         = nil
    state.rect_start          = nil
    state.drag_prev_x         = nil
    state.drag_prev_y         = nil
    state.last_paint_wx       = nil
    state.last_paint_wy       = nil
    state.overrides           = {}
    state.overrides_index     = {}
    state.entities            = {}
    state.selected_entity_idx = nil
    state.panel_locked        = false
    state.ptr_drag_idx        = nil
    state.ptr_drag_off_wx     = 0
    state.ptr_drag_off_wy     = 0
    state.ptr_drag_moved      = false
    state.ptr_start_mx        = 0
    state.ptr_start_my        = 0
    engine.editor.clear_markers()
end

-- ─── Save helpers ─────────────────────────────────────────────────────────────

local function do_save(path)
    if not path or path == "" then return end
    local tbl = state.panel_handle.get_scenario_table()

    -- Compact override storage (binary palette + base64)
    if #state.overrides > 0 then
        tbl.overrides_v2 = engine.overrides.encode(state.overrides)
    end
    -- (no legacy overrides key — new format only)

    -- Entity placements
    if #state.entities > 0 then
        local ent_list = {}
        for _, ent in ipairs(state.entities) do
            ent_list[#ent_list + 1] = {
                def   = ent.def_id,
                x     = ent.x,
                y     = ent.y,
                props = ent.props,
            }
        end
        tbl.entities = ent_list
    end

    -- TODO(phase-4): persist overlay state (temperature, health, ignited, crack,
    -- stain) per dirty region so reopening a map restores all reactive state.

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

-- O(1) channel-selective upsert using an index table keyed by x*65536+y.
-- When mat_id or bg_id is "", that channel is left unchanged on existing overrides.
local function upsert_override(x, y, mat_id, bg_id)
    local k   = x * 65536 + y
    local idx = state.overrides_index[k]
    if idx then
        local ov = state.overrides[idx]
        if mat_id ~= "" then ov.material_id   = mat_id end
        if bg_id  ~= "" then ov.background_id = bg_id  end
    else
        local n = #state.overrides + 1
        state.overrides[n] = { x = x, y = y, material_id = mat_id, background_id = bg_id }
        state.overrides_index[k] = n
    end
end

-- Batch-upsert from a flat C++ paint_line result {x0,y0,x1,y1,...}
local function upsert_overrides_flat(flat, mat_id, bg_id)
    local i = 1
    local n = #flat
    while i <= n do
        local x, y = flat[i], flat[i+1]
        i = i + 2
        local k   = x * 65536 + y
        local idx = state.overrides_index[k]
        if idx then
            local ov = state.overrides[idx]
            if mat_id ~= "" then ov.material_id   = mat_id end
            if bg_id  ~= "" then ov.background_id = bg_id  end
        else
            local ni = #state.overrides + 1
            state.overrides[ni] = { x = x, y = y, material_id = mat_id, background_id = bg_id }
            state.overrides_index[k] = ni
        end
    end
end

-- Lock the scenario panel and close it on first terrain/entity edit
local function lock_panel_if_needed()
    if state.panel_locked then return end
    state.panel_locked = true
    if state.panel_handle then
        state.panel_handle.lock_panel()
    end
    if state.panel_frame then
        state.panel_visible = false
        state.panel_frame.visible = false
    end
end

local function lock_seed()
    if state.seed_locked then return end
    local seed = engine.terrain.get_last_seed()
    if seed == 0 then seed = 1 end
    state.seed_locked = true
    state.locked_seed = seed
    if state.panel_handle then
        state.panel_handle.set_seed(seed)
        state.panel_handle.set_seed_locked(true)
    end
    lock_panel_if_needed()
    engine.log("Seed locked at " .. tostring(seed))
end

-- ─── Entity helpers ───────────────────────────────────────────────────────────

local ENTITY_SELECT_THRESHOLD_PX = 20

local function find_nearest_entity(sx, sy)
    local best_idx  = nil
    local best_dist = ENTITY_SELECT_THRESHOLD_PX * ENTITY_SELECT_THRESHOLD_PX
    local z = engine.camera.get_zoom()
    for i, ent in ipairs(state.entities) do
        local ecx = ent.x + (ent.size_w or 12) * 0.5
        local ecy = ent.y + (ent.size_h or 20) * 0.5
        local esx = (ecx - engine.camera.get_x()) * z
        local esy = (ecy - engine.camera.get_y()) * z
        local dx, dy = sx - esx, sy - esy
        local d2 = dx*dx + dy*dy
        if d2 < best_dist then
            best_dist = d2
            best_idx  = i
        end
    end
    return best_idx
end

local function push_entity_markers()
    if not engine.terrain.is_loaded() then
        engine.editor.clear_markers()
        return
    end
    local markers = {}
    for i, ent in ipairs(state.entities) do
        markers[#markers + 1] = {
            wx       = ent.x,
            wy       = ent.y,
            w        = ent.size_w or 12,
            h        = ent.size_h or 20,
            r        = ent.r or 200,
            g        = ent.g or 200,
            b        = ent.b or 200,
            selected = (i == state.selected_entity_idx),
            sprite   = ent.sprite_path or "",
        }
    end
    engine.editor.set_markers(markers)
end

-- Entity popup lifecycle
local function close_entity_popup()
    state.selected_entity_idx = nil
end

local function get_def_props(def_id)
    for _, obj in ipairs(engine.registry.get_objects()) do
        if obj.id == def_id then return obj.props or {} end
    end
    return {}
end

local function open_entity_popup(idx)
    local ent = state.entities[idx]
    if not ent then return end
    state.selected_entity_idx = idx
    local def_props = get_def_props(ent.def_id)
    EntityPopup.show(ent, def_props, function()
        state.selected_entity_idx = nil
    end)
end

-- Load entity list from a saved scenario table
local function load_entities_from_tbl(tbl)
    state.entities = {}
    if not tbl.entities then return end
    local all_objs = engine.registry.get_objects()
    local function find_obj(def_id)
        for _, obj in ipairs(all_objs) do
            if obj.id == def_id then return obj end
        end
        return nil
    end
    for _, e in ipairs(tbl.entities) do
        local def = find_obj(e.def)
        if not def then
            engine.log("WARN: entity def not found: " .. tostring(e.def))
        end
        state.entities[#state.entities + 1] = {
            def_id      = e.def,
            x           = e.x or 0,
            y           = e.y or 0,
            size_w      = def and def.size_w or 12,
            size_h      = def and def.size_h or 20,
            r           = def and def.r or 200,
            g           = def and def.g or 200,
            b           = def and def.b or 200,
            sprite_path = def and def.sprite_path or "",
            props       = e.props or {},
        }
    end
end

-- Rebuild the O(1) index from the overrides array (called after loading from disk)
local function rebuild_overrides_index()
    state.overrides_index = {}
    for i, ov in ipairs(state.overrides) do
        state.overrides_index[ov.x * 65536 + ov.y] = i
    end
end

-- Load overrides (handles both v2 compact and legacy formats)
local function load_overrides_from_tbl(tbl)
    if tbl.overrides_v2 then
        local decoded = engine.overrides.decode(tbl.overrides_v2)
        if decoded then
            state.overrides = decoded
            rebuild_overrides_index()
            return
        end
        engine.log("WARN: overrides_v2 decode failed, trying legacy")
    end
    state.overrides = tbl.overrides or {}
    rebuild_overrides_index()
end

-- ─── Tick callback ────────────────────────────────────────────────────────────

local function register_tick()
    local ZSTEP = layout.ZOOM_STEP
    local ZMIN  = layout.ZOOM_MIN
    local ZMAX  = layout.ZOOM_MAX

    engine.set_tick_callback(function(dt)
        local mx = engine.input.mouse_x()
        local my = engine.input.mouse_y()

        -- Refresh the live stats HUD (no-op when hidden)
        if state.stats_hud then state.stats_hud.update(dt) end

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

        -- ── Scroll wheel zoom (toward mouse position) ────────────────────────
        local scroll = engine.input.scroll_y()
        if scroll ~= 0 then
            local old_z = engine.camera.get_zoom()
            local new_z = math.max(ZMIN, math.min(ZMAX, old_z + scroll * ZSTEP))
            if new_z ~= old_z then
                local wx = engine.camera.get_x() + mx / old_z
                local wy = engine.camera.get_y() + my / old_z
                engine.camera.set_zoom(new_z)
                engine.camera.set_position(wx - mx / new_z, wy - my / new_z)
            end
        end

        -- ── Backtick: toggle log console ──────────────────────────────────────
        if engine.input.is_key_just_pressed(engine.key.GRAVE) then
            engine.log_console.toggle()
        end

        -- ── Ctrl+S save ───────────────────────────────────────────────────────
        local ctrl = engine.input.is_key_down(engine.key.LCTRL)
                  or engine.input.is_key_down(engine.key.RCTRL)
        if ctrl and engine.input.is_key_just_pressed(engine.key.S) then
            if state.current_path then do_save(state.current_path)
            else save_dialog() end
        end

        -- ── Paint / entity tools ──────────────────────────────────────────────
        if not engine.terrain.is_loaded() then
            push_entity_markers()
            return
        end

        local panel_right = WIN_W - (state.panel_visible and PANEL_W or 0)
        local in_viewport = (mx > TOOLBAR_W and mx < panel_right and my > MENU_H)
        -- Suppress paint / entity tools when the cursor is over an interactive
        -- UI element (open menu dropdown, popup dialog button, etc.) so that
        -- clicking a menu entry doesn't also paint the map underneath it.
        if in_viewport and engine.ui.is_mouse_over() then
            in_viewport = false
        end

        -- ── Tooltip (always update while terrain loaded) ───────────────────────
        if state.tooltip_label then
            if in_viewport then
                local wx, wy = screen_to_world(mx, my)
                local cell   = engine.terrain.get_cell(wx, wy)
                local parts  = {}
                if cell and cell.material_id ~= "" then parts[#parts+1] = cell.material_id end
                if engine.terrain.is_loaded() then
                    parts[#parts+1] = string.format("T=%.1f", engine.sim.get_temperature(wx, wy))
                    local hp = engine.sim.get_health(wx, wy)
                    if hp > 0 then parts[#parts+1] = string.format("HP=%d", hp) end
                    if engine.sim.is_ignited(wx, wy) then parts[#parts+1] = "[BURNING]" end
                    local crack = engine.sim.get_crack(wx, wy)
                    if crack > 0 then parts[#parts+1] = string.format("crack=%d", crack) end
                end
                local eidx   = find_nearest_entity(mx, my)
                if eidx then parts[#parts+1] = state.entities[eidx].def_id or "" end
                state.tooltip_label.text = table.concat(parts, " | ")
                state.tooltip_label.x    = mx + 14
                state.tooltip_label.y    = my - 4
            else
                state.tooltip_label.text = ""
            end
        end

        if not in_viewport then
            push_entity_markers()
            return
        end

        local tool   = state.toolbar_handle and state.toolbar_handle.get_tool() or "brush"
        local mat    = (tool ~= "eraser" and tool ~= "add_entity"
                        and tool ~= "rem_entity" and tool ~= "pointer")
                       and state.toolbar_handle and state.toolbar_handle.get_material() or nil
        local bsize  = state.toolbar_handle and state.toolbar_handle.get_brush_size() or 1

        local lmb       = engine.input.mouse_button(1)
        local lmb_press = engine.input.mouse_just_pressed(1)
        local lmb_rel   = engine.input.mouse_just_released(1)

        -- Clear last paint position on release
        if lmb_rel then
            state.last_paint_wx = nil
            state.last_paint_wy = nil
        end

        -- ── Ctrl+LMB: select entity (works with any tool) ─────────────────────
        if ctrl and lmb_press then
            local eidx = find_nearest_entity(mx, my)
            if eidx then
                open_entity_popup(eidx)
                push_entity_markers()
                return
            end
        end

        -- Helper: paint a Bresenham stroke (or single point) in one C++ call.
        -- engine.terrain.paint_line handles Bresenham + brush + one UpdateRegion.
        -- Returns a flat {x0,y0,x1,y1,...} array for override batch-upsert.
        local function do_paint(wx, wy, mat_id, bg_id)
            local flat
            if state.last_paint_wx ~= nil then
                flat = engine.terrain.paint_line(
                    state.last_paint_wx, state.last_paint_wy, wx, wy,
                    mat_id, bg_id, bsize)
            else
                flat = engine.terrain.paint_line(wx, wy, wx, wy,
                    mat_id, bg_id, bsize)
            end
            upsert_overrides_flat(flat, mat_id, bg_id)
            state.last_paint_wx = wx
            state.last_paint_wy = wy
        end

        -- ── Pointer tool ──────────────────────────────────────────────────────
        if tool == "pointer" then
            if lmb_press then
                local eidx = find_nearest_entity(mx, my)
                state.ptr_start_mx   = mx
                state.ptr_start_my   = my
                state.ptr_drag_moved = false
                if eidx then
                    local ent = state.entities[eidx]
                    local cwx, cwy = screen_to_world(mx, my)
                    state.ptr_drag_idx    = eidx
                    state.ptr_drag_off_wx = cwx - ent.x
                    state.ptr_drag_off_wy = cwy - ent.y
                    state.selected_entity_idx = eidx
                else
                    state.ptr_drag_idx        = nil
                    state.selected_entity_idx = nil
                end
            end

            if lmb and state.ptr_drag_idx then
                local ddx = mx - state.ptr_start_mx
                local ddy = my - state.ptr_start_my
                if ddx*ddx + ddy*ddy > 25 then state.ptr_drag_moved = true end
                if state.ptr_drag_moved then
                    local cwx, cwy = screen_to_world(mx, my)
                    local ent = state.entities[state.ptr_drag_idx]
                    ent.x = cwx - state.ptr_drag_off_wx
                    ent.y = cwy - state.ptr_drag_off_wy
                end
            end

            if lmb_rel then
                local eidx = state.ptr_drag_idx
                if eidx and not state.ptr_drag_moved then
                    -- Click (no drag) on already-selected entity → open property popup
                    if state.selected_entity_idx == eidx then
                        open_entity_popup(eidx)
                    end
                end
                state.ptr_drag_idx   = nil
                state.ptr_drag_moved = false
            end

        -- ── Terrain tools ─────────────────────────────────────────────────────
        elseif tool == "brush" and lmb then
            local wx, wy = screen_to_world(mx, my)
            local layer  = state.toolbar_handle and state.toolbar_handle.get_layer() or "fg"
            if layer == "fg" and mat then
                do_paint(wx, wy, mat.id, "")
                lock_seed()
            elseif layer == "bg" then
                local bg = state.toolbar_handle and state.toolbar_handle.get_bg_def()
                if bg then
                    do_paint(wx, wy, "", bg.id)
                    lock_seed()
                end
            end

        elseif tool == "eraser" and lmb then
            local wx, wy = screen_to_world(mx, my)
            local layer  = state.toolbar_handle and state.toolbar_handle.get_layer() or "fg"
            if layer == "fg" then
                do_paint(wx, wy, "base:Air", "base:Sky")
            else
                do_paint(wx, wy, "", "base:Sky")
            end
            lock_seed()

        elseif tool == "rect" then
            if lmb_press then
                local wx, wy = screen_to_world(mx, my)
                state.rect_start = { x = wx, y = wy }
            elseif lmb_rel and state.rect_start then
                local wx, wy = screen_to_world(mx, my)
                local layer  = state.toolbar_handle and state.toolbar_handle.get_layer() or "fg"
                local x0 = math.min(state.rect_start.x, wx)
                local x1 = math.max(state.rect_start.x, wx)
                local y0 = math.min(state.rect_start.y, wy)
                local y1 = math.max(state.rect_start.y, wy)
                if layer == "fg" and mat then
                    for rx = x0, x1 do
                        for ry = y0, y1 do
                            engine.terrain.set_cell(rx, ry, mat.id, "")
                            upsert_override(rx, ry, mat.id, "")
                        end
                    end
                    engine.log("Rect: " .. mat.name .. " " ..
                        (x1-x0+1) .. "x" .. (y1-y0+1))
                    lock_seed()
                elseif layer == "bg" then
                    local bg = state.toolbar_handle and state.toolbar_handle.get_bg_def()
                    if bg then
                        for rx = x0, x1 do
                            for ry = y0, y1 do
                                engine.terrain.set_cell(rx, ry, "", bg.id)
                                upsert_override(rx, ry, "", bg.id)
                            end
                        end
                        engine.log("Rect BG: " .. bg.name .. " " ..
                            (x1-x0+1) .. "x" .. (y1-y0+1))
                        lock_seed()
                    end
                end
                state.rect_start = nil
            elseif not lmb then
                state.rect_start = nil
            end

        -- ── Entity tools ──────────────────────────────────────────────────────
        elseif tool == "add_entity" and lmb_press then
            local def = state.toolbar_handle and state.toolbar_handle.get_entity_def()
            if def then
                local wx, wy = screen_to_world(mx, my)
                state.entities[#state.entities + 1] = {
                    def_id      = def.id,
                    x           = wx,
                    y           = wy,
                    size_w      = def.size_w,
                    size_h      = def.size_h,
                    r           = def.r,
                    g           = def.g,
                    b           = def.b,
                    sprite_path = def.sprite_path or "",
                    props       = {},
                }
                lock_panel_if_needed()
                engine.log("Placed: " .. def.id .. " at " .. wx .. "," .. wy)
            end

        elseif tool == "rem_entity" and lmb_press then
            local eidx = find_nearest_entity(mx, my)
            if eidx then
                if state.selected_entity_idx == eidx then
                    close_entity_popup()
                elseif state.selected_entity_idx and state.selected_entity_idx > eidx then
                    state.selected_entity_idx = state.selected_entity_idx - 1
                end
                table.remove(state.entities, eidx)
                lock_panel_if_needed()
                engine.log("Removed entity #" .. eidx)
            end

        -- ── Sim tools ────────────────────────────────────────────────────────
        elseif lmb and engine.terrain.is_loaded() then
            local wx, wy = screen_to_world(mx, my)
            local bsize  = state.brush_size or 1
            local half   = math.floor(bsize / 2)

            local function for_each_brush_cell(cx, cy, r, fn)
                for dy2 = -r, r do for dx2 = -r, r do
                    if dx2*dx2 + dy2*dy2 <= r*r then fn(cx + dx2, cy + dy2) end
                end end
            end

            if tool == "ignite" then
                for_each_brush_cell(wx, wy, half, function(bx, by) engine.sim.ignite(bx, by) end)
            elseif tool == "extinguish" then
                for_each_brush_cell(wx, wy, half, function(bx, by) engine.sim.extinguish(bx, by) end)
            elseif tool == "damage" then
                for_each_brush_cell(wx, wy, half, function(bx, by) engine.sim.apply_damage(bx, by, 50) end)
            elseif tool == "heat" then
                for_each_brush_cell(wx, wy, half, function(bx, by)
                    engine.sim.set_temperature(bx, by, engine.sim.get_temperature(bx, by) + 100)
                end)
            elseif tool == "chill" then
                for_each_brush_cell(wx, wy, half, function(bx, by)
                    engine.sim.set_temperature(bx, by, engine.sim.get_temperature(bx, by) - 100)
                end)
            elseif tool == "explode" and lmb_press then
                engine.sim.trigger_explosion(wx, wy, bsize * 4, 8)
            end
        end

        push_entity_markers()
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
    state.panel_frame = panel_frame
    local panel_handle = ScenarioPanel.build(panel_frame, function(map_config)
        generate(map_config)
    end)
    state.panel_handle = panel_handle

    if initial_tbl then
        panel_handle.set_from_scenario(initial_tbl)
    end

    -- Stats HUD — non-modal, top-right corner panel. Hidden by default; toggled
    -- by the View → Stats menu entry. Refreshed per-tick from register_tick().
    state.stats_hud = StatsDialog.build(screen, state)

    -- Tooltip label — floats near the mouse cursor
    state.tooltip_label = screen:add_label("", 0, 0)

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
            engine.editor.clear_markers()
            engine.ui.pop_screen()
        end,
        on_undo = function() engine.log("Undo — not yet implemented") end,
        on_redo = function() engine.log("Redo — not yet implemented") end,
        toggle_panel = function()
            state.panel_visible = not state.panel_visible
            panel_frame.visible = state.panel_visible
        end,
        toggle_overlay = function(mode)
            -- Toggle: if current overlay matches, turn off; otherwise set it.
            local cur = engine.debug.get_overlay()
            if cur == mode then
                engine.debug.set_overlay("none")
            else
                engine.debug.set_overlay(mode)
            end
        end,
        sim_speed = function(s)
            engine.sim.set_time_scale(s)
            engine.log("Simulation speed: " .. (s == 0 and "Paused" or (s .. "x")))
            if state.menu_bar_ctrl and state.menu_bar_ctrl.set_active_speed then
                state.menu_bar_ctrl.set_active_speed(s)
            end
        end,
        on_about = function() AboutDialog.show() end,
        on_stats = function() if state.stats_hud then state.stats_hud.toggle() end end,
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
    reset_state()
    load_overrides_from_tbl(tbl)
    load_entities_from_tbl(tbl)

    -- If the editor is already open, update panel + regenerate in place
    if state.panel_handle then
        state.panel_handle.set_from_scenario(tbl)
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
    state.panel_visible = true
    reset_state()
    load_overrides_from_tbl(tbl)
    load_entities_from_tbl(tbl)

    local panel_handle = build_editor_screen(tbl)

    if #state.overrides > 0 then
        state.locked_seed = tbl.map and tbl.map.seed or 0
        state.seed_locked = true
        panel_handle.set_seed_locked(true)
    end

    generate(panel_handle.get_config())
end

return M
