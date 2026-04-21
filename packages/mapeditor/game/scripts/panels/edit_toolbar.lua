-- Left-side editing toolbar for the map editor.
--
-- Layout (vertical strip, TOOLBAR_W px wide):
--   [Tool buttons]
--   [FG] [BG] layer toggle   (hidden for entity/pointer tools)
--   [Brush size: − N +]      (hidden for entity/pointer tools)
--   [Section label]          (Mat / BG / Ent — or blank for pointer)
--   [Material list]          (visible when FG layer + terrain tool)
--   [Background list]        (visible when BG layer + terrain tool)
--   [Entity type list]       (visible when entity tools active)
--   [Pointer hint label]     (visible when pointer tool active)
--
-- handle.get_tool()        → "pointer"|"brush"|"eraser"|"rect"|"add_entity"|"rem_entity"
-- handle.get_layer()       → "fg" | "bg"
-- handle.get_material()    → {id, name, r, g, b} or nil
-- handle.get_bg_def()      → {id, name, r, g, b} or nil
-- handle.get_background()  → string bg id used by the eraser in FG mode
-- handle.get_brush_size()  → int 1..9
-- handle.get_entity_def()  → {id, name, size_w, size_h, r, g, b, props} or nil

local layout  = require("util/layout")
local widgets = require("util/widgets")

local WIN_H  = layout.WIN_H
local MENU_H = layout.MENU_H
local W      = layout.TOOLBAR_W
local BTN_H  = 24
local BTN_W  = W - 4
local BTN_X  = 2
local ROW_H  = 20
local MAT_W  = W - 4
local SMALL_BTN = 20

local TOOLS = {
    { id = "pointer",    label = "Pointer" },
    { id = "brush",      label = "Brush"   },
    { id = "eraser",     label = "Eraser"  },
    { id = "rect",       label = "Rect"    },
    { id = "add_entity", label = "+Entity" },
    { id = "rem_entity", label = "-Entity" },
    { id = "ignite",     label = "Ignite"  },
    { id = "extinguish", label = "Snuff"   },
    { id = "damage",     label = "Damage"  },
    { id = "heat",       label = "Heat+"   },
    { id = "chill",      label = "Heat-"   },
    { id = "explode",    label = "Boom"    },
}

local M = {}

