#include "kernel.h"

static bool left;

static void put_uint(uint32_t v, unsigned base, int width, char pad, bool upper) {
    char buf[12]; int n = 0;
    const char *d = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    do { buf[n++] = d[v % base]; v /= base; } while (v);
    int len = n;
    if (!left) for (int i = n; i < width; i++) console_putc(pad);
    while (n--) console_putc(buf[n]);
    if (left) for (int i = len; i < width; i++) console_putc(' ');
}

static void kvprintf(const char *f, va_list ap) {
    for (; *f; f++) {
        if (*f != '%') { console_putc(*f); continue; }
        f++;
        char pad = ' '; int width = 0;
        left = false;
        if (*f == '-') { left = true; f++; }
        if (*f == '0') { pad = '0'; f++; }
        while (*f >= '0' && *f <= '9') width = width * 10 + (*f++ - '0');
        switch (*f) {
        case 'd': case 'i': {
            int32_t v = va_arg(ap, int32_t);
            if (v < 0) { console_putc('-'); put_uint((uint32_t)(-(v + 1)) + 1u, 10, width ? width - 1 : 0, pad, false); }
            else put_uint((uint32_t)v, 10, width, pad, false);
            break; }
        case 'u': put_uint(va_arg(ap, uint32_t), 10, width, pad, false); break;
        case 'x': put_uint(va_arg(ap, uint32_t), 16, width, pad, false); break;
        case 'X': put_uint(va_arg(ap, uint32_t), 16, width, pad, true);  break;
        case 'p': console_puts("0x"); put_uint((uint32_t)va_arg(ap, void *), 16, 8, '0', false); break;
        case 'c': console_putc((char)va_arg(ap, int)); break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            int len = (int)strlen(s);
            if (!left) for (int i = len; i < width; i++) console_putc(' ');
            console_puts(s);
            if (left) for (int i = len; i < width; i++) console_putc(' ');
            break; }
        case '%': console_putc('%'); break;
        case 0:   return;
        default:  console_putc('%'); console_putc(*f); break;
        }
    }
}

void kprintf(const char *fmt, ...) {
    uint32_t fl = irq_save();
    va_list ap; va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    irq_restore(fl);
}