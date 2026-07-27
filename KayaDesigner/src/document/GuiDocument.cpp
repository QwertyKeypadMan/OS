#include "document/GuiDocument.h"

#include <algorithm>

namespace kaya::designer {

void GuiDocument::createDefault(const WidgetRegistry &registry)
{
    widgets_.clear();
    nextId_ = 1;
    addWidget(registry.definition(WidgetType::Window), { 0, 0 }, InvalidWidgetId);
}

WidgetNode &GuiDocument::addWidget(const WidgetDefinition &definition, Vec2i position, WidgetId parent)
{
    WidgetNode node;
    node.id = allocateId();
    node.type = definition.type;
    node.objectName = toString(definition.type) + std::to_string(node.id);
    node.text = definition.defaultText;
    node.rect = definition.defaultRect;
    node.rect.x = position.x;
    node.rect.y = position.y;
    node.parent = parent;

    widgets_.push_back(std::move(node));

    if (auto *parentNode = find(parent)) {
        parentNode->children.push_back(widgets_.back().id);
    }

    return widgets_.back();
}

void GuiDocument::removeWidget(WidgetId id)
{
    if (id == InvalidWidgetId || (rootWindow() && rootWindow()->id == id)) {
        return;
    }

    for (auto &node : widgets_) {
        node.children.erase(std::remove(node.children.begin(), node.children.end(), id), node.children.end());
    }

    widgets_.erase(std::remove_if(widgets_.begin(), widgets_.end(),
        [id](const WidgetNode &node) { return node.id == id || node.parent == id; }), widgets_.end());
}

WidgetNode *GuiDocument::find(WidgetId id)
{
    for (auto &node : widgets_) {
        if (node.id == id) {
            return &node;
        }
    }
    return nullptr;
}

const WidgetNode *GuiDocument::find(WidgetId id) const
{
    for (const auto &node : widgets_) {
        if (node.id == id) {
            return &node;
        }
    }
    return nullptr;
}

WidgetNode *GuiDocument::rootWindow()
{
    for (auto &node : widgets_) {
        if (node.type == WidgetType::Window) {
            return &node;
        }
    }
    return nullptr;
}

const WidgetNode *GuiDocument::rootWindow() const
{
    for (const auto &node : widgets_) {
        if (node.type == WidgetType::Window) {
            return &node;
        }
    }
    return nullptr;
}

WidgetId GuiDocument::hitTest(Vec2i point) const
{
    for (auto it = widgets_.rbegin(); it != widgets_.rend(); ++it) {
        if (it->visible && it->rect.contains(point)) {
            return it->id;
        }
    }
    return InvalidWidgetId;
}

WidgetId GuiDocument::allocateId()
{
    return nextId_++;
}

}