function M.build(screen)
    local state = {
        tool         = "pointer",
        layer        = "fg",
        material     = nil,
        bg_def       = nil,
        brush_size   = 1,
        entity_def   = nil,
        tool_buttons = {},
        mat_buttons  = {},
        bg_buttons   = {},
        ent_buttons  = {},
        mat_scroll   = 0,
        bg_scroll    = 0,
        ent_scroll   = 0,
    }

    local total_h = WIN_H - MENU_H
    local toolbar = widgets.frame(screen, { x = 0, y = MENU_H, w = W, h = total_h })

    local function is_entity_tool()  return state.tool == "add_entity" or state.tool == "rem_entity" end
    local function is_pointer_tool() return state.tool == "pointer" end
    local function is_terrain_tool() return not is_entity_tool() and not is_pointer_tool() end

    -- Forward declarations so click handlers can call refresh_all / refresh_*
    local refresh_all
    local refresh_mat_buttons, refresh_bg_buttons, refresh_ent_buttons

    -- ── Tool buttons ──────────────────────────────────────────────────────────
    local y = 4
    for _, tool in ipairs(TOOLS) do
        local tool_id = tool.id
        local btn = widgets.button(toolbar, {
            text = tool.label, x = BTN_X, y = y, w = BTN_W, h = BTN_H,
            on_click = function() state.tool = tool_id; refresh_all() end,
        })
        state.tool_buttons[#state.tool_buttons + 1] =
            { btn = btn, id = tool.id, label = tool.label }
        y = y + BTN_H + 2
    end

    -- ── Layer toggle [FG] [BG] ────────────────────────────────────────────────
    y = y + 4
    local y_layer = y
    local half    = math.floor(BTN_W / 2) - 1
    local fg_btn = widgets.button(toolbar, {
        text = "[FG]", x = BTN_X, y = y_layer, w = half, h = BTN_H,
        on_click = function()
            state.layer = "fg"; refresh_all()
        end,
    })
    local bg_btn = widgets.button(toolbar, {
        text = "BG", x = BTN_X + half + 2, y = y_layer, w = half, h = BTN_H,
        on_click = function()
            state.layer = "bg"; refresh_all()
        end,
    })

    -- ── Brush size row ────────────────────────────────────────────────────────
    local y_size = y_layer + BTN_H + 4
    local size_label = widgets.label(toolbar, { text = "Size", x = BTN_X, y = y_size + 3 })

    local SIZE_LABEL_W = 30
    local size_lbl_x   = BTN_X + SIZE_LABEL_W + 2
    local size_num_w   = BTN_W - SIZE_LABEL_W - SMALL_BTN * 2 - 4

    local size_display
    local btn_size_dec = widgets.button(toolbar, {
        text = "-", x = size_lbl_x, y = y_size, w = SMALL_BTN, h = BTN_H,
        on_click = function()
            state.brush_size = math.max(1, state.brush_size - 2)
            size_display.text = tostring(state.brush_size)
        end,
    })
    size_display = widgets.button(toolbar, {
        text = "1", x = size_lbl_x + SMALL_BTN, y = y_size, w = size_num_w, h = BTN_H,
        disabled = true,
    })
    local btn_size_inc = widgets.button(toolbar, {
        text = "+", x = size_lbl_x + SMALL_BTN + size_num_w, y = y_size, w = SMALL_BTN, h = BTN_H,
        on_click = function()
            state.brush_size = math.min(9, state.brush_size + 2)
            size_display.text = tostring(state.brush_size)
        end,
    })
    local size_row_elements = { size_label, btn_size_dec, size_display, btn_size_inc }

    -- ── Section label ─────────────────────────────────────────────────────────
    local y_sep = y_size + BTN_H + 4
    local sep_label = widgets.label(toolbar, { text = "Mat", x = BTN_X, y = y_sep + 2 })

    -- ── Lists area ────────────────────────────────────────────────────────────
    local LIST_Y   = y_sep + 14
    local LIST_H   = total_h - LIST_Y - 22
    local LIST_VIS = math.floor(LIST_H / ROW_H)

    local function build_list(items, key_store, on_pick)
        for i, item in ipairs(items) do
            local item_ref = item
            local btn = widgets.button(toolbar, {
                text      = item.name,
                text_left = true,
                visible   = false,
                x = BTN_X, y = LIST_Y + (i - 1) * ROW_H, w = MAT_W, h = ROW_H - 1,
                on_click  = function() on_pick(item_ref) end,
            })
            key_store[#key_store + 1] = { btn = btn, def = item }
        end
    end

    local function build_scroll(items, on_up, on_down)
        if #items <= LIST_VIS then return nil, nil end
        local up = widgets.button(toolbar, {
            text = "^", x = BTN_X, y = total_h - 18, w = MAT_W, h = 10,
            on_click = on_up,
        })
        local dn = widgets.button(toolbar, {
            text = "v", x = BTN_X, y = total_h - 8,  w = MAT_W, h = 10,
            on_click = on_down,
        })
        return up, dn
    end

    -- Material list
    local mats = engine.registry.get_materials()
    build_list(mats, state.mat_buttons, function(m)
        state.material = m; refresh_mat_buttons()
    end)
    if #mats > 0 then state.material = mats[1] end
    local mat_up, mat_dn = build_scroll(mats,
        function()
            state.mat_scroll = math.max(0, state.mat_scroll - 1)
            refresh_mat_buttons()
        end,
        function()
            state.mat_scroll = math.min(#mats - LIST_VIS, state.mat_scroll + 1)
            refresh_mat_buttons()
        end)

    -- Background list
    local bgs = engine.registry.get_backgrounds()
    build_list(bgs, state.bg_buttons, function(b)
        state.bg_def = b; refresh_bg_buttons()
    end)
    if #bgs > 0 then state.bg_def = bgs[1] end
    local bg_up, bg_dn = build_scroll(bgs,
        function()
            state.bg_scroll = math.max(0, state.bg_scroll - 1)
            refresh_bg_buttons()
        end,
        function()
            state.bg_scroll = math.min(#bgs - LIST_VIS, state.bg_scroll + 1)
            refresh_bg_buttons()
        end)

    -- Entity list
    local objs = engine.registry.get_objects()
    build_list(objs, state.ent_buttons, function(o)
        state.entity_def = o; refresh_ent_buttons()
    end)
    if #objs > 0 then state.entity_def = objs[1] end
    local ent_up, ent_dn = build_scroll(objs,
        function()
            state.ent_scroll = math.max(0, state.ent_scroll - 1)
            refresh_ent_buttons()
        end,
        function()
            state.ent_scroll = math.min(#objs - LIST_VIS, state.ent_scroll + 1)
            refresh_ent_buttons()
        end)

    -- Pointer hint
    local pointer_hint1 = widgets.label(toolbar, {
        text = "Click: select", x = BTN_X, y = LIST_Y, visible = false,
    })
    local pointer_hint2 = widgets.label(toolbar, {
        text = "Drag: move",    x = BTN_X, y = LIST_Y + 16, visible = false,
    })

    -- ── Refreshers ────────────────────────────────────────────────────────────
    local function refresh_tool_buttons()
        for _, tb in ipairs(state.tool_buttons) do
            tb.btn.text = (state.tool == tb.id) and ("[" .. tb.label .. "]") or tb.label
        end
    end

    local function refresh_layer_buttons()
        local show = is_terrain_tool()
        fg_btn.visible = show
        bg_btn.visible = show
        fg_btn.text = (state.layer == "fg") and "[FG]" or "FG"
        bg_btn.text = (state.layer == "bg") and "[BG]" or "BG"
    end

    local function refresh_size_row()
        local show = is_terrain_tool()
        for _, el in ipairs(size_row_elements) do el.visible = show end
    end

    local function refresh_section_label()
        if is_pointer_tool()     then sep_label.text = ""
        elseif is_entity_tool()  then sep_label.text = "Ent"
        elseif state.layer == "bg" then sep_label.text = "BG"
        else                          sep_label.text = "Mat" end
    end

    refresh_mat_buttons = function()
        for i, mb in ipairs(state.mat_buttons) do
            local row = i - 1
            local visible = is_terrain_tool()
                        and state.layer == "fg"
                        and row >= state.mat_scroll
                        and row <  state.mat_scroll + LIST_VIS
            mb.btn.visible = visible
            if visible then mb.btn.y = LIST_Y + (row - state.mat_scroll) * ROW_H end
            mb.btn.text = (state.material and state.material.id == mb.def.id)
                      and (">" .. mb.def.name)
                      or   mb.def.name
        end
        if mat_up then
            mat_up.visible = is_terrain_tool() and state.layer == "fg"
            mat_dn.visible = is_terrain_tool() and state.layer == "fg"
        end
    end

    refresh_bg_buttons = function()
        for i, bb in ipairs(state.bg_buttons) do
            local row = i - 1
            local visible = is_terrain_tool()
                        and state.layer == "bg"
                        and row >= state.bg_scroll
                        and row <  state.bg_scroll + LIST_VIS
            bb.btn.visible = visible
            if visible then bb.btn.y = LIST_Y + (row - state.bg_scroll) * ROW_H end
            bb.btn.text = (state.bg_def and state.bg_def.id == bb.def.id)
                      and (">" .. bb.def.name)
                      or   bb.def.name
        end
        if bg_up then
            bg_up.visible = is_terrain_tool() and state.layer == "bg"
            bg_dn.visible = is_terrain_tool() and state.layer == "bg"
        end
    end

    refresh_ent_buttons = function()
        for i, eb in ipairs(state.ent_buttons) do
            local row = i - 1
            local visible = is_entity_tool()
                        and row >= state.ent_scroll
                        and row <  state.ent_scroll + LIST_VIS
            eb.btn.visible = visible
            if visible then eb.btn.y = LIST_Y + (row - state.ent_scroll) * ROW_H end
            eb.btn.text = (state.entity_def and state.entity_def.id == eb.def.id)
                      and (">" .. eb.def.name)
                      or   eb.def.name
        end
        if ent_up then
            ent_up.visible = is_entity_tool()
            ent_dn.visible = is_entity_tool()
        end
    end

    local function refresh_pointer_hint()
        local show = is_pointer_tool()
        pointer_hint1.visible = show
        pointer_hint2.visible = show
    end

    refresh_all = function()
        refresh_tool_buttons()
        refresh_layer_buttons()
        refresh_size_row()
        refresh_section_label()
        refresh_mat_buttons()
        refresh_bg_buttons()
        refresh_ent_buttons()
        refresh_pointer_hint()
    end

    refresh_all()

    local handle = {}
    function handle.get_tool()       return state.tool       end
    function handle.get_layer()      return state.layer      end
    function handle.get_material()   return state.material   end
    function handle.get_bg_def()     return state.bg_def     end
    function handle.get_brush_size() return state.brush_size end
    function handle.get_entity_def() return state.entity_def end
    function handle.get_background() return "base:Sky"       end
    return handle
end

return M
