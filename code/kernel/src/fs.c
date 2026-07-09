#include "fs.h"
#include "assert.h"
#include "math.h"
#include "memory.h"
#include "printk.h"
#include "string.h"

/**
 * @file fs.c
 * @brief Minimal inode-based file system: formatting, mounting and init.
 *
 * Each partition is laid out on disk as (LBAs relative to @c ptn->start_lba):
 *
 *     +0                boot sector (reserved)
 *     +1                super block
 *     +2                inode bitmap   (inode_btmp_sec sectors)
 *     +3 ...            inode table    (inode_table_sec sectors)
 *     ...               block bitmap   (blk_btmp_sec sectors)
 *     blk_lba ...       data blocks    (BLOCK_SIZE bytes each)
 *
 * Inodes hold 13 direct block pointers and no indirection. The root directory
 * is memory-resident (@ref root_dir / @ref root_blk); every mounted partition
 * is added to it as a sub-directory entry.
 */

//=========================
// debugging
//=========================
#define DEBUG

#ifdef DEBUG
    #define pr_debug(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
    #define pr_debug(fmt, ...)                                                 \
        do {                                                                   \
        } while (0)
#endif

//=========================
// internal struct
//=========================
/**
 * @brief On-disk super block; the first sector describing a formatted ptn.
 *
 * Padded to exactly one sector so it can be read/written as a single LBA.
 */
struct super_block {
    uint32_t magic; /**< @c FS_MAGIC; marks the partition as formatted. */

    /* inode */
    uint32_t inode_cnt;      /**< Total inodes (@c INODE_CNT_MAX).      */
    uint32_t inode_btmp_lba; /**< LBA of the inode bitmap.             */
    uint32_t inode_btmp_sec; /**< Inode-bitmap length in sectors.      */
    uint32_t inode_table_lba; /**< LBA of the inode table.             */
    uint32_t inode_table_sec; /**< Inode-table length in sectors.      */

    /* data block */
    uint32_t blk_cnt;     /**< Total allocatable data blocks.          */
    uint32_t blk_btmp_lba; /**< LBA of the block bitmap.               */
    uint32_t blk_btmp_sec; /**< Block-bitmap length in sectors.        */
    uint32_t blk_lba;     /**< LBA of the first data block.            */

    uint8_t pad[472];      /**< Pad the super block out to one sector.  */
} __attribute__((packed)); /**< Match the on-disk layout; no padding.   */

//=========================
// global variable
//=========================
struct dirstream root_dir;
struct dirent root_blk[ROOT_DIR_MAX];

//=========================
// internal functions
//=========================
/**
 * @brief Build the in-memory root directory ("/").
 *
 * The root is not backed by a partition: its inode and data block live in RAM
 * (@ref root_blk). Seeds the directory with "." and ".." (both inode 0) so
 * mounted partitions can later be appended as sub-directories.
 */
static void root_dir_init() {
    struct inode_sys *root_inode =
        (struct inode_sys *)sys_malloc(sizeof(struct inode_sys));

    struct dirent *dir = (struct dirent *)root_blk;
    memset(dir, 0, BLOCK_SIZE);

    /* Init "." */
    memcpy(dir->filename, ".", 1);
    dir->i_no = 0;
    dir->f_type = FT_DIR;
    dir++;

    /* Init ".." */
    memcpy(dir->filename, "..", 2);
    dir->i_no = 0;
    dir->f_type = FT_DIR;

    /* Init root inode */
    root_inode->i_no = 0;
    root_inode->i_size = sizeof(struct dirent) * 2;
    root_inode->i_block[0] = (uint32_t)root_blk;
    root_inode->open_cnt = 0;

    /* Init root dir */
    root_dir.inode = root_inode;
    root_dir.pos = 0;
}

//=========================
// external functions
//=========================
/**
 * @brief Lay out an empty file system on @p ptn and write it to disk.
 *
 * Computes the super block, then zeroes and writes the inode bitmap, inode
 * table, block bitmap and the root directory's data block. Only inode 0 and
 * data block 0 (both the root directory) are marked in use.
 *
 * @param ptn  Partition to format.
 */
