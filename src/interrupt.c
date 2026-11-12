#include "kernel.h"
#include "interrupt.h"

/* ---------------- GDT (flat 4 GB) ----------------
 * 0x08 = kernel code, 0x10 = kernel data.
 * Dibuat sendiri supaya tidak bergantung pada GDT bawaan multiboot. */
typedef struct {
    uint16_t limit_lo, base_lo;
    uint8_t  base_mid, access, gran, base_hi;
} __attribute__((packed)) gdt_entry_t;

typedef struct { uint16_t limit; uint32_t base; } __attribute__((packed)) dtr_t;

static gdt_entry_t gdt[3];
static dtr_t gdtr;

static void gdt_set(int i, uint8_t access, uint8_t gran) {
    gdt[i].limit_lo = 0xFFFF;
    gdt[i].base_lo  = 0;
    gdt[i].base_mid = 0;
    gdt[i].access   = access;
    gdt[i].gran     = gran;
    gdt[i].base_hi  = 0;
}

void gdt_init(void) {
    memset(gdt, 0, sizeof(gdt));
    gdt_set(1, 0x9A, 0xCF);   /* code ring 0 */
    gdt_set(2, 0x92, 0xCF);   /* data ring 0 */
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = (uint32_t)gdt;
    __asm__ volatile (
        "lgdt %0\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "ljmp $0x08, $1f\n\t"
        "1:"
        : : "m"(gdtr) : "ax", "memory");
}

/* ---------------- PIC ---------------- */
static void io_wait(void) { outb(0x80, 0); }

void pic_init(void) {
    outb(0x20, 0x11); io_wait();   /* ICW1: init */
    outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait();   /* ICW2: master mulai vektor 32 */
    outb(0xA1, 0x28); io_wait();   /* slave mulai vektor 40 */
    outb(0x21, 0x04); io_wait();   /* ICW3 */
    outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait();   /* ICW4: mode 8086 */
    outb(0xA1, 0x01); io_wait();
    outb(0x21, 0xFE);              /* hanya IRQ0 (timer) aktif */
    outb(0xA1, 0xFF);
}

/* ---------------- IDT ---------------- */
typedef struct {
    uint16_t base_lo, sel;
    uint8_t  zero, flags;
    uint16_t base_hi;
} __attribute__((packed)) idt_entry_t;

static idt_entry_t idt[256];
static dtr_t idtr;

void idt_set_gate(uint8_t num, uint32_t handler) {
    idt[num].base_lo = handler & 0xFFFF;
    idt[num].sel     = 0x08;
    idt[num].zero    = 0;
    idt[num].flags   = 0x8E;            /* present, ring 0, interrupt gate (IF dimatikan otomatis) */
    idt[num].base_hi = (handler >> 16) & 0xFFFF;
}

/* Dipanggil isr_default untuk semua vektor yang belum punya handler.
 * Tanpa ini, exception berujung triple fault (QEMU reset diam-diam). */
void unhandled_interrupt(void) {
    console_set_color(COLOR_ERR);
    kprintf("\n[PANIC] Interrupt/exception tidak ditangani. Sistem berhenti.\n");
    for (;;) __asm__ volatile ("cli; hlt");
}

void idt_init(void) {
    for (int i = 0; i < 256; i++) idt_set_gate((uint8_t)i, (uint32_t)isr_default);
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint32_t)idt;
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}
