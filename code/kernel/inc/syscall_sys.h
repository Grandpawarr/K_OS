#ifndef __KERNEL_INC_SYSCALL_SYS_H
#define __KERNEL_INC_SYSCALL_SYS_H

/**
 * @file syscall_sys.h
 * @brief Kernel side of the system-call interface (int 0x80).
 *
 * The user-side wrappers that issue @c int @c 0x80 live in
 * usr/inc/syscall_usr.h; this header covers the kernel dispatch table setup.
 */

//=========================
// define
//=========================
/** @brief Capacity of the dispatch table @c syscall_func[] (max syscalls). */
#define SYSCALL_MAX (32)

/**
 * @brief System-call numbers: the index the caller loads into EAX before
 *        @c int @c 0x80, and the slot used in @c syscall_func[].
 */
enum SyscallNR
{
    SYS_TEST0, /**< Test syscall, no arguments. */
    SYS_TEST1, /**< Test syscall, 1 argument (EBX). */
    SYS_TEST2, /**< Test syscall, 2 arguments (EBX, ECX). */
    SYS_TEST3  /**< Test syscall, 3 arguments (EBX, ECX, EDX). */
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