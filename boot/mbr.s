%include "boot.inc"
section mbr vstart=0x7c00

;-------------------------
; Initialize segment registers(sreg)
;-------------------------
    mov ax, 0x0
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

;-------------------------
; Set stack pointer
;-------------------------
    mov sp, 0x8000

;-------------------------
; Get current cursor
;-------------------------
    mov ah, 0x3     ; Function code
    mov bh, 0x0     ; Page Number
    int 0x10        ; BIOS interrupt call
                    ; output: dh=row, dl=column

;-------------------------
; Print message
;-------------------------
    mov ah, 0x13    ; Function code
    mov al, 0x01    ; Write mode
    mov bh, 0x0     ; Page Number
    mov bl, 0x2     ; font color is Green
    mov cx, 0x5     ; Number of characters in message
                    ; input: dh=row, dl=column
    mov bp, msg     ; absolute address of string ( ES has been set to 0x00)
    int 0x10        ; BIOS interrupt call

;-------------------------
; Load loader
;-------------------------
    mov eax, LOADER_START_SECTOR
    mov bx, LOADER_BASE_ADDR
    mov cl, LOADER_SECTOR_COUNT
    call read_hd

;-------------------------
; Jump to loader
;-------------------------
    jmp LOADER_START_ADDR

;-------------------------
; Read hard disk
;   eax : LBA, logic base address
;   ebx : RAM address
;   cl  : sector count
;-------------------------
read_hd:
; step 1 : set sector count
    mov esi, eax    ; backup eax
                    ; cause port operation only accept al/ax
    mov dx, 0x1f2   
    mov al, cl
    out dx, al
    mov eax, esi    ; restore eax

; step 2 : set LBA from ATA port 0x1f3 to 0x1f6
    ; LBA[7:0]
    mov dx, 0x1f3
    out dx, al

    ; LBA[15:8]
    shr eax, 8          ; EAX lowest 8 bits is AL
    mov dx, 0x1f4
    out dx, al

    ; LBA[23:16]
    shr eax, 8  
    mov dx, 0x1f5
    out dx, al

    ; LBA[27:24] and setting (LBA only 28 bits)
    shr eax, 8
    and al, 0x0f    ; mask: only keep the lower 4 bits, 
                    ;       clear the higher 4 bits
    or al, 0xe0     ; b'1110 for master drive / LBA addressing
    mov dx, 0x1f6
    out dx, al

; step 3 : start read command
    mov dx, 0x1f7   ; ATA(command register)
    mov al, 0x20    ; ATA(read sectors with retry)
    out dx, al

; step 4 : polling status
    polling:
        nop             ; hardware delay
        in al, dx
        and al, 0x88    ; status register: [3]DRQ/[7]:BSY
        cmp al, 0x08    ; Data is ready and not busy
                        ; result store in EFLAGS(zf bit)
        jnz polling     ; keep polling

; step 5 : read data from hard disk
    mov al, cl
    mov dx, 256     ; sector count * 512 / 2 
                    ; (2 byte for each read)
    mul dx          ; ax = ax * dx (only support ax)
    mov cx, ax      ; cx = total loop time
    mov dx, 0x1f0
    read:
        in ax, dx
        mov [bx], ax
        add bx, 2
        loop read
    ret

;-------------------------
; Data
;-------------------------
    msg db "1 MBR"
    times 510-($-$$) db 0
    db 0x55,0xaa