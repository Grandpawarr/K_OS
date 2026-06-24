/**
 * @file lock.h
 * @brief Synchronisation primitives for K_OS: spinlock, mutex, semaphore.
 *
 * The spinlock is the low-level building block (a single atomically-swapped
 * word) and guards the internal state of the higher-level mutex and
 * semaphore. The mutex and semaphore are blocking: contending threads are
 * parked on a wait list and woken via @ref kthread_unblock rather than
 * busy-spinning.
 */

#ifndef __KERNEL_INC_LOCK_H
#define __KERNEL_INC_LOCK_H
#include "stdint.h"
#include "list.h"
#include "thread.h"

//=========================
// define
//=========================

/**
 * @brief Atomically write @p x into @p *ptr and return the previous value.
 *
 * Wraps the x86 @c xchg instruction, which is implicitly locked, giving a
 * memory barrier and an atomic read-modify-write in one step. Used to claim a
 * spinlock without a separate compare.
 *
 * @param ptr Word to swap into.
 * @param x   Value to store.
 * @return The value @p *ptr held before the swap.
 */
static inline uint32_t xchg(volatile uint32_t *ptr, int x)
{
    asm volatile("xchg %0, %1"
                 : "=r"(x), "=m"(*ptr)
                 : "0"(x), "m"(*ptr)
                 : "memory");

    return x;
}

//=========================
// struct
//=========================

/** @brief Low-level busy-wait lock: 0 = free, 1 = held. */
struct spinlock
{
    uint32_t value; /**< Lock state; flipped atomically via @ref xchg. */
};

/** @brief Blocking mutual-exclusion lock with an owner and a wait list. */
struct mutex
{
    struct spinlock slock; /**< Guards @c owner and @c wait_list. */
    struct task_struct *owner; /**< Holding thread, or NULL if free. */
    struct list wait_list; /**< Threads blocked waiting for the lock. */
};

/** @brief Counting semaphore with a wait list for blocked threads. */
struct semaphore
{
    struct spinlock slock; /**< Guards @c count and @c wait_list. */
    uint32_t count;        /**< Available permits; 0 means callers block. */
    struct list wait_list; /**< Threads blocked on @ref sema_down. */
};

//=========================
// external variable
//=========================

//=========================
// function
//=========================

/**
 * @brief Initialise a spinlock to the free state.
 * @param lock Spinlock to initialise.
 */
void spin_lock_init(struct spinlock *lock);

/**
 * @brief Acquire a spinlock, yielding the CPU while it is contended.
 *
 * Spins on an atomic @ref xchg until it observes the lock free and claims it.
 * While waiting it calls @ref kthread_yield so other threads can make progress
 * (and release the lock) instead of burning the whole time slice.
 *
 * @param lock Spinlock to acquire.
 */
void spin_lock(struct spinlock *lock);

/**
 * @brief Release a spinlock.
 * @param lock Spinlock to release.
 */
void spin_unlock(struct spinlock *lock);

/**
 * @brief Initialise a mutex to the unlocked, unowned state.
 * @param mlock Mutex to initialise.
 */
void mutex_init(struct mutex *mlock);

/**
 * @brief Acquire the mutex, blocking until it is owned by the caller.
 *
 * If the lock is free the caller becomes the owner; otherwise it parks itself
 * on the wait list and blocks, retrying once woken.
 *
 * @param mlock Mutex to acquire.
 */
void mutex_lock(struct mutex *mlock);

/**
 * @brief Release the mutex and wake one waiting thread, if any.
 * @param mlock Mutex to release.
 */
void mutex_unlock(struct mutex *mlock);

/**
 * @brief Initialise a semaphore with @p value permits.
 * @param sema  Semaphore to initialise.
 * @param value Initial permit count (1 makes it a binary lock).
 */
void sema_init(struct semaphore *sema, uint32_t value);

/**
 * @brief Take one permit, blocking until one is available.
 *
 * Decrements the count when a permit is free; otherwise parks the caller on
 * the wait list and blocks, retrying once woken by @ref sema_up.
 *
 * @param sema Semaphore to take from.
 */
void sema_down(struct semaphore *sema);

/**
 * @brief Return one permit and wake one waiting thread, if any.
 * @param sema Semaphore to return to.
 */
void sema_up(struct semaphore *sema);

#endif
