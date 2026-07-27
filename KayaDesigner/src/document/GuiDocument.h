#pragma once

#include "document/WidgetNode.h"
#include "document/WidgetRegistry.h"

#include <vector>

namespace kaya::designer {

class GuiDocument {
public:
    void createDefault(const WidgetRegistry &registry);

    WidgetNode &addWidget(const WidgetDefinition &definition, Vec2i position, WidgetId parent);
    void removeWidget(WidgetId id);

    [[nodiscard]] WidgetNode *find(WidgetId id);
    [[nodiscard]] const WidgetNode *find(WidgetId id) const;
    [[nodiscard]] WidgetNode *rootWindow();
    [[nodiscard]] const WidgetNode *rootWindow() const;
    [[nodiscard]] const std::vector<WidgetNode> &widgets() const { return widgets_; }

    [[nodiscard]] WidgetId hitTest(Vec2i point) const;

private:
    WidgetId allocateId();
    std::vector<WidgetNode> widgets_;
    WidgetId nextId_ = 1;
};

}

