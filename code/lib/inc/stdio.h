/**
 * @file stdio.h
 * @brief Minimal formatted-output (sprintf family) for K_OS.
 *
 * Provides a hand-rolled @c stdarg replacement and a small @c sprintf that
 * understands @c %s, @c %c, @c %d and @c %x. The variadic macros assume the
 * x86 cdecl convention: arguments are pushed right-to-left and sit in
 * ascending memory above the format string, each occupying 4 bytes.
 */

#ifndef __LIB_INC_STDIO_H
#define __LIB_INC_STDIO_H
#include "stdint.h"
#include "stddef.h"

//=========================
// define
//=========================
// Variable Argument(va); Argument Pointer(ap)

/** @brief Point @p ap at the first variadic arg (just past fixed arg @p v). */
#define va_start(ap, v) ap = (va_list) & v

/** @brief Read the next arg as type @p t and advance @p ap by one slot (4B). */
#define va_arg(ap, t) *((t *)(ap += 4))

/** @brief Invalidate the argument pointer. */
#define va_end(ap) ap = NULL

/** @brief Cursor over the variadic argument list (raw byte pointer). */
typedef char *va_list;

//=========================
// function
//=========================

/**
 * @brief Format into @p str using an already-initialised arg pointer.
 *
 * Core formatter shared by @ref sprintf. Supports @c %s, @c %c, @c %d and
 * @c %x; any other character after @c % is dropped. Writes a terminating NUL
 * at the end of the output.
 *
 * @param str    Destination buffer (caller must size it; no bounds check).
 * @param format Format string.
 * @param ap     Argument pointer, e.g. from @ref va_start.
 * @return Number of characters written, excluding the NUL terminator.
 */
uint32_t vsprintf(char *str, const char *format, char *ap);

/**
 * @brief Format variadic arguments into @p buf, @c printf-style.
 *
 * @param buf    Destination buffer (caller must size it; no bounds check).
 * @param format Format string.
 * @param ...    Arguments matching the conversions in @p format.
 * @return Number of characters written, excluding the NUL terminator.
 */
uint32_t sprintf(char *buf, const char *format, ...);
#endif
