-- Layout constants for the map editor UI
-- All sizes are in screen pixels.

local M = {}

M.WIN_W   = 1280
M.WIN_H   = 720
M.MENU_H  = 24    -- height of the top menu bar
M.PANEL_W   = 290   -- width of the right-side properties panel
M.TOOLBAR_W = 96    -- width of the left-side edit toolbar

-- Camera pan speed (kept for reference, keyboard pan removed)
M.CAM_PAN_SPEED = 8.0

-- Scroll pixels per wheel tick when zooming
M.ZOOM_STEP = 0.15

-- Minimum / maximum zoom levels
M.ZOOM_MIN = 0.25
M.ZOOM_MAX = 8.0

return M
