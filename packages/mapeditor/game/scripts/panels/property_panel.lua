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

local widgets = require("util/widgets")

local M = {}

local ROW_H   = 22
local GROUP_H = 18
local PAD     = 6
local SPACING = 2

function M.create(parent_frame, groups, on_change)
    local values   = {}   -- live values: {key = current_value}
    local inputs   = {}   -- input UIElements: {key = el}
    local controls = {}   -- all interactive elements per key: {key = {el, ...}}

    for _, grp in ipairs(groups) do
        for _, prop in ipairs(grp.props) do
            values[prop.key] = prop.value
        end
    end

    local function notify(key)
        if on_change then on_change(key, values[key]) end
    end

    local y = 2

    for _, grp in ipairs(groups) do
        widgets.label(parent_frame, { text = grp.label, x = PAD, y = y })
        y = y + GROUP_H

        for _, prop in ipairs(grp.props) do
            local lbl_w  = 90
            local ctrl_x = PAD + lbl_w + 4
            local ctrl_w = (parent_frame.w or 280) - ctrl_x - PAD

            widgets.label(parent_frame, { text = prop.label, x = PAD, y = y + 3 })

            if prop.type == "int" or prop.type == "float" then
                local sp = widgets.spinner(parent_frame, {
                    is_float = (prop.type == "float"),
                    value    = values[prop.key],
                    min      = prop.min,
                    max      = prop.max,
                    step     = prop.step,
                    x = ctrl_x, y = y, w = ctrl_w, h = ROW_H,
                    on_change = function(n)
                        values[prop.key] = n
                        notify(prop.key)
                    end,
                })
                inputs[prop.key]   = sp.inp
                controls[prop.key] = { sp.dec, sp.inp, sp.inc }

            elseif prop.type == "enum" then
                local opts   = prop.options or {}
                local obtn_w = math.max(1, math.floor(ctrl_w / math.max(1, #opts)))
                local opt_btns = {}

                local function refresh_enum()
                    for i, ob in ipairs(opt_btns) do
                        ob.text = opts[i] == values[prop.key]
                            and ("[" .. opts[i] .. "]") or opts[i]
                    end
                end

                for i, opt in ipairs(opts) do
                    opt_btns[i] = widgets.button(parent_frame, {
                        text = opt,
                        x = ctrl_x + (i - 1) * obtn_w, y = y,
                        w = obtn_w - 1, h = ROW_H,
                        on_click = function()
                            values[prop.key] = opt
                            refresh_enum()
                            notify(prop.key)
                        end,
                    })
                end
                refresh_enum()
                controls[prop.key] = opt_btns

            elseif prop.type == "bool" then
                local toggle_btn
                toggle_btn = widgets.button(parent_frame, {
                    text = values[prop.key] and "[ON]" or "[OFF]",
                    x = ctrl_x, y = y, w = ctrl_w, h = ROW_H,
                    on_click = function()
                        values[prop.key] = not values[prop.key]
                        toggle_btn.text = values[prop.key] and "[ON]" or "[OFF]"
                        notify(prop.key)
                    end,
                })
                controls[prop.key] = { toggle_btn }

            elseif prop.type == "string" then
                local inp = widgets.input(parent_frame, {
                    placeholder = prop.label .. "…",
                    value       = tostring(values[prop.key] or ""),
                    x = ctrl_x, y = y, w = ctrl_w, h = ROW_H,
                    on_change = function(text)
                        values[prop.key] = text
                        notify(prop.key)
                    end,
                })
                inputs[prop.key]   = inp
                controls[prop.key] = { inp }
            end

            y = y + ROW_H + SPACING
        end

        y = y + 4  -- extra gap between groups
    end

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
                if inputs[k] then inputs[k].value = tostring(v) end
            end
        end
    end

    function handle.disable_field(key, locked)
        if controls[key] then
            for _, el in ipairs(controls[key]) do el.disabled = locked end
        end
        if inputs[key] then inputs[key].disabled = locked end
    end

    function handle.disable_all(locked)
        for key, _ in pairs(values) do handle.disable_field(key, locked) end
    end

    handle.content_height = y
    return handle
end

return M
