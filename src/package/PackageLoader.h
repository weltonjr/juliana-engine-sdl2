#pragma once

#include "package/DefinitionRegistry.h"
#include <string>

class PackageLoader {
public:
    PackageLoader(DefinitionRegistry& registry);

    void LoadAll(const std::string& packages_dir);

private:
    void LoadPackage(const std::string& package_dir, const std::string& package_id);
    void LoadDefinition(const std::string& dir_path, const std::string& package_id);

    void ParseMaterial(const std::string& file_path, const std::string& dir_path, const std::string& package_id);
    void ParseBackground(const std::string& file_path, const std::string& package_id);
    void ParseProcedure(const std::string& file_path, const std::string& dir_path, const std::string& package_id);
    void ParseObject(const std::string& file_path, const std::string& dir_path, const std::string& package_id);

    DefinitionRegistry& registry_;
};
