#pragma once

// Forward declarations keep sol2 headers isolated to the .cpp.
namespace sol { class state; }
class Engine;
class UISystem;

// Registers the UIElement / UIScreen usertypes and the `engine.ui` table.
// Called once from LuaState::BindAPI().
void RegisterLuaUIBindings(sol::state& lua, UISystem& ui, Engine& engine);
