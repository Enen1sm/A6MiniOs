#include "kernel.h"

#define VGA_W   80
#define VGA_H   25
#define TEXT_H  (VGA_H - 1)
#define COM1    0x3F8

static volatile uint16_t *const vga = (volatile uint16_t *)0xB8000;
static int cx, cy;                       
static uint8_t color = COLOR_NORMAL;

static void serial_init(void) {
    outb(COM1 + 1, 0x00);   
    outb(COM1 + 3, 0x80);   
    outb(COM1 + 0, 0x01);   
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);   
    outb(COM1 + 2, 0xC7);   
    outb(COM1 + 4, 0x0B);
}

static void serial_putc(char c) {
    if (c == '\n') serial_putc('\r');
    while (!(inb(COM1 + 5) & 0x20)) { }   
    outb(COM1, (uint8_t)c);
}

static void vga_cursor(void) {
    uint16_t pos = (uint16_t)(cy * VGA_W + cx);
    outb(0x3D4, 14); outb(0x3D5, pos >> 8);
    outb(0x3D4, 15); outb(0x3D5, pos & 0xFF);
}

static void vga_scroll(void) {
    if (cy < TEXT_H) return;
    for (int i = 0; i < (TEXT_H - 1) * VGA_W; i++) vga[i] = vga[i + VGA_W];
    for (int i = (TEXT_H - 1) * VGA_W; i < TEXT_H * VGA_W; i++) vga[i] = (uint16_t)(' ' | (color << 8));
    cy = TEXT_H - 1;
}

static void vga_putc(char c) {
    if (c == '\n')      { cx = 0; cy++; }
    else if (c == '\r') { cx = 0; }
    else if (c == '\t') { cx = (cx + 8) & ~7; }
    else if (c == '\b') { if (cx > 0) { cx--; vga[cy * VGA_W + cx] = (uint16_t)(' ' | (color << 8)); } }
    else { vga[cy * VGA_W + cx] = (uint16_t)((uint8_t)c | (color << 8)); cx++; }
    if (cx >= VGA_W) { cx = 0; cy++; }
    vga_scroll();
    vga_cursor();
}

void console_set_color(uint8_t c) { color = c; }

void console_clear(void) {
    for (int i = 0; i < TEXT_H * VGA_W; i++) vga[i] = (uint16_t)(' ' | (color << 8));
    cx = cy = 0;
    vga_cursor();
    for (const char *e = "\033[2J\033[H"; *e; e++) serial_putc(*e);  
}

void console_status_bar(const char *text) {
    for (int i = 0; i < VGA_W; i++) {
        char c = (*text) ? *text++ : ' ';
        vga[TEXT_H * VGA_W + i] = (uint16_t)((uint8_t)c | (0x1F << 8));
    }
}

void console_putc(char c) {
    uint32_t fl = irq_save();   /* cegah output dua thread saling menimpa saat preempt */
    vga_putc(c);
    if (c == '\b') { serial_putc('\b'); serial_putc(' '); serial_putc('\b'); }
    else           { serial_putc(c); }
    irq_restore(fl);
}

void console_puts(const char *s) { while (*s) console_putc(*s++); }

void console_init(void) {
    serial_init();
    for (int i = 0; i < VGA_W * VGA_H; i++) vga[i] = (uint16_t)(' ' | (color << 8));
    cx = cy = 0;
    vga_cursor();
}