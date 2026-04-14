-- Map Editor — startup script
-- Entry point: shows the main menu.
-- All editor logic is in the modules below.

local MainMenu     = require("main_menu")
local EditorScreen = require("editor_screen")

MainMenu.show(EditorScreen)

engine.log("Map Editor ready")
