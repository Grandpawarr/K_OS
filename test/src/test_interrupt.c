#include "interrupt.h"

void test_intr(void)
{
    /* test page fault */
    // uint32_t *addr = (uint32_t *)0xC0200000;
    // uint32_t val = *addr;

    /* test divide zero */
    // int dzero = 9527 / 0;

    /* Test any vectors */
    // asm volatile("int $0x1F");

    /* enable interrupt */
    intr_set_status(true);

    /* disable interrupt */
    // intr_set_status(false);

    /*  get interrupt status */
    put_str("intr_get_status: ");
    bool ret = intr_get_status();
    put_int(ret);
    put_str("\n");
}
