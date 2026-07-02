/**
 * @file interrupt.c
 * @brief IDT setup, 8259A PIC initialization and interrupt status control.
 *
 * External function contracts are documented in interrupt.h.
 */
#include "interrupt.h"
#include "io.h"
#include "kernel.h"
#include "print.h"
#include "stddef.h"

//=========================
// debugging
//=========================

/** @def DEBUG
 *  @brief Enable (1) or disable (0) debug trace output. */
#define DEBUG (0)

/**
 * @def TRACE_STR(x)
 * @brief Print a string literal when #DEBUG is non-zero.
 * @param x Null-terminated string to print.
 */
#define TRACE_STR(x)                                                           \
    do {                                                                       \
        if (DEBUG)                                                             \
            put_str(x);                                                        \
    } while (0)

/**
 * @def TRACE_INT(x)
 * @brief Print an integer value when #DEBUG is non-zero.
 * @param x Integer value to print.
 */
#define TRACE_INT(x)                                                           \
    do {                                                                       \
        if (DEBUG)                                                             \
            put_int(x);                                                        \
    } while (0)

//=========================
// internal struct
//=========================

/**
 * @brief x86 32-bit interrupt gate descriptor (8 bytes).
 *
 * Layout:
 * @code
 *  63          48 47      40 39    32 31        16 15          0
 *  ┌─────────────┬──────────┬────────┬────────────┬────────────┐
 *  │  offset_2   │ attribute│  zero  │  selector  │  offset_1  │
 *  │(addr[31:16]) │P|DPL|type│  (=0)  │(code seg.) │(addr[15:0])│
 *  └─────────────┴──────────┴────────┴────────────┴────────────┘
 * @endcode
 */
struct idt_desc {
    uint16_t offset_1; /**< Handler address bits [15:0]. */
    uint16_t selector; /**< Segment selector for the handler's code segment. */
    uint8_t zero;      /**< Reserved; must be 0 per x86 specification. */
    uint8_t attribute; /**< Gate attribute byte: P | DPL | 0 | type. */
    uint16_t offset_2; /**< Handler address bits [31:16]. */
} __attribute__((packed));

/** @brief IDT table holding one descriptor per interrupt vector. */
static struct idt_desc idt_table[IDT_VEC_MAX + 1];

//=========================
// global variable
//=========================

/** @brief Per-vector handler function pointers.
 *
 *  Indexed by interrupt vector number. Each entry points to the
 *  high-level C handler installed for that vector, or NULL if none. */
void *intr_func[IDT_VEC_MAX + 1];

/** @brief Human-readable names for each interrupt vector.
 *
 *  Indexed by interrupt vector number. Used for diagnostic output
 *  in the general handler. Defaults to "unknown" for unregistered vectors. */
char *intr_name[IDT_VEC_MAX + 1];

//=========================
// internal functions
//=========================

/**
 * @brief Default handler invoked for every unregistered interrupt.
 *
 * Spurious IRQs from the 8259A PIC (vectors 0x27 and 0x2F) are silently
 * discarded. All other unhandled interrupts print a diagnostic banner
 * (vector number, symbolic name, and — for page faults — the faulting
 * virtual address from CR2) then spin-halt the system.
 *
 * @param vec  Interrupt vector number delivered by the CPU.
 */
