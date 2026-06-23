/**
 * @file thread.h
 * @brief Kernel thread (PCB) definitions and lifecycle API for K_OS.
 *
 * Each kernel thread owns a single page. The Process Control Block
 * (@ref task_struct) lives at the base of that page, while the thread's
 * kernel stack grows downwards from the top of the same page. Because the
 * PCB is page-aligned, the current thread can be recovered from @c esp by
 * masking off the low 12 bits (see @ref kthread_current).
 */

#ifndef __KERNEL_INC_THREAD_H
#define __KERNEL_INC_THREAD_H
#include "list.h"

//=========================
// define
//=========================

/** @brief Entry-point signature for a kernel thread payload. */
typedef void (*threadfn)(void *);

/**
 * @enum task_status
 * @brief Lifecycle state of a kernel thread, as tracked by the scheduler.
 */
enum task_status
{
    TASK_RUNNING, /**< Currently executing on the CPU. */
    TASK_READY,   /**< Runnable and queued on the ready list. */
    TASK_BLOCKED, /**< Blocked, awaiting an explicit unblock. */
    TASK_WAITING, /**< Waiting on an event/resource. */
    TASK_HANGING, /**< Suspended. */
    TASK_DIED     /**< Terminated; PCB pending teardown. */
};

//=========================
// struct
//=========================

/**
 * @struct task_struct
 * @brief Process Control Block (PCB) for a single kernel thread.
 *
 * @note Allocated at the base of the thread's page. @c kstack must remain
 *       the first member so that @ref switch_to can save/restore the stack
 *       pointer by writing through the PCB address. @c stack_magic must
 *       remain the last member so it can act as a stack-overflow canary.
 */
struct task_struct
{
    void *kstack; /**< Saved kernel stack pointer. MUST be the first member. */

    /* Scheduler related */
    enum task_status status;       /**< Current lifecycle state. */
    uint32_t time_slice;           /**< Remaining ticks before preemption. */
    struct list_elem task_tag;     /**< Link node on the scheduler ready list. */
    struct list_elem task_all_tag; /**< Link node on the all-tasks list. */

    /* Task name */
    char name[16]; /**< Human-readable thread name (NUL-terminated). */

    uint32_t stack_magic; /**< Overflow canary. MUST be the last member. */
};

/**
 * @struct kstack_switch
 * @brief Represents the thread's stack layout during a context switch.
 *
 * @details
 * This structure serves two critical purposes in the OS scheduler:
 * 1. **Context Snapshot**: For an existing thread, it maps exactly to the stack frame
 * created by the `switch_to` assembly routine after pushing callee-saved registers.
 * 2. **Fake Stack Frame (Thread Bootstrapping)**: For a newly created thread, the OS
 * crafts this exact memory layout at the top of the new stack. When `switch_to`
 * executes `ret`, it "returns" into `kernel_thread()`, seamlessly starting the new task.
 *
 * **Stack Memory Layout (x86 grows downwards):**
 * @verbatim
 * High Address ->  +----------------+
 * |     fn_arg     | Argument 2 for kernel_thread
 * +----------------+
 * |       fn       | Argument 1 for kernel_thread
 * +----------------+
 * | unused_retaddr | Dummy return address
 * +----------------+
 * |    kthread     | EIP popped by switch_to (Entry point)
 * +----------------+
 * |      esi       | Pushed by switch_to
 * +----------------+
 * |      edi       | Pushed by switch_to
 * +----------------+
 * |      ebx       | Pushed by switch_to
 * Low Address  ->  +----------------+
 * (esp)          |      ebp       | Pushed by switch_to
 * +----------------+
 * @endverbatim
 */
struct kstack_switch
{
    /** * @name Callee-saved registers (System V ABI for x86 32-bit)
     * @brief The `switch_to` function must preserve these registers.
     * * They are pushed in reverse order (esi, edi, ebx, ebp) so `ebp`
     * sits at the lowest address, aligning perfectly with this struct.
     */
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;

