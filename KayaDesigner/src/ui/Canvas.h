#pragma once

#include "core/Types.h"

#include "imgui.h"

namespace kaya::designer {

class Editor;
struct WidgetNode;

class Canvas {
public:
    void draw(Editor &editor);

private:
    [[nodiscard]] Vec2i screenToCanvas(ImVec2 point) const;
    [[nodiscard]] ImVec2 canvasToScreen(Vec2i point) const;
    void drawGrid(ImDrawList *drawList, ImVec2 origin, ImVec2 size, int gridSize, float zoom) const;
    void drawWidget(ImDrawList *drawList, const WidgetNode &node, bool selected) const;
    void handleShortcuts(Editor &editor);

    ImVec2 origin_ = {};
    ImVec2 pan_ = { 24.0f, 24.0f };
    float zoom_ = 1.0f;
    bool moving_ = false;
    bool resizing_ = false;
    WidgetId activeWidget_ = InvalidWidgetId;
    Vec2i dragStartMouse_ = {};
    Recti dragStartRect_ = {};
};

}
