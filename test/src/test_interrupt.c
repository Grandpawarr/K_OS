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
}
