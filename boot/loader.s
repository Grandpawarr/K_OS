%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR

;-------------------------
; GDT
;-------------------------
;                       base,       limit,      attribute
gdt_base:   DESCRIPTOR  0x0,        0x0,        0x0
code_desc:  DESCRIPTOR  0x0,        0xf_ffff,   GDT_G_4K \
                                                + GDT_D_32 \
                                                + GDT_L_32 \
                                                + GDT_AVL_0 \
                                                + GDT_P_1 \
                                                + GDT_DPL_0 \
                                                + GDT_S_SW \
                                                + GDT_TYPE_CODE
data_desc:  DESCRIPTOR  0x0,        0xf_ffff,   GDT_G_4K \
                                                + GDT_D_32 \
                                                + GDT_L_32 \
                                                + GDT_AVL_0 \
                                                + GDT_P_1 \
                                                + GDT_DPL_0 \
                                                + GDT_S_SW \
                                                + GDT_TYPE_DATA
video_desc: DESCRIPTOR  0xb_8000,   0x0_7fff,   GDT_G_1 \
                                                + GDT_D_32 \
                                                + GDT_L_32 \
                                                + GDT_AVL_0 \
                                                + GDT_P_1 \
                                                + GDT_DPL_0 \
                                                + GDT_S_SW \
                                                + GDT_TYPE_DATA

GDT_SIZE    equ $ - gdt_base
GDT_LIMIT   equ GDT_SIZE - 1
times 256-($-$$) db 0           ; total 32 GDT element

;-------------------------
; fix data location
;-------------------------
ram_size    dd 0        ; LOADER_BASE_ADDR + 0x100 = 0x900
ards_nr     dd 0        ; number of ards
ards_buf times 200 db 0 ; total 10 ards

;-------------------------
; GDT selector
;-------------------------
SELECTOR_CODE   equ (code_desc - gdt_base) + TI_GDT + RPL_0
SELECTOR_DATA   equ (data_desc - gdt_base) + TI_GDT + RPL_0
SELECTOR_VIDEO  equ (video_desc - gdt_base) + TI_GDT + RPL_0

times 512-($-$$) db 0
loader_start:
;-------------------------
; Set stack pointer
;-------------------------
    mov sp, LOADER_STACK_TOP

;-------------------------
; Get current cursor
;-------------------------
    mov ah, 0x03        ; function code: Get cursor position and shape
    mov bh, 0x0         ; page number
    int 0x10            ; bios interrupt call
                        ; ouput: dh=row, dl=column

;-------------------------
; Print log
;-------------------------
; Using the bios interrupt to print message
    mov bp, msg_1       ; absolute address of string
    mov ah, 0x13        ; function code
    mov al, 0x01        ; write mode
    mov bh, 0x00        ; page number
    mov bl, 0x05        ; font color is Magenta
    mov cx, msg_1_len   ; length of string
    add dh, 1           ; input: dh=row, dl=column
    mov dl, 0           ; print to next line
    int 0x10            ; bios interrupt call

;-------------------------
; Get RAM size
;-------------------------
; Get all ARDS
get_ram_size:
    xor ebx, ebx        ; for Bios, must be zero
    mov edx, 0x534d4150 ; place "SMAP" into dx
    mov di, ards_buf    
e820:
    mov eax, 0x0000e820 ; it changes after trigger interrupt
    mov ecx, 20         ; size of ARDS
    int 0x15            ; Bios interrupt
    add di, cx          ; point to next buffer
    inc word [ards_nr]  ; increase the number of ARDS
    cmp ebx, 0x0        
    jne e820

; find the max adress
    mov ebx, ards_buf   ; base address of ARDS
    xor edx, edx        ; for max address
    mov cx, [ards_nr]   ; loop count
find_max_addr_loop:
    mov eax, [ebx]      ; base address low bits
    add eax, [ebx+8]    ; length low bits
    cmp edx, eax        ; compare with max address
    jae next_ards
    mov edx, eax        ; udpate the max address
next_ards:
    add ebx, 20         ; next ARDS structure
    loop find_max_addr_loop

; Set RAM size
    mov [ram_size], edx ; store the RAM size

;-------------------------
; enable protected mode
;-------------------------
; enable A20
    in al, 0x92
    or al, 0000_0010b
    out 0x92, al

; Load GDT
    lgdt [gdtr]

; Protection enable
    mov eax, cr0
    or eax, 0x0000_0001
    mov cr0, eax

    jmp SELECTOR_CODE:p_mode_start

;-------------------------
; protected mode start
;-------------------------
[bits 32]
p_mode_start:
    mov ax, SELECTOR_DATA
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov esp, LOADER_STACK_TOP    ; initial stack pointer (32bits)

;-------------------------
; print log
;-------------------------
    mov edi, msg_2
    mov ecx, msg_2_len
    call print_log

;-------------------------
; Pause the process
;-------------------------
    jmp $               ; stop here

;-------------------------
; print log
;   edi : address of message
;   ecx : length of message
;-------------------------
print_log:
; 1. Get current cursor, bx=current cursor
; get high bits of cursor
    mov dx, 0x03d4
    mov al, 0x0e
    out dx, al
    mov dx, 0x03d5
    in al, dx
    mov bh, al

; get low bits of cursor
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    in al, dx
    mov bl, al

; 2. Set bx to next line
; dividend: dx:ax, quotient: ax, remainder: dx
    xor dx, dx      ; clear to zero
    mov ax, bx
    mov si, 80
    div si          ; ax: quotient/row, dx: remainder/column
    sub bx, dx      ; minus remainder(80 characters per line)
    add bx, 80      ; move to the beginning of next line

    push bx         ; save next line position to stack (will be modified in loop)

; 3. Print log
    mov ax, SELECTOR_VIDEO
    mov gs, ax
    shl bx, 1       ; 1 char = 2 byte, 
                    ; converted character indexing to exact byte offsets in memory
    xor edx, edx    ; clear to zero
putchar_loop:
    mov byte al, [edi+edx]      ; get message
    mov byte [gs:bx], al        ; set character ASCII
    mov byte [gs:bx+1], 0x4e    ; set character color
    add dx, 1                   ; string pointer
    add bx, 2                   ; screen write pointer
    loop putchar_loop           ; ecx is initialize at the beginning

; 4. Set cursor to next line
    pop bx                      ; restore current line position from stack
    add bx, 80                  ;  move to the beginning of next line
; set high bits of cursor
    mov dx, 0x03d4
    mov al, 0x0e
    out dx, al
    mov dx, 0x03d5
    mov al, bh
    out dx, al

; set low bits of cursor
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    mov al, bl
    out dx, al

    ret

;-------------------------
; non-fix data location
;-------------------------
    msg_1       db "2 Loader"
    msg_1_len   equ 8
    msg_2       db "3 Protected"
    msg_2_len   equ 11
    gdtr    dw GDT_LIMIT    ; [15:00] GDT limit 16 bits
            dd gdt_base     ; [47:16] GDT base address 32 bits
