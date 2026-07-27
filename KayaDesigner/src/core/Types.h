#pragma once

#include <cstdint>
#include <string>

namespace kaya::designer {

using WidgetId = std::uint32_t;

constexpr WidgetId InvalidWidgetId = 0;

struct Vec2i {
    int x = 0;
    int y = 0;
};

struct Recti {
    int x = 0;
    int y = 0;
    int width = 100;
    int height = 32;

    [[nodiscard]] bool contains(Vec2i point) const
    {
        return point.x >= x && point.x < x + width && point.y >= y && point.y < y + height;
    }
};

struct Color {
    float r = 0.25f;
    float g = 0.55f;
    float b = 0.95f;
    float a = 1.0f;
};

enum class Anchor {
    None,
    Left,
    Right,
    Top,
    Bottom,
    Fill
};

enum class DockMode {
    None,
    Left,
    Right,
    Top,
    Bottom,
    Fill
};

enum class WidgetType {
    Window,
    Button,
    Label,
    TextBox,
    CheckBox,
    Image,
    ProgressBar,
    MenuBar,
    StatusBar,
    ListView,
    TreeView,
    ScrollView
};

std::string toString(WidgetType type);
std::string toString(Anchor anchor);
std::string toString(DockMode dock);

}

