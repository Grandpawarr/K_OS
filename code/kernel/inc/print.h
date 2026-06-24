/**
 * @file print.h
 * @brief Low-level VGA text-mode console output for K_OS.
 *
 * Thin C declarations over the assembly routines in @ref print.s. These write
 * directly to the 80x25 VGA text buffer and drive the hardware cursor; there
 * is no formatting here (see @ref stdio.h for @c sprintf-style formatting,
 * which ultimately feeds strings to @ref put_str).
 */

#ifndef __KERNEL_INT_PRINT_H
#define __KERNEL_INT_PRINT_H
#include "stdint.h"

/**
 * @brief Print one character at the cursor, advancing it.
 *
 * Handles backspace (0x08), carriage return (0x0d) and line feed (0x0a), and
 * scrolls the screen when the cursor runs past the last line.
 *
 * @param char_asci ASCII code of the character to print.
 */
void put_char(uint8_t char_asci);

/**
 * @brief Print a NUL-terminated string at the cursor.
 * @param message String to print.
 */
void put_str(char* message);

/**
 * @brief Print a 32-bit integer as hexadecimal, with leading zeros stripped.
 * @param num Value to print.
 */
void put_int(uint32_t num);

/**
 * @brief Move the hardware cursor to a linear screen position.
 * @param cursor_pos Cell index (0..1999, row-major over the 80x25 grid).
 */
void set_cursor(uint32_t cursor_pos);

/**
 * @brief Clear the screen and reset the cursor to the top-left.
 */
void cls_screen(void);
#endif
