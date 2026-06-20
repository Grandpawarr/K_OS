#include "test.h"

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

    /* test system call */
    // extern void test_syscall0(void);
    // extern int test_syscall1(int a);
    // extern int test_syscall2(int a, int b);
    // extern int test_syscall3(int a, int b, int c);

    // put_str("test syscall0()\n");
    // test_syscall0();
    // int sysret;

    // put_str("test_syscall1()\n");
    // sysret = test_syscall1(0x111);
    // put_str("ret= 0x");
    // put_int(sysret);
    // put_str("\n");

    // put_str("test_syscall2()\n");
    // sysret = test_syscall2(0x111, 0x222);
    // put_str("ret= 0x");
    // put_int(sysret);
    // put_str("\n");

    // put_str("test_syscall3()\n");
    // sysret = test_syscall3(0x111, 0x222, 0x333);
    // put_str("ret= 0x");
    // put_int(sysret);
    // put_str("\n");
}
