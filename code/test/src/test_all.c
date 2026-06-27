#include "test.h"

void test_all(void) {
    /* print.h */
    // test_put_char();
    // test_put_str();
    // test_put_int();
    // test_set_cursor();
    // test_cls_screen();

    /* string.h */
    // test_string();

    /* bitmap.h */
    // test_bitmap();

    /* list.h */
    // test_list();

    /* memory.h */
    // test_memory(1, 0);

    /* interrupt.h */
    // test_intr();

    /* timer.h */
    // test_timer();

    /* thread.h and lock.h */
    // test_thread();

    /* printk.h and stdio.h */
    // test_printk();
    test_concurrency();

    /* assert.h */
    // assert(1 == 2);
}
