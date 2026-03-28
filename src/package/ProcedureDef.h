#pragma once

#include <string>

struct ProcedureDef {
    std::string id;
    std::string name;
    std::string description;
    std::string qualified_id;
    std::string engine_impl;  // empty = pure Lua
    std::string script_path;  // path to script.lua if any
};
