#include "ui/Toolbar.h"

#include "editor/Editor.h"
#include "imgui.h"

namespace kaya::designer {

void Toolbar::draw(Editor &editor)
{
    ImGui::Begin("Toolbar", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (ImGui::Button("Export C")) {
        editor.exportC("src/generated/kaya_designer_ui.c");
    }
    ImGui::SameLine();
    if (ImGui::Button("Undo")) {
        editor.undo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Redo")) {
        editor.redo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy")) {
        editor.copySelection();
    }
    ImGui::SameLine();
    if (ImGui::Button("Paste")) {
        editor.pasteClipboard();
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        editor.deleteSelection();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Grid", &editor.gridVisible);
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &editor.snapToGrid);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::SliderFloat("Zoom", &editor.zoom, 0.35f, 3.0f, "%.2fx");

    ImGui::End();
}

}
