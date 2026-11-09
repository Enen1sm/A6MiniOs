#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

#define OS_NAME "A6"
#define OS_VERSION "0.1"

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0, %1" :: "a"(v), "Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t v; __asm__ volatile("inb %1, %0" : "=a"(v) :"Nd"(p)); return v; }

static inline uint32_t irq_save(void) {
    uint32_t f; __asm__ volatile("pushf; pop %0; cli" : "=r"(f) ::"memory"); return f;
}
static inline void irq_restore(uint32_t f) { __asm__ volatile("push %0; popf" :: "r"(f) : "memory", "cc"); }

#define COLOR_NORMAL 0x07
#define COLOR_TITLE 0x0B
#define COLOR_OK 0x0A
#define COLOR_WARN 0x0E
#define COLOR_ERR 0x0C

void console_init(void);
void console_putc(char c);
void console_puts(const char *s);
void console_clear(void);
void console_set_color(uint8_t c);
void console_status_bar(const char *text);
void kprintf(const char *fmt, ...);

size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
char *strncpy(char *d, const char *s, size_t n);
void *memset(void *d, int v, size_t n);
void *memcpy(void *d, const void *s, size_t n);

#endif