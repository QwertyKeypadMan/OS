#include "editor/Editor.h"

#include "codegen/CCodeGenerator.h"
#include "ui/Canvas.h"
#include "ui/HierarchyPanel.h"
#include "ui/MainMenuBar.h"
#include "ui/OutputPanel.h"
#include "ui/PropertyGrid.h"
#include "ui/Toolbar.h"
#include "ui/WidgetPalette.h"

#include "imgui.h"

#include <filesystem>
#include <fstream>
#include <utility>

namespace kaya::designer {

void Editor::initialize()
{
    document_.createDefault(registry_);
    logger_.info("Kaya Designer initialized.");
}

void Editor::draw()
{
    static MainMenuBar menu;
    static Toolbar toolbar;
    static WidgetPalette palette;
    static Canvas canvas;
    static PropertyGrid properties;
    static HierarchyPanel hierarchy;
    static OutputPanel output;

    menu.draw(*this);
    toolbar.draw(*this);

    palette.draw(*this);
    canvas.draw(*this);
    properties.draw(*this);
    hierarchy.draw(*this);
    output.draw(*this);
}

void Editor::selectWidget(WidgetId id)
{
    selectedWidget_ = id;
}

WidgetNode *Editor::selectedNode()
{
    return document_.find(selectedWidget_);
}

void Editor::addWidget(WidgetType type, Vec2i position)
{
    pushUndoPoint();
    WidgetId parent = document_.rootWindow() ? document_.rootWindow()->id : InvalidWidgetId;
    auto &node = document_.addWidget(registry_.definition(type), position, parent);
    selectedWidget_ = node.id;
    logger_.info("Added " + toString(type) + ".");
}

void Editor::deleteSelection()
{
    if (selectedWidget_ == InvalidWidgetId) {
        return;
    }

    pushUndoPoint();
    document_.removeWidget(selectedWidget_);
    selectedWidget_ = InvalidWidgetId;
    logger_.info("Selection deleted.");
}

void Editor::copySelection()
{
    if (auto *node = selectedNode()) {
        clipboard_ = *node;
        logger_.info("Widget copied.");
    }
}

void Editor::pasteClipboard()
{
    if (!clipboard_) {
        return;
    }

    pushUndoPoint();
    const auto &definition = registry_.definition(clipboard_->type);
    auto &node = document_.addWidget(definition,
        { clipboard_->rect.x + 24, clipboard_->rect.y + 24 },
        document_.rootWindow() ? document_.rootWindow()->id : InvalidWidgetId);
    node.text = clipboard_->text;
    node.rect.width = clipboard_->rect.width;
    node.rect.height = clipboard_->rect.height;
    node.visible = clipboard_->visible;
    node.enabled = clipboard_->enabled;
    node.font = clipboard_->font;
    node.color = clipboard_->color;
    node.anchor = clipboard_->anchor;
    node.dock = clipboard_->dock;
    selectedWidget_ = node.id;
    logger_.info("Widget pasted.");
}

void Editor::exportC(const std::filesystem::path &path)
{
    CCodeGenerator generator;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream file(path);
    if (!file) {
        logger_.error("Could not open export file.");
        return;
    }

    file << generator.generate(document_);
    logger_.info("Exported C file: " + path.string());
}

void Editor::pushUndoPoint()
{
    undoStack_.push_back(document_);
    redoStack_.clear();
    if (undoStack_.size() > 128) {
        undoStack_.erase(undoStack_.begin());
    }
}

void Editor::undo()
{
    if (undoStack_.empty()) {
        return;
    }

    redoStack_.push_back(document_);
    document_ = undoStack_.back();
    undoStack_.pop_back();
    selectedWidget_ = InvalidWidgetId;
    logger_.info("Undo.");
}

void Editor::redo()
{
    if (redoStack_.empty()) {
        return;
    }

    undoStack_.push_back(document_);
    document_ = redoStack_.back();
    redoStack_.pop_back();
    selectedWidget_ = InvalidWidgetId;
    logger_.info("Redo.");
}

}