    /* --- Execution Entry Point ---
     * When `switch_to` executes `ret`, this value is popped into EIP.
     * - Existing thread: Holds the return address back to the caller of `switch_to`.
     * - New thread: Holds the address of the `kernel_thread` wrapper function.
     */
    void (*kthread)(threadfn fn, void *func_arg);

    /* --- Fake Stack Frame for kernel_thread() ---
     * The following members act as the standard C calling convention stack frame
     * for the `kernel_thread` function when a new thread is spawned.
     */

    /* * Dummy return address. `kernel_thread` should never return normally
     * (it should call a thread_exit function instead). Thus, this value is unused.
     */
    void *unused_retaddr;

    /* * Argument 1 for `kernel_thread`: The actual function pointer to the
     * payload/business logic the thread is meant to execute.
     */
    threadfn fn;

    /* * Argument 2 for `kernel_thread`: The parameter passed into the payload function.
     */
    void *fn_arg;
};

//=========================
// external variable
//=========================

/** @brief Ready queue of runnable threads, consumed by @ref schedule. */
extern struct list task_ready_list;

/** @brief List of every thread regardless of state. */
extern struct list task_all_list;

/** @brief The idle thread, run when no other thread is ready. */
extern struct task_struct *g_idle_task;

//=========================
// external functions
//=========================

/**
 * @brief Get the PCB of the currently running thread.
 *
 * Derived by masking the live @c esp down to its page boundary, where the
 * PCB resides.
 *
 * @return Pointer to the running thread's @ref task_struct.
 */
struct task_struct *kthread_current(void);

/**
 * @brief Allocate and initialise a new (not-yet-runnable) kernel thread.
 *
 * Reserves one page for the PCB and kernel stack, then forges a
 * @ref kstack_switch frame so that the thread's first context switch
 * "returns" into @c kernel_thread, which in turn invokes @p fn(@p fn_arg).
 * The thread is created in an unqueued state; call @ref kthread_run to make
 * it schedulable.
 *
 * @param fn     Payload function the thread will execute.
 * @param fn_arg Argument forwarded to @p fn.
 * @param name   Thread name (copied into the PCB; up to 15 chars).
 * @return Pointer to the newly created @ref task_struct.
 */
struct task_struct *kthread_create(threadfn fn, void *fn_arg, char *name);

/**
 * @brief Make a thread schedulable by enqueuing it on the ready/all lists.
 *
 * Marks the thread @c TASK_READY, replenishes its time slice, and appends it
 * to both @ref task_ready_list and @ref task_all_list. Runs with interrupts
 * disabled and restores the prior interrupt state on return.
 *
 * @param task Thread to enqueue.
 */
void kthread_run(struct task_struct *task);

/**
 * @brief Terminate the calling thread and yield the CPU permanently.
 *
 * Marks the caller @c TASK_DIED, unlinks it from both scheduler lists, frees
 * its page (unless it is the main thread), then reschedules. Does not return.
 */
void kthread_exit(void);

/**
 * @brief Block the calling thread and reschedule.
 *
 * Sets the caller's status to @p stat and invokes @ref schedule. No effect
 * unless @p stat is @c TASK_BLOCKED, @c TASK_WAITING, or @c TASK_HANGING.
 *
 * @param stat Blocked state to transition into.
 */
void kthread_block(enum task_status stat);

/**
 * @brief Move a blocked thread back to the front of the ready queue.
 *
 * No effect unless @p task is currently blocked, waiting, or hanging. The
 * thread is pushed to the head of @ref task_ready_list so it runs soon, but
 * the current thread is not preempted here.
 *
 * @param task Thread to wake.
 */
void kthread_unblock(struct task_struct *task);

/**
 * @brief Voluntarily relinquish the CPU, staying runnable.
 *
 * Appends the caller to the tail of @ref task_ready_list and reschedules.
 */
void kthread_yield(void);

/**
 * @brief Bootstrap the threading subsystem.
 *
 * Initialises the scheduler lists, creates and enqueues the idle thread, and
 * promotes the current execution context into the @c main thread PCB.
 */
void kthread_init(void);
#endif