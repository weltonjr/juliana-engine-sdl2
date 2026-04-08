#pragma once
#include <string>
#include <memory>

// Forward declarations — keeps sol2 headers isolated to LuaState.cpp
class Engine;
class UISystem;

// Owns the sol2/Lua state and exposes the engine API to scripts.
// All sol2 types are hidden behind the Impl pimpl to avoid polluting compile times.
class LuaState {
public:
    LuaState(Engine& engine, UISystem& ui);
    ~LuaState();

    // Execute a Lua file; base_path sets package.path so require() works.
    // Returns false and prints the error on failure.
    bool RunScript(const std::string& path, const std::string& base_path = "");

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void BindAPI();
};
