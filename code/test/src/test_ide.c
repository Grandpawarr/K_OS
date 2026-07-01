#include "test.h"

void test_ide() {
    printk("Test ide\n");
    uint8_t aaa[512] = {0x95, 0x27};
    uint8_t bbb[512] = {0};
    uint8_t buf[512];
    struct ide_ptn *ptn =
        elem2entry(struct ide_ptn, ptn_tag, ptn_list.head.next);
    struct ide_hd *hd = ptn->hd;

    ide_read(hd, 1, buf, 1);
    printk("before write:0x%x, 0x%x\n", buf[0], buf[1]);

    ide_write(hd, 1, aaa, 1);

    ide_read(hd, 1, buf, 1);
    printk("after write:0x%x, 0x%x\n", buf[0], buf[1]);

    // recover
    ide_write(hd, 1, bbb, 1);
}