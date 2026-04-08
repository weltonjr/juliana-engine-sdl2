#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include "scripting/LuaState.h"
#include "core/Engine.h"
#include "ui/UISystem.h"
#include "ui/UIElement.h"
#include "ui/UIScreen.h"
#include <cstdio>

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

// ─── Script execution ─────────────────────────────────────────────────────────

bool LuaState::RunScript(const std::string& path, const std::string& base_path) {
    auto& lua = impl_->lua;

    // Let Lua's require() find scripts in <base_path>/scripts/ and <base_path>/
    if (!base_path.empty()) {
        std::string pkg_path = base_path + "/scripts/?.lua;"
                             + base_path + "/?.lua";
        lua["package"]["path"] = pkg_path;
    }

    auto result = lua.safe_script_file(path, [](lua_State*, sol::protected_function_result pfr) {
        return pfr;
    });

    if (!result.valid()) {
        sol::error err = result;
        std::fprintf(stderr, "Lua error in '%s': %s\n", path.c_str(), err.what());
        return false;
    }
    return true;
}

// ─── Engine API bindings ──────────────────────────────────────────────────────

void LuaState::BindAPI() {
    auto& lua    = impl_->lua;
    auto& engine = impl_->engine;
    auto& ui     = impl_->ui;

    lua.open_libraries(sol::lib::base, sol::lib::math,
                       sol::lib::string, sol::lib::table, sol::lib::io);

    // ── UIElement ──────────────────────────────────────────────────────────────
    // Exposed as a shared_ptr usertype so Lua can hold references to child elements.
    lua.new_usertype<UIElement>("UIElement",
        sol::no_constructor,
        "id",      &UIElement::id,
        "visible", &UIElement::visible,
        "text",    &UIElement::text,

        // Register a Lua function as the click handler
        "on_click", [](UIElement& el, sol::function fn) {
            el.on_click = [fn]() mutable { fn(); };
        },

        // Child factory helpers — mirrors the C++ API
        "add_frame",  &UIElement::AddFrame,
        "add_button", &UIElement::AddButton,
        "add_label",  &UIElement::AddLabel
    );

    // ── UIScreen ───────────────────────────────────────────────────────────────
    lua.new_usertype<UIScreen>("UIScreen",
        sol::no_constructor,
        "name",       &UIScreen::name,
        "add_frame",  &UIScreen::AddFrame,
        "add_button", &UIScreen::AddButton,
        "add_label",  &UIScreen::AddLabel
    );

    // ── engine global table ────────────────────────────────────────────────────
    auto eng = lua.create_named_table("engine");

    eng["quit"] = [&engine]() {
        engine.RequestQuit();
    };

    // engine.log(msg) — prints to stdout with a [Lua] prefix
    eng["log"] = [](const std::string& msg) {
        std::printf("[Lua] %s\n", msg.c_str());
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
}
