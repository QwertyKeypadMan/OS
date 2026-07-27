#include "ui/PropertyGrid.h"

#include "document/WidgetNode.h"
#include "editor/Editor.h"
#include "imgui.h"

#include <array>
#include <cstring>

namespace kaya::designer {

namespace {

template <std::size_t N>
void copyText(std::array<char, N> &buffer, const std::string &text)
{
    std::strncpy(buffer.data(), text.c_str(), N - 1);
    buffer[N - 1] = '\0';
}

}

void PropertyGrid::draw(Editor &editor)
{
    ImGui::Begin("Properties");

    WidgetNode *node = editor.selectedNode();
    if (!node) {
        ImGui::TextUnformatted("No widget selected.");
        ImGui::End();
        return;
    }

    ImGui::Text("Type: %s", toString(node->type).c_str());
    ImGui::Separator();

    static WidgetId activeId = InvalidWidgetId;
    static std::array<char, 128> idBuffer {};
    static std::array<char, 256> textBuffer {};
    static std::array<char, 128> fontBuffer {};

    if (activeId != node->id) {
        activeId = node->id;
        copyText(idBuffer, node->objectName);
        copyText(textBuffer, node->text);
        copyText(fontBuffer, node->font);
    }

    if (ImGui::InputText("ID", idBuffer.data(), idBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
        editor.pushUndoPoint();
        node->objectName = idBuffer.data();
    }

    if (ImGui::DragInt("X", &node->rect.x, 1.0f)) {}
    if (ImGui::DragInt("Y", &node->rect.y, 1.0f)) {}
    if (ImGui::DragInt("Width", &node->rect.width, 1.0f, 1, 4096)) {}
    if (ImGui::DragInt("Height", &node->rect.height, 1.0f, 1, 4096)) {}

    if (ImGui::InputText("Text", textBuffer.data(), textBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
        editor.pushUndoPoint();
        node->text = textBuffer.data();
    }

    if (ImGui::Checkbox("Visible", &node->visible)) {
        editor.pushUndoPoint();
    }
    if (ImGui::Checkbox("Enabled", &node->enabled)) {
        editor.pushUndoPoint();
    }

    if (ImGui::InputText("Font", fontBuffer.data(), fontBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
        editor.pushUndoPoint();
        node->font = fontBuffer.data();
    }

    float color[4] = { node->color.r, node->color.g, node->color.b, node->color.a };
    if (ImGui::ColorEdit4("Color", color)) {
        node->color = { color[0], color[1], color[2], color[3] };
    }

    const char *anchors[] = { "None", "Left", "Right", "Top", "Bottom", "Fill" };
    int anchor = static_cast<int>(node->anchor);
    if (ImGui::Combo("Anchor", &anchor, anchors, IM_ARRAYSIZE(anchors))) {
        node->anchor = static_cast<Anchor>(anchor);
    }

    const char *docks[] = { "None", "Left", "Right", "Top", "Bottom", "Fill" };
    int dock = static_cast<int>(node->dock);
    if (ImGui::Combo("Dock", &dock, docks, IM_ARRAYSIZE(docks))) {
        node->dock = static_cast<DockMode>(dock);
    }

    ImGui::End();
}

}

