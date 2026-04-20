-- Top menu bar: File / Edit / Scenario / Simulation / View / About
-- Creates a full-width 24px bar with dropdown menus.
--
-- Usage:
--   local MenuBar = require("menus/menu_bar")
--   MenuBar.build(screen, actions)
--
-- actions: {
--   on_new, on_open, on_save, on_save_as, on_quit,
--   on_undo, on_redo, on_about, on_stats,
--   toggle_panel,                  -- show/hide properties panel
--   toggle_overlay = fn(mode),     -- "diagnostics"|"heatmap"|"health"|"crack"|"stain"
--   sim_speed      = fn(s),        -- 0, 0.5, 1, 2, 10
-- }

local layout  = require("util/layout")
local widgets = require("util/widgets")

local M = {}

local MENU_H = layout.MENU_H
local WIN_W  = layout.WIN_W

local SPEED_LABELS = { [0] = "Paused", [0.5] = "0.5x", [1] = "1x", [2] = "2x", [10] = "10x" }
local SPEEDS       = { 0, 0.5, 1, 2, 10 }

local function speed_label(s) return SPEED_LABELS[s] or (tostring(s) .. "x") end

function M.build(screen, actions)
    actions = actions or {}

    local bar = widgets.frame(screen, { x = 0, y = 0, w = WIN_W, h = MENU_H })

    -- Build simulation items + a lookup from speed -> item index (for marker update)
    local sim_items = {}
    local speed_to_item_idx = {}
    for i, s in ipairs(SPEEDS) do
        sim_items[i] = {
            label    = "  " .. speed_label(s),
            on_click = function() if actions.sim_speed then actions.sim_speed(s) end end,
        }
        speed_to_item_idx[s] = i
    end
    sim_items[#sim_items + 1] = {
        label    = "Step Once",
        on_click = function() engine.sim.step(1) end,
    }

    local function overlay_item(label, mode)
        return {
            label    = label,
            on_click = function()
                if actions.toggle_overlay then actions.toggle_overlay(mode) end
            end,
        }
    end

    local menus = {
        { label = "File", w = 52, dd_w = 110, items = {
            { label = "New",        on_click = actions.on_new     },
            { label = "Open...",    on_click = actions.on_open    },
            { label = "Save",       on_click = actions.on_save    },
            { label = "Save As...", on_click = actions.on_save_as },
            { label = "Exit",       on_click = actions.on_quit    },
        }},
        { label = "Edit", w = 52, dd_w = 80, items = {
            { label = "Undo", on_click = actions.on_undo },
            { label = "Redo", on_click = actions.on_redo },
        }},
        { label = "Scenario", w = 80, dd_w = 110, items = {
            { label = "Settings", on_click = actions.toggle_panel },
        }},
        { label = "Simulation", w = 150, dd_w = 130, items = sim_items },
        { label = "View", w = 52, dd_w = 180, items = {
            { label = "Properties Panel",   on_click = actions.toggle_panel },
            overlay_item("Engine Diagnostics", "diagnostics"),
            overlay_item("Heatmap",            "heatmap"),
            overlay_item("Health / Damage",    "health"),
            overlay_item("Cracks",             "crack"),
            overlay_item("Stain",              "stain"),
            { label = "Stats...", on_click = actions.on_stats },
        }},
        { label = "About", w = 60, dd_w = 150, items = {
            { label = "About Map Editor", on_click = actions.on_about },
        }},
    }

    local dropdowns = {}

    local function close_all()
        for _, dd in ipairs(dropdowns) do dd.close() end
    end

    local x = 0
    for _, menu in ipairs(menus) do
        local handle = widgets.dropdown(bar, {
            x       = x,
            w       = menu.w,
            dd_w    = menu.dd_w,
            menu_h  = MENU_H,
            label   = menu.label,
            items   = menu.items,
            on_pick = close_all,   -- close siblings on open/pick
        })
        menu.handle = handle
        dropdowns[#dropdowns + 1] = handle
        x = x + menu.w
    end

    -- Simulation menu: update top-button label + marker on active speed row
    local sim_handle = menus[4].handle

    local function set_active_speed(s)
        sim_handle.set_label("Simulation [" .. speed_label(s) .. "]")
        for speed_val, item_idx in pairs(speed_to_item_idx) do
            sim_handle.set_item_label(item_idx,
                (speed_val == s and "● " or "  ") .. speed_label(speed_val))
        end
    end

    set_active_speed(0)

    return {
        close_all        = close_all,
        set_active_speed = set_active_speed,
    }
end

return M
