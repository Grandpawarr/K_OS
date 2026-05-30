#ifndef __KERNEL_INC_INTERRUPT_H
#define _KERNEL_INC_INTERRUPT_H
#include "stdint.h"
#include "print.h"

//=========================
// define
//=========================
#define IDT_VEC_MAX (0x80)  /* system call vector number */
#define IDT_DESC_CNT (0x30) /* number of interrupt descriptors */

//=========================
// struct
//=========================

//=========================
// external variable
//=========================
extern void *intr_entry[IDT_DESC_CNT];

//=========================
// function
//=========================
/**
 * @brief Top-level interrupt subsystem initializer.
 *
 * Calls idt_init() to build and load the IDT. Must be called once during
 * kernel startup before interrupts are enabled.
 */
void intr_init(void);

/**
 * @brief Register a custom handler for a specific interrupt vector.
 *
 * Replaces whatever handler (including the default intr_general_handler())
 * is currently installed for @p vec with @p func.
 *
 * @param vec   Interrupt vector number to override (0 – IDT_VEC_MAX).
 * @param func  Pointer to the new handler function.
 */
void register_handler(uint8_t vector_no, void *function);
#endif
