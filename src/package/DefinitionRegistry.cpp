#include "package/DefinitionRegistry.h"

// ─── Built-in material / background definitions ──────────────────────────────
// These are always available under the "base:" prefix so that engine code and
// editor scripts can rely on them without loading an external content package.

void DefinitionRegistry::RegisterBuiltins() {
    // Helper lambdas to reduce repetition
    auto mat = [&](const char* id, const char* name,
                   MaterialState st, int density, float friction, int hardness,
                   uint8_t r, uint8_t g, uint8_t b, int color_var = 0,
                   float transparency = 0.0f, bool glow = false,
                   bool gravity = false, bool flammable = false,
                   int blast_resist = 20, int flow_rate = 0, float liquid_drag = 0.0f) {
        auto d = std::make_unique<MaterialDef>();
        d->id = id;
        d->name = name;
        d->qualified_id = std::string("base:") + id;
        d->state = st;
        d->density = density;
        d->friction = friction;
        d->hardness = hardness;
        d->color = { r, g, b, 255 };
        d->color_variation = color_var;
        d->transparency = transparency;
        d->glow = glow;
        d->gravity = gravity;
        d->flammable = flammable;
        d->blast_resistance = blast_resist;
        d->flow_rate = flow_rate;
        d->liquid_drag = liquid_drag;
        RegisterMaterial(std::move(d));
    };

    auto bg = [&](const char* id, const char* name,
                  uint8_t r, uint8_t g, uint8_t b,
                  int color_var = 0, bool transparent = false) {
        auto d = std::make_unique<BackgroundDef>();
        d->id = id;
        d->name = name;
        d->qualified_id = std::string("base:") + id;
        d->color = { r, g, b, 255 };
        d->color_variation = color_var;
        d->transparent = transparent;
        RegisterBackground(std::move(d));
    };

    // ── Materials ─────────────────────────────────────────────────────────────
    // Only engine-essential materials live here.
    //              id        name      state                 dens  fric  hard   R    G    B   cvar  trans  glow  grav  flam  blast flow  ldrag
    mat("Air",     "Air",    MaterialState::None,               0, 0.0f,  0,   135, 206, 235);
    mat("Unknown", "Unknown",MaterialState::Solid,            100, 0.8f,  0,   255, 105, 180);
    mat("UnknownLiquid","Unknown Liquid",MaterialState::Liquid, 50, 0.2f, 0,   255, 150, 210, 0, 0.3f, false, true, false, 20, 3, 0.85f);
   
    // ── Backgrounds ───────────────────────────────────────────────────────────
    bg("Sky",      "Open Sky",   0,   0,   0,  0, true);

    std::printf("Registered %d built-in materials, %d built-in backgrounds\n",
                GetMaterialCount(), GetBackgroundCount());
}

MaterialID DefinitionRegistry::RegisterMaterial(std::unique_ptr<MaterialDef> def) {
    MaterialID id = next_material_id_++;
    def->runtime_id = id;
    const auto* ptr = def.get();
    materials_[def->qualified_id] = std::move(def);
    if (materials_by_id_.size() <= id) {
        materials_by_id_.resize(id + 1, nullptr);
    }
    materials_by_id_[id] = ptr;
    return id;
}

BackgroundID DefinitionRegistry::RegisterBackground(std::unique_ptr<BackgroundDef> def) {
    BackgroundID id = next_background_id_++;
    def->runtime_id = id;
    const auto* ptr = def.get();
    backgrounds_[def->qualified_id] = std::move(def);
    if (backgrounds_by_id_.size() <= id) {
        backgrounds_by_id_.resize(id + 1, nullptr);
    }
    backgrounds_by_id_[id] = ptr;
    return id;
}

void DefinitionRegistry::RegisterProcedure(std::unique_ptr<ProcedureDef> def) {
    procedures_[def->qualified_id] = std::move(def);
}

void DefinitionRegistry::RegisterObject(std::unique_ptr<ObjectDef> def) {
    objects_[def->qualified_id] = std::move(def);
}

const MaterialDef* DefinitionRegistry::GetMaterial(const std::string& qualified_id) const {
    auto it = materials_.find(qualified_id);
    return it != materials_.end() ? it->second.get() : nullptr;
}

const BackgroundDef* DefinitionRegistry::GetBackground(const std::string& qualified_id) const {
    auto it = backgrounds_.find(qualified_id);
    return it != backgrounds_.end() ? it->second.get() : nullptr;
}

const ProcedureDef* DefinitionRegistry::GetProcedure(const std::string& qualified_id) const {
    auto it = procedures_.find(qualified_id);
    return it != procedures_.end() ? it->second.get() : nullptr;
}

const ObjectDef* DefinitionRegistry::GetObject(const std::string& qualified_id) const {
    auto it = objects_.find(qualified_id);
    return it != objects_.end() ? it->second.get() : nullptr;
}

const MaterialDef* DefinitionRegistry::GetMaterialByRuntimeID(MaterialID id) const {
    if (id < materials_by_id_.size()) return materials_by_id_[id];
    return nullptr;
}

const BackgroundDef* DefinitionRegistry::GetBackgroundByRuntimeID(BackgroundID id) const {
    if (id < backgrounds_by_id_.size()) return backgrounds_by_id_[id];
    return nullptr;
}
