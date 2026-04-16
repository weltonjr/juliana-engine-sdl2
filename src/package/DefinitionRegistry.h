#pragma once

#include "package/MaterialDef.h"
#include "package/BackgroundDef.h"
#include "package/ProcedureDef.h"
#include "package/ObjectDef.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

class DefinitionRegistry {
public:
    // Register built-in materials and backgrounds (always available, "base:" prefix)
    void RegisterBuiltins();

    // Registration (called by PackageLoader)
    MaterialID RegisterMaterial(std::unique_ptr<MaterialDef> def);
    BackgroundID RegisterBackground(std::unique_ptr<BackgroundDef> def);
    void RegisterProcedure(std::unique_ptr<ProcedureDef> def);
    void RegisterObject(std::unique_ptr<ObjectDef> def);

    // Lookup by qualified ID ("base:Dirt")
    const MaterialDef* GetMaterial(const std::string& qualified_id) const;
    MaterialDef*       GetMutableMaterial(const std::string& qualified_id);
    const BackgroundDef* GetBackground(const std::string& qualified_id) const;
    const ProcedureDef* GetProcedure(const std::string& qualified_id) const;
    const ObjectDef* GetObject(const std::string& qualified_id) const;

    // Lookup by runtime ID
    const MaterialDef* GetMaterialByRuntimeID(MaterialID id) const;
    const BackgroundDef* GetBackgroundByRuntimeID(BackgroundID id) const;

    // Iteration
    const std::vector<const MaterialDef*>& GetAllMaterials() const { return materials_by_id_; }
    const auto& GetAllObjects() const { return objects_; }

    MaterialID GetMaterialCount() const { return next_material_id_; }
    BackgroundID GetBackgroundCount() const { return next_background_id_; }

private:
    // By qualified ID
    std::unordered_map<std::string, std::unique_ptr<MaterialDef>> materials_;
    std::unordered_map<std::string, std::unique_ptr<BackgroundDef>> backgrounds_;
    std::unordered_map<std::string, std::unique_ptr<ProcedureDef>> procedures_;
    std::unordered_map<std::string, std::unique_ptr<ObjectDef>> objects_;

    // By runtime ID (indexed)
    std::vector<const MaterialDef*> materials_by_id_;
    std::vector<const BackgroundDef*> backgrounds_by_id_;

    MaterialID next_material_id_ = 0;
    BackgroundID next_background_id_ = 0;
};
