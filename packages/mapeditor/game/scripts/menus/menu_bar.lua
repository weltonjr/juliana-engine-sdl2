-- Top menu bar: File / Edit / Scenario / About
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
-- }

local layout = require("util/layout")

local M = {}

local MENU_H  = layout.MENU_H
local WIN_W   = layout.WIN_W
local BTN_W   = 64   -- width of each top-level menu button

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

    local menus = {
        { label = "File",     x = 0 },
        { label = "Edit",     x = BTN_W },
        { label = "Scenario", x = BTN_W * 2 },
        { label = "About",    x = BTN_W * 3 },
    }

    local dropdowns = {}

    -- Build each top-level menu button + its dropdown frame
    for idx, menu in ipairs(menus) do
        local btn = bar:add_button(menu.label, menu.x, 0, BTN_W, MENU_H)

        -- Dropdown frame (hidden by default), attached to the bar
        local items
        if menu.label == "File" then
            items = {
                { label = "New",      fn = actions.on_new     },
                { label = "Open...",  fn = actions.on_open    },
                { label = "Save",     fn = actions.on_save    },
                { label = "Save As...",fn = actions.on_save_as },
                { label = "Exit",     fn = actions.on_quit    },
            }
        elseif menu.label == "Edit" then
            items = {
                { label = "Undo",  fn = actions.on_undo },
                { label = "Redo",  fn = actions.on_redo },
            }
        elseif menu.label == "Scenario" then
            items = {
                { label = "Properties", fn = actions.toggle_panel },
            }
        elseif menu.label == "About" then
            items = {
                { label = "About Map Editor", fn = actions.on_about },
            }
        end

        local dd_h = #items * MENU_H
        local dd = bar:add_frame(menu.x, MENU_H, BTN_W + 20, dd_h)
        dd.visible = false
        dropdowns[idx] = dd

        for i, item in ipairs(items) do
            local ib = dd:add_button(item.label, 0, (i-1) * MENU_H, BTN_W + 20, MENU_H)
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
