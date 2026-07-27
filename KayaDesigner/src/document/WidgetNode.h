#pragma once

#include "core/Types.h"

#include <string>
#include <vector>

namespace kaya::designer {

struct WidgetNode {
    WidgetId id = InvalidWidgetId;
    WidgetType type = WidgetType::Button;
    std::string objectName;
    std::string text;
    Recti rect;
    bool visible = true;
    bool enabled = true;
    std::string font = "default";
    Color color;
    Anchor anchor = Anchor::None;
    DockMode dock = DockMode::None;
    WidgetId parent = InvalidWidgetId;
    std::vector<WidgetId> children;
};

}

