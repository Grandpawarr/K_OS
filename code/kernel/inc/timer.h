/**
 * @file timer.h
 * @brief PIT (8253/8254) timer driver interface for K_OS.
 *
 * Wraps the i8253/i8254 Programmable Interval Timer and exposes
 * a jiffies-based time-keeping API.
 */

#ifndef __KERNEL_INC_TIMER_H
#define __KERNEL_INC_TIMER_H
#include "stdint.h"
#include "math.h"

//=========================
// define
//=========================

/** PIT input clock frequency in Hz (1.193180 MHz). */
#define INPUT_FREQUENCY (1193180)

/**
 * @brief Convert microseconds to PIT counter ticks (ceiling division).
 * @param us Microseconds.
 */
#define US_TO_TICKS(us) DIV_ROUND_UP(us *INPUT_FREQUENCY, 1000000)

/** System tick period in microseconds; sets the scheduler granularity. */
#define SYS_TICK_US (10)

/* counter data ports */
#define CONTRER_PORT_0 (0x40) /**< PIT counter 0 data port. */
#define CONTRER_PORT_1 (0x41) /**< PIT counter 1 data port. */
#define CONTRER_PORT_2 (0x42) /**< PIT counter 2 data port. */

/* counter control port and bit-field masks */
#define COUNTER_CTL_PORT (0x43)      /**< PIT mode/command register. */
#define COUNTER_CH_0 (0x00 << 6)     /**< Select channel 0. */
#define COUNTER_CH_1 (0x01 << 6)     /**< Select channel 1. */
#define COUNTER_CH_2 (0x02 << 6)     /**< Select channel 2. */
#define COUNTER_AC_LATCH (0x00 << 4) /**< Access mode: latch count. */
#define COUNTER_AC_LSB (0x01 << 4)   /**< Access mode: LSB only. */
#define COUNTER_AC_MSB (0x02 << 4)   /**< Access mode: MSB only. */
#define COUNTER_AC_ALL (0x03 << 4)   /**< Access mode: LSB then MSB. */
#define COUNTER_MODE_0 (0x00 << 1)   /**< Mode 0: interrupt on terminal count. */
#define COUNTER_MODE_1 (0x01 << 1)   /**< Mode 1: hardware re-triggerable one-shot. */
#define COUNTER_MODE_2 (0x02 << 1)   /**< Mode 2: rate generator. */
#define COUNTER_MODE_3 (0x03 << 1)   /**< Mode 3: square wave generator. */
#define COUNTER_MODE_4 (0x04 << 1)   /**< Mode 4: software triggered strobe. */
#define COUNTER_MODE_5 (0x05 << 1)   /**< Mode 5: hardware triggered strobe. */
#define COUNTER_BIN (0x00 << 0)      /**< Binary counting mode. */
#define COUNTER_BCD (0x01 << 0)      /**< BCD counting mode. */

//=========================
// external variable
//=========================

/**
 * Monotonically increasing tick counter.
 * Defined in timer.c; incremented by @c timer_handler on every IRQ 0 (every @ref SYS_TICK_US µs).
 * Reset to 0 by @ref timer_init.
 */
extern uint32_t jiffies;

//=========================
// function
//=========================

/**
 * @brief Convert milliseconds to jiffies.
 * @param ms Milliseconds.
 * @return Equivalent jiffies count.
 */
uint32_t msecs_to_jiffies(uint32_t ms);

/**
 * @brief Convert microseconds to jiffies.
 * @param us Microseconds.
 * @return Equivalent jiffies count.
 */
uint32_t usecs_to_jiffies(uint32_t us);

/**
 * @brief Convert jiffies to milliseconds.
 * @param js Jiffies count.
 * @return Equivalent milliseconds.
 */
uint32_t jiffies_to_msecs(uint32_t js);

/**
 * @brief Convert jiffies to microseconds.
 * @param js Jiffies count.
 * @return Equivalent microseconds.
 */
uint32_t jiffies_to_usecs(uint32_t js);

/**
 * @brief Sleep for at least @p ms milliseconds.
 *
 * Busy-waits by yielding the CPU until the requested number of jiffies has
 * elapsed, so other threads continue to run meanwhile.
 *
 * @param ms Duration to sleep, in milliseconds.
 */
void msleep(uint32_t ms);

/**
 * @brief Sleep for at least @p us microseconds.
 *
 * Yield-based busy-wait; resolution is bounded by @ref SYS_TICK_US, so
 * requests finer than one tick round up to a single tick.
 *
 * @param us Duration to sleep, in microseconds.
 */
void usleep(uint32_t us);

/**
 * @brief Initialize the PIT and register the timer IRQ handler.
 *
 * Sets counter 0 to rate-generator mode at @ref SYS_TICK_US period,
 * registers @c timer_handler on IRQ 0 (vector 0x20), and resets jiffies to 0.
 */
void timer_init(void);

#endif /* __KERNEL_INC_TIMER_H */
