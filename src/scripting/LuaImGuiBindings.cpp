#include "scripting/LuaImGuiBindings.h"

#include <sol/sol.hpp>
#include <imgui.h>

#include <string>
#include <tuple>
#include <vector>

namespace {

// Most ImGui calls return state directly. For widgets that need to mutate a
// reference (SliderFloat, Checkbox, InputText), we return a tuple so Lua can
// pick up the new value alongside the "did it change" boolean.

inline bool Begin1(const std::string& name) { return ImGui::Begin(name.c_str()); }
inline std::tuple<bool,bool> Begin2(const std::string& name, bool open) {
    bool drawn = ImGui::Begin(name.c_str(), &open);
    return {open, drawn};
}
inline std::tuple<bool,bool> Begin3(const std::string& name, bool open, int flags) {
    bool drawn = ImGui::Begin(name.c_str(), &open, static_cast<ImGuiWindowFlags>(flags));
    return {open, drawn};
}
inline void End() { ImGui::End(); }

inline void Text(const std::string& s) { ImGui::TextUnformatted(s.c_str()); }
inline void TextColored(float r, float g, float b, float a, const std::string& s) {
    ImGui::TextColored(ImVec4(r,g,b,a), "%s", s.c_str());
}
inline void TextDisabled(const std::string& s) { ImGui::TextDisabled("%s", s.c_str()); }
inline void TextWrapped (const std::string& s) { ImGui::TextWrapped ("%s", s.c_str()); }
inline void BulletText  (const std::string& s) { ImGui::BulletText  ("%s", s.c_str()); }
inline void LabelText   (const std::string& label, const std::string& v) { ImGui::LabelText(label.c_str(), "%s", v.c_str()); }

inline void Separator()      { ImGui::Separator(); }
inline void Spacing()        { ImGui::Spacing(); }
inline void NewLine()        { ImGui::NewLine(); }
inline void SameLine0()      { ImGui::SameLine(); }
inline void SameLine1(float offset) { ImGui::SameLine(offset); }
inline void Indent0()        { ImGui::Indent(); }
inline void Indent1(float w) { ImGui::Indent(w); }
inline void Unindent0()      { ImGui::Unindent(); }
inline void Unindent1(float w){ ImGui::Unindent(w); }

inline bool Button1(const std::string& label) { return ImGui::Button(label.c_str()); }
inline bool Button2(const std::string& label, float w, float h) { return ImGui::Button(label.c_str(), ImVec2(w,h)); }
inline bool SmallButton(const std::string& label) { return ImGui::SmallButton(label.c_str()); }

inline std::tuple<bool,bool> Checkbox(const std::string& label, bool v) {
    bool changed = ImGui::Checkbox(label.c_str(), &v);
    return {v, changed};
}

inline std::tuple<float,bool> SliderFloat(const std::string& label, float v, float vmin, float vmax) {
    bool changed = ImGui::SliderFloat(label.c_str(), &v, vmin, vmax);
    return {v, changed};
}
inline std::tuple<int,bool> SliderInt(const std::string& label, int v, int vmin, int vmax) {
    bool changed = ImGui::SliderInt(label.c_str(), &v, vmin, vmax);
    return {v, changed};
}
inline std::tuple<float,bool> InputFloat(const std::string& label, float v) {
    bool changed = ImGui::InputFloat(label.c_str(), &v);
    return {v, changed};
}
inline std::tuple<int,bool> InputInt(const std::string& label, int v) {
    bool changed = ImGui::InputInt(label.c_str(), &v);
    return {v, changed};
}
inline std::tuple<std::string,bool> InputText(const std::string& label, std::string v) {
    v.resize(std::max<size_t>(v.size() + 64, 256), '\0');
    bool changed = ImGui::InputText(label.c_str(), v.data(), v.size());
    v.resize(std::strlen(v.c_str()));
    return {v, changed};
}

inline std::tuple<int,bool> Combo(const std::string& label, int current, sol::table items_tbl) {
    std::vector<std::string>  items;
    std::vector<const char*>  ptrs;
    for (auto& kv : items_tbl) {
        if (kv.second.is<std::string>())
            items.push_back(kv.second.as<std::string>());
    }
    ptrs.reserve(items.size());
    for (auto& s : items) ptrs.push_back(s.c_str());
    bool changed = ImGui::Combo(label.c_str(), &current,
                                ptrs.empty() ? nullptr : ptrs.data(),
                                static_cast<int>(ptrs.size()));
    return {current, changed};
}

// Trees / collapsing headers
inline bool TreeNode(const std::string& label) { return ImGui::TreeNode(label.c_str()); }
inline void TreePop() { ImGui::TreePop(); }
inline bool CollapsingHeader(const std::string& label) { return ImGui::CollapsingHeader(label.c_str()); }

// Layout
inline void SetNextWindowPos (float x, float y) { ImGui::SetNextWindowPos (ImVec2(x,y)); }
inline void SetNextWindowSize(float w, float h) { ImGui::SetNextWindowSize(ImVec2(w,h)); }

// Queries the game side uses to gate input
inline bool WantCaptureMouse()    { return ImGui::GetIO().WantCaptureMouse;    }
inline bool WantCaptureKeyboard() { return ImGui::GetIO().WantCaptureKeyboard; }

inline std::tuple<float,float> GetMousePos() { ImVec2 p = ImGui::GetMousePos(); return {p.x, p.y}; }
inline float GetFrameRate() { return ImGui::GetIO().Framerate; }

} // namespace

