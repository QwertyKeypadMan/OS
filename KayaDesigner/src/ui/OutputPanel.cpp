#include "ui/OutputPanel.h"

#include "editor/Editor.h"
#include "imgui.h"

namespace kaya::designer {

void OutputPanel::draw(Editor &editor)
{
    ImGui::Begin("Output / Log");
    if (ImGui::Button("Clear")) {
        editor.logger().clear();
    }
    ImGui::Separator();
    for (const auto &message : editor.logger().messages()) {
        ImGui::TextUnformatted(message.c_str());
    }
    ImGui::End();
}

}

