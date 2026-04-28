-- main_menu.lua — Title screen with Deathmatch / Options / Exit.
-- Uses RmlUi's official Lua API via the `rmlui` global. No engine wrapper.

local M = {}

local current_doc

function M.show()
    local ctx = rmlui.contexts["main"]
    current_doc = ctx:LoadDocument("ui/main_menu.rml")

    current_doc:GetElementById("btn-deathmatch"):AddEventListener("click", function()
        current_doc:Close()
        current_doc = nil
        local ShipSelect = require("ship_select")
        ShipSelect.show()
    end)

    current_doc:GetElementById("btn-options"):AddEventListener("click", function()
        engine.log("Options — not yet implemented")
    end)

    current_doc:GetElementById("btn-exit"):AddEventListener("click", function()
        engine.quit()
    end)

    current_doc:Show()
end

return M
