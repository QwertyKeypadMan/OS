#include "ui/MainMenuBar.h"

#include "editor/Editor.h"
#include "imgui.h"

namespace kaya::designer {

void MainMenuBar::draw(Editor &editor)
{
    static char exportPath[260] = "src/generated/kaya_designer_ui.c";

    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New")) {
            editor.pushUndoPoint();
            editor.document().createDefault(editor.registry());
            editor.selectWidget(InvalidWidgetId);
            editor.logger().info("New designer document.");
        }
        ImGui::Separator();
        ImGui::InputText("Export path", exportPath, sizeof(exportPath));
        if (ImGui::MenuItem("Export C")) {
            editor.exportC(exportPath);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
            editor.undo();
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
            editor.redo();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Copy", "Ctrl+C")) {
            editor.copySelection();
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V")) {
            editor.pasteClipboard();
        }
        if (ImGui::MenuItem("Delete", "Del")) {
            editor.deleteSelection();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Grid", nullptr, &editor.gridVisible);
        ImGui::MenuItem("Snap to Grid", nullptr, &editor.snapToGrid);
        ImGui::SliderFloat("Zoom", &editor.zoom, 0.35f, 3.0f, "%.2fx");
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

}
