-- widgets.lua — declarative wrappers over engine.ui.
--
-- Every helper accepts a parent frame/screen plus an options table and
-- returns the created element. The options table mirrors the UIElement
-- fields, so reading a call site reads like a widget spec:
--
--     widgets.button(panel, {
--         text     = "Save",
--         x = 10, y = 10, w = 80, h = 24,
--         on_click = function() save() end,
--     })
--
-- Keep this module thin: anything more than "build element + set fields"
-- belongs in the caller or in a dedicated panel module.

local M = {}

local function apply_common(el, o)
    if o.id       ~= nil then el.id       = o.id       end
    if o.visible  ~= nil then el.visible  = o.visible  end
    if o.disabled ~= nil then el.disabled = o.disabled end
end

function M.frame(parent, o)
    local el = parent:add_frame(o.x or 0, o.y or 0, o.w or 0, o.h or 0)
    apply_common(el, o)
    return el
end

function M.label(parent, o)
    local el = parent:add_label(o.text or "", o.x or 0, o.y or 0)
    apply_common(el, o)
    return el
end

function M.button(parent, o)
    local el = parent:add_button(o.text or "", o.x or 0, o.y or 0, o.w or 80, o.h or 24)
    if o.text_left ~= nil then el.text_left = o.text_left end
    if o.on_click  ~= nil then el.on_click  = o.on_click  end
    apply_common(el, o)
    return el
end

function M.input(parent, o)
    local el = parent:add_input(o.placeholder or "",
                                o.x or 0, o.y or 0, o.w or 120, o.h or 20)
    if o.value      ~= nil then el.value      = o.value      end
    if o.max_length ~= nil then el.max_length = o.max_length end
    if o.on_change  ~= nil then el.on_change  = o.on_change  end
    apply_common(el, o)
    return el
end

-- Numeric [−] [value] [+] row. Calls on_change(new_value) after any change.
-- Handles float/int formatting and min/max clamping so callers don't repeat it.
function M.spinner(parent, o)
    local is_float = o.is_float or false
    local step     = o.step     or (is_float and 0.1 or 1)
    local vmin     = o.min      or (is_float and 0.0 or 0)
    local vmax     = o.max      or (is_float and 1.0 or 9999)
    local w        = o.w        or 80
    local h        = o.h        or 20
    local btn_w    = o.btn_w    or 20

    local function fmt(v)
        if is_float then return string.format("%.2f", v)
        else             return tostring(math.floor(v + 0.5)) end
    end

    local value = o.value or 0

    local dec = parent:add_button("-", o.x, o.y, btn_w, h)
    local inp = parent:add_input("", o.x + btn_w + 2, o.y, w - btn_w * 2 - 4, h)
    local inc = parent:add_button("+", o.x + w - btn_w, o.y, btn_w, h)

    inp.value = fmt(value)

    local function commit(new)
        if new < vmin then new = vmin end
        if new > vmax then new = vmax end
        if not is_float then new = math.floor(new + 0.5) end
        value     = new
        inp.value = fmt(new)
        if o.on_change then o.on_change(new) end
    end

    dec.on_click = function() commit(value - step) end
    inc.on_click = function() commit(value + step) end
    inp.on_change = function(text)
        local n = tonumber(text)
        if n then
            value = n
            if o.on_change then o.on_change(n) end
        end
    end

    return {
        dec = dec, inp = inp, inc = inc,
        get = function() return value end,
        set = function(v) commit(v) end,
    }
end

-- Dropdown menu attached to a top bar button.
--
-- opts = { x, w, menu_h, label, items = { { label, on_click, mark=fn? } , ... } }
--
-- Returns a handle with .close(), .set_item_label(i, text) and the top button.
function M.dropdown(bar, o)
    local MENU_H = o.menu_h or 24
    local btn    = bar:add_button(o.label, o.x, 0, o.w, MENU_H)
    local dd_w   = o.dd_w or o.w
    local dd_h   = #o.items * MENU_H

    local dd = bar:add_frame(o.x, MENU_H, dd_w, dd_h)
    dd.visible = false

    local item_btns = {}
    for i, item in ipairs(o.items) do
        local ib = dd:add_button(item.label, 0, (i-1) * MENU_H, dd_w, MENU_H)
        ib.text_left = true
        item_btns[i] = ib
        ib.on_click = function()
            dd.visible = false
            if o.on_pick then o.on_pick() end  -- let owner close sibling dropdowns
            if item.on_click then item.on_click() end
        end
    end

    btn.on_click = function()
        local opening = not dd.visible
        if o.on_pick then o.on_pick() end
        dd.visible = opening
    end

    return {
        btn       = btn,
        dropdown  = dd,
        items     = item_btns,
        close     = function() dd.visible = false end,
        set_label = function(text) btn.text = text end,
        set_item_label = function(i, text)
            if item_btns[i] then item_btns[i].text = text end
        end,
    }
end

return M
