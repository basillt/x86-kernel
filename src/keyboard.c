/*
 * keyboard.c - PS/2 keyboard driver (IRQ 1)
 *
 * Every key press and release makes the keyboard controller raise
 * IRQ 1 and put a "scancode" in port 0x60. A scancode is NOT a
 * character: it's a number for a physical key. Same key = same code,
 * whether Shift is held or not. Turning it into a character is our job.
 *
 * Scancode set 1:  press = code,  release = code | 0x80
 */
#include <stdint.h>
#include "keyboard.h"
#include "irq.h"
#include "io.h"
#include "vga.h"

#define KBD_DATA_PORT 0x60

#define SC_RELEASE     0x80
#define SC_LSHIFT      0x2A
#define SC_RSHIFT      0x36
#define SC_CAPSLOCK    0x3A
#define SC_F1          0x3B

/* US layout, index = scancode. 0 = key we don't turn into a character. */
static const char keymap[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ',
};

static const char keymap_shift[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ',
};

static int shift_held;
static int caps_lock;
static int line_len;    /* characters typed on the current line */

static void trigger_divide_by_zero(void)
{
    volatile int a = 1, b = 0;
    volatile int c = a / b;
    (void)c;
}

static void keyboard_handler(struct registers *r)
{
    (void)r;
    uint8_t sc = inb(KBD_DATA_PORT);    /* must read it, or no more IRQs come */

    if (sc & SC_RELEASE) {
        uint8_t key = sc & ~SC_RELEASE;
        if (key == SC_LSHIFT || key == SC_RSHIFT)
            shift_held = 0;
        return;
    }

    switch (sc) {
    case SC_LSHIFT:
    case SC_RSHIFT:   shift_held = 1;          return;
    case SC_CAPSLOCK: caps_lock = !caps_lock;  return;
    case SC_F1:       trigger_divide_by_zero(); return;   /* exception test */
    }

    char c = shift_held ? keymap_shift[sc] : keymap[sc];

    /* Caps Lock only affects letters (and Shift reverses it) */
    if (caps_lock) {
        if (c >= 'a' && c <= 'z')      c -= 32;
        else if (c >= 'A' && c <= 'Z') c += 32;
    }

    if (c == '\b') {
        if (line_len > 0) {         /* don't erase the prompt or older lines */
            vga_backspace();
            line_len--;
        }
    } else if (c == '\n') {
        vga_write("\n> ");
        line_len = 0;
    } else if (c && c != 27 && c != '\t') {
        vga_putchar(c);
        line_len++;
    }
}

void keyboard_init(void)
{
    irq_register_handler(1, keyboard_handler);
}
