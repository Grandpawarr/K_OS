/**
 * @file stdio.c
 * @brief Implementation of K_OS formatted output (see @ref stdio.h).
 *
 * The public @c sprintf/@c vsprintf contract lives in the header; this file
 * documents only the internals, chiefly the recursive @ref itoa digit
 * conversion used by the @c %d and @c %x conversions.
 */

#include "stdio.h"
#include "string.h"

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

//=========================
// global variable
//=========================

//=========================
// internal functions
//=========================
/**
 * @brief Convert an unsigned integer to its digit string in a given base.
 *
 * Recurses on the quotient first so the most-significant digit is written
 * last-but-emitted-first, producing the digits in natural order. Each digit is
 * appended at @c *buf_ptr_addr, which is advanced past it so the caller's
 * cursor ends just after the number. No NUL terminator is written.
 *
 * @param value         Value to convert.
 * @param buf_ptr_addr  Address of the output cursor; advanced per digit.
 * @param base          Numeric base (10 for @c %d, 16 for @c %x).
 */
static void itoa(uint32_t value, char **buf_ptr_addr, uint8_t base)
{
    uint32_t m = value % base; // remainder (the lowest digit at present)
    uint32_t i = value / base; // quotient (the remaining unprocessed number)

    if (i)
    {
        itoa(i, buf_ptr_addr, base);
    }
    if (m < 10)
    {
        // 0 ~ 9
        *((*buf_ptr_addr)++) = m + '0';
    }
    else
    {
        // 11 ~ 15 : A ~ F
        *((*buf_ptr_addr)++) = m - 10 + 'A';
    }
}

//=========================
// external functions
//=========================
uint32_t vsprintf(char *str, const char *format, char *ap)
{
    char *out = str;
    char c = (*format);

    /* Argument type */
    int32_t arg_int;
    char *arg_str;

    while (c)
    {
        /* Plain text */
        if (c != '%')
        {
            *(out++) = c;
            c = *(++format);
            continue;
        }

        /* Decode the format*/
        c = *(++format);
        switch (c)
        {
        case 's': // %s string
            arg_str = va_arg(ap, char *);
            strcpy(out, arg_str);
            out += strlen(arg_str); // move pointer out
            break;

        case 'c': // %c char
            *(out++) = va_arg(ap, char);
            break;

        case 'd': // %d decimal
            arg_int = va_arg(ap, int);
            if (arg_int < 0)
            {
                arg_int = 0 - arg_int; // convert to a positive number
                *(out++) = '-';
            }
            itoa(arg_int, &out, 10);
            break;

        case 'x': // %x hexadecimal
            arg_int = va_arg(ap, int);
            itoa(arg_int, &out, 16);
            break;
        }

        c = *(++format);
    }

    *out = '\0';

    return out - str;
}

uint32_t sprintf(char *buf, const char *format, ...)
{
    va_list args;
    uint32_t retval;

    va_start(args, format);
    retval = vsprintf(buf, format, args);
    va_end(args);

    return retval;
}