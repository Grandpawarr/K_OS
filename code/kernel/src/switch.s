;==========================================================================
; @file switch.s
; @brief Low-level kernel-thread context switch for K_OS.
;
; Implements switch_to(), the routine that swaps the CPU from one thread to
; another by saving/restoring the callee-saved registers and stack pointer.
; The saved esp is stored at the base of each thread's PCB (task_struct),
; whose first member is `kstack` -- hence writing to [task] updates kstack.
;==========================================================================
[bits 32]
;-------------------------
; external variable
;-------------------------

;-------------------------
; define
;-------------------------

;-------------------------
; macro
;-------------------------

;-------------------------
; external function
;-------------------------
section .text
global switch_to

;--------------------------------------------------------------------------
; @brief Switch execution context from `cur` to `next`.
;
; C prototype: void switch_to(struct task_struct *cur, struct task_struct *next);
;
; Saves the System V callee-saved registers (esi, edi, ebx, ebp) of the
; outgoing thread, stashes its esp into cur->kstack, loads next->kstack into
; esp, then pops the incoming thread's registers and `ret`s to wherever it
; last yielded. For a brand-new thread this `ret` lands in kernel_thread via
; the forged kstack_switch frame.
;
; Stack offsets after the 4 pushes (each 4 bytes):
;   [esp + 16] -> return address
;   [esp + 20] -> arg 1 (cur)
;   [esp + 24] -> arg 2 (next)
;--------------------------------------------------------------------------
switch_to:
; backup registers based on calling convention
    push esi
    push edi
    push ebx
    push ebp

    mov eax, [esp + 20] ; get argument 1: current task
    mov [eax], esp      ; backup current task esp -> cur->kstack
    mov eax, [esp + 24] ; get argument 2: next task
    mov esp, [eax]      ; restore next task esp <- next->kstack

; restore registers
    pop ebp
    pop ebx
    pop edi
    pop esi

    ret
