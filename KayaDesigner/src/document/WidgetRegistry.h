#pragma once

#include "core/Types.h"

#include <span>
#include <string>
#include <vector>

namespace kaya::designer {

struct WidgetDefinition {
    WidgetType type;
    std::string displayName;
    Recti defaultRect;
    std::string defaultText;
};

class WidgetRegistry {
public:
    WidgetRegistry();

    [[nodiscard]] std::span<const WidgetDefinition> definitions() const;
    [[nodiscard]] const WidgetDefinition &definition(WidgetType type) const;

private:
    std::vector<WidgetDefinition> definitions_;
};

}

