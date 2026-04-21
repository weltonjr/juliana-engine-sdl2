#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include "scripting/LuaState.h"
#include "scripting/LuaUIBindings.h"
#include "scripting/SimCell.h"
#include "terrain/TerrainSimulator.h"
#include "package/DefinitionRegistry.h"
#include "core/Engine.h"
#include "core/EngineLog.h"
#include "ui/UISystem.h"
#include "scenario/ScenarioDef.h"
#include "scenario/ScenarioLoader.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdio>

namespace fs = std::filesystem;
using json = nlohmann::json;

// ─── Pimpl ────────────────────────────────────────────────────────────────────

struct LuaState::Impl {
    sol::state lua;
    Engine&    engine;
    UISystem&  ui;

    Impl(Engine& e, UISystem& u) : engine(e), ui(u) {}
};

// ─── Lifecycle ────────────────────────────────────────────────────────────────

LuaState::LuaState(Engine& engine, UISystem& ui)
    : impl_(std::make_unique<Impl>(engine, ui))
{
    BindAPI();
}

LuaState::~LuaState() = default;

// ─── Package path helper ──────────────────────────────────────────────────────

void LuaState::SetPackagePath(const std::string& base_path) {
    if (!base_path.empty()) {
        std::string pkg_path = base_path + "/scripts/?.lua;"
                             + base_path + "/?.lua";
        impl_->lua["package"]["path"] = pkg_path;
    }
}

// ─── Script execution ─────────────────────────────────────────────────────────

bool LuaState::RunScript(const std::string& path, const std::string& base_path) {
    SetPackagePath(base_path);

    auto result = impl_->lua.safe_script_file(path, [](lua_State*, sol::protected_function_result pfr) {
        return pfr;
    });

    if (!result.valid()) {
        sol::error err = result;
        std::string msg = std::string("[Lua error] ") + path + ": " + err.what();
        EngineLog::Log(msg);
        std::fprintf(stderr, "%s\n", msg.c_str());
        return false;
    }
    return true;
}

bool LuaState::RunSandboxedScript(const std::string& path, const std::string& base_path) {
    SetPackagePath(base_path);

    lua_State* L = impl_->lua.lua_state();

    // Load file as a chunk — pushes the function onto the stack
    if (luaL_loadfile(L, path.c_str()) != LUA_OK) {
        std::string msg = std::string("[Lua error] loading ") + path + ": " + lua_tostring(L, -1);
        EngineLog::Log(msg);
        std::fprintf(stderr, "%s\n", msg.c_str());
        lua_pop(L, 1);
        return false;
    }

    // Build sandbox table that inherits _G but shadows dangerous keys
    lua_newtable(L);                                    // sandbox = {}
    lua_newtable(L);                                    // mt = {}
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);  // _G
    lua_setfield(L, -2, "__index");                     // mt.__index = _G
    lua_setmetatable(L, -2);                            // setmetatable(sandbox, mt)

    // Remove dangerous globals from the sandbox (shadow _G)
    static const char* blocked[] = { "io", "os", "dofile", "loadfile", "require", nullptr };
    for (int i = 0; blocked[i]; ++i) {
        lua_pushnil(L);
        lua_setfield(L, -2, blocked[i]);
    }

    // Build restricted engine proxy (engine table without fs and json)
    lua_getglobal(L, "engine");   // push original engine table
    lua_newtable(L);              // restricted_engine = {}
    lua_pushnil(L);
    while (lua_next(L, -3) != 0) {
        // key at -2, value at -1
        const char* k = lua_tostring(L, -2);
        if (k && (std::string(k) == "fs" || std::string(k) == "json")) {
            lua_pop(L, 1);  // skip value, keep key for next iteration
            continue;
        }
        lua_pushvalue(L, -2);   // duplicate key
        lua_insert(L, -2);      // stack: ... key key value → duplicate key before value
        lua_settable(L, -4);    // restricted_engine[key] = value (pops key+value)
    }
    lua_pop(L, 1);              // pop original engine table
    lua_setfield(L, -2, "engine");  // sandbox.engine = restricted_engine

    // Replace _ENV of the chunk (first upvalue) with sandbox
    lua_setupvalue(L, -2, 1);   // pops sandbox, sets upvalue

    // Execute the chunk
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        std::string msg = std::string("[Lua sandbox error] ") + path + ": " + lua_tostring(L, -1);
        EngineLog::Log(msg);
        std::fprintf(stderr, "%s\n", msg.c_str());
        lua_pop(L, 1);
        return false;
    }
    return true;
}

// ─── JSON helpers ─────────────────────────────────────────────────────────────

