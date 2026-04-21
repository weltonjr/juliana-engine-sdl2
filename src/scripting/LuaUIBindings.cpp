#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include "scripting/LuaUIBindings.h"
#include "ui/UIElement.h"
#include "ui/UIScreen.h"
#include "ui/UISystem.h"
#include "core/Engine.h"
#include "core/EngineLog.h"
#include <cstdio>
#include <string>
#include <utility>

namespace {

// Wraps a Lua function into a safe std::function<void()>. Logs Lua errors
// through EngineLog so scripts don't silently swallow exceptions.
std::function<void()> WrapVoid(sol::function fn, const char* name) {
    return [fn = std::move(fn), name]() mutable {
        auto res = fn();
        if (!res.valid()) {
            sol::error err = res;
            std::string msg = std::string("[Lua ") + name + "] " + err.what();
            EngineLog::Log(msg);
            std::fprintf(stderr, "%s\n", msg.c_str());
        }
    };
}

std::function<void(const std::string&)> WrapStr(sol::function fn, const char* name) {
    return [fn = std::move(fn), name](const std::string& s) mutable {
        auto res = fn(s);
        if (!res.valid()) {
            sol::error err = res;
            std::string msg = std::string("[Lua ") + name + "] " + err.what();
            EngineLog::Log(msg);
            std::fprintf(stderr, "%s\n", msg.c_str());
        }
    };
}

// sol2's SOL_ALL_SAFETIES_ON rejects fractional doubles in int slots. Scripts
// frequently do WIN_W/2, which yields a Lua float — coerce once at the boundary.
int ToInt(double v) { return static_cast<int>(v); }

}  // namespace

void RegisterLuaUIBindings(sol::state& lua, UISystem& ui, Engine& engine) {
    // ── UIElement ─────────────────────────────────────────────────────────────
    // Direct field bindings replace the 12 hand-written lambda-property pairs.
    lua.new_usertype<UIElement>("UIElement",
        sol::no_constructor,
        "id",         &UIElement::id,
        "visible",    &UIElement::visible,
        "x",          &UIElement::x,
        "y",          &UIElement::y,
        "w",          &UIElement::w,
        "h",          &UIElement::h,
        "text",       &UIElement::text,
        "text_left",  &UIElement::text_left,
        "disabled",   &UIElement::disabled,
        "value",      &UIElement::value,
        "focused",    &UIElement::focused,
        "max_length", &UIElement::max_length,

        // Writable properties for callbacks — scripts write `btn.on_click = fn`
        // for consistency with the other field assignments.
        "on_click", sol::property(
            [](UIElement&) -> sol::object { return sol::nil; },
            [](UIElement& el, sol::function fn) {
                el.on_click = WrapVoid(std::move(fn), "on_click");
            }),
        "on_change", sol::property(
            [](UIElement&) -> sol::object { return sol::nil; },
            [](UIElement& el, sol::function fn) {
                el.on_change = WrapStr(std::move(fn), "on_change");
            }),

        // Child factories. Args are doubles so Lua float divisions (WIN_W/2) work.
        "add_frame",  [](UIElement& el, double x, double y, double w, double h) {
            return el.AddFrame(ToInt(x), ToInt(y), ToInt(w), ToInt(h));
        },
        "add_button", [](UIElement& el, const std::string& text,
                         double x, double y, double w, double h) {
            return el.AddButton(text, ToInt(x), ToInt(y), ToInt(w), ToInt(h));
        },
        "add_label",  [](UIElement& el, const std::string& text, double x, double y) {
            return el.AddLabel(text, ToInt(x), ToInt(y));
        },
        "add_input",  [](UIElement& el, const std::string& placeholder,
                         double x, double y, double w, double h) {
            return el.AddInput(placeholder, ToInt(x), ToInt(y), ToInt(w), ToInt(h));
        }
    );

    // ── UIScreen ──────────────────────────────────────────────────────────────
    // All factories forward to the root frame on the C++ side; the Lua-facing
    // surface stays the same (screen:add_button, screen:add_frame, etc).
    lua.new_usertype<UIScreen>("UIScreen",
        sol::no_constructor,
        "name",       &UIScreen::name,
        "add_frame",  [](UIScreen& s, double x, double y, double w, double h) {
            return s.AddFrame(ToInt(x), ToInt(y), ToInt(w), ToInt(h));
        },
        "add_button", [](UIScreen& s, const std::string& text,
                         double x, double y, double w, double h) {
            return s.AddButton(text, ToInt(x), ToInt(y), ToInt(w), ToInt(h));
        },
        "add_label",  [](UIScreen& s, const std::string& text, double x, double y) {
            return s.AddLabel(text, ToInt(x), ToInt(y));
        },
        "add_input",  [](UIScreen& s, const std::string& placeholder,
                         double x, double y, double w, double h) {
            return s.AddInput(placeholder, ToInt(x), ToInt(y), ToInt(w), ToInt(h));
        }
    );

    // ── engine.ui table ───────────────────────────────────────────────────────
    sol::table eng = lua["engine"];
    sol::table ui_tbl = eng.create_named("ui");

    ui_tbl["create_screen"] = [&ui](const std::string& name) {
        return ui.CreateScreen(name);
    };
    ui_tbl["show_screen"] = [&ui](std::shared_ptr<UIScreen> s) {
        ui.ShowScreen(std::move(s));
    };
    ui_tbl["pop_screen"] = [&ui]() {
        ui.PopScreen();
    };
    // Scripts call this on mouse-down / mouse-up handlers to suppress world
    // clicks when the pointer is over an interactive UI element on the top screen.
    ui_tbl["is_mouse_over"] = [&ui, &engine]() {
        return ui.IsPointOverUI(engine.GetInput().GetMouseX(),
                                engine.GetInput().GetMouseY());
    };
}
