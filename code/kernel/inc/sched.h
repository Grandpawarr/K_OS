/**
 * @file sched.h
 * @brief Round-robin CPU scheduler interface for K_OS.
 */

#ifndef __KERNEL_INC_SCHED_H
#define __KERNEL_INC_SCHED_H
#include "thread.h"
#include "timer.h"

//=========================
// define
//=========================

/**
 * @brief Default round-robin time slice, expressed in scheduler ticks.
 *
 * Equals 100 ms divided by the @ref SYS_TICK_US tick period.
 */
#define SCHED_RR_TIME_SLICE (100 * 1000 / SYS_TICK_US)

//=========================
// struct
//=========================

//=========================
// external variable
//=========================

/**
 * @brief Low-level context switch (defined in switch.s).
 *
 * Saves the callee-saved registers and stack pointer of @p cur, then restores
 * those of @p next, so execution resumes wherever @p next last left off.
 *
 * @param cur  Outgoing thread whose context is saved.
 * @param next Incoming thread whose context is restored.
 */
extern void switch_to(struct task_struct *cur, struct task_struct *next);

//=========================
// function
//=========================

/**
 * @brief Pick the next runnable thread and switch to it.
 *
 * Requeues the current thread if it is still running, falls back to the idle
 * thread when the ready list is empty, then context-switches into the head of
 * @ref task_ready_list. Must be called with interrupts disabled.
 *
 * @todo Implement STF (shortest-task-first) and FF policies.
 */
void schedule(void);
#endif