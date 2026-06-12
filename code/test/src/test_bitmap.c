#include "bitmap.h"
#include "print.h"

void test_bitmap(void)
{
    uint8_t test[4] = {0x1, 0x2, 0x3, 0x4};
    struct bitmap btmp;

    // test bitmap_init()
    put_str("test bitmap_init\n");
    bitmap_init(&btmp, test, 4);
    put_str("bitmap: "); // 1234
    put_int(test[0]);
    put_int(test[1]);
    put_int(test[2]);
    put_int(test[3]);
    put_char('\n');

    // test bitmap_reset()
    put_str("test bitmap_reset\n");
    bitmap_reset(&btmp);
    put_str("bitmap: "); // 0000
    put_int(test[0]);
    put_int(test[1]);
    put_int(test[2]);
    put_int(test[3]);
    put_char('\n');

    // test bitmap_set()
    put_str("test bitmap_set\n");
    bitmap_set(&btmp, 12, true);
    put_str("bitmap[0]: "); // 00
    put_int(test[0]);
    put_str("\nbitmap[1]: "); // 10
    put_int(test[1]);
    put_str("\nbitmap[2]: "); // 00
    put_int(test[2]);
    put_str("\nbitmap[3]: "); // 00
    put_int(test[3]);
    put_char('\n');

    // test bitmap_check()
    put_str("test bitmap_check\n");
    int value = bitmap_check(&btmp, 11);
    put_str("bitmap[11]= "); // 0
    put_int(value);
    put_char('\n');

    value = bitmap_check(&btmp, 12);
    put_str("bitmap[12]= "); // 1
    put_int(value);
    put_char('\n');

    // test bitmap_acquire()
    put_str("test bitmap_acquire\n");
    put_str("acquire 1, ");
    int idx = bitmap_acquire(&btmp, 1);
    put_str("free_idx: "); // 0
    put_int(idx);
    put_char('\n');

    put_str("acquire 11, ");
    idx = bitmap_acquire(&btmp, 11);
    put_str("free_idx: "); // 1
    put_int(idx);
    put_char('\n');

    put_str("acquire 19, ");
    idx = bitmap_acquire(&btmp, 19);
    put_str("free_idx: "); // D(13)
    put_int(idx);
    put_char('\n');

    put_str("acquire 12, ");
    idx = bitmap_acquire(&btmp, 12);
    put_str("free_idx: "); // FFFFFFFF(-1)
    put_int(idx);
    put_char('\n');

    put_str("test bitmap_release\n");
    bitmap_release(&btmp, 16, 4);

    put_str("bitmap[0]: "); // FF
    put_int(test[0]);
    put_str("\nbitmap[1]: "); // FF
    put_int(test[1]);
    put_str("\nbitmap[2]: "); // F0
    put_int(test[2]);
    put_str("\nbitmap[3]: "); // FF
    put_int(test[3]);
    put_char('\n');
}
