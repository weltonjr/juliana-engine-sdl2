#include "ui/ImGuiBackend.h"
#include "core/EngineLog.h"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

ImGuiBackend::ImGuiBackend() = default;

ImGuiBackend::~ImGuiBackend() { Shutdown(); }

bool ImGuiBackend::Init(SDL_Window* window, SDL_Renderer* renderer) {
    if (initialised_) return true;

    window_   = window;
    renderer_ = renderer;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // no imgui.ini side-effect file
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL2_InitForSDLRenderer(window, renderer)) {
        EngineLog::Log("[ImGui] ImGui_ImplSDL2_InitForSDLRenderer failed");
        return false;
    }
    if (!ImGui_ImplSDLRenderer2_Init(renderer)) {
        EngineLog::Log("[ImGui] ImGui_ImplSDLRenderer2_Init failed");
        return false;
    }

    initialised_ = true;
    EngineLog::Log("[ImGui] backend initialised");
    return true;
}

void ImGuiBackend::Shutdown() {
    if (!initialised_) return;
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    initialised_ = false;
    callbacks_.clear();
}

void ImGuiBackend::BeginFrame() {
    if (!initialised_) return;
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    for (auto& slot : callbacks_) {
        if (slot.cb) slot.cb();
    }
}

void ImGuiBackend::EndFrame() {
    if (!initialised_) return;
    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer_);
    (void)renderer_;  // silences unused warnings on older backend signatures
}

void ImGuiBackend::ProcessEvent(SDL_Event& event) {
    if (!initialised_) return;
    ImGui_ImplSDL2_ProcessEvent(&event);
}

int ImGuiBackend::AddRenderCallback(RenderCallback cb) {
    int id = next_cb_id_++;
    callbacks_.push_back({id, std::move(cb)});
    return id;
}

void ImGuiBackend::RemoveRenderCallback(int id) {
    for (auto it = callbacks_.begin(); it != callbacks_.end(); ++it) {
        if (it->id == id) { callbacks_.erase(it); return; }
    }
}

bool ImGuiBackend::WantCaptureMouse() const {
    if (!initialised_) return false;
    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiBackend::WantCaptureKeyboard() const {
    if (!initialised_) return false;
    return ImGui::GetIO().WantCaptureKeyboard;
}
