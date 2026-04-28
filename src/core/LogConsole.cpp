#include "core/LogConsole.h"
#include "core/EngineLog.h"

#include <imgui.h>

void LogConsole::DrawImGui() {
    ImGui::SetNextWindowPos (ImVec2(8, 540), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(700, 170), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Log")) {
        ImGui::End();
        return;
    }

    if (ImGui::SmallButton("Clear")) EngineLog::Clear();
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoscroll_);
    ImGui::Separator();

    ImGui::BeginChild("##log-scroll", ImVec2(0, 0), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    const auto& msgs = EngineLog::GetMessages();
    for (const auto& m : msgs) {
        ImGui::TextUnformatted(m.c_str());
    }
    if (autoscroll_ && static_cast<int>(msgs.size()) != last_count_) {
        ImGui::SetScrollHereY(1.0f);
    }
    last_count_ = static_cast<int>(msgs.size());
    ImGui::EndChild();

    ImGui::End();
}
