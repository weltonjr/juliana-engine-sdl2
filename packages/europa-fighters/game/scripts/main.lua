-- main.lua — Europa Fighters entry point.
-- Boots the entity/physics layer, loads UI fonts, sets Europa gravity,
-- then shows the main menu.

local MainMenu = require("main_menu")

-- Load the package's UI font(s). RmlUi resolves the path through the engine's
-- FileInterface, which is anchored to the active package root.
rmlui:LoadFontFace("ui/Hack-Regular.ttf")

-- Europa surface gravity: ~1.31 m/s², approximated as 130 px/s²
-- (engine default is 980 px/s² for Earth-standard gravity).
local EUROPA_GRAVITY_Y = 130

engine.entity.init()
engine.physics.set_gravity(0, EUROPA_GRAVITY_Y)

engine.log("Europa Fighters ready")

MainMenu.show()
