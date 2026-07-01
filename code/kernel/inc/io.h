/**
 * @file io.h
 * @author grandpawarr (twtnssh@gmail.com)
 * @brief Inline wrappers for x86 I/O port instructions (IN / OUT).
 *
 * All functions compile down to a single x86 instruction.
 * The @c volatile qualifier prevents the compiler from reordering or
 * eliminating port accesses, which is mandatory for memory-mapped and
 * port-mapped hardware registers.
 *
 * @version 0.1
 * @date 2026-06-30
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef __KERNEL_INC_IO_H
#define __KERNEL_INC_IO_H
#include "stdint.h"

/**
 * @brief Write one byte to an I/O port.
 *
 * Emits the @c outb instruction:
 * @code
 *   outb  ax, dx       ; AX = data, DX = port
 * @endcode
 *
 * @param port  16-bit I/O port address (0x0000 – 0xFFFF).
 * @param data  8-bit byte value to write.
 */
static inline void outb(uint16_t port, uint8_t data) {
    asm volatile("outb %b0, %w1" : : "a"(data), "d"(port));
}

/**
 * @brief Write one word to an I/O port.
 *
 * Emits the @c outw instruction:
 * @code
 *   outw  ax, dx       ; AX = data, DX = port
 * @endcode
 *
 * @param port  16-bit I/O port address (0x0000 – 0xFFFF).
 * @param data  16-bit word value to write.
 */
static inline void outw(uint16_t port, uint16_t data) {
    asm volatile("outw %w0, %w1" : : "a"(data), "d"(port));
}

/**
 * @brief Write a block of words to an I/O port (string output).
 *
 * Emits @c cld followed by @c rep @c outsw, which transfers @p cnt words
 * from the buffer at @p addr to @p port in ascending address order.
 * The direction flag is cleared first to guarantee forward traversal.
 *
 * @param port  16-bit I/O port address.
 * @param addr  Pointer to the source buffer; must hold at least @p cnt words.
 * @param cnt   Number of words to transfer.
 *
 * @note SI (source index) and CX (count) are updated by the instruction;
 *       the compiler constraints reflect this so the values are not cached
 *       in registers across the call.
 */
static inline void outsw(uint16_t port, const void *addr, uint32_t cnt) {
    asm volatile("cld; rep outsw" : "+S"(addr), "+c"(cnt) : "d"(port));
}

/**
 * @brief Read one byte from an I/O port.
 *
 * Emits the @c inb instruction:
 * @code
 *   inb   al, dx       ; AL ← [port DX]
 * @endcode
 *
 * @param port  16-bit I/O port address.
 * @return      Byte value read from the port.
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t data;
    asm volatile("inb %w1, %b0" : "=a"(data) : "d"(port));
    return data;
}

/**
 * @brief Read a block of words from an I/O port (string input).
 *
 * Emits @c cld followed by @c rep @c insw, which transfers @p cnt words
 * from @p port into the buffer at @p addr in ascending address order.
 * The @c "memory" clobber forces the compiler to treat the destination
 * buffer as modified, preventing stale cached reads.
 *
 * @param port  16-bit I/O port address.
 * @param addr  Pointer to the destination buffer; must have room for @p cnt
 * words.
 * @param cnt   Number of words to transfer.
 *
 * @note DI (destination index) and CX (count) are updated by the instruction.
 */
static inline void insw(uint16_t port, void *addr, uint32_t cnt) {
    asm volatile("cld; rep insw"
                 : "+D"(addr), "+c"(cnt)
                 : "d"(port)
                 : "memory");
}

#endif
