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
local widgets       = require("util/widgets")
local PropertyPanel = require("panels/property_panel")

local WIN_W   = layout.WIN_W
local WIN_H   = layout.WIN_H
local PANEL_W = layout.PANEL_W
local MENU_H  = layout.MENU_H

local M = {}

function M.show(entity, def_props, on_close)
    local prop_list = {}
    for k, default_v in pairs(def_props) do
        local override = entity.props[k]
        local display  = override ~= nil and override or default_v
        local label    = override ~= nil and ("*" .. k) or k
        prop_list[#prop_list + 1] = {
            key = k, label = label, type = "float",
            value = display, min = -999999, max = 999999, step = 1.0,
        }
    end
    table.sort(prop_list, function(a, b) return a.key < b.key end)

    local PROP_ROW_H = 26
    local POPUP_W    = 280
    local POPUP_H    = 80 + math.max(3, #prop_list) * PROP_ROW_H + 44
    POPUP_H = math.min(POPUP_H, WIN_H - MENU_H - 16)

    local POPUP_X = WIN_W - PANEL_W - POPUP_W - 8
    local POPUP_Y = MENU_H + 8

    local screen = engine.ui.create_screen("entity_popup")
    local bg  = widgets.frame(screen, { x = 0,       y = 0,       w = WIN_W,   h = WIN_H })
    local dlg = widgets.frame(bg,     { x = POPUP_X, y = POPUP_Y, w = POPUP_W, h = POPUP_H })

    widgets.label(dlg, { text = "Entity: " .. tostring(entity.def_id), x = 8, y = 8 })

    -- X / Y position inputs
    widgets.label(dlg, { text = "X:", x = 8, y = 32 })
    widgets.input(dlg, {
        placeholder = "", value = tostring(math.floor(entity.x)),
        x = 30, y = 32, w = 80, h = 20,
        on_change = function(v)
            local n = tonumber(v); if n then entity.x = n end
        end,
    })

    widgets.label(dlg, { text = "Y:", x = 130, y = 32 })
    widgets.input(dlg, {
        placeholder = "", value = tostring(math.floor(entity.y)),
        x = 152, y = 32, w = 80, h = 20,
        on_change = function(v)
            local n = tonumber(v); if n then entity.y = n end
        end,
    })

    -- Properties section
    local prop_y = 62
    widgets.label(dlg, { text = "Properties:", x = 8, y = prop_y })
    prop_y = prop_y + 18

    local prop_frame = widgets.frame(dlg, {
        x = 4, y = prop_y, w = POPUP_W - 8, h = POPUP_H - prop_y - 44,
    })

    if #prop_list > 0 then
        local groups = { { label = "Properties", props = prop_list } }
        PropertyPanel.create(prop_frame, groups, function(key, val)
            local default_v = def_props[key]
            if default_v ~= nil and val == default_v then
                entity.props[key] = nil
            else
                entity.props[key] = val
            end
        end)
    else
        widgets.label(prop_frame, { text = "(no properties defined)", x = 4, y = 6 })
    end

    widgets.button(dlg, {
        text = "Close", x = POPUP_W - 76, y = POPUP_H - 34, w = 68, h = 28,
        on_click = function()
            engine.ui.pop_screen()
            if on_close then on_close() end
        end,
    })

    engine.ui.show_screen(screen)
end

return M
