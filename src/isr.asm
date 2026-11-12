; isr.asm - stub interrupt (NASM, 32-bit)
bits 32

global irq0_stub
global isr_default
extern timer_irq_handler
extern unhandled_interrupt

section .text

; IRQ0 (timer) -> vektor 32
irq0_stub:
    pushad
    cld
    call timer_irq_handler
    popad
    iret

; Semua vektor lain (exception dan IRQ yang belum ditangani)
isr_default:
    cld
    call unhandled_interrupt
.hang:
    cli
    hlt
    jmp .hang
