#ifndef __KERNEL_INC_SYSCALL_SYS_H
#define __KERNEL_INC_SYSCALL_SYS_H

//=========================
// define
//=========================
#define SYSCALL_MAX (32)

enum SyscallNR
{
    SYS_TEST0,
    SYS_TEST1,
    SYS_TEST2,
    SYS_TEST3
};

//=========================
// struct
//=========================

//=========================
// external variable
//=========================

//=========================
// function
//=========================
/**
 * @brief Initialize the system-call dispatch table.
 *
 * Registers every built-in kernel system-call handler into @ref syscall_func[]
 * at the index defined by @ref SyscallNR.  Must be called once during kernel
 * startup (see kernel_init()) before interrupts are enabled; calling it after
 * @c sti risks a concurrent @c int @c 0x80 dispatching through an
 * uninitialized (NULL) slot.
 *
 * Current registrations:
 * | Index        | Handler               | Arguments |
 * |:------------:|:---------------------:|:---------:|
 * | SYS_TEST0    | sys_test_syscall0()   | none      |
 * | SYS_TEST1    | sys_test_syscall1()   | 1 (EBX)   |
 * | SYS_TEST2    | sys_test_syscall2()   | 2 (EBX, ECX) |
 * | SYS_TEST3    | sys_test_syscall3()   | 3 (EBX, ECX, EDX) |
 */
void syscall_init(void);

#endif