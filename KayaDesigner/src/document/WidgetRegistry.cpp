#include "document/WidgetRegistry.h"

#include <stdexcept>

namespace kaya::designer {

std::string toString(WidgetType type)
{
    switch (type) {
    case WidgetType::Window: return "Window";
    case WidgetType::Button: return "Button";
    case WidgetType::Label: return "Label";
    case WidgetType::TextBox: return "TextBox";
    case WidgetType::CheckBox: return "CheckBox";
    case WidgetType::Image: return "Image";
    case WidgetType::ProgressBar: return "ProgressBar";
    case WidgetType::MenuBar: return "MenuBar";
    case WidgetType::StatusBar: return "StatusBar";
    case WidgetType::ListView: return "ListView";
    case WidgetType::TreeView: return "TreeView";
    case WidgetType::ScrollView: return "ScrollView";
    }
    return "Unknown";
}

std::string toString(Anchor anchor)
{
    switch (anchor) {
    case Anchor::None: return "None";
    case Anchor::Left: return "Left";
    case Anchor::Right: return "Right";
    case Anchor::Top: return "Top";
    case Anchor::Bottom: return "Bottom";
    case Anchor::Fill: return "Fill";
    }
    return "None";
}

std::string toString(DockMode dock)
{
    switch (dock) {
    case DockMode::None: return "None";
    case DockMode::Left: return "Left";
    case DockMode::Right: return "Right";
    case DockMode::Top: return "Top";
    case DockMode::Bottom: return "Bottom";
    case DockMode::Fill: return "Fill";
    }
    return "None";
}

WidgetRegistry::WidgetRegistry()
{
    definitions_ = {
        { WidgetType::Window, "Window", { 40, 40, 720, 480 }, "Window" },
        { WidgetType::Button, "Button", { 40, 40, 120, 36 }, "Button" },
        { WidgetType::Label, "Label", { 40, 40, 140, 28 }, "Label" },
        { WidgetType::TextBox, "TextBox", { 40, 40, 220, 36 }, "" },
        { WidgetType::CheckBox, "CheckBox", { 40, 40, 160, 30 }, "CheckBox" },
        { WidgetType::Image, "Image", { 40, 40, 160, 120 }, "image.bmp" },
        { WidgetType::ProgressBar, "ProgressBar", { 40, 40, 220, 24 }, "" },
        { WidgetType::MenuBar, "MenuBar", { 0, 0, 700, 28 }, "File Edit View" },
        { WidgetType::StatusBar, "StatusBar", { 0, 450, 700, 28 }, "Ready" },
        { WidgetType::ListView, "ListView", { 40, 40, 220, 160 }, "" },
        { WidgetType::TreeView, "TreeView", { 40, 40, 220, 180 }, "" },
        { WidgetType::ScrollView, "ScrollView", { 40, 40, 260, 180 }, "" },
    };
}

std::span<const WidgetDefinition> WidgetRegistry::definitions() const
{
    return definitions_;
}

const WidgetDefinition &WidgetRegistry::definition(WidgetType type) const
{
    for (const auto &definition : definitions_) {
        if (definition.type == type) {
            return definition;
        }
    }
    throw std::runtime_error("Unknown widget type");
}

}

