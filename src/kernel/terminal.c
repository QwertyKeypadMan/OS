#include "kernel/terminal.h"
#include "kernel/graphics.h"
#include "kernel/kstring.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static const size_t GRAPHICS_CELL_WIDTH = 12;
static const size_t GRAPHICS_CELL_HEIGHT = 18;
static volatile uint16_t *const VGA_MEMORY = (uint16_t *)0xB8000;

static size_t terminal_row;
static size_t terminal_column;
static size_t terminal_cells_width;
static size_t terminal_cells_height;
static uint8_t terminal_color;

// ---- KayaOS PSF Font Entegrasyonu (Düzeltildi) ----
static const uint8_t *terminal_font_buffer = 0;

static const uint32_t vga_palette[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
    0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
    0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

static uint8_t vga_entry_color(vga_color_t foreground, vga_color_t background)
{
    return (uint8_t)(foreground | background << 4);
}

static uint16_t vga_entry(unsigned char ch, uint8_t color)
{
    return (uint16_t)ch | (uint16_t)color << 8;
}

// Yeni font tamponunu set eden şanlı fonksiyonumuz
void terminal_set_font(const uint8_t *font_buffer)
{
    terminal_font_buffer = font_buffer;
}

static void terminal_putentryat(char ch, uint8_t color, size_t x, size_t y)
{
    if (graphics_available()) {
        uint32_t foreground = vga_palette[color & 0x0F];
        uint32_t background = vga_palette[(color >> 4) & 0x0F];
        int pixel_x = (int)(x * GRAPHICS_CELL_WIDTH);
        int pixel_y = (int)(y * GRAPHICS_CELL_HEIGHT);
        
        // Üst üste binmeleri önlemek için hücreyi temizle
        graphics_fill_rect(pixel_x, pixel_y, (int)GRAPHICS_CELL_WIDTH, (int)GRAPHICS_CELL_HEIGHT, background);
        
        // Eğer yüklenmiş bir font varsa yeni PSF motorunu kullanıyoruz!
        graphics_draw_char(pixel_x + 1, pixel_y + 1, ch, foreground, background, 1);
        return;
    }

    const size_t index = y * VGA_WIDTH + x;
    VGA_MEMORY[index] = vga_entry((unsigned char)ch, color);
}

static void terminal_scroll(void)
{
    if (terminal_row < terminal_cells_height) {
        return;
    }

    if (graphics_available()) {
        uint32_t background = vga_palette[(terminal_color >> 4) & 0x0F];
        graphics_scroll_up((uint32_t)GRAPHICS_CELL_HEIGHT, background);
        terminal_row = terminal_cells_height - 1;
        return;
    }
    
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    }
    terminal_row = VGA_HEIGHT - 1;
}

void terminal_clear(void)
{
    if (graphics_available()) {
        uint32_t background = vga_palette[(terminal_color >> 4) & 0x0F];
        graphics_clear(background);
        graphics_present(); // Ekranı temizleyince anında yansıt
    } else {
        for (size_t y = 0; y < VGA_HEIGHT; y++) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                terminal_putentryat(' ', terminal_color, x, y);
            }
        }
    }

    terminal_row = 0;
    terminal_column = 0;
}

void terminal_initialize(void)
{
    terminal_row = 0;
    terminal_column = 0;
    terminal_font_buffer = 0; 
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    if (graphics_available()) {
        terminal_cells_width = graphics_width() / GRAPHICS_CELL_WIDTH;
        terminal_cells_height = graphics_height() / GRAPHICS_CELL_HEIGHT;
        if (terminal_cells_width == 0) terminal_cells_width = 1;
        if (terminal_cells_height == 0) terminal_cells_height = 1;
    } else {
        terminal_cells_width = VGA_WIDTH;
        terminal_cells_height = VGA_HEIGHT;
    }

    terminal_clear();
}

void terminal_setcolor(vga_color_t foreground, vga_color_t background)
{
    terminal_color = vga_entry_color(foreground, background);
}

void terminal_draw_at(char ch, vga_color_t foreground, vga_color_t background, size_t x, size_t y)
{
    if (x >= terminal_cells_width || y >= terminal_cells_height) {
        return;
    }
    terminal_putentryat(ch, vga_entry_color(foreground, background), x, y);
    if (graphics_available()) {
        graphics_present(); // Koordinatlı tek atış çizimlerde ekrana bas
    }
}

void terminal_fill_rect(size_t x, size_t y, size_t width, size_t height, char ch,
    vga_color_t foreground, vga_color_t background)
{
    for (size_t row = y; row < y + height && row < terminal_cells_height; row++) {
        for (size_t col = x; col < x + width && col < terminal_cells_width; col++) {
            terminal_putentryat(ch, vga_entry_color(foreground, background), col, row);
        }
    }
    if (graphics_available()) {
        graphics_present(); // Blok çizim bitti, tek seferde ekrana yansıt
    }
}

size_t terminal_width(void)
{
    return terminal_cells_width;
}

size_t terminal_height(void)
{
    return terminal_cells_height;
}

void terminal_putchar(char ch)
{
    if (ch == '\n') {
        terminal_column = 0;
        terminal_row++;
        terminal_scroll();
        return;
    }

    if (ch == '\r') {
        terminal_column = 0;
        return;
    }

    if (ch == '\b') {
        if (terminal_column == 0) {
            if (terminal_row == 0) return;
            terminal_row--;
            terminal_column = terminal_cells_width - 1;
        } else {
            terminal_column--;
        }

        terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
        return;
    }

    terminal_putentryat(ch, terminal_color, terminal_column, terminal_row);

    terminal_column++;
    if (terminal_column == terminal_cells_width) {
        terminal_column = 0;
        terminal_row++;
        terminal_scroll();
    }
}

void terminal_write(const char *data, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        terminal_putchar(data[i]);
    }
    // REİS KRİTİK NOKTA: Yazı bittiği an buffer'ı ekrana fırlatıyoruz!
    if (graphics_available()) {
        graphics_present();
    }
}

void terminal_writestring(const char *data)
{
    terminal_write(data, k_strlen(data));
}

void terminal_write_dec(uint32_t value)
{
    char buffer[11];
    size_t index = 0;

    if (value == 0) {
        terminal_putchar('0');
        if (graphics_available()) graphics_present();
        return;
    }

    while (value > 0 && index < sizeof(buffer)) {
        buffer[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index > 0) {
        terminal_putchar(buffer[--index]);
    }
    
    if (graphics_available()) {
        graphics_present(); // Sayı yazımı bitince ekrana bas
    }
}

void terminal_write_hex(uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";

    terminal_putchar('0');
    terminal_putchar('x');
    
    for (int shift = 28; shift >= 0; shift -= 4) {
        terminal_putchar(digits[(value >> shift) & 0xF]);
    }
    
    if (graphics_available()) {
        graphics_present(); // Hex yazımı bitince ekrana bas
    }
}