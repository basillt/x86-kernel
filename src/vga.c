/*
 * vga.c - Text output by writing directly to video memory.
 *
 * In VGA text mode the screen is an 80x25 grid mapped at physical
 * address 0xB8000. Each cell is 2 bytes:
 *
 *     low byte  = ASCII character
 *     high byte = color: (background << 4) | foreground
 *
 * No printf, no OS: writing to this memory IS printing.
 */
#include "vga.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)

static size_t   cursor_row;
static size_t   cursor_col;
static uint8_t  current_color;

/* Combine a character and a color into one 16-bit cell */
static inline uint16_t vga_entry(char c, uint8_t color)
{
    return (uint16_t)(unsigned char)c | ((uint16_t)color << 8);
}

void vga_set_color(enum vga_color fg, enum vga_color bg)
{
    current_color = (uint8_t)(fg | (bg << 4));
}

void vga_clear(void)
{
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry(' ', current_color);
    cursor_row = 0;
    cursor_col = 0;
}

void vga_init(void)
{
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
}

/* Move every line up by one and blank the last line */
static void vga_scroll(void)
{
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];

    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', current_color);

    cursor_row = VGA_HEIGHT - 1;
}

void vga_putchar(char c)
{
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else {
        VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(c, current_color);
        if (++cursor_col == VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row == VGA_HEIGHT)
        vga_scroll();
}

/* Fill the rest of the current line with spaces (in the current color)
 * and move to the next line. Used to draw full-width colored bars. */
void vga_pad_line(void)
{
    do {
        vga_putchar(' ');
    } while (cursor_col != 0);
}

void vga_write(const char *str)
{
    while (*str)
        vga_putchar(*str++);
}

/* Print a 32-bit value as 0xXXXXXXXX (handy for debugging) */
void vga_write_hex(uint32_t value)
{
    const char *digits = "0123456789ABCDEF";
    vga_write("0x");
    for (int shift = 28; shift >= 0; shift -= 4)
        vga_putchar(digits[(value >> shift) & 0xF]);
}
