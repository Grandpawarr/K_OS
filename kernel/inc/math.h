#ifndef __KERNEL_INC_MATH_H
#define __KERNEL_INC_MATH_H

/**
 * @brief Integer division, rounding up to the nearest multiple of STEP.
 *
 * @param X     Dividend.
 * @param STEP  Divisor.
 * @return Ceiling of (X / STEP).
 */
#define DIV_ROUND_UP(X, STEP) ((X + STEP - 1) / (STEP))

#endif