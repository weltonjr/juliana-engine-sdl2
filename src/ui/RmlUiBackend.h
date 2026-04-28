#pragma once

#include <SDL.h>
#include <memory>
#include <string>
#include <vector>

namespace Rml { class Context; class SystemInterface; class FileInterface; }
class RenderInterface_SDL;

struct lua_State;

// RmlUiBackend — the entire engine-side UI system.
//
// Owns: Rml::Context, RenderInterface (SDL2), SystemInterface (SDL+EngineLog),
// FileInterface (package-aware). Registers RmlUi's official Lua plugin so game
// scripts use the `rmlui` global directly with no engine wrapper.
//
// Lifecycle: Init AFTER Lua state exists; UpdateAndRender() in the render path
// (NOT the fixed-tick sim path); Shutdown before SDL teardown.
class RmlUiBackend {
public:
    RmlUiBackend();
    ~RmlUiBackend();

    RmlUiBackend(const RmlUiBackend&) = delete;
    RmlUiBackend& operator=(const RmlUiBackend&) = delete;

    // Initialize RmlUi, install interfaces, register the Lua plugin, create context.
    // Must be called after sol2 / lua_State is created and before any Lua scripts
    // that load documents are run.
    bool Init(SDL_Window* window, SDL_Renderer* renderer, lua_State* L,
              int width, int height, float dpi_scale, bool enable_debugger);
    void Shutdown();

    // Per-frame: calls Context::Update() and Context::Render(). Drives animations
    // and event propagation off real frame time, so call from the render path —
    // not from the fixed-60Hz simulation tick.
    void UpdateAndRender();

    // Forward a single SDL event to RmlUi. Returns true if the event is still
    // propagating (game code may consume it), false if RmlUi handled it.
    bool ProcessEvent(SDL_Event& event);

    // Window resize / DPI change.
    void OnWindowResize(int w, int h);
    void SetDensityIndependentPixelRatio(float ratio);

    // Push/pop a base path the FileInterface uses to resolve non-absolute paths.
    // Top of stack wins. Leaves room for the map editor to preview a target
    // package's UI without losing its own resolution context.
    void PushPackageBase(const std::string& base_path);
    void PopPackageBase();
    const std::string& GetCurrentPackageBase() const;

    // Built-in debugger overlay (toggle with F8 in dev, controllable from Lua).
    void ToggleDebugger();
    void SetDebuggerVisible(bool visible);

    Rml::Context* GetContext() { return context_; }

private:
    class JulianaSystemInterface;
    class JulianaFileInterface;

    std::unique_ptr<RenderInterface_SDL>    render_iface_;
    std::unique_ptr<JulianaSystemInterface> system_iface_;
    std::unique_ptr<JulianaFileInterface>   file_iface_;

    Rml::Context* context_ = nullptr;
    SDL_Window*   window_  = nullptr;
    SDL_Renderer* renderer_= nullptr;
    bool initialised_      = false;
    bool debugger_attached_= false;
};
