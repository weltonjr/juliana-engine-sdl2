-- Entity property editor popup for the map editor.
-- Shown when clicking an entity with the Pointer tool (or Ctrl+click with any tool).
--
-- Usage:
--   local EntityPopup = require("panels/entity_popup")
--   EntityPopup.show(entity, def_props, on_close)
--
-- entity:    {def_id, x, y, props={key=float}}
-- def_props: {key=default_value} — all properties defined in the ObjectDef
-- on_close(): called when the dialog is dismissed

local layout        = require("util/layout")
local PropertyPanel = require("panels/property_panel")

local WIN_W   = layout.WIN_W
local WIN_H   = layout.WIN_H
local PANEL_W = layout.PANEL_W
local MENU_H  = layout.MENU_H

local M = {}

function M.show(entity, def_props, on_close)
    -- Build property list from def_props (all ObjectDef defaults).
    -- Properties already overridden in entity.props are marked with "*".
    local prop_list = {}
    for k, default_v in pairs(def_props) do
        local override = entity.props[k]
        local display  = override ~= nil and override or default_v
        local label    = override ~= nil and ("*" .. k) or k
        prop_list[#prop_list + 1] = {
            key   = k,
            label = label,
            type  = "float",
            value = display,
            min   = -999999,
            max   = 999999,
            step  = 1.0,
        }
    end
    -- Sort for stable alphabetical order
    table.sort(prop_list, function(a, b) return a.key < b.key end)

    -- Dynamic height: grow with number of properties, capped at screen height
    local PROP_ROW_H = 26
    local POPUP_W    = 280
    local POPUP_H    = 80 + math.max(3, #prop_list) * PROP_ROW_H + 44
    POPUP_H = math.min(POPUP_H, WIN_H - MENU_H - 16)

    local POPUP_X = WIN_W - PANEL_W - POPUP_W - 8
    local POPUP_Y = MENU_H + 8

    local screen = engine.ui.create_screen("entity_popup")
    local bg     = screen:add_frame(0, 0, WIN_W, WIN_H)
    local dlg    = bg:add_frame(POPUP_X, POPUP_Y, POPUP_W, POPUP_H)

    -- Title
    dlg:add_label("Entity: " .. tostring(entity.def_id), 8, 8)

    -- X / Y position inputs
    dlg:add_label("X:", 8, 32)
    local x_inp = dlg:add_input("", 30, 32, 80, 20)
    x_inp.value = tostring(math.floor(entity.x))
    x_inp:on_change(function(v)
        local n = tonumber(v)
        if n then entity.x = n end
    end)

    dlg:add_label("Y:", 130, 32)
    local y_inp = dlg:add_input("", 152, 32, 80, 20)
    y_inp.value = tostring(math.floor(entity.y))
    y_inp:on_change(function(v)
        local n = tonumber(v)
        if n then entity.y = n end
    end)

    -- Properties section
    local prop_y = 62
    dlg:add_label("Properties:", 8, prop_y)
    prop_y = prop_y + 18

    local prop_frame = dlg:add_frame(4, prop_y, POPUP_W - 8, POPUP_H - prop_y - 44)

    if #prop_list > 0 then
        local groups = { { label = "Properties", props = prop_list } }
        PropertyPanel.create(prop_frame, groups, function(key, val)
            local default_v = def_props[key]
            if default_v ~= nil and val == default_v then
                -- Value matches default: remove override (not stored in save)
                entity.props[key] = nil
            else
                entity.props[key] = val
            end
        end)
    else
        prop_frame:add_label("(no properties defined)", 4, 6)
    end

    -- Close button
    local close_btn = dlg:add_button("Close", POPUP_W - 76, POPUP_H - 34, 68, 28)
    close_btn:on_click(function()
        engine.ui.pop_screen()
        if on_close then on_close() end
    end)

    engine.ui.show_screen(screen)
end

return M
