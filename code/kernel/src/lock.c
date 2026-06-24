/**
 * @file lock.c
 * @brief Implementation of K_OS synchronisation primitives.
 *
 * Public behaviour is documented in @ref lock.h; the notes here cover only the
 * internal mechanics, namely how waiters are parked and how the spinlock
 * serialises access to each blocking lock's state.
 */

#include "lock.h"
#include "stddef.h"

//=========================
// debugging
//=========================
#define DEBUG (1)
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
/**
 * @brief A thread parked on a lock's wait list.
 *
 * Allocated on the blocking caller's own stack: it stays valid because the
 * thread sleeps inside the locking call and only returns (unwinding the frame)
 * after it has been popped off the wait list and woken.
 */
struct lock_waiter
{
    struct task_struct *task;  /**< The blocked thread to wake. */
    struct list_elem wait_tag; /**< Link node on the lock's wait list. */
};

//=========================
// global variable
//=========================

//=========================
// internal functions
//=========================

//=========================
// external functions
//=========================
void spin_lock_init(struct spinlock *lock)
{
    lock->value = 0;
}

/**
 * @brief
 *
 * @param lock 1: lock , 0: free
 */
void spin_lock(struct spinlock *lock)
{
    while (1)
    {
        /* xchg returns the old value: 0 means it was free and we just took it */
        if (!xchg(&lock->value, 1))
        {
            break;
        }

        /* Contended: yield instead of spinning hot until the holder releases,
         * then loop back and retry the atomic claim. */
        while (lock->value)
        {
            kthread_yield();
        }
    }
}
void spin_unlock(struct spinlock *lock)
{
    lock->value = 0;
}

void mutex_init(struct mutex *mlock)
{
    spin_lock_init(&mlock->slock);
    mlock->owner = NULL;
    list_init(&mlock->wait_list);
}

void mutex_lock(struct mutex *mlock)
{
    struct task_struct *task = kthread_current();
    struct lock_waiter waiter;
    waiter.task = task;

    while (1)
    {
        // lock
        spin_lock(&mlock->slock);

        if (NULL == mlock->owner)
        {
            mlock->owner = task;

            // unlock
            spin_unlock(&mlock->slock);
            break;
        }
        else
        {
            /* Owned: park on the wait list, drop the spinlock, then block.
             * On wakeup we loop and re-check, since another thread may have
             * grabbed ownership first. */
            list_append(&mlock->wait_list, &waiter.wait_tag);

            // unlock
            spin_unlock(&mlock->slock);
            kthread_block(TASK_BLOCKED);
        }
    }
}

void mutex_unlock(struct mutex *mlock)
{
    // lock
    spin_lock(&mlock->slock);

    mlock->owner = NULL;

    // wakeup waiting task
    if (!list_empty(&mlock->wait_list))
    {
        struct list_elem *waiter_tag = list_pop(&mlock->wait_list);
        struct lock_waiter *waiter =
            elem2entry(struct lock_waiter, wait_tag, waiter_tag);
        struct task_struct *task = waiter->task;
        kthread_unblock(task);
    }

    // unlock
    spin_unlock(&mlock->slock);
}

void sema_init(struct semaphore *sema, uint32_t value)
{
    spin_lock_init(&sema->slock);
    sema->count = value;
    list_init(&sema->wait_list);
}

void sema_down(struct semaphore *sema)
{
    struct task_struct *task = kthread_current();
    struct lock_waiter waiter;
    waiter.task = task;

    while (1)
    {
        // lock
        spin_lock(&sema->slock);

        if (sema->count > 0)
        {
            sema->count--;

            // unlock
            spin_unlock(&sema->slock);
            break;
        }
        else
        {
            /* No permit: park on the wait list, drop the spinlock, then block.
             * Re-check on wakeup in case the freed permit was taken first. */
            list_append(&sema->wait_list, &waiter.wait_tag);

            // unlock
            spin_unlock(&sema->slock);
            kthread_block(TASK_BLOCKED);
        }
    }
}

void sema_up(struct semaphore *sema)
{
    // lock
    spin_lock(&sema->slock);

    sema->count++;

    /* Wakeup waiting task */
    if (!list_empty(&sema->wait_list))
    {
        struct list_elem *waiter_tag = list_pop(&sema->wait_list);
        struct lock_waiter *waiter =
            elem2entry(struct lock_waiter, wait_tag, waiter_tag);
        struct task_struct *task = waiter->task;
        kthread_unblock(task);
    }

    // unlock
    spin_unlock(&sema->slock);
}