void RegisterLuaImGuiBindings(sol::state& lua) {
    sol::table tbl = lua.create_named_table("ImGui");

    tbl.set_function("Begin", sol::overload(
        &Begin1,
        &Begin2,
        &Begin3
    ));
    tbl.set_function("End", &End);

    tbl.set_function("Text",         &Text);
    tbl.set_function("TextColored",  &TextColored);
    tbl.set_function("TextDisabled", &TextDisabled);
    tbl.set_function("TextWrapped",  &TextWrapped);
    tbl.set_function("BulletText",   &BulletText);
    tbl.set_function("LabelText",    &LabelText);

    tbl.set_function("Separator", &Separator);
    tbl.set_function("Spacing",   &Spacing);
    tbl.set_function("NewLine",   &NewLine);
    tbl.set_function("SameLine",  sol::overload(&SameLine0, &SameLine1));
    tbl.set_function("Indent",    sol::overload(&Indent0,   &Indent1));
    tbl.set_function("Unindent",  sol::overload(&Unindent0, &Unindent1));

    tbl.set_function("Button",      sol::overload(&Button1, &Button2));
    tbl.set_function("SmallButton", &SmallButton);
    tbl.set_function("Checkbox",    &Checkbox);
    tbl.set_function("SliderFloat", &SliderFloat);
    tbl.set_function("SliderInt",   &SliderInt);
    tbl.set_function("InputFloat",  &InputFloat);
    tbl.set_function("InputInt",    &InputInt);
    tbl.set_function("InputText",   &InputText);
    tbl.set_function("Combo",       &Combo);

    tbl.set_function("TreeNode",         &TreeNode);
    tbl.set_function("TreePop",          &TreePop);
    tbl.set_function("CollapsingHeader", &CollapsingHeader);

    tbl.set_function("SetNextWindowPos",  &SetNextWindowPos);
    tbl.set_function("SetNextWindowSize", &SetNextWindowSize);

    tbl.set_function("WantCaptureMouse",    &WantCaptureMouse);
    tbl.set_function("WantCaptureKeyboard", &WantCaptureKeyboard);
    tbl.set_function("GetMousePos",         &GetMousePos);
    tbl.set_function("GetFrameRate",        &GetFrameRate);

    // Common window flag constants — exposed as integers so Lua can OR them.
    tbl["WindowFlags_NoTitleBar"]      = ImGuiWindowFlags_NoTitleBar;
    tbl["WindowFlags_NoResize"]        = ImGuiWindowFlags_NoResize;
    tbl["WindowFlags_NoMove"]          = ImGuiWindowFlags_NoMove;
    tbl["WindowFlags_NoScrollbar"]     = ImGuiWindowFlags_NoScrollbar;
    tbl["WindowFlags_NoCollapse"]      = ImGuiWindowFlags_NoCollapse;
    tbl["WindowFlags_AlwaysAutoResize"]= ImGuiWindowFlags_AlwaysAutoResize;
    tbl["WindowFlags_NoBackground"]    = ImGuiWindowFlags_NoBackground;
    tbl["WindowFlags_NoSavedSettings"] = ImGuiWindowFlags_NoSavedSettings;
}
