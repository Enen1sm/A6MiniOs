bits 32

MB_MAGIC equ 0x1BADB002
MB_FLAGS equ 0x00000003
MB_CHECK equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECK

section .bss
align 16
stack_bottom: resb 16384
stack_top:

section .text
global _start
extern kmain
_start:
    cli
    mov esp, stack_top
    push ebx
    push eax
    call kmain
.hang:
    cli
    hlt
    jmp .hang