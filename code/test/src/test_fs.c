#include "test.h"

void test_fs() {
    struct ide_ptn *ptn;
    struct list_elem *cur_elem;

    printk("%s +++\n", __func__);

    // print root_dir
    printk("root_dir:");
    struct dirent *dir = (struct dirent *)root_dir.inode->i_block[0];
    int dir_idx = 0;
    while (dir_idx < ROOT_DIR_MAX) {
        if (FT_UNKNOWN != (dir + dir_idx)->f_type) {
            printk("%s ", (dir + dir_idx)->filename);
        }
        dir_idx++;
    } // while
    printk("\n");

    // test fs_unmount(), fs_format(), fs_unmount()
    cur_elem = ptn_list.head.next;
    while (cur_elem != &ptn_list.tail) {
        ptn = elem2entry(struct ide_ptn, ptn_tag, cur_elem);

        fs_unmount(ptn);
        fs_format(ptn);
        fs_mount(ptn);

        // next partition
        cur_elem = cur_elem->next;
    }

    // print root_dir
    printk("root_dir:");
    dir_idx = 0;
    while (dir_idx < ROOT_DIR_MAX) {
        if (FT_UNKNOWN != (dir + dir_idx)->f_type) {
            printk("%s ", (dir + dir_idx)->filename);
        }
        dir_idx++;
    } // while
    printk("\n");

    printk("%s ---\n", __func__);
}