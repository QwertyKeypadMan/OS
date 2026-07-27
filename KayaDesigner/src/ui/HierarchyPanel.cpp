#include "ui/HierarchyPanel.h"

#include "document/WidgetNode.h"
#include "editor/Editor.h"
#include "imgui.h"

#include <cstdint>

namespace kaya::designer {

void HierarchyPanel::draw(Editor &editor)
{
    ImGui::Begin("Hierarchy");

    for (const auto &node : editor.document().widgets()) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
        if (node.id == editor.selectedWidget()) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (node.children.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }

        bool open = ImGui::TreeNodeEx(reinterpret_cast<void *>(static_cast<std::uintptr_t>(node.id)),
            flags, "%s (%s)", node.objectName.c_str(), toString(node.type).c_str());
        if (ImGui::IsItemClicked()) {
            editor.selectWidget(node.id);
        }
        if (open) {
            ImGui::TreePop();
        }
    }

    ImGui::End();
}

}
