#ifndef __KERNEL_INC_BITMAP_H
#define __KERNEL_INC_BITMAP_H
#include "stdint.h"
#include "stdbool.h"
struct bitmap {
   uint32_t len; // unit is byte
   uint8_t* bits;
};

/**
 * @brief The bitmap_init() function initializes the value of bitmap.
 * 
 * @param btmp 
 * @param btmp_addr 
 * @param len 
 */
void    bitmap_init(struct bitmap* btmp, uint8_t* btmp_addr, uint32_t len);

/**
 * @brief The bitmap_reset() function resets bits to zero.
 * 
 * @param btmp 
 */
void    bitmap_reset(struct bitmap* btmp);

/**
 * @brief The bitmap_set() function sets the value(0 or 1) to the bitmap.
 * 
 * @param btmp 
 * @param btmp_idx 
 * @param value 
 */
void    bitmap_set(struct bitmap* btmp, uint32_t btmp_idx, bool value);

/**
 * @brief The bitmap_check() function checks the value of bitmap.
 * 
 * @param btmp 
 * @param btmp_idx 
 * @return true 
 * @return false 
 */
bool    bitmap_check(struct bitmap* btmp, uint32_t btmp_idx);

/**
 * @brief The bitmap_acquire() function acquires the available contiguous bitmap.
 * if successful, then set the bitmap. Otherwise return -1.
 * 
 * @param btmp 
 * @param cnt 
 * @return int 
 */
int     bitmap_acquire(struct bitmap* btmp, uint32_t cnt);

/**
 * @brief The bitmap_release() function releases contiguous bits
 * in the bitmap beginning at btmp_idx.
 * 
 * @param btmp 
 * @param btmp_idx 
 * @param cnt 
 */
void    bitmap_release(struct bitmap* btmp, uint32_t btmp_idx, uint32_t cnt);
#endif