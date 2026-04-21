#pragma once
#include <string>
#include <memory>

// Forward declarations — keeps sol2 headers isolated to LuaState.cpp
class Engine;
class UISystem;
class TerrainSimulator;

// Owns the sol2/Lua state and exposes the engine API to scripts.
// All sol2 types are hidden behind the Impl pimpl to avoid polluting compile times.
class LuaState {
public:
    LuaState(Engine& engine, UISystem& ui);
    ~LuaState();

    // Execute a Lua file with full engine API access (trusted game scripts).
    // base_path sets package.path so require() works.
    // Returns false and prints the error on failure.
    bool RunScript(const std::string& path, const std::string& base_path = "");

    // Execute a Lua file in a sandboxed environment (mod/aspect scripts).
    // Removes: engine.fs, engine.json, io, os, dofile, loadfile, require.
    bool RunSandboxedScript(const std::string& path, const std::string& base_path = "");

    // Load material behavior scripts from the registry and register on_tick callbacks
    // into the simulator. Call after all packages are loaded and LUTs are built.
    void LoadMaterialScripts(TerrainSimulator& sim);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void BindAPI();
    void SetPackagePath(const std::string& base_path);
};
