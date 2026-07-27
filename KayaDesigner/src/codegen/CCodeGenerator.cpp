#include "codegen/CCodeGenerator.h"

#include <sstream>
#include <iomanip>

namespace kaya::designer {

std::string CCodeGenerator::generate(const GuiDocument &document) const
{
    std::ostringstream out;
    const WidgetNode *root = document.rootWindow();

    out << "#include \"kernel/gui.h\"\n";
    out << "#include \"kernel/graphics.h\"\n\n";
    out << "static void kaya_designer_ui_draw(struct gui_window *win, int cx, int cy, int cw, int ch)\n";
    out << "{\n";
    out << "    (void)win;\n";
    out << "    (void)cw;\n";
    out << "    (void)ch;\n";

    if (!root) {
        out << "    ui_draw_text(\"No root window in designer document.\", cx + 16, cy + 16,\n";
        out << "        graphics_rgb(235, 235, 240), graphics_rgb(26, 27, 33), 14.0f);\n";
        out << "}\n";
        return out.str();
    }

    out << "    graphics_fill_rect(cx, cy, cw, ch, graphics_rgb(26, 27, 33));\n";

    for (const auto &node : document.widgets()) {
        if (node.id == root->id) {
            continue;
        }
        out << createLine(node);
    }

    out << "}\n";
    out << "\n";
    out << "void kaya_designer_ui_open(void)\n";
    out << "{\n";
    out << "    opa_window_create(\"" << escape(root->text.empty() ? "Designer UI" : root->text) << "\", "
        << root->rect.width << ", " << root->rect.height << ", kaya_designer_ui_draw);\n";
    out << "}\n";
    return out.str();
}

std::string CCodeGenerator::createLine(const WidgetNode &node) const
{
    std::ostringstream out;
    int x = node.rect.x;
    int y = node.rect.y;
    int w = node.rect.width;
    int h = node.rect.height;
    int r = static_cast<int>(node.color.r * 255.0f);
    int g = static_cast<int>(node.color.g * 255.0f);
    int b = static_cast<int>(node.color.b * 255.0f);

    switch (node.type) {
    case WidgetType::Button:
        out << "    graphics_draw_rounded_rect(cx + " << x << ", cy + " << y << ", "
            << w << ", " << h << ", 6, graphics_rgb(" << r << ", " << g << ", " << b << "));\n";
        out << "    ui_draw_text(\"" << escape(node.text.empty() ? node.objectName : node.text) << "\", "
            << "cx + " << (x + 10) << ", cy + " << (y + h / 2 - 7)
            << ", graphics_rgb(245, 245, 248), graphics_rgb(" << r << ", " << g << ", " << b
            << "), 14.0f);\n";
        return out.str();
    case WidgetType::Label:
        out << "    ui_draw_text(\"" << escape(node.text.empty() ? node.objectName : node.text) << "\", "
            << "cx + " << x << ", cy + " << y
            << ", graphics_rgb(230, 230, 235), graphics_rgb(26, 27, 33), 14.0f);\n";
        return out.str();
    case WidgetType::TextBox:
        out << "    graphics_fill_rect(cx + " << x << ", cy + " << y << ", "
            << w << ", " << h << ", graphics_rgb(18, 20, 26));\n";
        out << "    graphics_draw_rounded_rect(cx + " << x << ", cy + " << y << ", "
            << w << ", " << h << ", 4, graphics_rgb(60, 64, 78));\n";
        out << "    ui_draw_text(\"" << escape(node.text.empty() ? node.objectName : node.text) << "\", "
            << "cx + " << (x + 8) << ", cy + " << (y + h / 2 - 7)
            << ", graphics_rgb(150, 155, 168), graphics_rgb(18, 20, 26), 13.0f);\n";
        return out.str();
    case WidgetType::CheckBox:
        out << "    graphics_draw_rect(cx + " << x << ", cy + " << y << ", 18, 18, "
            << "graphics_rgb(120, 130, 150));\n";
        out << "    ui_draw_text(\"" << escape(node.text.empty() ? node.objectName : node.text) << "\", "
            << "cx + " << (x + 28) << ", cy + " << y
            << ", graphics_rgb(230, 230, 235), graphics_rgb(26, 27, 33), 14.0f);\n";
        return out.str();
    case WidgetType::Image:
        out << "    graphics_fill_rect(cx + " << x << ", cy + " << y << ", "
            << w << ", " << h << ", graphics_rgb(34, 38, 48));\n";
        out << "    graphics_draw_rect(cx + " << x << ", cy + " << y << ", "
            << w << ", " << h << ", graphics_rgb(90, 100, 120));\n";
        out << "    ui_draw_text(\"Image\", cx + " << (x + 8) << ", cy + " << (y + 8)
            << ", graphics_rgb(180, 190, 205), graphics_rgb(34, 38, 48), 13.0f);\n";
        return out.str();
    case WidgetType::ProgressBar:
        out << "    graphics_fill_rect(cx + " << x << ", cy + " << y << ", "
            << w << ", " << h << ", graphics_rgb(18, 20, 26));\n";
        out << "    graphics_fill_rect(cx + " << x << ", cy + " << y << ", "
            << (w / 2) << ", " << h << ", graphics_rgb(75, 150, 240));\n";
        out << "    graphics_draw_rect(cx + " << x << ", cy + " << y << ", "
            << w << ", " << h << ", graphics_rgb(80, 90, 110));\n";
        return out.str();
    case WidgetType::MenuBar:
        out << "    graphics_fill_rect(cx + " << x << ", cy + " << y << ", "
            << w << ", " << h << ", graphics_rgb(40, 42, 52));\n";
        out << "    ui_draw_text(\"" << escape(node.text.empty() ? "File Edit View" : node.text) << "\", "
            << "cx + " << (x + 8) << ", cy + " << (y + 6)
            << ", graphics_rgb(235, 235, 240), graphics_rgb(40, 42, 52), 13.0f);\n";
        return out.str();
    case WidgetType::StatusBar:
        out << "    graphics_fill_rect(cx + " << x << ", cy + " << y << ", "
            << w << ", " << h << ", graphics_rgb(18, 19, 24));\n";
        out << "    ui_draw_text(\"" << escape(node.text.empty() ? "Ready" : node.text) << "\", "
            << "cx + " << (x + 8) << ", cy + " << (y + 6)
            << ", graphics_rgb(180, 185, 198), graphics_rgb(18, 19, 24), 13.0f);\n";
        return out.str();
    case WidgetType::ListView:
    case WidgetType::TreeView:
    case WidgetType::ScrollView:
        out << "    graphics_fill_rect(cx + " << x << ", cy + " << y << ", "
            << w << ", " << h << ", graphics_rgb(24, 27, 34));\n";
        out << "    graphics_draw_rect(cx + " << x << ", cy + " << y << ", "
            << w << ", " << h << ", graphics_rgb(70, 78, 96));\n";
        out << "    ui_draw_text(\"" << escape(toString(node.type)) << "\", cx + " << (x + 8)
            << ", cy + " << (y + 8)
            << ", graphics_rgb(190, 200, 215), graphics_rgb(24, 27, 34), 13.0f);\n";
        return out.str();
    case WidgetType::Window:
        return "    /* nested windows are not exported yet */\n";
    }
    return "";
}

std::string CCodeGenerator::escape(const std::string &text) const
{
    std::string result;
    result.reserve(text.size());
    for (char ch : text) {
        if (ch == '\\' || ch == '"') {
            result.push_back('\\');
        }
        if (ch == '\n') {
            result += "\\n";
        } else {
            result.push_back(ch);
        }
    }
    return result;
}

}
