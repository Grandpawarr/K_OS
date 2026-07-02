#ifndef __KERNEL_INC_BITMAP_H
#define __KERNEL_INC_BITMAP_H
#include "stdint.h"
#include "stdbool.h"

/**
 * @file bitmap.h
 * @brief Bit-array allocator used by the memory pools (one bit = one unit,
 *        e.g. a 4 KB page). Bit value 1 = used, 0 = free.
 */

/**
 * @brief A plain bit array; the backing storage is supplied by the caller.
 */
struct bitmap
{
   uint32_t len;  /**< Length of @p bits in bytes (capacity = len * 8 bits). */
   uint8_t *bits; /**< Caller-provided storage; 1 = used, 0 = free. */
};

/**
 * @brief The bitmap_init() function initializes the value of bitmap.
 *
 * Only binds the storage and length; does NOT clear the bits —
 * call bitmap_reset() afterwards if a zeroed bitmap is required.
 *
 * @param btmp       Bitmap to initialize.
 * @param btmp_addr  Backing storage for the bit array.
 * @param len        Length of @p btmp_addr in bytes.
 */
void bitmap_init(struct bitmap *btmp, uint8_t *btmp_addr, uint32_t len);

/**
 * @brief The bitmap_reset() function resets bits to zero.
 *
 * @param btmp  Bitmap whose entire bit array is cleared (all bits = free).
 */
void bitmap_reset(struct bitmap *btmp);

/**
 * @brief The bitmap_set() function sets the value(0 or 1) to the bitmap.
 *
 * @param btmp      Bitmap to modify.
 * @param btmp_idx  Bit index to modify (0-based).
 * @param value     true sets the bit to 1 (used); false clears it to 0 (free).
 */
void bitmap_set(struct bitmap *btmp, uint32_t btmp_idx, bool value);

/**
 * @brief The bitmap_check() function checks the value of bitmap.
 *
 * @param btmp      Bitmap to inspect.
 * @param btmp_idx  Bit index to test (0-based).
 * @return true     The bit is 1 (used).
 * @return false    The bit is 0 (free).
 */
bool bitmap_check(struct bitmap *btmp, uint32_t btmp_idx);

/**
 * @brief The bitmap_acquire() function acquires the available contiguous bitmap.
 * if successful, then set the bitmap. Otherwise return -1.
 *
 * @param btmp  Pointer to the bitmap to search.
 * @param cnt   Number of contiguous free bits required.
 * @return      Starting bit index of the allocated region on success,
 *              or -1 if no contiguous free region of length @p cnt exists.
 */
int bitmap_acquire(struct bitmap *btmp, uint32_t cnt);

/**
 * @brief The bitmap_release() function releases contiguous bits
 * in the bitmap beginning at btmp_idx.
 *
 * Asserts that every released bit is currently set (double-free guard).
 *
 * @param btmp      Bitmap to modify.
 * @param btmp_idx  Starting bit index of the region to release.
 * @param cnt       Number of contiguous bits to clear.
 */
void bitmap_release(struct bitmap *btmp, uint32_t btmp_idx, uint32_t cnt);
#endif
