-- File dialog — browse for scenario.json files.
--
-- Usage:
--   local FileDialog = require("menus/file_dialog")
--   FileDialog.show(search_dir, on_select)
--
-- Scans search_dir recursively for files named "scenario.json".
-- Pushes a modal UIScreen listing matches; clicking one calls on_select(path)
-- and pops the screen.

local layout = require("util/layout")

local M = {}

local WIN_W  = layout.WIN_W
local WIN_H  = layout.WIN_H
local DLG_W  = 600
local DLG_H  = 420
local DLG_X  = (WIN_W - DLG_W) / 2
local DLG_Y  = (WIN_H - DLG_H) / 2
local ROW_H  = 28
local MAX_VISIBLE = 12   -- rows visible at once without scrolling

function M.show(search_dir, on_select)
    -- Gather scenario.json files
    local files = engine.fs.list_dir(search_dir, ".json")
    local matches = {}
    for _, f in ipairs(files) do
        if f.name == "scenario.json" then
            matches[#matches + 1] = f
        end
    end

    local screen = engine.ui.create_screen("file_dialog")

    -- Semi-transparent backdrop
    local backdrop = screen:add_frame(0, 0, WIN_W, WIN_H)

    -- Dialog box
    local dlg = backdrop:add_frame(DLG_X, DLG_Y, DLG_W, DLG_H)

    dlg:add_label("Open Scenario", 10, 8)

    -- Scrollable list area
    local LIST_X  = 10
    local LIST_Y  = 30
    local LIST_W  = DLG_W - 20
    local LIST_H  = DLG_H - 80
    local list_frame = dlg:add_frame(LIST_X, LIST_Y, LIST_W, LIST_H)

    local scroll_offset = 0   -- first visible item index (0-based)

    -- Build list item buttons
    local item_btns = {}
    for i, f in ipairs(matches) do
        local display = f.path  -- show full path for clarity
        local btn = list_frame:add_button(display, 0, (i-1) * ROW_H, LIST_W - 2, ROW_H - 2)
        btn.visible = false  -- will be shown by refresh
        item_btns[i] = btn
        btn:on_click(function()
            engine.ui.pop_screen()
            if on_select then on_select(f.path) end
        end)
    end

    -- Refresh visible buttons based on scroll_offset
    local function refresh()
        for i, btn in ipairs(item_btns) do
            local row = i - 1  -- 0-based
            btn.visible = (row >= scroll_offset and row < scroll_offset + MAX_VISIBLE)
            -- Reposition visible buttons
            if btn.visible then
                btn.y = (row - scroll_offset) * ROW_H
            end
        end
    end
    refresh()

    -- Show empty message if no files found
    if #matches == 0 then
        list_frame:add_label("No scenario files found in: " .. search_dir, 4, 10)
    end

    -- Cancel button
    local cancel_btn = dlg:add_button("Cancel", DLG_W - 100, DLG_H - 36, 90, 28)
    cancel_btn:on_click(function()
        engine.ui.pop_screen()
    end)

    -- Scroll up / down buttons
    if #matches > MAX_VISIBLE then
        local up_btn   = dlg:add_button("^", DLG_W - 16, LIST_Y, 14, 14)
        local down_btn = dlg:add_button("v", DLG_W - 16, LIST_Y + LIST_H - 16, 14, 14)
        up_btn:on_click(function()
            scroll_offset = math.max(0, scroll_offset - 1)
            refresh()
        end)
        down_btn:on_click(function()
            scroll_offset = math.min(#matches - MAX_VISIBLE, scroll_offset + 1)
            refresh()
        end)
    end

    -- Register scroll-wheel support via tick callback wrapper
    -- (the editor_screen tick callback is replaced while this dialog is open,
    --  then restored when it closes — handled by the caller if needed)
    local prev_tick = nil  -- placeholder; editor_screen manages tick callbacks

    engine.ui.show_screen(screen)
end

return M
