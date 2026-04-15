-- Top menu bar: File / Edit / Scenario / Simulation / View / About
-- Creates a full-width 24px bar with dropdown menus.
--
-- Usage:
--   local MenuBar = require("menus/menu_bar")
--   MenuBar.build(screen, actions)
--
-- actions: {
--   on_new      = fn(),   on_open    = fn(),   on_save = fn(),
--   on_save_as  = fn(),   on_quit    = fn(),
--   on_undo     = fn(),   on_redo    = fn(),
--   on_about    = fn(),
--   toggle_panel = fn(),  -- show/hide properties panel
--   on_stats    = fn(),   -- open stats dialog
--   toggle_debug = fn(),  -- toggle debug overlay (chunk borders + collision)
--   sim_speed   = fn(s),  -- set simulation time scale (0, 0.5, 1, 2, 10)
-- }

local layout = require("util/layout")

local M = {}

local MENU_H  = layout.MENU_H
local WIN_W   = layout.WIN_W

-- Width per top-level menu button (sized to fit label + padding)
local MENU_DEFS = {
    { label = "File",       w = 52  },
    { label = "Edit",       w = 52  },
    { label = "Scenario",   w = 80  },
    { label = "Simulation", w = 150 },
    { label = "View",       w = 52  },
    { label = "About",      w = 60  },
}

-- Track which dropdown is open (nil = all closed)
local open_dropdown = nil

-- Speed label helpers
local SPEED_LABELS = { [0] = "Paused", [0.5] = "0.5x", [1] = "1x", [2] = "2x", [10] = "10x" }
local function speed_label(s)
    return SPEED_LABELS[s] or (tostring(s) .. "x")
end

local function close_all(dropdowns)
    for _, dd in ipairs(dropdowns) do
        dd.visible = false
    end
    open_dropdown = nil
end

function M.build(screen, actions)
    actions = actions or {}

    -- Background bar frame
    local bar = screen:add_frame(0, 0, WIN_W, MENU_H)

    -- Assign x positions from widths
    local x = 0
    for _, m in ipairs(MENU_DEFS) do
        m.x = x
        x = x + m.w
    end

    local menus = MENU_DEFS

    local dropdowns = {}
    local sim_top_btn   = nil   -- top-level "Simulation" button (for label update)
    local sim_item_btns = {}    -- speed item buttons keyed by speed value

    -- Build each top-level menu button + its dropdown frame
    for idx, menu in ipairs(menus) do
        local btn = bar:add_button(menu.label, menu.x, 0, menu.w, MENU_H)
        if menu.label == "Simulation" then sim_top_btn = btn end

        -- Dropdown frame (hidden by default), attached to the bar
        -- Dropdown is at least as wide as the button, wider for long item labels
        local items
        local dd_w = menu.w  -- minimum dropdown width matches button
        if menu.label == "File" then
            dd_w = 110
            items = {
                { label = "New",       fn = actions.on_new     },
                { label = "Open...",   fn = actions.on_open    },
                { label = "Save",      fn = actions.on_save    },
                { label = "Save As...",fn = actions.on_save_as },
                { label = "Exit",      fn = actions.on_quit    },
            }
        elseif menu.label == "Edit" then
            dd_w = 80
            items = {
                { label = "Undo",  fn = actions.on_undo },
                { label = "Redo",  fn = actions.on_redo },
            }
        elseif menu.label == "Scenario" then
            dd_w = 110
            items = {
                { label = "Settings", fn = actions.toggle_panel },
            }
        elseif menu.label == "Simulation" then
            dd_w = 130
            local speeds = { 0, 0.5, 1, 2, 10 }
            items = {}
            for _, s in ipairs(speeds) do
                local speed_val = s
                table.insert(items, {
                    label = "  " .. speed_label(s),
                    speed = s,
                    fn = function()
                        if actions.sim_speed then actions.sim_speed(speed_val) end
                    end,
                })
            end
        elseif menu.label == "View" then
            dd_w = 150
            items = {
                { label = "Properties Panel", fn = actions.toggle_panel },
                { label = "Debug View",       fn = actions.toggle_debug },
                { label = "Stats...",         fn = actions.on_stats     },
            }
        elseif menu.label == "About" then
            dd_w = 150
            items = {
                { label = "About Map Editor", fn = actions.on_about },
            }
        end

        local dd_h = #items * MENU_H
        local dd = bar:add_frame(menu.x, MENU_H, dd_w, dd_h)
        dd.visible = false
        dropdowns[idx] = dd

        for i, item in ipairs(items) do
            local ib = dd:add_button(item.label, 0, (i-1) * MENU_H, dd_w, MENU_H)
            ib.text_left = true
            if item.speed ~= nil then
                sim_item_btns[item.speed] = ib
            end
            ib:on_click(function()
                close_all(dropdowns)
                if item.fn then item.fn() end
            end)
        end

        btn:on_click(function()
            if open_dropdown == idx then
                close_all(dropdowns)
            else
                close_all(dropdowns)
                dd.visible    = true
                open_dropdown = idx
            end
        end)
    end

    -- Marks the active speed in the dropdown and updates the top button label.
    local function set_active_speed(s)
        local lbl = speed_label(s)
        if sim_top_btn then
            sim_top_btn.text = "Simulation [" .. lbl .. "]"
        end
        for speed_val, ib in pairs(sim_item_btns) do
            ib.text = (speed_val == s and "● " or "  ") .. speed_label(speed_val)
        end
    end

    -- Apply default active marker (1x on load)
    set_active_speed(0)

    -- Return a close function so editor_screen can close dropdowns on backdrop click
    return {
        close_all      = function() close_all(dropdowns) end,
        set_active_speed = set_active_speed,
    }
end

return M
