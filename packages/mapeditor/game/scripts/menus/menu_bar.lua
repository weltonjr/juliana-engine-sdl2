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
    { label = "Simulation", w = 90  },
    { label = "View",       w = 52  },
    { label = "About",      w = 60  },
}

-- Track which dropdown is open (nil = all closed)
local open_dropdown = nil

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

    -- Build each top-level menu button + its dropdown frame
    for idx, menu in ipairs(menus) do
        local btn = bar:add_button(menu.label, menu.x, 0, menu.w, MENU_H)

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
            dd_w = 110
            items = {
                { label = "Paused",   fn = function() if actions.sim_speed then actions.sim_speed(0)   end end },
                { label = "0.5x",     fn = function() if actions.sim_speed then actions.sim_speed(0.5) end end },
                { label = "1x",       fn = function() if actions.sim_speed then actions.sim_speed(1)   end end },
                { label = "2x",       fn = function() if actions.sim_speed then actions.sim_speed(2)   end end },
                { label = "10x",      fn = function() if actions.sim_speed then actions.sim_speed(10)  end end },
            }
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

    -- Return a close function so editor_screen can close dropdowns on backdrop click
    return {
        close_all = function() close_all(dropdowns) end
    }
end

return M
