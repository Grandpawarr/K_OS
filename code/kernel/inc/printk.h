#ifndef __KERNEL_INC_PRINTK_H
#define __KERNEL_INC_PRINTK_H

/**
 * @brief Format a message and print it to the screen (printf-style).
 *
 * Thread-safe: serialized by an internal mutex. Call printk_init() once
 * before the first use. The formatted result must fit in a 1024-byte buffer.
 *
 * @param format printf-style format string.
 * @param ... Arguments matching @p format.
 */
void printk(const char *format, ...);

/**
 * @brief Initialize printk's internal lock. Must be called once at startup
 * before any printk() call.
 */
void printk_init(void);

#endif
