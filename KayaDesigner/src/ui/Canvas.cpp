#include "ui/Canvas.h"

#include "document/WidgetNode.h"
#include "editor/Editor.h"

#include <algorithm>
#include <cmath>

namespace kaya::designer {

namespace {

ImU32 colorToU32(const Color &color)
{
    return ImGui::ColorConvertFloat4ToU32({ color.r, color.g, color.b, color.a });
}

Vec2i snap(Vec2i point, int gridSize)
{
    if (gridSize <= 1) {
        return point;
    }

    point.x = (point.x / gridSize) * gridSize;
    point.y = (point.y / gridSize) * gridSize;
    return point;
}

Recti snap(Recti rect, int gridSize)
{
    Vec2i point = snap(Vec2i { rect.x, rect.y }, gridSize);
    rect.x = point.x;
    rect.y = point.y;
    rect.width = std::max(gridSize, (rect.width / gridSize) * gridSize);
    rect.height = std::max(gridSize, (rect.height / gridSize) * gridSize);
    return rect;
}

}

void Canvas::draw(Editor &editor)
{
    handleShortcuts(editor);

    ImGui::Begin("Canvas");
    zoom_ = editor.zoom;

    ImGui::Text("Pan: middle mouse | Zoom: Ctrl + wheel | Drop widgets from palette");
    ImGui::Separator();

    const ImVec2 available = ImGui::GetContentRegionAvail();
    origin_ = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("canvas_area", available,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);

    const bool hovered = ImGui::IsItemHovered();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(origin_, { origin_.x + available.x, origin_.y + available.y },
        IM_COL32(26, 30, 38, 255));

    if (editor.gridVisible) {
        drawGrid(drawList, origin_, available, editor.gridSize, editor.zoom);
    }

    drawList->PushClipRect(origin_, { origin_.x + available.x, origin_.y + available.y }, true);
    for (const auto &node : editor.document().widgets()) {
        drawWidget(drawList, node, node.id == editor.selectedWidget());
    }
    drawList->PopClipRect();

    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        pan_.x += delta.x;
        pan_.y += delta.y;
    }

    if (hovered && ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0.0f) {
        editor.zoom = std::clamp(editor.zoom + ImGui::GetIO().MouseWheel * 0.08f, 0.35f, 3.0f);
    }

    if (hovered && ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("KAYA_WIDGET_TYPE")) {
            auto type = static_cast<WidgetType>(*static_cast<const int *>(payload->Data));
            Vec2i position = screenToCanvas(ImGui::GetMousePos());
            if (editor.snapToGrid) {
                position = snap(position, editor.gridSize);
            }
            editor.addWidget(type, position);
        }
        ImGui::EndDragDropTarget();
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        Vec2i point = screenToCanvas(ImGui::GetMousePos());
        WidgetId hit = editor.document().hitTest(point);
        editor.selectWidget(hit);
        activeWidget_ = hit;
        moving_ = false;
        resizing_ = false;

        if (auto *node = editor.document().find(hit)) {
            Recti handle { node->rect.x + node->rect.width - 10, node->rect.y + node->rect.height - 10, 10, 10 };
            editor.pushUndoPoint();
            dragStartMouse_ = point;
            dragStartRect_ = node->rect;
            resizing_ = handle.contains(point);
            moving_ = !resizing_;
        }
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        moving_ = false;
        resizing_ = false;
        activeWidget_ = InvalidWidgetId;
    }

    if ((moving_ || resizing_) && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && activeWidget_ != InvalidWidgetId) {
        if (auto *node = editor.document().find(activeWidget_)) {
            Vec2i point = screenToCanvas(ImGui::GetMousePos());
            int dx = point.x - dragStartMouse_.x;
            int dy = point.y - dragStartMouse_.y;

            if (moving_) {
                node->rect.x = dragStartRect_.x + dx;
                node->rect.y = dragStartRect_.y + dy;
            } else {
                node->rect.width = std::max(16, dragStartRect_.width + dx);
                node->rect.height = std::max(16, dragStartRect_.height + dy);
            }

            if (editor.snapToGrid) {
                node->rect = snap(node->rect, editor.gridSize);
            }
        }
    }

    ImGui::End();
}

Vec2i Canvas::screenToCanvas(ImVec2 point) const
{
    return {
        static_cast<int>((point.x - origin_.x - pan_.x) / zoom_),
        static_cast<int>((point.y - origin_.y - pan_.y) / zoom_)
    };
}

ImVec2 Canvas::canvasToScreen(Vec2i point) const
{
    return {
        origin_.x + pan_.x + point.x * zoom_,
        origin_.y + pan_.y + point.y * zoom_
    };
}

void Canvas::drawGrid(ImDrawList *drawList, ImVec2 origin, ImVec2 size, int gridSize, float zoom) const
{
    float step = std::max(4.0f, gridSize * zoom);
    ImU32 color = IM_COL32(58, 64, 76, 120);

    for (float x = std::fmod(pan_.x, step); x < size.x; x += step) {
        drawList->AddLine({ origin.x + x, origin.y }, { origin.x + x, origin.y + size.y }, color);
    }
    for (float y = std::fmod(pan_.y, step); y < size.y; y += step) {
        drawList->AddLine({ origin.x, origin.y + y }, { origin.x + size.x, origin.y + y }, color);
    }
}

void Canvas::drawWidget(ImDrawList *drawList, const WidgetNode &node, bool selected) const
{
    ImVec2 min = canvasToScreen({ node.rect.x, node.rect.y });
    ImVec2 max = canvasToScreen({ node.rect.x + node.rect.width, node.rect.y + node.rect.height });
    ImU32 fill = colorToU32(node.color);
    ImU32 border = selected ? IM_COL32(255, 210, 90, 255) : IM_COL32(160, 172, 190, 255);
    ImU32 text = IM_COL32(245, 248, 252, 255);

    switch (node.type) {
    case WidgetType::Window:
        fill = IM_COL32(44, 50, 62, 255);
        drawList->AddRectFilled(min, max, fill, 6.0f);
        drawList->AddRectFilled(min, { max.x, min.y + 32.0f }, IM_COL32(28, 34, 48, 255), 6.0f);
        break;
    case WidgetType::Label:
        fill = IM_COL32(0, 0, 0, 0);
        break;
    case WidgetType::TextBox:
        fill = IM_COL32(18, 22, 30, 255);
        break;
    default:
        break;
    }

    if (node.type != WidgetType::Label) {
        drawList->AddRectFilled(min, max, fill, 5.0f);
        drawList->AddRect(min, max, border, 5.0f, 0, selected ? 2.0f : 1.0f);
    }

    drawList->AddText({ min.x + 8.0f, min.y + 8.0f }, text, node.text.empty() ? node.objectName.c_str() : node.text.c_str());

    if (selected) {
        drawList->AddRectFilled({ max.x - 10.0f, max.y - 10.0f }, max, IM_COL32(255, 210, 90, 255));
    }
}

void Canvas::handleShortcuts(Editor &editor)
{
    ImGuiIO &io = ImGui::GetIO();
    if (io.WantTextInput) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        editor.deleteSelection();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
        editor.copySelection();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
        editor.pasteClipboard();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        editor.undo();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        editor.redo();
    }
}

}
