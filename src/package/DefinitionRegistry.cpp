#include "package/DefinitionRegistry.h"

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
