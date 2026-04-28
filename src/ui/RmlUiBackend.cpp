#include "ui/RmlUiBackend.h"
#include "ui/backend/RmlUi_Renderer_SDL.h"
#include "ui/backend/RmlUi_Platform_SDL.h"
#include "core/EngineLog.h"

#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>
#include <RmlUi/Lua.h>

#include <cstdio>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

// ─── SystemInterface ─────────────────────────────────────────────────────────
// Extends the upstream SDL platform interface with EngineLog forwarding.

class RmlUiBackend::JulianaSystemInterface : public SystemInterface_SDL {
public:
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
        const char* prefix = "RmlUi";
        switch (type) {
            case Rml::Log::LT_ERROR:   prefix = "RmlUi ERROR";   break;
            case Rml::Log::LT_WARNING: prefix = "RmlUi WARN";    break;
            case Rml::Log::LT_INFO:    prefix = "RmlUi INFO";    break;
            case Rml::Log::LT_DEBUG:   prefix = "RmlUi DEBUG";   break;
            case Rml::Log::LT_ASSERT:  prefix = "RmlUi ASSERT";  break;
            default: break;
        }
        char buf[1024];
        std::snprintf(buf, sizeof(buf), "[%s] %s", prefix, message.c_str());
        EngineLog::Log(buf);
        return SystemInterface_SDL::LogMessage(type, message);
    }
};

// ─── FileInterface ───────────────────────────────────────────────────────────
// Resolves non-absolute paths against a stack of package roots. RmlUi already
// resolves <link href="..."> and url(...) relative to the document URL, so the
// stack mainly handles the initial LoadDocument call from Lua.

class RmlUiBackend::JulianaFileInterface : public Rml::FileInterface {
public:
    void Push(const std::string& base) { stack_.push_back(base); }
    void Pop()                         { if (!stack_.empty()) stack_.pop_back(); }
    const std::string& Top() const {
        static const std::string empty;
        return stack_.empty() ? empty : stack_.back();
    }

    Rml::FileHandle Open(const Rml::String& path) override {
        FILE* fp = std::fopen(path.c_str(), "rb");
        if (!fp && !stack_.empty() && !path.empty() && path[0] != '/') {
            // Relative path — try resolving against current package root.
            fs::path resolved = fs::path(stack_.back()) / path;
            fp = std::fopen(resolved.string().c_str(), "rb");
        }
        return reinterpret_cast<Rml::FileHandle>(fp);
    }

    void Close(Rml::FileHandle file) override {
        if (file) std::fclose(reinterpret_cast<FILE*>(file));
    }

    size_t Read(void* buffer, size_t size, Rml::FileHandle file) override {
        return std::fread(buffer, 1, size, reinterpret_cast<FILE*>(file));
    }

    bool Seek(Rml::FileHandle file, long offset, int origin) override {
        return std::fseek(reinterpret_cast<FILE*>(file), offset, origin) == 0;
    }

    size_t Tell(Rml::FileHandle file) override {
        return static_cast<size_t>(std::ftell(reinterpret_cast<FILE*>(file)));
    }

private:
    std::vector<std::string> stack_;
};

// ─── RmlUiBackend ────────────────────────────────────────────────────────────

RmlUiBackend::RmlUiBackend() = default;
RmlUiBackend::~RmlUiBackend() { Shutdown(); }

bool RmlUiBackend::Init(SDL_Window* window, SDL_Renderer* renderer, lua_State* L,
                        int width, int height, float dpi_scale, bool enable_debugger) {
    if (initialised_) return true;

    window_   = window;
    renderer_ = renderer;

    // 1) Create interfaces BEFORE Rml::Initialise.
    render_iface_ = std::make_unique<RenderInterface_SDL>(renderer);
    system_iface_ = std::make_unique<JulianaSystemInterface>();
    file_iface_   = std::make_unique<JulianaFileInterface>();
    system_iface_->SetWindow(window);

    Rml::SetSystemInterface(system_iface_.get());
    Rml::SetRenderInterface(render_iface_.get());
    Rml::SetFileInterface(file_iface_.get());

    if (!Rml::Initialise()) {
        EngineLog::Log("[RmlUi] Rml::Initialise() failed");
        return false;
    }

    // 2) Lua plugin AFTER Rml::Initialise, BEFORE LoadDocument calls from scripts.
    if (L) Rml::Lua::Initialise(L);

    // 3) Context creation.
    context_ = Rml::CreateContext("main", Rml::Vector2i(width, height));
    if (!context_) {
        EngineLog::Log("[RmlUi] CreateContext() failed");
        return false;
    }
    if (dpi_scale > 0.0f && dpi_scale != 1.0f) {
        context_->SetDensityIndependentPixelRatio(dpi_scale);
    }

    // 4) Optional debugger overlay.
    if (enable_debugger) {
        Rml::Debugger::Initialise(context_);
        debugger_attached_ = true;
    }

    initialised_ = true;
    EngineLog::Log("[RmlUi] backend initialised");
    return true;
}

void RmlUiBackend::Shutdown() {
    if (!initialised_) return;

    context_ = nullptr;
    Rml::Shutdown();

    file_iface_.reset();
    system_iface_.reset();
    render_iface_.reset();

    initialised_      = false;
    debugger_attached_= false;
}

void RmlUiBackend::UpdateAndRender() {
    if (!context_) return;
    context_->Update();
    render_iface_->BeginFrame();
    context_->Render();
    render_iface_->EndFrame();
}

bool RmlUiBackend::ProcessEvent(SDL_Event& event) {
    if (!context_) return true;
    return RmlSDL::InputEventHandler(context_, window_, event);
}

void RmlUiBackend::OnWindowResize(int w, int h) {
    if (context_) context_->SetDimensions(Rml::Vector2i(w, h));
}

void RmlUiBackend::SetDensityIndependentPixelRatio(float ratio) {
    if (context_ && ratio > 0.0f) context_->SetDensityIndependentPixelRatio(ratio);
}

void RmlUiBackend::PushPackageBase(const std::string& base_path) {
    if (file_iface_) file_iface_->Push(base_path);
}

void RmlUiBackend::PopPackageBase() {
    if (file_iface_) file_iface_->Pop();
}

const std::string& RmlUiBackend::GetCurrentPackageBase() const {
    static const std::string empty;
    return file_iface_ ? file_iface_->Top() : empty;
}

void RmlUiBackend::ToggleDebugger() {
    if (!debugger_attached_) return;
    Rml::Debugger::SetVisible(!Rml::Debugger::IsVisible());
}

void RmlUiBackend::SetDebuggerVisible(bool visible) {
    if (!debugger_attached_) return;
    Rml::Debugger::SetVisible(visible);
}
