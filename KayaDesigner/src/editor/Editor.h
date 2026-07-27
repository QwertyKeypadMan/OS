#pragma once

#include "core/Logger.h"
#include "document/GuiDocument.h"
#include "document/WidgetRegistry.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace kaya::designer {

class Editor {
public:
    void initialize();
    void draw();

    GuiDocument &document() { return document_; }
    const GuiDocument &document() const { return document_; }
    WidgetRegistry &registry() { return registry_; }
    Logger &logger() { return logger_; }

    [[nodiscard]] WidgetId selectedWidget() const { return selectedWidget_; }
    void selectWidget(WidgetId id);
    WidgetNode *selectedNode();

    void addWidget(WidgetType type, Vec2i position);
    void deleteSelection();
    void copySelection();
    void pasteClipboard();
    void exportC(const std::filesystem::path &path);

    void pushUndoPoint();
    void undo();
    void redo();

    bool gridVisible = true;
    bool snapToGrid = true;
    int gridSize = 16;
    float zoom = 1.0f;

private:
    WidgetRegistry registry_;
    GuiDocument document_;
    Logger logger_;
    WidgetId selectedWidget_ = InvalidWidgetId;
    std::optional<WidgetNode> clipboard_;
    std::vector<GuiDocument> undoStack_;
    std::vector<GuiDocument> redoStack_;
};

}