static void intr_general_handler(uint8_t vec) {
    /* Spurious interrupt from master (0x27) or slave (0x2F) PIC — ignore */
    if (vec == 0x27 || vec == 0x2F) {
        return;
    }

    /* Print diagnostic banner */
    put_str("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    put_str("    vector number: 0x");
    put_int(vec);
    put_str("\n");
    put_str("    ");
    put_str(intr_name[vec]);
    put_str("\n");

    /* For page fault (vector 0x0E), read the faulting address from CR2 */
    if (0xE == vec) {
        int page_fault_vaddr = 0;
        asm("movl %%cr2, %0" : "=r"(page_fault_vaddr));
        put_str("    page fault addr is ");
        put_int(page_fault_vaddr);
        put_str("\n");
    }
    put_str("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

    /* Halt the system — unhandled exceptions are unrecoverable */
    while (1)
        ;
}

/**
 * @brief Populate a single 32-bit interrupt-gate IDT descriptor.
 *
 * The @p attr argument reuses the GDT segment-descriptor bit layout, where
 * the P bit lives at bit 15 and DPL occupies bits [14:13]:
 *
 * @code
 *  GDT attr (16-bit):   bit15=P  bits[14:13]=DPL  ...
 *  IDT attribute byte:  bit7 =P  bits[6:5]  =DPL  bit4=0  bits[3:0]=type
 * @endcode
 *
 * Extraction: @c (attr>>8)&0xE0 shifts P|DPL from bits [15:13] down to
 * bits [7:5], and the mask 0xE0 (11100000b) clears the lower five bits so
 * only the P and DPL fields remain.  The gate type @c 0x0E is then OR-ed in
 * to select a 32-bit interrupt gate, which causes the CPU to clear IF on entry.
 *
 * The IDT selector is always @ref SELECTOR_K_CODE (ring 0), so the handler
 * always executes in ring 0 regardless of the DPL value.  DPL only governs
 * which privilege levels may invoke the gate via a software @c int instruction:
 * - DPL=0 : only ring-0 code or CPU-generated exceptions may use this gate.
 * - DPL=3 : user-space code (CPL=3) may also trigger it via @c int (used for
 *            system calls at vector 0x80).
 *
 * @param p_dest  Pointer to the IDT descriptor entry to fill.
 * @param attr    GDT-format attribute word (e.g. @c GDT_P_1|GDT_DPL_0 or
 *                @c GDT_P_1|GDT_DPL_3); only bits [15:13] (P and DPL) are used.
 * @param func    Address of the interrupt handler routine.
 */
static void make_idt_table(struct idt_desc *p_dest, uint32_t attr, void *func) {
    /* Lower 16 bits of the handler address */
    p_dest->offset_1 = (uint32_t)func & 0x000FFFF;

    /* Kernel code segment selector (ring 0) */
    p_dest->selector = SELECTOR_K_CODE;

    /* Reserved field — must be zero per x86 specification */
    p_dest->zero = 0;

    /* Extract P bit and DPL from the GDT attribute (bits[7:5]),
     * then OR in 0x0E to set the gate type to 32-bit interrupt gate. */
    p_dest->attribute = (attr >> 8) & 0xE0 | 0x0E;

    /* Upper 16 bits of the handler address */
    p_dest->offset_2 = ((uint32_t)func & 0xFFFF0000) >> 16;
}

/**
 * @brief Initialize the Interrupt Descriptor Table (IDT) and load it into the
 * CPU.
 *
 * Steps performed:
 *  1. For every vector in [0, IDT_VEC_MAX], set @ref intr_name to "unknown"
 *     and @ref intr_func to NULL.
 *  2. For each vector that has an assembly entry stub in @c intr_entry[], call
 *     make_idt_table() with DPL=0 and install intr_general_handler() as the
 *     default C-level handler.  DPL=0 prevents user-space code from triggering
 *     hardware-interrupt or exception vectors via a software @c int
 * instruction.
 *  3. Install @c syscall_entry at vector 0x80 with DPL=3 so that user-space
 *     programs (CPL=3) may invoke kernel services via @c int @c 0x80 without
 *     triggering a #GP fault.  The CPU still switches to ring 0 on entry
 *     because the IDT selector always points to @ref SELECTOR_K_CODE.
 *  4. Assign human-readable names to the standard x86 exception vectors.
 *  5. Pack the IDT base address and limit into the IDTR format and execute @c
 * lidt.
 */
static void idt_init(void) {
    uint32_t attr;
    int intr_entry_size = sizeof(intr_entry) / 4;
    TRACE_STR("intr_enrty_size: ");
    TRACE_INT(intr_entry_size);
    TRACE_STR("\n");

    /* Step 1 & 2: fill descriptors and set default handler */
    for (int idx = 0; idx <= IDT_VEC_MAX; idx++) {
        intr_name[idx] = "unknown";
        intr_func[idx] = NULL;
        if (idx < intr_entry_size) {
            attr = GDT_P_1 + GDT_DPL_0;
            make_idt_table(&idt_table[idx], attr, intr_entry[idx]);
            intr_func[idx] = intr_general_handler;
        }
    }

    /* For system call 0x80 */
    attr = GDT_P_1 + GDT_DPL_3;
    make_idt_table(&idt_table[0x80], attr, syscall_entry);

    /* Step 3: assign symbolic names to standard x86 exception vectors */
    intr_name[0x00] = "#DE Divide Error";
    intr_name[0x01] = "#DB Debug Exception";
    intr_name[0x02] = "NMI Interrupt";
    intr_name[0x03] = "#BP Breakpoint Exception";
    intr_name[0x04] = "#OF Overflow Exception";
    intr_name[0x05] = "#BR BOUND Range Exceeded Exception";
    intr_name[0x06] = "#UD Invalid Opcode Exception";
    intr_name[0x07] = "#NM Device Not Available Exception";
    intr_name[0x08] = "#DF Double Fault Exception";
    intr_name[0x09] = "Coprocessor Segment Overrun";
    intr_name[0x0A] = "#TS Invalid TSS Exception";
    intr_name[0x0B] = "#NP Segment Not Present";
    intr_name[0x0C] = "#SS Stack Fault Exception";
    intr_name[0x0D] = "#GP General Protection Exception";
    intr_name[0x0E] = "#PF Page-Fault Exception";
    // intr_name[0x0F] reserved
    intr_name[0x10] = "#MF x87 FPU Floating-Point Error";
    intr_name[0x11] = "#AC Alignment Check Exception";
    intr_name[0x12] = "#MC Machine-Check Exception";
    intr_name[0x13] = "#XF SIMD Floating-Point Exception";
    intr_name[0x14] = "#VE Virtualization Exception";
    intr_name[0x15] = "#CP Control Protection Exception";
    // intr_name[0x16-0x1B] reserved
    intr_name[0x1C] = "#HV Hypervisor Injection Exception";
    intr_name[0x1D] = "#VC VMM Communication Exception";
    intr_name[0x1E] = "#SX Security Exception";
    // intr_name[0x1F] reserved

    /* Step 4: pack IDTR operand (base-16 | limit-16) and load into CPU */
    uint64_t idt_operand =
        ((sizeof(idt_table) - 1) | ((uint64_t)(uint32_t)idt_table << 16));
    asm volatile("lidt %0" : : "m"(idt_operand));
}

/**
 * @brief Initialize the dual 8259A Programmable Interrupt Controllers (PICs).
 *
 * Programs both the master and slave 8259A in cascade mode using the standard
 * four Initialization Command Words (ICW1-ICW4), then sets the Interrupt Mask
 * Registers so that only the IRQ lines K_OS actually services are unmasked:
 *
 * - Master IMR = 0xFA (1111_1010b): unmask IRQ0 (PIT timer) and IRQ2
 *   (cascade to slave); all other master lines masked.
 * - Slave  IMR = 0x33 (0011_0011b): unmask IRQ10 / IRQ11 / IRQ14 / IRQ15
 *   (the four IDE channels); IRQ8 / 9 / 12 / 13 masked.
 *
 * @par Initialization sequence (per PIC)
 * | Command Word | Value (M / S) | Meaning |
 * |:------------:|:-------------:|:-----------------------------------------------|
 * | ICW1         | 0x11 / 0x11   | Start init; edge-triggered; ICW4 required |
 * | ICW2         | 0x20 / 0x28   | Remap IRQ0-7 → vec 0x20-0x27, IRQ8-15 →
 * 0x28-0x2F | | ICW3         | 0x04 / 0x02   | Slave wired to master IRQ2 /
 * slave identity = 2 | | ICW4         | 0x01 / 0x01   | 8086/88 mode; manual
 * (normal) EOI               | | IMR          | 0xFA / 0x33   | See unmask
 * summary above                        |
 *
 * @note To enable another IRQ later, clear its bit in the master or slave IMR.
 *
 * @note The legacy 8259A conflicts with APIC on SMP systems.
 *       This implementation targets single-processor (UP) x86 machines only.
 *
 * @see outb()
 * @see PIC_M_CTRL, PIC_M_DATA, PIC_S_CTRL, PIC_S_DATA
 */
static void pic_init(void) {
    /* Master PIC */
    outb(PIC_M_CTRL, 0x11); /* ICW1: edge trigger mode, ICW4 needed */
    outb(PIC_M_DATA, 0x20); /* ICW2: vector address start from 0x20 */
    outb(PIC_M_DATA, 0x04); /* ICW3: IRQ2 for slave */
    outb(PIC_M_DATA, 0x01); /* ICW4: 8086 mode, normal EOI */

    /*  slave PIC */
    outb(PIC_S_CTRL, 0x11); /* ICW1: edge trigger mode, ICW4 needed */
    outb(PIC_S_DATA, 0x28); /* ICW2: vector address start from 0x28 */
    outb(PIC_S_DATA, 0x02); /* ICW3: IRQ2 for slave */
    outb(PIC_S_DATA, 0x01); /* ICW4: 8086 mode, normal EOI */

    /* IMR: unmask only the IRQs K_OS services (0 = enabled, 1 = masked) */
    outb(PIC_M_DATA, 0xFA); /* master: IRQ0 timer + IRQ2 cascade enabled */
    outb(PIC_S_DATA, 0x33); /* slave : IRQ10/11/14/15 (IDE channels) enabled */
}

//=========================
// external functions
//=========================

bool intr_get_status(void) {
    uint32_t eflags = 0;
    asm volatile("pushfl; popl %0" : "=g"(eflags));
    return (eflags & 0x200) ? true : false;
}

void intr_set_status(bool intr_status) {
    if (true == intr_status) {
        asm volatile("sti");
    } else {
        asm volatile("cli");
    }
}

void register_handler(uint8_t vec, void *func) { intr_func[vec] = func; }

void intr_init() {
    TRACE_STR("intr_init()\n");
    idt_init();
    pic_init();
}
