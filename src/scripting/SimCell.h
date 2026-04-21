#pragma once

// SimCell — lightweight Lua proxy for reading/writing a single terrain cell
// during simulation callbacks.  Bound to Lua via sol2.
//
// Usage in a material script:
//   return {
//       on_tick = function(cell)
//           if cell:temperature() > 100 then
//               cell:convert_to("test:Steam")
//           end
//       end
//   }
//
// All mutations are queued in a pending list and applied after all callbacks
// complete, so Lua cannot see a half-updated world during its tick.

#include "terrain/Terrain.h"
#include "terrain/TerrainSimulator.h"
#include "package/DefinitionRegistry.h"
#include <string>
#include <vector>
#include <optional>

struct SimCell {
    // References to simulator state (non-owning)
    Terrain*    terrain  = nullptr;
    int x = 0, y = 0;
    int w = 0, h = 0;
    float*    temp    = nullptr;
    int16_t*  health  = nullptr;
    uint8_t*  ignited = nullptr;
    std::vector<PendingMutation>* pending = nullptr;
    const DefinitionRegistry*    registry = nullptr;

    // ── Lua-visible API ───────────────────────────────────────────────────────

    std::string material_id() const {
        if (!terrain) return "";
        Cell c = terrain->GetCell(x, y);
        auto* mat = registry ? registry->GetMaterialByRuntimeID(c.material_id) : nullptr;
        return mat ? mat->qualified_id : "";
    }

    float temperature() const {
        if (!temp) return 0.f;
        return temp[y * w + x];
    }

    int get_health() const {
        if (!health) return 0;
        return static_cast<int>(health[y * w + x]);
    }

    bool is_ignited() const {
        if (!ignited) return false;
        return ignited[y * w + x] != 0;
    }

    void convert_to(const std::string& qid) {
        if (!registry || !pending) return;
        auto* mat = registry->GetMaterial(qid);
        int id = mat ? static_cast<int>(mat->runtime_id) : -1;
        pending->push_back({PendingMutation::Type::Convert, x, y, id});
    }

    void deal_damage(int amount) {
        if (!pending) return;
        pending->push_back({PendingMutation::Type::Damage, x, y, -1, amount});
    }

    void add_temperature(float dt) {
        if (!pending) return;
        pending->push_back({PendingMutation::Type::TempChange, x, y, -1, 0, dt});
    }

    void ignite_cell() {
        if (!pending) return;
        pending->push_back({PendingMutation::Type::Ignite, x, y});
    }

    void extinguish_cell() {
        if (!pending) return;
        pending->push_back({PendingMutation::Type::Extinguish, x, y});
    }

    std::optional<SimCell> neighbor(int dx, int dy) const {
        int nx = x + dx, ny = y + dy;
        if (nx < 0 || nx >= w || ny < 0 || ny >= h) return std::nullopt;
        SimCell n = *this;
        n.x = nx; n.y = ny;
        return n;
    }
};
