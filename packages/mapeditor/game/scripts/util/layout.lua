-- Layout constants and helpers for the map editor UI.
-- All sizes are in screen pixels.

local M = {}

M.WIN_W     = 1280
M.WIN_H     = 720
M.MENU_H    = 24    -- height of the top menu bar
M.PANEL_W   = 290   -- width of the right-side properties panel
M.TOOLBAR_W = 96    -- width of the left-side edit toolbar

M.CAM_PAN_SPEED = 8.0
M.ZOOM_STEP     = 0.15
M.ZOOM_MIN      = 0.25
M.ZOOM_MAX      = 8.0

-- Returns the top-left (x, y) that centers a (w, h) rectangle in the window.
function M.center(w, h)
    return math.floor((M.WIN_W - w) / 2),
           math.floor((M.WIN_H - h) / 2)
end

-- Builds a stateful vertical-stack cursor. Each call advances by `step` (or the
-- override passed in) and returns the y-coordinate of the previous row.
--
--     local y = layout.stack_v(0, 24)
--     widgets.label(f, { y = y() })   -- y = 0
--     widgets.label(f, { y = y() })   -- y = 24
--     widgets.label(f, { y = y(8) })  -- y = 48, next step shrinks to 8
function M.stack_v(y0, step)
    local y = y0 or 0
    return function(override_step)
        local cur = y
        y = y + (override_step or step)
        return cur
    end
end

-- Splits a (w, h) row starting at (x, y) into N rects with the given widths.
-- A width of 0 means "fill remaining space" (at most one such entry allowed).
-- Returns an array of { x, y, w, h } tables.
function M.split_h(x, y, w, h, widths)
    local fixed, flex_idx = 0, nil
    for i, ww in ipairs(widths) do
        if ww == 0 then flex_idx = i else fixed = fixed + ww end
    end
    local flex = math.max(0, w - fixed)
    local out, cx = {}, x
    for i, ww in ipairs(widths) do
        local rw = (i == flex_idx) and flex or ww
        out[i] = { x = cx, y = y, w = rw, h = h }
        cx = cx + rw
    end
    return out
end

return M
