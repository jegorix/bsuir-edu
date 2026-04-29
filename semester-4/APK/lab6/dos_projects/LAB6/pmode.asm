.386p
.model small, c

CODE_SELECTOR  equ 08h
DATA_SELECTOR  equ 10h
STACK_SELECTOR equ 18h

.code

extrn _gdt_ptr:byte
extrn _idt_ptr:byte
extrn _realmode_idt_ptr:byte
extrn _rm_cs:word
extrn _rm_ds:word
extrn _rm_ss:word
extrn _pm_active:byte
extrn _rtc_handler_body:near
extrn _keyboard_handler_body:near

public _enter_protected_mode
public _leave_protected_mode
public _rtc_handler
public _keyboard_handler

_enter_protected_mode proc near
    cli

    lgdt fword ptr _gdt_ptr
    lidt fword ptr _idt_ptr

    mov eax, cr0
    or  eax, 1
    mov cr0, eax

    ; Far jump flushes the prefetch queue and loads protected-mode CS.
    db 0EAh
    dw offset pm_entry
    dw CODE_SELECTOR

pm_entry:
    mov ax, DATA_SELECTOR
    mov ds, ax
    mov es, ax

    mov ax, STACK_SELECTOR
    mov ss, ax

    mov byte ptr _pm_active, 1
    ret
_enter_protected_mode endp

_leave_protected_mode proc near
    cli

    lidt fword ptr _realmode_idt_ptr

    mov eax, cr0
    and eax, 0FFFFFFFEh
    mov cr0, eax

    ; Far return reloads real-mode CS after PE is cleared.
    mov ax, word ptr _rm_cs
    push ax
    mov ax, offset rm_entry
    push ax
    retf

rm_entry:
    mov ax, word ptr _rm_ds
    mov ds, ax
    mov es, ax

    mov ax, word ptr _rm_ss
    mov ss, ax

    mov byte ptr _pm_active, 0
    ret
_leave_protected_mode endp

_rtc_handler proc far
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds
    push es

    mov ax, DATA_SELECTOR
    mov ds, ax
    mov es, ax

    call _rtc_handler_body

    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    iret
_rtc_handler endp

_keyboard_handler proc far
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds
    push es

    mov ax, DATA_SELECTOR
    mov ds, ax
    mov es, ax

    call _keyboard_handler_body

    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    iret
_keyboard_handler endp

end