void fs_format(struct ide_ptn *ptn) {
    struct super_block sb;

    /* 1. Init super block */
    sb.magic = FS_MAGIC;

    // 1a. init super block: inode
    sb.inode_cnt = INODE_CNT_MAX;
    sb.inode_btmp_lba = ptn->start_lba + 2;
    sb.inode_btmp_sec = 1;
    sb.inode_table_lba = sb.inode_btmp_lba + 1;
    sb.inode_table_sec = INODE_CNT_MAX / INODE_PER_SEC;

    // 1b. init super block: data block
    uint32_t used_sec = 2 + sb.inode_btmp_sec + sb.inode_table_sec;
    uint32_t free_sec = ptn->sec_cnt - used_sec;
    sb.blk_cnt = free_sec * SECTOR_SIZE / BLOCK_SIZE;
    sb.blk_btmp_lba = sb.inode_table_lba + sb.inode_table_sec;
    sb.blk_btmp_sec = DIV_ROUND_UP(sb.blk_cnt, SECTOR_SIZE * 8);

    // init super block: data block must exclude bitmap for block
    sb.blk_cnt = (free_sec - sb.blk_btmp_sec) * SECTOR_SIZE / BLOCK_SIZE;

    // init super block: data block
    sb.blk_lba = sb.blk_btmp_lba + sb.blk_btmp_sec;

    pr_debug("%s format: ", ptn->name);
    pr_debug("inode_table_sec:%d ", sb.inode_table_sec);
    pr_debug("blk_cnt:%d, ", sb.blk_cnt);
    pr_debug("blk_lba:%d ", sb.blk_lba);
    pr_debug("\n");

    // Write super block to hd
    ide_write(ptn->hd, ptn->start_lba + 1, &sb, 1);

    /* 2. Reset inode bitmap */
    // The block bitmap is larger than the inode bitmap
    uint32_t buf_size = sb.blk_btmp_sec * SECTOR_SIZE;
    uint8_t *buf = (uint8_t *)sys_malloc(buf_size); // temporary bitmap
    memset(buf, 0, buf_size);

    // for root dir
    buf[0] = 0x01;
    ide_write(ptn->hd, sb.inode_btmp_lba, buf, sb.inode_btmp_sec);

    // set inode table for root dir
    memset(buf, 0, buf_size);
    struct inode_hd *root = (struct inode_hd *)buf;
    root->i_no = 0;
    root->i_size = sizeof(struct dirent) * 2;
    root->i_block[0] = sb.blk_lba;
    ide_write(ptn->hd, sb.inode_table_lba, buf, 1);

    /* 3. Reset k bitmap */
    memset(buf, 0, buf_size);

    // for root dir
    buf[0] = 0x01;

    /*
     * Mark every bit past the last real block as "used" so the allocator
     * never returns a block that does not exist: fill the tail of the
     * bitmap's final sector with 0xFF, then re-clear the bits that map to
     * real blocks in the last partially-filled byte.
     */
    uint32_t last_byte = sb.blk_cnt / 8; // byte holding the boundary bit
    uint32_t last_bit = sb.blk_cnt % 8;  // boundary bit within that byte
    uint32_t last_size =
        SECTOR_SIZE - (last_byte % SECTOR_SIZE); // bytes to end of sector
    memset(&buf[last_byte], 0xFF, last_size);
    int idx = 0;
    while (idx <= last_bit) {
        buf[last_byte] &= ~(1 << idx);
        idx++;
    }
    ide_write(ptn->hd, sb.blk_btmp_lba, buf, sb.blk_btmp_sec);

    // set root dir data block
    memset(buf, 0, buf_size);
    struct dirent *dir = (struct dirent *)buf;

    // init '.'
    memcpy(dir->filename, ".", 1);
    dir->i_no = 0;
    dir->f_type = FT_DIR;
    dir++;

    // init '..'
    memcpy(dir->filename, "..", 2);
    dir->i_no = 0;
    dir->f_type = FT_DIR;

    // write to hd
    ide_write(ptn->hd, sb.blk_lba, buf, 1);

    sys_free(buf);
}

/**
 * @brief Mount a formatted partition and expose it under the root directory.
 *
 * Reads the super block, loads the inode and block bitmaps into memory, sets
 * up the partition's open-inode list and lock, assigns its global inode-number
 * base, and adds it to @ref root_dir as a sub-directory.
 *
 * @param ptn  Partition to mount (must already be formatted).
 */
