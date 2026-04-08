#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include "scripting/LuaState.h"
#include "core/Engine.h"
#include "core/EngineLog.h"
#include "ui/UISystem.h"
#include "ui/UIElement.h"
#include "ui/UIScreen.h"
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

    // ── UIElement ──────────────────────────────────────────────────────────────
    lua.new_usertype<UIElement>("UIElement",
        sol::no_constructor,
        "id",      sol::property(
            [](const UIElement& el) { return el.id; },
            [](UIElement& el, const std::string& v) { el.id = v; }),
        "visible", sol::property(
            [](const UIElement& el) { return el.visible; },
            [](UIElement& el, bool v) { el.visible = v; }),
        "text",    sol::property(
            [](const UIElement& el) { return el.text; },
            [](UIElement& el, const std::string& v) { el.text = v; }),
        "value",   sol::property(
            [](const UIElement& el) { return el.value; },
            [](UIElement& el, const std::string& v) { el.value = v; }),
        "focused", sol::property(
            [](const UIElement& el) { return el.focused; },
            [](UIElement& el, bool v) { el.focused = v; }),

        "on_click", [](UIElement& el, sol::function fn) {
            el.on_click = [fn]() mutable { fn(); };
        },
        "on_change", [](UIElement& el, sol::function fn) {
            el.on_change = [fn](const std::string& s) mutable { fn(s); };
        },

        "add_frame",  &UIElement::AddFrame,
        "add_button", &UIElement::AddButton,
        "add_label",  &UIElement::AddLabel,
        "add_input",  &UIElement::AddInput
    );

    // ── UIScreen ───────────────────────────────────────────────────────────────
    lua.new_usertype<UIScreen>("UIScreen",
        sol::no_constructor,
        "name",       sol::property(
            [](const UIScreen& s) { return s.name; },
            [](UIScreen& s, const std::string& v) { s.name = v; }),
        "add_frame",  &UIScreen::AddFrame,
        "add_button", &UIScreen::AddButton,
        "add_label",  &UIScreen::AddLabel,
        "add_input",  &UIScreen::AddInput
    );

    // ── engine global table ────────────────────────────────────────────────────
    auto eng = lua.create_named_table("engine");

    eng["quit"] = [&engine]() { engine.RequestQuit(); };

    eng["log"] = [](const std::string& msg) {
        EngineLog::Log("[Lua] " + msg);
    };

    eng["set_tick_callback"] = [&engine](sol::function fn) {
        engine.SetTickCallback([fn](double dt) mutable { fn(dt); });
    };

    // ── engine.ui table ────────────────────────────────────────────────────────
    auto ui_tbl = eng.create("ui");

    ui_tbl["create_screen"] = [&ui](const std::string& name) -> std::shared_ptr<UIScreen> {
        return ui.CreateScreen(name);
    };
    ui_tbl["show_screen"] = [&ui](std::shared_ptr<UIScreen> screen) {
        ui.ShowScreen(screen);
    };
    ui_tbl["pop_screen"] = [&ui]() {
        ui.PopScreen();
    };

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

    // ── engine.camera table ───────────────────────────────────────────────────
    auto cam_tbl = eng.create("camera");

    cam_tbl["get_x"]        = [&engine]() -> float { return engine.GetCameraX(); };
    cam_tbl["get_y"]        = [&engine]() -> float { return engine.GetCameraY(); };
    cam_tbl["set_position"] = [&engine](float x, float y) { engine.SetCameraPosition(x, y); };
    cam_tbl["move"]         = [&engine](float dx, float dy) { engine.MoveCamera(dx, dy); };
    cam_tbl["get_zoom"]     = [&engine]() -> float { return engine.GetCameraZoom(); };
    cam_tbl["set_zoom"]     = [&engine](float s) { engine.SetCameraZoom(s); };

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
}