static sol::object JsonToLua(const json& j, sol::state& lua) {
    if (j.is_null())    return sol::nil;
    if (j.is_boolean()) return sol::make_object(lua, j.get<bool>());
    if (j.is_number_integer()) return sol::make_object(lua, j.get<int64_t>());
    if (j.is_number())  return sol::make_object(lua, j.get<double>());
    if (j.is_string())  return sol::make_object(lua, j.get<std::string>());
    if (j.is_array()) {
        sol::table t = lua.create_table();
        int idx = 1;
        for (auto& v : j) t[idx++] = JsonToLua(v, lua);
        return t;
    }
    if (j.is_object()) {
        sol::table t = lua.create_table();
        for (auto& [k, v] : j.items()) t[k] = JsonToLua(v, lua);
        return t;
    }
    return sol::nil;
}

static json LuaToJson(sol::object obj) {
    if (!obj.valid() || obj.get_type() == sol::type::nil) return json(nullptr);
    if (obj.get_type() == sol::type::boolean) return obj.as<bool>();
    if (obj.get_type() == sol::type::number) {
        // Try int first
        double d = obj.as<double>();
        if (d == static_cast<double>(static_cast<int64_t>(d)))
            return static_cast<int64_t>(d);
        return d;
    }
    if (obj.get_type() == sol::type::string) return obj.as<std::string>();
    if (obj.get_type() == sol::type::table) {
        sol::table tbl = obj.as<sol::table>();
        // Detect array: keys are sequential integers starting at 1
        bool is_array = true;
        int  max_idx  = 0;
        for (auto& [k, v] : tbl) {
            if (k.get_type() != sol::type::number) { is_array = false; break; }
            int i = static_cast<int>(k.as<double>());
            if (i != max_idx + 1 && i != max_idx) { is_array = false; break; }
            max_idx = std::max(max_idx, i);
        }
        if (is_array && max_idx > 0) {
            json arr = json::array();
            for (int i = 1; i <= max_idx; ++i) arr.push_back(LuaToJson(tbl[i]));
            return arr;
        }
        json obj2 = json::object();
        for (auto& [k, v] : tbl) {
            if (k.get_type() == sol::type::string)
                obj2[k.as<std::string>()] = LuaToJson(v);
        }
        return obj2;
    }
    return json(nullptr);
}

// ─── Terrain config helper ────────────────────────────────────────────────────

// Helper: read typed field from a sol::table, with a default
template<typename T>
static T tget(const sol::table& t, const char* key, T def) {
    return t[key].get<sol::optional<T>>().value_or(def);
}

static ScenarioDef TableToScenarioDef(sol::table cfg) {
    ScenarioDef def;
    def.map.width  = tget<int>(cfg, "width",  2048);
    def.map.height = tget<int>(cfg, "height", 512);
    def.map.seed   = static_cast<uint32_t>(tget<double>(cfg, "seed", 0.0));
    def.map.shape  = tget<std::string>(cfg, "shape", std::string("flat"));

    if (auto sp = cfg.get<sol::optional<sol::table>>("shape_params")) {
        for (auto& [k, v] : *sp) {
            if (k.get_type() == sol::type::string && v.get_type() == sol::type::number)
                def.map.shape_params.params[k.as<std::string>()] = v.as<float>();
        }
    }

    if (auto mats = cfg.get<sol::optional<sol::table>>("materials")) {
        for (auto& [_, m] : *mats) {
            if (m.get_type() != sol::type::table) continue;
            sol::table mt = m.as<sol::table>();
            MaterialRule rule;
            rule.material_id   = tget<std::string>(mt, "id",         std::string(""));
            rule.rule          = tget<std::string>(mt, "rule",       std::string("fill"));
            rule.background_id = tget<std::string>(mt, "background", std::string(""));
            rule.depth         = tget<int>(mt, "depth",     0);
            rule.min_depth     = tget<int>(mt, "min_depth", 0);
            def.map.materials.push_back(rule);
        }
    }

    if (auto feats = cfg.get<sol::optional<sol::table>>("features")) {
        for (auto& [_, f] : *feats) {
            if (f.get_type() != sol::type::table) continue;
            sol::table ft = f.as<sol::table>();
            FeatureConfig fc;
            fc.type        = tget<std::string>(ft, "type",       std::string(""));
            fc.material    = tget<std::string>(ft, "material",   std::string(""));
            fc.zone        = tget<std::string>(ft, "zone",       std::string("all"));
            fc.density     = static_cast<float>(tget<double>(ft, "density",     0.05));
            fc.count       = tget<int>(ft, "count",       0);
            fc.min_size    = tget<int>(ft, "min_size",    10);
            fc.max_size    = tget<int>(ft, "max_size",    40);
            fc.vein_radius = tget<int>(ft, "vein_radius", 8);
            def.map.features.push_back(fc);
        }
    }

    if (auto ovrs = cfg.get<sol::optional<sol::table>>("overrides")) {
        for (auto& [_, o] : *ovrs) {
            if (o.get_type() != sol::type::table) continue;
            sol::table ot = o.as<sol::table>();
            CellOverride co;
            co.x             = tget<int>(ot, "x", 0);
            co.y             = tget<int>(ot, "y", 0);
            co.material_id   = tget<std::string>(ot, "material_id",   std::string(""));
            co.background_id = tget<std::string>(ot, "background_id", std::string(""));
            def.overrides.push_back(co);
        }
    }

    return def;
}

