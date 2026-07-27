#pragma once

#include "document/GuiDocument.h"

#include <string>

namespace kaya::designer {

class CCodeGenerator {
public:
    [[nodiscard]] std::string generate(const GuiDocument &document) const;

private:
    [[nodiscard]] std::string createLine(const WidgetNode &node) const;
    [[nodiscard]] std::string escape(const std::string &text) const;
};

}

