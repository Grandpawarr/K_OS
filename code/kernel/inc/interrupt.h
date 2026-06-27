#ifndef _KERNEL_INC_INTERRUPT_H
#define _KERNEL_INC_INTERRUPT_H
#include "print.h"
#include "stdbool.h"
#include "stdint.h"

//=========================
// define
//=========================

/** @brief Maximum interrupt vector number supported by the IDT (inclusive). */
#define IDT_VEC_MAX (0x80)

/** @brief Number of assembly entry stubs compiled into @ref intr_entry[]. */
#define IDT_DESC_CNT (0x30)

/** @defgroup pic_ports 8259A PIC I/O Port Addresses
 *  Fixed I/O addresses assigned by IBM PC/AT hardware.
 *  @{
 */
#define PIC_M_CTRL (0x20) /**< Master PIC — command / status register. */
#define PIC_M_DATA (0x21) /**< Master PIC — data register (IMR / ICW2-4). */
#define PIC_S_CTRL (0xA0) /**< Slave  PIC — command / status register. */
#define PIC_S_DATA (0xA1) /**< Slave  PIC — data register (IMR / ICW2-4). */
/** @} */

//=========================
// struct
//=========================

//=========================
// external variable
//=========================

/**
 * @brief Array of assembly-level interrupt entry stubs.
 *
 * Each element is the address of a per-vector trampoline generated in
 * assembly (interrupt_entry.asm).  The trampoline pushes the vector number,
 * saves general-purpose registers, then calls the corresponding C handler
 * stored in @ref intr_func[].
 *
 * Only the first @ref IDT_DESC_CNT entries are populated; vectors beyond
 * that range are left unmapped in the IDT.
 */
extern void *intr_entry[IDT_DESC_CNT];

/**
 * @brief Assembly entry point for the system-call gate (vector 0x80).
 *
 * Installed in the IDT at vector 0x80 with DPL=3, allowing user-space
 * programs (CPL=3) to enter the kernel via @c int @c 0x80 without
 * triggering a #GP fault.  Upon entry the CPU has already performed the
 * ring-3 → ring-0 privilege switch: CS is set to @ref SELECTOR_K_CODE
 * and the stack is switched to the kernel stack defined in the TSS.
 *
 * Unlike the generic stubs in @ref intr_entry[], this entry point is
 * dedicated to system-call dispatch and does not go through the
 * @ref intr_func[] table.
 *
 * @note Declared as returning @c void* to match the function-pointer
 *       convention used in this subsystem; the actual ABI is defined
 *       in the assembly source.
 */
extern void *syscall_entry(void);

//=========================
// function
//=========================

/**
 * @brief Query whether hardware interrupts are currently enabled.
 *
 * Reads the EFLAGS register via @c pushfl / @c popl and tests the
 * Interrupt Enable Flag (IF, bit 9).
 *
 * @return @c true  if IF = 1 (interrupts enabled).
 * @return @c false if IF = 0 (interrupts disabled).
 */
bool intr_get_status(void);

/**
 * @brief Enable or disable hardware interrupts.
 *
 * Issues @c sti when @p intr_status is @c true and @c cli when @c false.
 * The change takes effect on the instruction immediately following the call.
 *
 * @param intr_status  @c true to enable interrupts; @c false to disable them.
 *
 * @note This function does not save the previous interrupt state.
 *       Use @ref intr_get_status() before calling if the old state must
 *       be restored later.
 */
void intr_set_status(bool intr_status);

/**
 * @brief Initialize the interrupt subsystem.
 *
 * Performs the following steps in order:
 *  1. Calls idt_init() to fill the IDT with default handlers and load it
 *     into the CPU via @c lidt.
 *  2. Calls pic_init() to initialize the dual 8259A PICs and mask all IRQs.
 *
 * Must be called once during kernel startup before enabling interrupts.
 */
void intr_init(void);

/**
 * @brief Install a C-level handler for a specific interrupt vector.
 *
 * Overwrites the entry in the @ref intr_func[] table at index @p vector_no
 * with @p function.  The assembly trampoline for that vector will call
 * @p function on the next occurrence of the interrupt.
 *
 * @param vector_no  Interrupt vector number (0 – @ref IDT_VEC_MAX).
 * @param function   Pointer to the handler; prototype must be
 *                   @code void handler(uint8_t vec); @endcode
 *
 * @note No bounds check is performed — the caller must ensure
 *       @p vector_no ≤ @ref IDT_VEC_MAX.
 */
void register_handler(uint8_t vector_no, void *function);

#endif