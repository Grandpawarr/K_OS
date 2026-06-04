#include "interrupt.h"
#include "kernel.h"
#include "stddef.h"
#include "print.h"

//=========================
// debugging
//=========================

/** @def DEBUG
 *  @brief Enable (1) or disable (0) debug trace output. */
#define DEBUG (1)

/**
 * @def TRACE_STR(x)
 * @brief Print a string literal when #DEBUG is non-zero.
 * @param x Null-terminated string to print.
 */
#define TRACE_STR(x)    \
    do                  \
    {                   \
        if (DEBUG)      \
            put_str(x); \
    } while (0)

/**
 * @def TRACE_INT(x)
 * @brief Print an integer value when #DEBUG is non-zero.
 * @param x Integer value to print.
 */
#define TRACE_INT(x)    \
    do                  \
    {                   \
        if (DEBUG)      \
            put_int(x); \
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
struct idt_desc
{
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
static void intr_general_handler(uint8_t vec)
{
    /* Spurious interrupt from master (0x27) or slave (0x2F) PIC — ignore */
    if (vec == 0x27 || vec == 0x2F)
    {
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
    if (0xE == vec)
    {
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
 * The attribute byte is composed as:
 *   - bits[7:5] : P (present) and DPL, extracted from @p attr via (attr >> 8) & 0xE0
 *   - bits[3:0] : gate type 0x0E = 32-bit interrupt gate (clears IF on entry)
 *
 * @param p_dest  Pointer to the IDT descriptor entry to fill.
 * @param attr    GDT-format attribute word (e.g. GDT_P_1 | GDT_DPL_0);
 *                only the P bit and DPL fields are used.
 * @param func    Address of the interrupt handler routine.
 */
static void make_idt_table(struct idt_desc *p_dest, uint32_t attr, void *func)
{
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
 * @brief Initialize the Interrupt Descriptor Table (IDT) and load it into the CPU.
 *
 * Steps performed:
 *  1. For every vector in [0, IDT_VEC_MAX], set @ref intr_name to "unknown" and
 *     @ref intr_func to NULL.
 *  2. For each vector that has an assembly entry stub in @c intr_entry[], call
 *     make_idt_table() to fill the descriptor and install intr_general_handler()
 *     as the default C-level handler.
 *  3. Assign human-readable names to the standard x86 exception vectors.
 *  4. Pack the IDT base address and limit into the IDTR format and execute @c lidt.
 */
static void idt_init(void)
{
    uint32_t attr;
    int intr_entry_size = sizeof(intr_entry) / 4;
    TRACE_STR("intr_enrty_size: ");
    TRACE_INT(intr_entry_size);
    TRACE_STR("\n");

    /* Step 1 & 2: fill descriptors and set default handler */
    for (int idx = 0; idx <= IDT_VEC_MAX; idx++)
    {
        intr_name[idx] = "unkown";
        intr_func[idx] = NULL;
        if (idx < intr_entry_size)
        {
            attr = GDT_P_1 + GDT_DPL_0;
            make_idt_table(&idt_table[idx], attr, intr_entry[idx]);
            intr_func[idx] = intr_general_handler;
        }
    }

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
    uint64_t idt_operand = ((sizeof(idt_table) - 1) | ((uint64_t)(uint32_t)idt_table << 16));
    asm volatile("lidt %0" : : "m"(idt_operand));
}

//=========================
// external functions
//=========================

void register_handler(uint8_t vec, void *func)
{
    intr_func[vec] = func;
}

void intr_init()
{
    TRACE_STR("intr_init()\n");
    idt_init();
}