// ─── Engine API bindings ──────────────────────────────────────────────────────

void LuaState::BindAPI() {
    auto& lua    = impl_->lua;
    auto& engine = impl_->engine;
    auto& ui     = impl_->ui;

    lua.open_libraries(sol::lib::base, sol::lib::math,
                       sol::lib::string, sol::lib::table, sol::lib::io,
                       sol::lib::package);

    // ── engine global table ────────────────────────────────────────────────────
    auto eng = lua.create_named_table("engine");

    eng["quit"] = [&engine]() { engine.RequestQuit(); };

    eng["log"] = [](const std::string& msg) {
        EngineLog::Log("[Lua] " + msg);
    };

    eng["set_tick_callback"] = [&engine](sol::function fn) {
        engine.SetTickCallback([fn](double dt) mutable {
            auto res = fn(dt);
            if (!res.valid()) {
                sol::error err = res;
                std::string msg = std::string("[Lua error in tick_callback] ") + err.what();
                EngineLog::Log(msg);
                std::fprintf(stderr, "%s\n", msg.c_str());
            }
        });
    };

    // UI usertypes + engine.ui live in LuaUIBindings.cpp.
    RegisterLuaUIBindings(lua, ui, engine);

    // ── engine.terrain table ───────────────────────────────────────────────────
    auto ter_tbl = eng.create("terrain");

    ter_tbl["generate"] = [&engine](sol::table cfg) {
        ScenarioDef def = TableToScenarioDef(cfg);
        engine.GenerateTerrain(def);
    };

    ter_tbl["unload"] = [&engine]() {
        engine.UnloadTerrain();
    };

    ter_tbl["is_loaded"] = [&engine]() -> bool {
        // GenerateTerrain sets terrain_renderer_; check via GetRaw (proxy: no direct access)
        // We expose this indirectly — Lua uses engine.terrain.get_width() != 0 as fallback,
        // but a dedicated flag is cleaner; we use get_width returning 0 as the "not loaded" state.
        // To keep Engine header clean, we add a small accessor:
        return engine.IsTerrainLoaded();
    };

    ter_tbl["get_width"]  = [&engine]() -> int { return engine.GetTerrainWidth(); };
    ter_tbl["get_height"] = [&engine]() -> int { return engine.GetTerrainHeight(); };

    ter_tbl["set_cell"] = [&engine](int x, int y,
                                     const std::string& mat, const std::string& bg) {
        engine.SetTerrainCell(x, y, mat, bg);
    };
    ter_tbl["get_cell"] = [&lua, &engine](int x, int y) -> sol::table {
        auto [mat_id, bg_id] = engine.GetTerrainCell(x, y);
        sol::table t = lua.create_table();
        t["material_id"]   = mat_id;
        t["background_id"] = bg_id;
        return t;
    };

    // Returns the actual seed used by the most recent generate() call.
    // When the Lua-side seed was 0, MapGenerator picks a random one internally;
    // this binding lets Lua recover that value to lock the seed correctly.
    ter_tbl["get_last_seed"] = [&engine]() -> uint32_t {
        return engine.GetLastTerrainSeed();
    };

    // ── engine.registry table ────────────────────────────────────────────────
    auto reg_tbl = eng.create("registry");

    reg_tbl["get_materials"] = [&lua, &engine]() -> sol::table {
        sol::table out = lua.create_table();
        int idx = 1;
        for (const auto* m : engine.GetRegistry().GetAllMaterials()) {
            if (!m) continue;
            sol::table t = lua.create_table();
            t["id"]   = m->qualified_id;
            t["name"] = m->name;
            t["r"]    = static_cast<int>(m->color.r);
            t["g"]    = static_cast<int>(m->color.g);
            t["b"]    = static_cast<int>(m->color.b);
            out[idx++] = t;
        }
        return out;
    };

    reg_tbl["get_backgrounds"] = [&lua, &engine]() -> sol::table {
        sol::table out = lua.create_table();
        int idx = 1;
        int count = static_cast<int>(engine.GetRegistry().GetBackgroundCount());
        for (int i = 0; i < count; ++i) {
            const auto* b = engine.GetRegistry().GetBackgroundByRuntimeID(
                static_cast<BackgroundID>(i));
            if (!b) continue;
            sol::table t = lua.create_table();
            t["id"]   = b->qualified_id;
            t["name"] = b->name;
            t["r"]    = static_cast<int>(b->color.r);
            t["g"]    = static_cast<int>(b->color.g);
            t["b"]    = static_cast<int>(b->color.b);
            out[idx++] = t;
        }
        return out;
    };

    // ── engine.camera table ───────────────────────────────────────────────────
    auto cam_tbl = eng.create("camera");

    cam_tbl["get_x"]        = [&engine]() -> float { return engine.GetCameraX(); };
    cam_tbl["get_y"]        = [&engine]() -> float { return engine.GetCameraY(); };
    cam_tbl["set_position"] = [&engine](float x, float y) { engine.SetCameraPosition(x, y); };
    cam_tbl["move"]         = [&engine](float dx, float dy) { engine.MoveCamera(dx, dy); };
    cam_tbl["get_zoom"]     = [&engine]() -> float { return engine.GetCameraZoom(); };
    cam_tbl["set_zoom"]     = [&engine](float s) { engine.SetCameraZoom(s); };

    // ── engine.log_console table ─────────────────────────────────────────────
    auto lc_tbl = eng.create("log_console");
    lc_tbl["show"]       = [&engine]() { engine.SetLogConsoleVisible(true); };
    lc_tbl["hide"]       = [&engine]() { engine.SetLogConsoleVisible(false); };
    lc_tbl["toggle"]     = [&engine]() { engine.ToggleLogConsole(); };
    lc_tbl["is_visible"] = [&engine]() -> bool { return engine.IsLogConsoleVisible(); };

    // ── engine.debug table ───────────────────────────────────────────────────
    auto dbg_tbl = eng.create("debug");
    dbg_tbl["set_overlay"] = [&engine](const std::string& m) { engine.SetRenderOverlay(m); };
    dbg_tbl["get_overlay"] = [&engine]() -> std::string { return engine.GetRenderOverlay(); };
    dbg_tbl["is_overlay_visible"] = [&engine]() -> bool { return engine.IsDebugOverlayVisible(); };

    // ── engine.sim table ─────────────────────────────────────────────────────
    auto sim_tbl = eng.create("sim");
    sim_tbl["get_time_scale"] = [&engine]() -> float { return engine.GetSimTimeScale(); };
    sim_tbl["set_time_scale"] = [&engine](float s) { engine.SetSimTimeScale(s); };
    sim_tbl["get_active_chunks"] = [&engine]() -> int {
        auto* s = engine.GetTerrainSimulator();
        return s ? s->GetActiveChunkCount() : 0;
    };
    sim_tbl["get_total_chunks"] = [&engine]() -> int {
        auto* s = engine.GetTerrainSimulator();
        return s ? s->GetTotalChunkCount() : 0;
    };
    // Phase 3 — per-cell accessors
    sim_tbl["get_temperature"]   = [&engine](int x, int y) -> float { return engine.GetCellTemperature(x, y); };
    sim_tbl["get_health"]        = [&engine](int x, int y) -> int   { return engine.GetCellHealth(x, y); };
    sim_tbl["is_ignited"]        = [&engine](int x, int y) -> bool  { return engine.GetCellIgnited(x, y); };
    sim_tbl["get_crack"]         = [&engine](int x, int y) -> int   { return static_cast<int>(engine.GetCellCrack(x, y)); };
    sim_tbl["set_temperature"]   = [&engine](int x, int y, float t) { engine.SetCellTemperature(x, y, t); };
    sim_tbl["set_health"]        = [&engine](int x, int y, int h)   { engine.SetCellHealth(x, y, h); };
    sim_tbl["ignite"]            = [&engine](int x, int y)          { engine.SetCellIgnited(x, y, true); };
    sim_tbl["extinguish"]        = [&engine](int x, int y)          { engine.SetCellIgnited(x, y, false); };
    sim_tbl["apply_damage"]      = [&engine](int x, int y, int d)   { engine.ApplyDamageAt(x, y, d); };
    sim_tbl["trigger_explosion"] = [&engine](int x, int y, int r, int s) { engine.TriggerExplosionAt(x, y, r, s); };
    sim_tbl["dynamic_body_count"]= [&engine]() -> int {
        auto* dbm = engine.GetDynamicBodies(); return dbm ? dbm->ActiveBodyCount() : 0;
    };
    sim_tbl["pause"]  = [&engine]()      { engine.SetSimTimeScale(0.0f); };
    sim_tbl["resume"] = [&engine]()      { engine.SetSimTimeScale(1.0f); };
    sim_tbl["step"]   = [&engine](int n) { engine.StepSim(n); };
    sim_tbl["spawn_particle"] = [&engine](const std::string& q, int x, int y,
                                          float vx, float vy, int ttl) {
        engine.SpawnParticle(q, x, y, vx, vy, ttl);
    };

    // ── engine.materials table ───────────────────────────────────────────────
    auto mat_tbl = eng.create("materials");
    mat_tbl["get_conducts_heat"] = [&engine](const std::string& q) -> bool {
        return engine.GetMaterialConductsHeat(q);
    };
    mat_tbl["set_conducts_heat"] = [&engine](const std::string& q, bool on) {
        engine.SetMaterialConductsHeat(q, on);
    };

    // ── engine.physics table ─────────────────────────────────────────────────
    auto phys_tbl = eng.create("physics");
    phys_tbl["on_collision"] = [&engine](sol::function fn) {
        engine.SetPhysicsCollisionCallback(
            [fn](EntityID e, int mat, float v) mutable {
                auto res = fn(static_cast<int>(e), mat, v);
                if (!res.valid()) {
                    sol::error err = res;
                    std::fprintf(stderr, "[on_collision error] %s\n", err.what());
                }
            });
    };

    // ── engine.input table ────────────────────────────────────────────────────
    auto inp_tbl = eng.create("input");

    inp_tbl["is_key_down"] = [&engine](int sc) -> bool {
        return engine.GetRaw().IsKeyDown(static_cast<SDL_Scancode>(sc));
    };
    inp_tbl["is_key_just_pressed"] = [&engine](int sc) -> bool {
        return engine.GetRaw().IsJustPressed(static_cast<SDL_Scancode>(sc));
    };
    inp_tbl["mouse_x"]      = [&engine]() -> int { return engine.GetInput().GetMouseX(); };
    inp_tbl["mouse_y"]      = [&engine]() -> int { return engine.GetInput().GetMouseY(); };
    inp_tbl["scroll_y"]     = [&engine]() -> int { return engine.GetInput().GetScrollY(); };
    inp_tbl["mouse_button"] = [&engine](int btn) -> bool {
        return engine.GetInput().IsMouseDown(btn);
    };
    inp_tbl["mouse_just_pressed"] = [&engine](int btn) -> bool {
        return engine.GetRaw().IsMouseJustPressed(btn);
    };
    inp_tbl["mouse_just_released"] = [&engine](int btn) -> bool {
        return engine.GetRaw().IsMouseJustReleased(btn);
    };

    // ── engine.key constants ──────────────────────────────────────────────────
    auto key_tbl = eng.create("key");
    key_tbl["LEFT"]      = static_cast<int>(SDL_SCANCODE_LEFT);
    key_tbl["RIGHT"]     = static_cast<int>(SDL_SCANCODE_RIGHT);
    key_tbl["UP"]        = static_cast<int>(SDL_SCANCODE_UP);
    key_tbl["DOWN"]      = static_cast<int>(SDL_SCANCODE_DOWN);
    key_tbl["W"]         = static_cast<int>(SDL_SCANCODE_W);
    key_tbl["A"]         = static_cast<int>(SDL_SCANCODE_A);
    key_tbl["S"]         = static_cast<int>(SDL_SCANCODE_S);
    key_tbl["D"]         = static_cast<int>(SDL_SCANCODE_D);
    key_tbl["N"]         = static_cast<int>(SDL_SCANCODE_N);
    key_tbl["O"]         = static_cast<int>(SDL_SCANCODE_O);
    key_tbl["Z"]         = static_cast<int>(SDL_SCANCODE_Z);
    key_tbl["RETURN"]    = static_cast<int>(SDL_SCANCODE_RETURN);
    key_tbl["ESCAPE"]    = static_cast<int>(SDL_SCANCODE_ESCAPE);
    key_tbl["BACKSPACE"] = static_cast<int>(SDL_SCANCODE_BACKSPACE);
    key_tbl["LCTRL"]     = static_cast<int>(SDL_SCANCODE_LCTRL);
    key_tbl["RCTRL"]     = static_cast<int>(SDL_SCANCODE_RCTRL);
    key_tbl["GRAVE"]     = static_cast<int>(SDL_SCANCODE_GRAVE);

    // ── engine.fs table ───────────────────────────────────────────────────────
    auto fs_tbl = eng.create("fs");

    fs_tbl["read_text"] = [](const std::string& path) -> sol::optional<std::string> {
        std::ifstream f(path);
        if (!f.is_open()) return sol::nullopt;
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    };

    fs_tbl["write_text"] = [](const std::string& path, const std::string& text) -> bool {
        try {
            fs::path p(path);
            if (p.has_parent_path()) fs::create_directories(p.parent_path());
            std::ofstream f(path);
            if (!f.is_open()) return false;
            f << text;
            return true;
        } catch (...) { return false; }
    };

    fs_tbl["file_exists"] = [](const std::string& path) -> bool {
        return fs::exists(path);
    };

    fs_tbl["list_dir"] = [&lua](const std::string& dir, const std::string& ext) -> sol::table {
        sol::table result = lua.create_table();
        int idx = 1;
        try {
            for (auto& entry : fs::recursive_directory_iterator(dir,
                     fs::directory_options::skip_permission_denied)) {
                if (!entry.is_regular_file()) continue;
                if (!ext.empty() && entry.path().extension() != ext) continue;
                sol::table item = lua.create_table();
                item["path"] = entry.path().string();
                item["name"] = entry.path().filename().string();
                item["stem"] = entry.path().stem().string();
                result[idx++] = item;
            }
        } catch (...) {}
        return result;
    };

    // ── engine.json table ─────────────────────────────────────────────────────
    auto json_tbl = eng.create("json");

    json_tbl["encode"] = [](sol::object obj) -> std::string {
        return LuaToJson(obj).dump(2);
    };

    json_tbl["decode"] = [&lua](const std::string& str) -> sol::object {
        try {
            return JsonToLua(json::parse(str), lua);
        } catch (...) { return sol::nil; }
    };

    // ── engine.terrain additions ──────────────────────────────────────────────

    ter_tbl["trace_line"] = [&lua, &engine](int x0, int y0, int x1, int y1) -> sol::table {
        auto pts = engine.TraceLine(x0, y0, x1, y1);
        sol::table out = lua.create_table();
        for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
            sol::table p = lua.create_table();
            p["x"] = pts[i].first;
            p["y"] = pts[i].second;
            out[i + 1] = p;
        }
        return out;
    };

    // paint_line: does Bresenham + brush painting entirely in C++.
    // Returns a flat Lua array {x0,y0,x1,y1,...} of every cell painted so
    // Lua can update the override map in one O(N) pass — no nested tables,
    // no per-cell C++ boundary crossing, one UpdateRegion for the whole stroke.
    ter_tbl["paint_line"] = [&lua, &engine](int x0, int y0, int x1, int y1,
                                             const std::string& mat_id,
                                             const std::string& bg_id,
                                             int brush_size) -> sol::table {
        auto flat = engine.PaintLine(x0, y0, x1, y1, mat_id, bg_id, brush_size);
        sol::table out = lua.create_table(static_cast<int>(flat.size()), 0);
        for (int i = 0; i < static_cast<int>(flat.size()); ++i)
            out[i + 1] = flat[i];
        return out;
    };

    // ── engine.registry additions ─────────────────────────────────────────────

    reg_tbl["get_objects"] = [&lua, &engine]() -> sol::table {
        sol::table out = lua.create_table();
        int idx = 1;
        for (const auto& [qid, obj_ptr] : engine.GetRegistry().GetAllObjects()) {
            if (!obj_ptr) continue;
            sol::table t = lua.create_table();
            t["id"]     = obj_ptr->qualified_id;
            t["name"]   = obj_ptr->name;
            t["size_w"] = obj_ptr->size_w;
            t["size_h"] = obj_ptr->size_h;
            t["r"]      = static_cast<int>(obj_ptr->color.r);
            t["g"]      = static_cast<int>(obj_ptr->color.g);
            t["b"]      = static_cast<int>(obj_ptr->color.b);
            t["sprite_path"] = obj_ptr->sprite_path;
            sol::table props = lua.create_table();
            for (const auto& [k, v] : obj_ptr->properties) props[k] = v;
            t["props"] = props;
            out[idx++] = t;
        }
        return out;
    };

    // ── engine.editor table ───────────────────────────────────────────────────

    auto edit_tbl = eng.create("editor");

    edit_tbl["set_markers"] = [&engine](sol::table markers) {
        std::vector<Engine::WorldMarker> list;
        for (auto& [_, v] : markers) {
            if (v.get_type() != sol::type::table) continue;
            sol::table m = v.as<sol::table>();
            Engine::WorldMarker wm;
            wm.wx       = m.get_or("wx", 0.0f);
            wm.wy       = m.get_or("wy", 0.0f);
            wm.w        = m.get_or("w",  12);
            wm.h        = m.get_or("h",  20);
            wm.r        = static_cast<uint8_t>(m.get_or("r", 200));
            wm.g        = static_cast<uint8_t>(m.get_or("g", 200));
            wm.b        = static_cast<uint8_t>(m.get_or("b", 200));
            wm.selected     = m.get_or("selected", false);
            wm.sprite_path  = m.get_or<std::string>("sprite", "");
            list.push_back(wm);
        }
        engine.SetEditorMarkers(std::move(list));
    };

    edit_tbl["clear_markers"] = [&engine]() {
        engine.SetEditorMarkers({});
    };

    // ── engine stats ──────────────────────────────────────────────────────────

    eng["get_fps"]              = [&engine]() -> int { return engine.GetFPS(); };
    eng["get_solid_cell_count"] = [&engine]() -> int { return engine.GetNonAirCellCount(); };

    // ── engine.overrides table (compact binary+base64 encoding) ───────────────

    // Base64 alphabet
    static const char B64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    auto b64_encode = [](const std::vector<uint8_t>& data) -> std::string {
        std::string out;
        out.reserve(((data.size() + 2) / 3) * 4);
        for (size_t i = 0; i < data.size(); i += 3) {
            uint32_t b = static_cast<uint32_t>(data[i]) << 16;
            if (i + 1 < data.size()) b |= static_cast<uint32_t>(data[i+1]) << 8;
            if (i + 2 < data.size()) b |= static_cast<uint32_t>(data[i+2]);
            out += B64[(b >> 18) & 0x3F];
            out += B64[(b >> 12) & 0x3F];
            out += (i + 1 < data.size()) ? B64[(b >> 6) & 0x3F] : '=';
            out += (i + 2 < data.size()) ? B64[(b     ) & 0x3F] : '=';
        }
        return out;
    };

    auto b64_decode = [](const std::string& s) -> std::vector<uint8_t> {
        auto val = [](char c) -> int {
            if (c >= 'A' && c <= 'Z') return c - 'A';
            if (c >= 'a' && c <= 'z') return c - 'a' + 26;
            if (c >= '0' && c <= '9') return c - '0' + 52;
            if (c == '+') return 62;
            if (c == '/') return 63;
            return -1;
        };
        std::vector<uint8_t> out;
        out.reserve(s.size() * 3 / 4);
        for (size_t i = 0; i + 3 < s.size(); i += 4) {
            int a = val(s[i]), b = val(s[i+1]), c = val(s[i+2]), d = val(s[i+3]);
            if (a < 0 || b < 0) break;
            out.push_back(static_cast<uint8_t>((a << 2) | (b >> 4)));
            if (c >= 0) out.push_back(static_cast<uint8_t>((b << 4) | (c >> 2)));
            if (d >= 0) out.push_back(static_cast<uint8_t>((c << 6) |  d));
        }
        return out;
    };

    auto ov_tbl = eng.create("overrides");

    // encode(lua_override_list) → JSON string (overrides_v2 format)
    // Each override: {x, y, material_id, background_id}
    // Wire format: uint16 x, uint16 y, uint8 mat_idx, uint8 bg_idx → 6 bytes/cell
    ov_tbl["encode"] = [b64_encode](sol::table overrides) -> std::string {
        // Build palettes
        std::vector<std::string> mat_pal, bg_pal;
        auto pal_idx = [](std::vector<std::string>& pal, const std::string& s) -> uint8_t {
            for (size_t i = 0; i < pal.size(); ++i)
                if (pal[i] == s) return static_cast<uint8_t>(i);
            pal.push_back(s);
            return static_cast<uint8_t>(pal.size() - 1);
        };

        // Pre-scan
        for (auto& [_, v] : overrides) {
            if (v.get_type() != sol::type::table) continue;
            sol::table ov = v.as<sol::table>();
            pal_idx(mat_pal, ov.get_or<std::string>("material_id", ""));
            pal_idx(bg_pal,  ov.get_or<std::string>("background_id", ""));
        }

        // Build binary blob
        std::vector<uint8_t> blob;
        for (auto& [_, v] : overrides) {
            if (v.get_type() != sol::type::table) continue;
            sol::table ov = v.as<sol::table>();
            int x = ov.get_or("x", 0);
            int y = ov.get_or("y", 0);
            uint8_t mi = pal_idx(mat_pal, ov.get_or<std::string>("material_id", ""));
            uint8_t bi = pal_idx(bg_pal,  ov.get_or<std::string>("background_id", ""));
            blob.push_back(static_cast<uint8_t>(x & 0xFF));
            blob.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
            blob.push_back(static_cast<uint8_t>(y & 0xFF));
            blob.push_back(static_cast<uint8_t>((y >> 8) & 0xFF));
            blob.push_back(mi);
            blob.push_back(bi);
        }

        json j;
        j["mat_palette"] = mat_pal;
        j["bg_palette"]  = bg_pal;
        j["data"]        = b64_encode(blob);
        return j.dump();
    };

    // decode(json_string) → lua override list
    ov_tbl["decode"] = [&lua, b64_decode](const std::string& s) -> sol::object {
        try {
            json j = json::parse(s);
            auto mat_pal = j["mat_palette"].get<std::vector<std::string>>();
            auto bg_pal  = j["bg_palette"].get<std::vector<std::string>>();
            auto blob    = b64_decode(j["data"].get<std::string>());

            sol::table out = lua.create_table();
            int idx = 1;
            for (size_t i = 0; i + 5 < blob.size(); i += 6) {
                int x = blob[i]   | (blob[i+1] << 8);
                int y = blob[i+2] | (blob[i+3] << 8);
                uint8_t mi = blob[i+4];
                uint8_t bi = blob[i+5];
                sol::table ov = lua.create_table();
                ov["x"]           = x;
                ov["y"]           = y;
                ov["material_id"] = (mi < mat_pal.size()) ? mat_pal[mi] : "";
                ov["background_id"]=(bi < bg_pal.size())  ? bg_pal[bi]  : "";
                out[idx++] = ov;
            }
            return out;
        } catch (...) { return sol::nil; }
    };

    // ── SimCell (material simulation proxy) ───────────────────────────────────
    lua.new_usertype<SimCell>("SimCell",
        sol::no_constructor,
        "material_id",    &SimCell::material_id,
        "temperature",    &SimCell::temperature,
        "get_health",     &SimCell::get_health,
        "is_ignited",     &SimCell::is_ignited,
        "convert_to",     &SimCell::convert_to,
        "deal_damage",    &SimCell::deal_damage,
        "add_temperature",&SimCell::add_temperature,
        "ignite_cell",    &SimCell::ignite_cell,
        "extinguish_cell",&SimCell::extinguish_cell,
        "neighbor",       [](SimCell& sc, int dx, int dy) -> sol::optional<SimCell> {
            auto opt = sc.neighbor(dx, dy);
            if (opt) return *opt;
            return sol::nullopt;
        }
    );
}

