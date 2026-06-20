#include "syscall_sys.h"
#include "print.h"

//=========================
// debugging
//=========================
#define DEBUG (0)
#define TRACE_STR(x)    \
    do                  \
    {                   \
        if (DEBUG)      \
            put_str(x); \
    } while (0)
#define TRACE_INT(x)    \
    do                  \
    {                   \
        if (DEBUG)      \
            put_int(x); \
    } while (0)

//=========================
// internal struct
//=========================

//=========================
// global variable
//=========================

/**
 * @brief Kernel-side system-call dispatch table.
 *
 * Indexed by the system-call number passed in EAX when the user program
 * executes @c int @c 0x80.  The assembly stub @c syscall_entry uses this
 * table to dispatch to the correct kernel function:
 * @code
 *   call [syscall_func + eax * 4]
 * @endcode
 *
 * Each slot holds a function pointer registered by syscall_init().
 * Unregistered slots remain NULL; calling through a NULL slot causes a
 * page fault, so all used slots must be initialized before interrupts
 * are enabled.
 */
void *syscall_func[SYSCALL_MAX];

//=========================
// internal functions
//=========================

//=========================
// external functions
//=========================

/**
 * @brief Stub system call — no arguments, no return value.
 *
 * Registered at @ref SYS_TEST0.  Prints a banner to confirm that the
 * basic @c int @c 0x80 dispatch path (CPU gate → @c syscall_entry →
 * @c syscall_func[] → C handler) is working end-to-end.
 */
void sys_test_syscall0(void)
{
    put_str("sys_test_syscall0() called\n");
}

/**
 * @brief Stub system call — one argument, returns a magic value.
 *
 * Registered at @ref SYS_TEST1.  Prints @p arg to verify that the first
 * argument (passed in EBX by the caller and pushed onto the kernel stack
 * by @c syscall_entry) is forwarded correctly to the C handler.
 *
 * The return value @c 0x9527 is a recognizable magic number used to verify
 * that the return path (@c mov @c [esp+7*4],eax in @c syscall_entry) writes
 * the result back into the saved-EAX slot so @c popad restores it to the
 * caller's EAX register.
 *
 * @param arg   First system-call argument (caller passes in EBX).
 * @return      0x9527 — fixed sentinel to confirm the return-value path.
 */
uint32_t sys_test_syscall1(uint32_t arg)
{
    put_str("sys_test_syscall1()\n");
    put_str("argument 1: 0x");
    put_int(arg);
    put_str("\n");
    return 0x9527;
}

/**
 * @brief Stub system call — two arguments, returns a magic value.
 *
 * Registered at @ref SYS_TEST2.  Extends @ref sys_test_syscall1 by adding a
 * second argument (ECX) to verify that the second slot in the kernel stack
 * argument frame is also forwarded correctly.
 *
 * The return value @c 0x9528 (= 0x9527 + 1) distinguishes this call from
 * @ref sys_test_syscall1 when inspecting EAX after the system call returns.
 *
 * @param arg1  First system-call argument (caller passes in EBX).
 * @param arg2  Second system-call argument (caller passes in ECX).
 * @return      0x9528 — fixed sentinel to confirm the return-value path.
 */
uint32_t sys_test_syscall2(uint32_t arg1, uint32_t arg2)
{
    put_str("sys_test_syscall2()\n");
    put_str("argument 1: 0x");
    put_int(arg1);
    put_str("\n");
    put_str("argument 2: 0x");
    put_int(arg2);
    put_str("\n");
    return 0x9528;
}

/**
 * @brief Stub system call — three arguments, returns a magic value.
 *
 * Registered at @ref SYS_TEST3.  Extends @ref sys_test_syscall2 by adding a
 * third argument (EDX) to verify all three argument registers (EBX, ECX, EDX)
 * pushed by @c syscall_entry are forwarded correctly to the C handler.
 * This exercises the maximum argument count supported by the current ABI.
 *
 * The return value @c 0x9529 (= 0x9527 + 2) distinguishes this call from
 * the other stubs when inspecting EAX after the system call returns.
 *
 * @param arg1  First system-call argument (caller passes in EBX).
 * @param arg2  Second system-call argument (caller passes in ECX).
 * @param arg3  Third system-call argument (caller passes in EDX).
 * @return      0x9529 — fixed sentinel to confirm the return-value path.
 */
uint32_t sys_test_syscall3(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    put_str("sys_test_syscall3()\n");
    put_str("argument 1: 0x");
    put_int(arg1);
    put_str("\n");
    put_str("argument 2: 0x");
    put_int(arg2);
    put_str("\n");
    put_str("argument 3: 0x");
    put_int(arg3);
    put_str("\n");
    return 0x9529;
}

void syscall_init(void)
{
    TRACE_STR("syscall_init()\n");
    syscall_func[SYS_TEST0] = sys_test_syscall0;
    syscall_func[SYS_TEST1] = sys_test_syscall1;
    syscall_func[SYS_TEST2] = sys_test_syscall2;
    syscall_func[SYS_TEST3] = sys_test_syscall3;
}