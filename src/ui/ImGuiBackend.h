#pragma once

#include <SDL.h>
#include <functional>
#include <vector>

// ImGuiBackend — engine-side glue for Dear ImGui (debug overlays only).
//
// Game UI lives in RmlUi. ImGui is reserved for dev tools: FPS, log console,
// entity inspector, and any Lua-driven debug panels game packages register.
//
// Lifecycle: Init AFTER RmlUi (so debug overlays render on top of game UI);
// per-frame BeginFrame() / EndFrame() bracket the moment Lua scripts emit
// ImGui calls; Shutdown before SDL teardown.
class ImGuiBackend {
public:
    ImGuiBackend();
    ~ImGuiBackend();

    ImGuiBackend(const ImGuiBackend&) = delete;
    ImGuiBackend& operator=(const ImGuiBackend&) = delete;

    bool Init(SDL_Window* window, SDL_Renderer* renderer);
    void Shutdown();

    // BeginFrame/EndFrame bracket the per-frame ImGui call site. Engine drives
    // the order: BeginFrame -> render-time callbacks (DebugUI, LogConsole, Lua
    // panels) -> EndFrame. EndFrame submits draw lists to the SDL renderer.
    void BeginFrame();
    void EndFrame();

    // Forward an SDL event to ImGui's platform backend. Wired into the same
    // listener pipeline RmlUi uses, so all UI sees raw SDL input.
    void ProcessEvent(SDL_Event& event);

    // Register a callback fired between BeginFrame and EndFrame. Used by both
    // C++ debug panels (DebugUI, LogConsole) and the Lua engine.on_render hook.
    using RenderCallback = std::function<void()>;
    int  AddRenderCallback(RenderCallback cb);
    void RemoveRenderCallback(int id);

    // Query: does ImGui want to capture the mouse / keyboard right now?
    // Game input should respect this when an ImGui window is hovered/focused.
    bool WantCaptureMouse() const;
    bool WantCaptureKeyboard() const;

private:
    SDL_Window*   window_   = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool initialised_       = false;

    struct CallbackSlot { int id; RenderCallback cb; };
    std::vector<CallbackSlot> callbacks_;
    int next_cb_id_ = 1;
};
