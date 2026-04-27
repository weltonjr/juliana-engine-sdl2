-- main.lua — Europa Fighters entry point.
-- Boots the entity/physics layer, sets Europa gravity, then shows the main menu.

local MainMenu = require("main_menu")

-- Europa surface gravity: ~1.31 m/s², approximated as 130 px/s²
-- (engine default is 980 px/s² for Earth-standard gravity).
local EUROPA_GRAVITY_Y = 130

engine.entity.init()
engine.physics.set_gravity(0, EUROPA_GRAVITY_Y)

engine.log("Europa Fighters ready")

MainMenu.show()
