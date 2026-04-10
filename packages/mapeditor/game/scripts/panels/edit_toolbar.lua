-- Left-side editing toolbar for the map editor.
--
-- Layout (vertical strip, TOOLBAR_W px wide):
--   [Tool buttons]
--   [Brush size: − N +]
--   [Material list — scrollable]
--
-- Usage:
--   local EditToolbar = require("panels/edit_toolbar")
--   local handle = EditToolbar.build(screen)
--
-- handle.get_tool()        → "brush" | "eraser" | "rect" | "add_entity" | "rem_entity"
-- handle.get_material()    → {id, name, r, g, b} or nil
-- handle.get_background()  → string qualified_id (background to pair with eraser)
-- handle.get_brush_size()  → integer 1..9 (cells painted = size×size square)

local layout = require("util/layout")

local WIN_H     = layout.WIN_H
local MENU_H    = layout.MENU_H
local W         = layout.TOOLBAR_W   -- 96
local BTN_H     = 24
local BTN_W     = W - 4
local BTN_X     = 2
local ROW_H     = 20
local MAT_W     = W - 4
local SMALL_BTN = 20   -- width of +/- buttons for brush size

local M = {}

function M.build(screen)
    local state = {
        tool         = "brush",
        material     = nil,
        brush_size   = 1,
        tool_buttons = {},
        mat_buttons  = {},
        scroll       = 0,
    }

    local total_h = WIN_H - MENU_H
    local toolbar = screen:add_frame(0, MENU_H, W, total_h)

    -- ── Tool section ──────────────────────────────────────────────────────────

    local TOOLS = {
        { id = "brush",      label = "Brush"    },
        { id = "eraser",     label = "Eraser"   },
        { id = "rect",       label = "Rect"     },
        { id = "add_entity", label = "+Entity",  disabled = true },
        { id = "rem_entity", label = "-Entity",  disabled = true },
    }

    local function refresh_tool_buttons()
        for _, tb in ipairs(state.tool_buttons) do
            tb.btn.text = (state.tool == tb.id) and ("[" .. tb.label .. "]") or tb.label
        end
    end

    local y = 4
    for _, tool in ipairs(TOOLS) do
        local btn = toolbar:add_button(tool.label, BTN_X, y, BTN_W, BTN_H)
        btn.text_left = false
        if tool.disabled then
            btn.disabled = true
        else
            local tool_id = tool.id
            btn:on_click(function()
                state.tool = tool_id
                refresh_tool_buttons()
            end)
        end
        state.tool_buttons[#state.tool_buttons + 1] = {
            btn = btn, id = tool.id, label = tool.label
        }
        y = y + BTN_H + 2
    end
    refresh_tool_buttons()

    -- ── Brush size row ────────────────────────────────────────────────────────

    y = y + 4
    toolbar:add_label("Size", BTN_X, y + 3)
    local SIZE_LABEL_W = 30
    local size_lbl_x   = BTN_X + SIZE_LABEL_W + 2
    local size_num_w   = BTN_W - SIZE_LABEL_W - SMALL_BTN * 2 - 4

    local btn_size_dec = toolbar:add_button("-", size_lbl_x, y, SMALL_BTN, BTN_H)
    local size_display = toolbar:add_button("1", size_lbl_x + SMALL_BTN, y, size_num_w, BTN_H)
    size_display.disabled = true   -- non-interactive display only
    local btn_size_inc = toolbar:add_button("+", size_lbl_x + SMALL_BTN + size_num_w, y, SMALL_BTN, BTN_H)

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

    -- Separator label
    toolbar:add_label("Mat", BTN_X, y + 2)
    y = y + 14

    -- ── Material section ─────────────────────────────────────────────────────

    local mats = engine.registry.get_materials()

    local LIST_Y      = y
    local LIST_H      = total_h - LIST_Y - 22   -- leave room for scroll buttons
    local MAX_VISIBLE = math.floor(LIST_H / ROW_H)

    local function refresh_mat_buttons()
        for i, mb in ipairs(state.mat_buttons) do
            local row     = i - 1  -- 0-based
            local visible = (row >= state.scroll and row < state.scroll + MAX_VISIBLE)
            mb.btn.visible = visible
            if visible then
                mb.btn.y = LIST_Y + (row - state.scroll) * ROW_H
            end
            -- Highlight selected
            if state.material and state.material.id == mb.mat.id then
                mb.btn.text = ">" .. mb.mat.name
            else
                mb.btn.text = mb.mat.name
            end
        end
    end

    for i, mat in ipairs(mats) do
        local btn = toolbar:add_button(mat.name, BTN_X, LIST_Y + (i-1) * ROW_H, MAT_W, ROW_H - 1)
        btn.text_left = true
        btn.visible   = false
        local mat_ref = mat
        btn:on_click(function()
            state.material = mat_ref
            refresh_mat_buttons()
        end)
        state.mat_buttons[#state.mat_buttons + 1] = { btn = btn, mat = mat }
    end

    -- Select first material by default
    if #mats > 0 then
        state.material = mats[1]
    end
    refresh_mat_buttons()

    -- Scroll buttons (only shown if list overflows)
    if #mats > MAX_VISIBLE then
        local up_btn = toolbar:add_button("^", BTN_X, total_h - 18, MAT_W, 10)
        local dn_btn = toolbar:add_button("v", BTN_X, total_h - 8,  MAT_W, 10)
        up_btn:on_click(function()
            state.scroll = math.max(0, state.scroll - 1)
            refresh_mat_buttons()
        end)
        dn_btn:on_click(function()
            state.scroll = math.min(#mats - MAX_VISIBLE, state.scroll + 1)
            refresh_mat_buttons()
        end)
    end

    -- ── Public handle ─────────────────────────────────────────────────────────

    local handle = {}

    function handle.get_tool()
        return state.tool
    end

    function handle.get_material()
        return state.material
    end

    function handle.get_background()
        -- For eraser: pair Air with Sky background
        return "base:Sky"
    end

    function handle.get_brush_size()
        return state.brush_size
    end

    return handle
end

return M
