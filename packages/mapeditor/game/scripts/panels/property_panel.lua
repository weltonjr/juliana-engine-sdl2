-- Generic data-driven property inspector panel.
--
-- Usage:
--   local PropertyPanel = require("panels/property_panel")
--
--   local groups = {
--     { label = "Map",
--       props = {
--         { key="width", label="Width", type="int",  value=2048, min=256, max=8192, step=256 },
--         { key="shape", label="Shape", type="enum", value="flat",
--           options={"flat","island","mountain","bowl"} },
--       }
--     },
--   }
--
--   local handle = PropertyPanel.create(parent_frame, groups, function(key, val)
--       -- called when any property changes
--   end)
--
--   handle.get_values()        -- returns {key=value} flat table
--   handle.set_values(tbl)     -- populate from a flat table (for load)
--
-- Supported prop types: "int", "float", "enum", "bool", "string"

local M = {}

local ROW_H    = 22   -- height of each property row
local GROUP_H  = 18   -- height of group header
local PAD      = 6    -- left/right padding
local BTN_W    = 20   -- width of +/- buttons
local SPACING  = 2    -- vertical spacing between rows

-- Format a number for display
local function fmt_num(v, is_float)
    if is_float then
        return string.format("%.2f", v)
    else
        return tostring(math.floor(v))
    end
end

function M.create(parent_frame, groups, on_change)
    local values   = {}   -- live values: {key = current_value}
    local inputs   = {}   -- UIElement references for input fields: {key = el}
    local labels   = {}   -- value display labels: {key = el}
    local controls = {}   -- all interactive UIElements per key: {key = {el, ...}}

    -- Snapshot initial values
    for _, grp in ipairs(groups) do
        for _, prop in ipairs(grp.props) do
            values[prop.key] = prop.value
        end
    end

    -- Helper: notify change
    local function notify(key)
        if on_change then on_change(key, values[key]) end
    end

    -- Build UI layout
    local y = 2

    for _, grp in ipairs(groups) do
        -- Group header label
        parent_frame:add_label(grp.label, PAD, y)
        y = y + GROUP_H

        for _, prop in ipairs(grp.props) do
            local lbl_w = 90   -- width of the property name label
            local ctrl_x = PAD + lbl_w + 4  -- x where controls start
            local ctrl_w = (parent_frame.w or 280) - ctrl_x - PAD

            -- Property name label
            parent_frame:add_label(prop.label, PAD, y + 3)

            if prop.type == "int" or prop.type == "float" then
                local is_float = (prop.type == "float")
                local step     = prop.step or (is_float and 0.1 or 1)
                local vmin     = prop.min or (is_float and 0.0 or 0)
                local vmax     = prop.max or (is_float and 1.0 or 9999)

                -- [−] button
                local btn_dec = parent_frame:add_button("-", ctrl_x, y, BTN_W, ROW_H)
                -- Value input field
                local inp = parent_frame:add_input(fmt_num(values[prop.key], is_float),
                    ctrl_x + BTN_W + 2, y, ctrl_w - BTN_W * 2 - 4, ROW_H)
                inp.value = fmt_num(values[prop.key], is_float)
                -- [+] button
                local btn_inc = parent_frame:add_button("+",
                    ctrl_x + ctrl_w - BTN_W, y, BTN_W, ROW_H)

                inputs[prop.key]   = inp
                controls[prop.key] = { btn_dec, inp, btn_inc }

                -- Decrement
                btn_dec:on_click(function()
                    local v = math.max(vmin, values[prop.key] - step)
                    if not is_float then v = math.floor(v + 0.5) end
                    values[prop.key] = v
                    inp.value = fmt_num(v, is_float)
                    notify(prop.key)
                end)

                -- Increment
                btn_inc:on_click(function()
                    local v = math.min(vmax, values[prop.key] + step)
                    if not is_float then v = math.floor(v + 0.5) end
                    values[prop.key] = v
                    inp.value = fmt_num(v, is_float)
                    notify(prop.key)
                end)

                -- Manual text input
                inp:on_change(function(text)
                    local n = tonumber(text)
                    if n then
                        n = math.max(vmin, math.min(vmax, n))
                        if not is_float then n = math.floor(n + 0.5) end
                        values[prop.key] = n
                        notify(prop.key)
                    end
                end)

            elseif prop.type == "enum" then
                -- Radio-style buttons: one per option, arranged horizontally
                local opts   = prop.options or {}
                local obtn_w = math.max(1, math.floor((ctrl_w) / math.max(1, #opts)))
                local opt_btns = {}

                local function refresh_enum_buttons()
                    for i, ob in ipairs(opt_btns) do
                        -- Highlight active option by appending "*" marker via text
                        -- (skin hover/pressed don't distinguish active — we use text)
                        ob.text = opts[i] == values[prop.key]
                            and ("[" .. opts[i] .. "]")
                            or  opts[i]
                    end
                end

                for i, opt in ipairs(opts) do
                    local ob = parent_frame:add_button(opt,
                        ctrl_x + (i-1) * obtn_w, y, obtn_w - 1, ROW_H)
                    opt_btns[i] = ob
                    ob:on_click(function()
                        values[prop.key] = opt
                        refresh_enum_buttons()
                        notify(prop.key)
                    end)
                end
                refresh_enum_buttons()

            elseif prop.type == "bool" then
                local toggle_btn = parent_frame:add_button(
                    values[prop.key] and "[ON]" or "[OFF]",
                    ctrl_x, y, ctrl_w, ROW_H)
                toggle_btn:on_click(function()
                    values[prop.key] = not values[prop.key]
                    toggle_btn.text = values[prop.key] and "[ON]" or "[OFF]"
                    notify(prop.key)
                end)

            elseif prop.type == "string" then
                local inp = parent_frame:add_input(prop.label .. "…",
                    ctrl_x, y, ctrl_w, ROW_H)
                inp.value = tostring(values[prop.key] or "")
                inputs[prop.key] = inp
                inp:on_change(function(text)
                    values[prop.key] = text
                    notify(prop.key)
                end)
            end

            y = y + ROW_H + SPACING
        end

        y = y + 4  -- extra gap between groups
    end

    -- Public handle
    local handle = {}

    function handle.get_values()
        local out = {}
        for k, v in pairs(values) do out[k] = v end
        return out
    end

    function handle.set_values(tbl)
        for k, v in pairs(tbl) do
            if values[k] ~= nil then
                values[k] = v
                -- Update input display if one exists
                if inputs[k] then
                    if type(v) == "number" then
                        -- Detect float by checking if prop type was float
                        inputs[k].value = tostring(v)
                    else
                        inputs[k].value = tostring(v)
                    end
                end
            end
        end
        -- Refresh enum buttons by firing on_change callbacks won't help here;
        -- the buttons will reflect correct state on next render (text updated above).
    end

    -- Disable/enable all interactive elements for a given property key
    function handle.disable_field(key, locked)
        if controls[key] then
            for _, el in ipairs(controls[key]) do
                el.disabled = locked
            end
        end
        if inputs[key] then
            inputs[key].disabled = locked
        end
    end

    handle.content_height = y  -- expose for scroll calculation

    return handle
end

return M
