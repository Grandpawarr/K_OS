/**
 * @file timer.c
 * @brief PIT timer driver implementation for K_OS.
 */

#include "timer.h"
#include "print.h"
#include "io.h"
#include "interrupt.h"


//=========================
// debugging
//=========================
#define DEBUG (1)
#define TRACE_STR(x) do {if(DEBUG) put_str(x);} while(0)
#define TRACE_INT(x) do {if(DEBUG) put_int(x);} while(0)

//=========================
// global variable
//=========================
uint32_t jiffies;

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
    } else {
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
}

//=========================
// external functions
//=========================

uint32_t msecs_to_jiffies(uint32_t ms)
{
    return (ms * 1000 / SYS_TICK_US);
}

uint32_t usecs_to_jiffies(uint32_t us)
{
    return (us / SYS_TICK_US);
}

uint32_t jiffies_to_msecs(uint32_t js)
{
    return (js * SYS_TICK_US / 1000);
}

uint32_t jiffies_to_usecs(uint32_t js)
{
    return (js * SYS_TICK_US);
}

void timer_init(void)
{
    TRACE_STR("timer_init()\n");
    jiffies = 0;
    register_handler(0x20, timer_handler);
    timer_set(SYS_TICK_US);
}