// ─── Material script loader ───────────────────────────────────────────────────

void LuaState::LoadMaterialScripts(TerrainSimulator& sim) {
    auto& lua      = impl_->lua;
    auto& registry = impl_->engine.GetRegistry();

    namespace fs = std::filesystem;

    for (int i = 0; i < 256; i++) {
        const auto* mat = registry.GetMaterialByRuntimeID(static_cast<MaterialID>(i));
        if (!mat || mat->script_path.empty()) continue;
        if (!fs::exists(mat->script_path)) continue;

        // Load the material script into its own isolated environment.
        // The script must return a table with optional fields: on_tick, on_contact, on_heat.
        sol::environment env(lua, sol::create, lua.globals());
        auto result = lua.safe_script_file(mat->script_path, env,
            [](lua_State*, sol::protected_function_result pfr) { return pfr; });

        if (!result.valid()) {
            sol::error err = result;
            std::fprintf(stderr, "[material script] %s: %s\n",
                         mat->script_path.c_str(), err.what());
            continue;
        }

        // Script should return a table
        if (result.get_type() != sol::type::table) {
            std::fprintf(stderr, "[material script] %s: must return a table\n",
                         mat->script_path.c_str());
            continue;
        }

        sol::table tbl = result;

        if (auto fn = tbl.get<sol::optional<sol::function>>("on_tick"); fn) {
            const_cast<MaterialDef*>(mat)->has_on_tick = true;
            sol::function captured = *fn;
            sim.SetOnTickCallback(i, [captured](SimCell& sc) mutable {
                auto res = captured(sc);
                if (!res.valid()) {
                    sol::error err = res;
                    std::fprintf(stderr, "[on_tick error] %s\n", err.what());
                }
            });
            std::printf("  Loaded on_tick script for material %s (id=%d)\n",
                        mat->qualified_id.c_str(), i);
        }

        if (auto fn = tbl.get<sol::optional<sol::function>>("on_contact"); fn) {
            const_cast<MaterialDef*>(mat)->has_on_contact = true;
            sol::function captured = *fn;
            sim.SetOnContactCallback(i, [captured](SimCell& a, SimCell& b) mutable {
                auto res = captured(a, b);
                if (!res.valid()) {
                    sol::error err = res;
                    std::fprintf(stderr, "[on_contact error] %s\n", err.what());
                }
            });
            std::printf("  Loaded on_contact script for material %s (id=%d)\n",
                        mat->qualified_id.c_str(), i);
        }

        if (auto fn = tbl.get<sol::optional<sol::function>>("on_heat"); fn) {
            const_cast<MaterialDef*>(mat)->has_on_heat = true;
            sol::function captured = *fn;
            sim.SetOnHeatCallback(i, [captured](SimCell& sc, float delta) mutable {
                auto res = captured(sc, delta);
                if (!res.valid()) {
                    sol::error err = res;
                    std::fprintf(stderr, "[on_heat error] %s\n", err.what());
                }
            });
            std::printf("  Loaded on_heat script for material %s (id=%d)\n",
                        mat->qualified_id.c_str(), i);
        }
    }
}
