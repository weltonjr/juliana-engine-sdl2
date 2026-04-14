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

local layout = require("util/layout")

local WIN_H     = layout.WIN_H
local MENU_H    = layout.MENU_H
local W         = layout.TOOLBAR_W   -- 96
local BTN_H     = 24
local BTN_W     = W - 4
local BTN_X     = 2
local ROW_H     = 20
local MAT_W     = W - 4
local SMALL_BTN = 20

local M = {}

function M.build(screen)
    local state = {
        tool         = "pointer",
        layer        = "fg",       -- "fg" | "bg"
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
    local toolbar = screen:add_frame(0, MENU_H, W, total_h)

    -- ── Tool classification helpers ───────────────────────────────────────────

    local function is_entity_tool()
        return state.tool == "add_entity" or state.tool == "rem_entity"
    end

    local function is_pointer_tool()
        return state.tool == "pointer"
    end

    local function is_terrain_tool()
        return not is_entity_tool() and not is_pointer_tool()
    end

    -- ── Tool buttons ──────────────────────────────────────────────────────────

    local TOOLS = {
        { id = "pointer",    label = "Pointer" },
        { id = "brush",      label = "Brush"   },
        { id = "eraser",     label = "Eraser"  },
        { id = "rect",       label = "Rect"    },
        { id = "add_entity", label = "+Entity" },
        { id = "rem_entity", label = "-Entity" },
    }

    local y = 4
    for _, tool in ipairs(TOOLS) do
        local btn = toolbar:add_button(tool.label, BTN_X, y, BTN_W, BTN_H)
        btn.text_left = false
        state.tool_buttons[#state.tool_buttons + 1] = {
            btn = btn, id = tool.id, label = tool.label
        }
        y = y + BTN_H + 2
    end

    -- ── Layer toggle [FG] [BG] ────────────────────────────────────────────────

    y = y + 4
    local half_btn_w = math.floor(BTN_W / 2) - 1
    local fg_btn = toolbar:add_button("[FG]", BTN_X, y, half_btn_w, BTN_H)
    local bg_btn = toolbar:add_button("BG",   BTN_X + half_btn_w + 2, y, half_btn_w, BTN_H)
    fg_btn.text_left = false
    bg_btn.text_left = false

    y = y + BTN_H + 4

    -- ── Brush size row ────────────────────────────────────────────────────────

    local size_label = toolbar:add_label("Size", BTN_X, y + 3)
    local SIZE_LABEL_W = 30
    local size_lbl_x   = BTN_X + SIZE_LABEL_W + 2
    local size_num_w   = BTN_W - SIZE_LABEL_W - SMALL_BTN * 2 - 4

    local btn_size_dec = toolbar:add_button("-", size_lbl_x, y, SMALL_BTN, BTN_H)
    local size_display = toolbar:add_button("1", size_lbl_x + SMALL_BTN, y, size_num_w, BTN_H)
    size_display.disabled = true
    local btn_size_inc = toolbar:add_button("+", size_lbl_x + SMALL_BTN + size_num_w, y, SMALL_BTN, BTN_H)

    local size_row_elements = { size_label, btn_size_dec, size_display, btn_size_inc }

    local function update_size_display()
        size_display.text = tostring(state.brush_size)
    end

    btn_size_dec:on_click(function()
        state.brush_size = math.max(1, state.brush_size - 2)
        update_size_display()
    end)
    btn_size_inc:on_click(function()
        state.brush_size = math.min(9, state.brush_size + 2)
        update_size_display()
    end)

    y = y + BTN_H + 4

    -- ── Section label ─────────────────────────────────────────────────────────

    local sep_label = toolbar:add_label("Mat", BTN_X, y + 2)
    y = y + 14

    -- ── Lists area ────────────────────────────────────────────────────────────

    local LIST_Y  = y
    local LIST_H  = total_h - LIST_Y - 22
    local LIST_VIS = math.floor(LIST_H / ROW_H)

    -- ── Material list ─────────────────────────────────────────────────────────

    local mats = engine.registry.get_materials()

    for i, mat in ipairs(mats) do
        local btn = toolbar:add_button(mat.name, BTN_X, LIST_Y + (i-1) * ROW_H, MAT_W, ROW_H - 1)
        btn.text_left = true
        btn.visible   = false
        local mat_ref = mat
        btn:on_click(function()
            state.material = mat_ref
            -- refresh called via refresh_all below
        end)
        state.mat_buttons[#state.mat_buttons + 1] = { btn = btn, mat = mat }
    end
    if #mats > 0 then state.material = mats[1] end

    local mat_up_btn, mat_dn_btn
    if #mats > LIST_VIS then
        mat_up_btn = toolbar:add_button("^", BTN_X, total_h - 18, MAT_W, 10)
        mat_dn_btn = toolbar:add_button("v", BTN_X, total_h - 8,  MAT_W, 10)
        mat_up_btn:on_click(function()
            state.mat_scroll = math.max(0, state.mat_scroll - 1)
        end)
        mat_dn_btn:on_click(function()
            state.mat_scroll = math.min(#mats - LIST_VIS, state.mat_scroll + 1)
        end)
    end

    -- ── Background list ───────────────────────────────────────────────────────

    local bgs = engine.registry.get_backgrounds()

    for i, bg in ipairs(bgs) do
        local btn = toolbar:add_button(bg.name, BTN_X, LIST_Y + (i-1) * ROW_H, MAT_W, ROW_H - 1)
        btn.text_left = true
        btn.visible   = false
        local bg_ref = bg
        btn:on_click(function()
            state.bg_def = bg_ref
            -- refresh called via refresh_all below
        end)
        state.bg_buttons[#state.bg_buttons + 1] = { btn = btn, bg = bg }
    end
    if #bgs > 0 then state.bg_def = bgs[1] end

    local bg_up_btn, bg_dn_btn
    if #bgs > LIST_VIS then
        bg_up_btn = toolbar:add_button("^", BTN_X, total_h - 18, MAT_W, 10)
        bg_dn_btn = toolbar:add_button("v", BTN_X, total_h - 8,  MAT_W, 10)
        bg_up_btn:on_click(function()
            state.bg_scroll = math.max(0, state.bg_scroll - 1)
        end)
        bg_dn_btn:on_click(function()
            state.bg_scroll = math.min(#bgs - LIST_VIS, state.bg_scroll + 1)
        end)
    end

    -- ── Entity type list ──────────────────────────────────────────────────────

    local objs = engine.registry.get_objects()

    for i, obj in ipairs(objs) do
        local btn = toolbar:add_button(obj.name, BTN_X, LIST_Y + (i-1) * ROW_H, MAT_W, ROW_H - 1)
        btn.text_left = true
        btn.visible   = false
        local obj_ref = obj
        btn:on_click(function()
            state.entity_def = obj_ref
            -- refresh called via refresh_all below
        end)
        state.ent_buttons[#state.ent_buttons + 1] = { btn = btn, obj = obj }
    end
    if #objs > 0 then state.entity_def = objs[1] end

    local ent_up_btn, ent_dn_btn
    if #objs > LIST_VIS then
        ent_up_btn = toolbar:add_button("^", BTN_X, total_h - 18, MAT_W, 10)
        ent_dn_btn = toolbar:add_button("v", BTN_X, total_h - 8,  MAT_W, 10)
        ent_up_btn:on_click(function()
            state.ent_scroll = math.max(0, state.ent_scroll - 1)
        end)
        ent_dn_btn:on_click(function()
            state.ent_scroll = math.min(#objs - LIST_VIS, state.ent_scroll + 1)
        end)
    end

    -- ── Pointer hint label ────────────────────────────────────────────────────

    local pointer_hint1 = toolbar:add_label("Click: select", BTN_X, LIST_Y)
    local pointer_hint2 = toolbar:add_label("Drag: move",   BTN_X, LIST_Y + 16)
    pointer_hint1.visible = false
    pointer_hint2.visible = false

    -- ── Refresh functions ─────────────────────────────────────────────────────

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
        if is_pointer_tool() then
            sep_label.text = ""
        elseif is_entity_tool() then
            sep_label.text = "Ent"
        elseif state.layer == "bg" then
            sep_label.text = "BG"
        else
            sep_label.text = "Mat"
        end
    end

    local function refresh_mat_buttons()
        for i, mb in ipairs(state.mat_buttons) do
            local row     = i - 1
            local visible = is_terrain_tool()
                            and state.layer == "fg"
                            and row >= state.mat_scroll
                            and row < state.mat_scroll + LIST_VIS
            mb.btn.visible = visible
            if visible then mb.btn.y = LIST_Y + (row - state.mat_scroll) * ROW_H end
            mb.btn.text = (state.material and state.material.id == mb.mat.id)
                          and (">" .. mb.mat.name)
                          or  mb.mat.name
        end
        if mat_up_btn then
            mat_up_btn.visible = is_terrain_tool() and state.layer == "fg"
            mat_dn_btn.visible = is_terrain_tool() and state.layer == "fg"
        end
    end

    local function refresh_bg_buttons()
        for i, bb in ipairs(state.bg_buttons) do
            local row     = i - 1
            local visible = is_terrain_tool()
                            and state.layer == "bg"
                            and row >= state.bg_scroll
                            and row < state.bg_scroll + LIST_VIS
            bb.btn.visible = visible
            if visible then bb.btn.y = LIST_Y + (row - state.bg_scroll) * ROW_H end
            bb.btn.text = (state.bg_def and state.bg_def.id == bb.bg.id)
                          and (">" .. bb.bg.name)
                          or  bb.bg.name
        end
        if bg_up_btn then
            bg_up_btn.visible = is_terrain_tool() and state.layer == "bg"
            bg_dn_btn.visible = is_terrain_tool() and state.layer == "bg"
        end
    end

    local function refresh_ent_buttons()
        for i, eb in ipairs(state.ent_buttons) do
            local row     = i - 1
            local visible = is_entity_tool()
                            and row >= state.ent_scroll
                            and row < state.ent_scroll + LIST_VIS
            eb.btn.visible = visible
            if visible then eb.btn.y = LIST_Y + (row - state.ent_scroll) * ROW_H end
            eb.btn.text = (state.entity_def and state.entity_def.id == eb.obj.id)
                          and (">" .. eb.obj.name)
                          or  eb.obj.name
        end
        if ent_up_btn then
            ent_up_btn.visible = is_entity_tool()
            ent_dn_btn.visible = is_entity_tool()
        end
    end

    local function refresh_pointer_hint()
        local show = is_pointer_tool()
        pointer_hint1.visible = show
        pointer_hint2.visible = show
    end

    local function refresh_all()
        refresh_tool_buttons()
        refresh_layer_buttons()
        refresh_size_row()
        refresh_section_label()
        refresh_mat_buttons()
        refresh_bg_buttons()
        refresh_ent_buttons()
        refresh_pointer_hint()
    end

    -- ── Wire up on_click handlers ─────────────────────────────────────────────

    for _, tb in ipairs(state.tool_buttons) do
        local tool_id = tb.id
        tb.btn:on_click(function()
            state.tool = tool_id
            refresh_all()
        end)
    end

    fg_btn:on_click(function()
        state.layer = "fg"
        refresh_layer_buttons()
        refresh_section_label()
        refresh_mat_buttons()
        refresh_bg_buttons()
    end)

    bg_btn:on_click(function()
        state.layer = "bg"
        refresh_layer_buttons()
        refresh_section_label()
        refresh_mat_buttons()
        refresh_bg_buttons()
    end)

    -- Hook mat/bg/ent item clicks to refresh selection markers
    for _, mb in ipairs(state.mat_buttons) do
        local orig_click = mb.btn   -- already has on_click set above; re-bind with refresh
        local mat_ref = mb.mat
        mb.btn:on_click(function()
            state.material = mat_ref
            refresh_mat_buttons()
        end)
    end

    for _, bb in ipairs(state.bg_buttons) do
        local bg_ref = bb.bg
        bb.btn:on_click(function()
            state.bg_def = bg_ref
            refresh_bg_buttons()
        end)
    end

    for _, eb in ipairs(state.ent_buttons) do
        local obj_ref = eb.obj
        eb.btn:on_click(function()
            state.entity_def = obj_ref
            refresh_ent_buttons()
        end)
    end

    -- Hook scroll buttons to also call refresh
    if mat_up_btn then
        mat_up_btn:on_click(function()
            state.mat_scroll = math.max(0, state.mat_scroll - 1)
            refresh_mat_buttons()
        end)
        mat_dn_btn:on_click(function()
            state.mat_scroll = math.min(#mats - LIST_VIS, state.mat_scroll + 1)
            refresh_mat_buttons()
        end)
    end

    if bg_up_btn then
        bg_up_btn:on_click(function()
            state.bg_scroll = math.max(0, state.bg_scroll - 1)
            refresh_bg_buttons()
        end)
        bg_dn_btn:on_click(function()
            state.bg_scroll = math.min(#bgs - LIST_VIS, state.bg_scroll + 1)
            refresh_bg_buttons()
        end)
    end

    if ent_up_btn then
        ent_up_btn:on_click(function()
            state.ent_scroll = math.max(0, state.ent_scroll - 1)
            refresh_ent_buttons()
        end)
        ent_dn_btn:on_click(function()
            state.ent_scroll = math.min(#objs - LIST_VIS, state.ent_scroll + 1)
            refresh_ent_buttons()
        end)
    end

    -- Initial pass
    refresh_all()

    -- ── Public handle ─────────────────────────────────────────────────────────

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
