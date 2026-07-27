#include "ui/WidgetPalette.h"

#include "document/WidgetRegistry.h"
#include "editor/Editor.h"
#include "imgui.h"

namespace kaya::designer {

void WidgetPalette::draw(Editor &editor)
{
    ImGui::Begin("Widget Palette");
    ImGui::TextUnformatted("Drag widgets to the canvas.");
    ImGui::Separator();

    for (const auto &definition : editor.registry().definitions()) {
        ImGui::Selectable(definition.displayName.c_str());
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            auto type = static_cast<int>(definition.type);
            ImGui::SetDragDropPayload("KAYA_WIDGET_TYPE", &type, sizeof(type));
            ImGui::TextUnformatted(definition.displayName.c_str());
            ImGui::EndDragDropSource();
        }
    }

    ImGui::End();
}

}

