/**
 * @file timer.c
 * @brief PIT timer driver implementation for K_OS.
 */

#include "timer.h"
#include "print.h"
#include "io.h"
#include "interrupt.h"
#include "thread.h"
#include "sched.h"

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
// global variable
//=========================
uint32_t jiffies; /**< @copydoc jiffies */

//=========================
// internal functions
//=========================

/**
 * @brief Program PIT counter 0 for the requested period.
 *
 * Uses rate-generator mode (mode 2), binary counting, LSB+MSB access.
 * The maximum expressible period is ~54924 us (0xFFFF ticks); anything
 * larger saturates to 0xFFFF.
 *
 * @param us Desired tick period in microseconds.
 */
static void timer_set(int us)
{
    uint8_t counter_ctl = COUNTER_CH_0 + COUNTER_AC_ALL + COUNTER_MODE_2 + COUNTER_BIN;
    outb(COUNTER_CTL_PORT, counter_ctl);

    /* 0xFFFF * 1000000 / 1193180 ≈ 54924 us — clamp to max if exceeded */
    uint16_t counter_data;
    if (us < 54924)
    {
        counter_data = US_TO_TICKS(us);
    }
    else
    {
        counter_data = 0xffff;
    }

    outb(CONTRER_PORT_0, (uint8_t)counter_data);
    outb(CONTRER_PORT_0, (uint8_t)(counter_data >> 8));
}

/**
 * @brief IRQ 0 handler — advances the jiffies counter.
 *
 * Registered via @ref register_handler and called on every timer tick.
 */
static void timer_handler(void)
{
    jiffies++;

    /* Debug*/
    // TRACE_STR("system jiffies: 0x");
    // TRACE_INT(jiffies);
    // TRACE_STR("\n");

    struct task_struct *cur_task = kthread_current();
    if (0 == cur_task->time_slice)
    {
        schedule();
    }
    else
    {
        cur_task->time_slice--;
    }
}

//=========================
// external functions
//=========================

/** @brief Convert milliseconds to jiffies. @see timer.h */
uint32_t msecs_to_jiffies(uint32_t ms)
{
    return (ms * 1000 / SYS_TICK_US);
}

/** @brief Convert microseconds to jiffies. @see timer.h */
uint32_t usecs_to_jiffies(uint32_t us)
{
    return (us / SYS_TICK_US);
}

/** @brief Convert jiffies to milliseconds. @see timer.h */
uint32_t jiffies_to_msecs(uint32_t js)
{
    return (js * SYS_TICK_US / 1000);
}

/** @brief Convert jiffies to microseconds. @see timer.h */
uint32_t jiffies_to_usecs(uint32_t js)
{
    return (js * SYS_TICK_US);
}

/** @brief Sleep for at least @p ms milliseconds. @see timer.h */
void msleep(uint32_t ms)
{
    uint32_t sleep_ticks = msecs_to_jiffies(ms);
    uint32_t start_ticks = jiffies;

    while (jiffies - start_ticks < sleep_ticks)
    {
        kthread_yield();
    }
}

/** @brief Sleep for at least @p us microseconds. @see timer.h */
void usleep(uint32_t us)
{
    uint32_t sleep_ticks = usecs_to_jiffies(us);
    uint32_t start_tick = jiffies;

    while (jiffies - start_tick < sleep_ticks)
    {
        kthread_yield();
    }
}

/** @brief Initialise the PIT and register IRQ 0. @see timer.h */
void timer_init(void)
{
    TRACE_STR("timer_init()\n");
    jiffies = 0;
    register_handler(0x20, timer_handler);
    timer_set(SYS_TICK_US);
}
