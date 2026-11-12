; switch.asm - context switch (NASM, 32-bit)
bits 32

global switch_context

section .text

; void switch_context(uint32_t *old_esp, uint32_t new_esp)
; Simpan register callee-saved thread lama di stack-nya, simpan ESP,
; lalu pindah ke stack thread baru dan pulihkan register-nya.
switch_context:
    mov eax, [esp + 4]      ; eax = &old->esp
    mov edx, [esp + 8]      ; edx = new->esp
    push ebp
    push ebx
    push esi
    push edi
    mov [eax], esp          ; simpan ESP thread lama
    mov esp, edx            ; <<< pindah stack = pindah thread
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret                     ; thread baru : lompat ke thread_start
                            ; thread lama : kembali ke dalam schedule()
