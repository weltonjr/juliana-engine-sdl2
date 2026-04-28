#pragma once

namespace sol { class state; }

// Registers a minimal `ImGui` Lua table on the sol2 state for debug overlays.
//
// Intentionally a tight surface — only the calls we actually want game-side
// dev panels to use. Easy to grow on demand; no transitive dependency on a
// stale third-party binding header.
void RegisterLuaImGuiBindings(sol::state& lua);
