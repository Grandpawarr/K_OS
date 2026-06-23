/**
 * @file sched.c
 * @brief Round-robin scheduler implementation for K_OS.
 */

#include "sched.h"
#include "interrupt.h"
#include "thread.h"

//=========================
// debugging
//=========================
#define DEGUG (0)
#define TRACE_STR(x)    \
    do                  \
    {                   \
        if (DEGUG)      \
            put_str(x); \
    } while (0)
#define TRACE_INT(x)    \
    do                  \
    {                   \
        if (DEGUG)      \
            put_int(x); \
    } while (0)

//=========================
// internal struct
//=========================

//=========================
// global variable
//=========================

//=========================
// internal functions
//=========================

//=========================
// external functions
//=========================

void schedule(void)
{
    /* Round-Robin Scheduling */
    struct task_struct *curTask = kthread_current();
    if (TASK_RUNNING == curTask->status)
    {
        curTask->status = TASK_READY;
        curTask->time_slice = SCHED_RR_TIME_SLICE; // reset time slice
        list_append(&task_ready_list, &curTask->task_tag);
    }

    /* Check the ready list */
    if (list_empty(&task_ready_list))
    {
        kthread_unblock(g_idle_task);
    }

    /* Pick up the task from ready list */
    struct list_elem *nextTaskTag = list_pop(&task_ready_list);
    struct task_struct *nextTask = elem2entry(struct task_struct, task_tag, nextTaskTag);
    nextTask->status = TASK_RUNNING;

    TRACE_STR("next task: ");
    TRACE_STR(nextTask->name);
    TRACE_STR("\n");

    switch_to(curTask, nextTask);
}