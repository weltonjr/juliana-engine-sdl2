#pragma once

// In-game log console rendered with Dear ImGui. Reads from EngineLog and
// renders the rolling message buffer in a scrolling window.
//
// Toggle visibility from outside via Engine::SetLogConsoleVisible. The owner
// gates this draw call so the window only appears when requested.
class LogConsole {
public:
    LogConsole() = default;
    ~LogConsole() = default;

    LogConsole(const LogConsole&) = delete;
    LogConsole& operator=(const LogConsole&) = delete;

    // Emit the ImGui window. Call between ImGui::NewFrame() and ImGui::Render().
    void DrawImGui();

private:
    bool autoscroll_ = true;
    int  last_count_ = 0;
};