void fs_mount(struct ide_ptn *ptn) {
    struct super_block *sb;
    static uint32_t inode_max = 0;

    // pr_debug("%s +++\n", __func__);

    /* Init partition table */
    // get super block
    sb = (struct super_block *)sys_malloc(sizeof(struct super_block));
    ide_read(ptn->hd, ptn->start_lba + 1, sb, 1);

    /* set inode */
    ptn->inode_cnt = sb->inode_cnt;
    ptn->inode_btmp_lba = sb->inode_btmp_lba;
    ptn->inode_btmp.len = sb->inode_btmp_sec * SECTOR_SIZE;
    ptn->inode_btmp.bits = (uint8_t *)sys_malloc(ptn->inode_btmp.len);
    ide_read(ptn->hd, ptn->inode_btmp_lba, ptn->blk_btmp.bits,
             sb->inode_btmp_sec);
    ptn->inode_table_lba = sb->inode_table_lba;

    // set block
    ptn->blk_btmp_lba = sb->blk_btmp_lba;
    ptn->blk_btmp.len = sb->blk_btmp_sec * SECTOR_SIZE;
    ptn->blk_btmp.bits = (uint8_t *)sys_malloc(ptn->blk_btmp.len);
    ide_read(ptn->hd, ptn->blk_btmp_lba, ptn->blk_btmp.bits, sb->blk_btmp_sec);
    ptn->blk_lba = sb->blk_lba;

    // set list
    list_init(&ptn->open_inodes);

    // set mlock
    mutex_init(&ptn->mlock);

    // assign a global inode-number range (inode 0 is reserved for the root)
    if (0 == ptn->inode_base) {
        ptn->inode_base = inode_max + 1;
        inode_max += sb->inode_cnt;
    }

    pr_debug("%s mount:inode base=%d, ", ptn->name, ptn->inode_base);

    /* Add to dir */
    struct dirent *dir = (struct dirent *)root_dir.inode->i_block[0];
    // max idx = 4096 / 24 = 170 (round down)
    int dir_idx = 0;
    while (dir_idx < ROOT_DIR_MAX) {
        if (FT_UNKNOWN == (dir + dir_idx)->f_type) {
            strcpy((dir + dir_idx)->filename, ptn->name);
            (dir + dir_idx)->i_no = ptn->inode_base;
            (dir + dir_idx)->f_type = FT_DIR;
            root_dir.inode->i_size += sizeof(struct dirent);
            break;
        }
        dir_idx++;
    }

    // check result
    assert(dir_idx < ROOT_DIR_MAX);
    pr_debug("root dir size: %d\n", root_dir.inode->i_size);

    sys_free(sb);
}

/**
 * @brief Remove a mounted partition's entry from the root directory.
 *
 * @param ptn  Partition to unmount.
 */
void fs_unmount(struct ide_ptn *ptn) {
    // remove from root dir
    struct dirent *dir = (struct dirent *)root_dir.inode->i_block[0];
    // max idx = 4096 / 24 = 170 (round down)
    int dir_idx = 0;
    while (dir_idx < ROOT_DIR_MAX) {
        if (!strcmp((dir + dir_idx)->filename, ptn->name)) {
            (dir + dir_idx)->f_type = FT_UNKNOWN;
            root_dir.inode->i_size -= sizeof(struct dirent);
            pr_debug("%s unmount:root dir size: %d\n",
                     (dir + dir_idx)->filename, root_dir.inode->i_size);
            break;
        }
        dir_idx++;
    } // while
}

/**
 * @brief Initialise the file system: build the root dir and mount every ptn.
 *
 * Formats any partition whose super-block magic does not match @c FS_MAGIC,
 * then mounts each one. Call once during kernel startup, after @ref ide_init.
 */
void fs_init() {
    struct ide_ptn *ptn;
    struct list_elem *cur_elem;

    pr_debug("%s +++\n", __func__);

    // init root dir
    root_dir_init();

    // check empty
    if (list_empty(&ptn_list)) {
        pr_debug("There is no partition\n");
        return;
    }

    struct super_block *sb_buf = (struct super_block *)sys_malloc(SECTOR_SIZE);

    // parsing each partition
    cur_elem = ptn_list.head.next;
    while (cur_elem != &ptn_list.tail) {
        ptn = elem2entry(struct ide_ptn, ptn_tag, cur_elem);
        // pr_debug("partition: %s\n", ptn->name);

        // get the super block
        ide_read(ptn->hd, ptn->start_lba + 1, sb_buf, 1);

        // check file system
        if (FS_MAGIC != sb_buf->magic) {
            fs_format(ptn);
        }

        // mount each partition
        fs_mount(ptn);

        // next partition
        cur_elem = cur_elem->next;
    } // while
    sys_free(sb_buf);

    pr_debug("%s ---\n", __func__);
}
