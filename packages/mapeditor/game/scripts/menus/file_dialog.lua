-- File dialog — browse for scenario.json files.
--
-- Usage:
--   local FileDialog = require("menus/file_dialog")
--   FileDialog.show(search_dir, on_select)
--
-- Scans search_dir recursively for files named "scenario.json".
-- Pushes a modal UIScreen listing matches; clicking one calls on_select(path)
-- and pops the screen.

local layout  = require("util/layout")
local widgets = require("util/widgets")

local M = {}

local WIN_W       = layout.WIN_W
local WIN_H       = layout.WIN_H
local DLG_W       = 600
local ROW_H       = 28
local MAX_VISIBLE = 12

function M.show(search_dir, on_select)
    local files   = engine.fs.list_dir(search_dir, ".json")
    local matches = {}
    for _, f in ipairs(files) do
        if f.name == "scenario.json" then matches[#matches + 1] = f end
    end

    local visible_rows = math.max(1, math.min(#matches, MAX_VISIBLE))
    local HEADER_H, FOOTER_H = 30, 50
    local list_h = visible_rows * ROW_H
    local dlg_h  = HEADER_H + list_h + FOOTER_H
    local dlg_x, dlg_y = layout.center(DLG_W, dlg_h)

    local screen   = engine.ui.create_screen("file_dialog")
    local backdrop = widgets.frame(screen,   { x = 0, y = 0, w = WIN_W, h = WIN_H })
    local dlg      = widgets.frame(backdrop, { x = dlg_x, y = dlg_y, w = DLG_W, h = dlg_h })

    widgets.label(dlg, { text = "Open Scenario", x = 10, y = 8 })

    local LIST_X, LIST_Y, LIST_W = 10, HEADER_H, DLG_W - 20
    local list_frame = widgets.frame(dlg, { x = LIST_X, y = LIST_Y, w = LIST_W, h = list_h })

    local scroll_offset = 0

    local item_btns = {}
    for i, f in ipairs(matches) do
        item_btns[i] = widgets.button(list_frame, {
            text      = f.path,
            text_left = true,
            visible   = false,
            x = 0, y = (i - 1) * ROW_H, w = LIST_W - 2, h = ROW_H - 2,
            on_click  = function()
                engine.ui.pop_screen()
                if on_select then on_select(f.path) end
            end,
        })
    end

    local function refresh()
        for i, btn in ipairs(item_btns) do
            local row = i - 1
            btn.visible = (row >= scroll_offset and row < scroll_offset + MAX_VISIBLE)
            if btn.visible then btn.y = (row - scroll_offset) * ROW_H end
        end
    end
    refresh()

    if #matches == 0 then
        widgets.label(list_frame, {
            text = "No scenario files found in: " .. search_dir,
            x = 4, y = 10,
        })
    end

    widgets.button(dlg, {
        text = "Cancel", x = DLG_W - 100, y = dlg_h - 36, w = 90, h = 28,
        on_click = function() engine.ui.pop_screen() end,
    })

    if #matches > MAX_VISIBLE then
        widgets.button(dlg, {
            text = "^", x = DLG_W - 16, y = LIST_Y, w = 14, h = 14,
            on_click = function()
                scroll_offset = math.max(0, scroll_offset - 1)
                refresh()
            end,
        })
        widgets.button(dlg, {
            text = "v", x = DLG_W - 16, y = LIST_Y + list_h - 16, w = 14, h = 14,
            on_click = function()
                scroll_offset = math.min(#matches - MAX_VISIBLE, scroll_offset + 1)
                refresh()
            end,
        })
    end

    engine.ui.show_screen(screen)
end

return M
