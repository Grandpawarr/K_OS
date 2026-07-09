#ifndef __KERNEL_INC_IDE_H
#define __KERNEL_INC_IDE_H
#include "bitmap.h"
#include "list.h"
#include "lock.h"
#include "stdbool.h"
#include "stdint.h"

//=========================
// define
//=========================
#define SECTOR_SIZE (512)

/* IDE channel number */
#define IDE_CH_NR (4)

/* IDE port offset */
#define IDE_DATA_REG  (0x0)
#define IDE_ERROR_REG (0x1)
#define IDE_SEC_CNT   (0x2)
#define IDE_LBA_LOW   (0x3)
#define IDE_LBA_MID   (0x4)
#define IDE_LBA_HIGH  (0x5)
#define IDE_DRIVE_SET (0x6)
#define IDE_STA_CMD   (0x7)

/* IDE drive set */
#define DRIVE_DEFAULT (0xA0) // 1_0_1_0_0000
#define DRIVE_MASTER  (0x0 << 4)
#define DRIVE_SLAVE   (0x1 << 4)
#define DRIVE_LBA     (0x1 << 6)

/* IDE commands */
#define CMD_IDENTIFY  (0xEC)
#define CMD_READ_SEC  (0x20)
#define CMD_WRITE_SEC (0x30)

// IDE status
#define STAT_BSY (0x01 << 7)
#define STAT_DRQ (0x01 << 3)

// IDE ID
#define ID_SEC_NUM (60 * 2) // each element 2-byte

//=========================
// struct
//=========================
/**
 * @brief A logical partition discovered on a disk.
 *
 * Filled in during init and linked into the global @ref ptn_list.
 */
struct ide_ptn {
    char name[8];             /**< Partition name, e.g. "sda_1".           */
    uint32_t start_lba;       /**< Absolute starting LBA on the disk.      */
    uint32_t sec_cnt;         /**< Partition length in sectors.            */
    struct ide_hd *hd;        /**< Disk this partition lives on.           */
    struct list_elem ptn_tag; /**< Link node for @ref ptn_list.            */

    /* For file system */
    uint32_t inode_base;       /**< First global inode number for this ptn. */
    uint32_t inode_cnt;        /**< Total inodes on the partition.          */
    uint32_t inode_btmp_lba;   /**< LBA of the inode bitmap.                */
    struct bitmap inode_btmp;  /**< In-memory copy of the inode bitmap.     */
    uint32_t inode_table_lba;  /**< LBA of the inode table.                 */
    uint32_t blk_btmp_lba;     /**< LBA of the block bitmap.                */
    struct bitmap blk_btmp;    /**< In-memory copy of the block bitmap.     */
    uint32_t blk_lba;          /**< LBA of the first data block.            */
    struct list open_inodes;   /**< Currently-open inodes on this ptn.      */
    struct mutex mlock;        /**< Serialises file-system access.          */
};

/**
 * @brief A single ATA hard disk (the master or slave of a channel).
 */
struct ide_hd {
    char name[8];      /**< Disk name, e.g. "sda".                  */
    struct ide_ch *ch; /**< Owning channel.                         */
    uint8_t dev_nr;    /**< 0 = master, 1 = slave.                  */
    bool valid;        /**< True if the drive responded to IDENTIFY.*/
};

/**
 * @brief One ATA/IDE channel; its two drives share a command-block port range.
 */
struct ide_ch {
    char name[8];          /**< Channel name, e.g. "ide0".              */
    struct ide_hd dev[2];  /**< [0] master, [1] slave.                  */
    struct mutex mlock;    /**< Serialises access to the channel.       */
    struct semaphore sema; /**< Raised by the IRQ handler on completion.*/
    uint16_t port_base;    /**< Base I/O port of the command block.     */
    uint8_t irq;           /**< Remapped IDT vector (0x20 + IRQ line).  */
};

//=========================
// external variable
//=========================
/** @brief All partitions discovered during init (nodes are @ref ide_ptn). */
extern struct list ptn_list;

//=========================
// function
//=========================

/**
 * @brief Read @p cnt sectors starting at @p lba into @p dst (blocking PIO).
 *
 * @param hd   Source disk.
 * @param lba  28-bit starting LBA.
 * @param dst  Destination buffer of at least @p cnt * @c SECTOR_SIZE bytes.
 * @param cnt  Number of sectors to read.
 */
void ide_read(struct ide_hd *hd, uint32_t lba, void *dst, uint32_t cnt);

/**
 * @brief Write @p cnt sectors from @p src starting at @p lba (blocking PIO).
 *
 * @param hd   Target disk.
 * @param lba  28-bit starting LBA.
 * @param src  Source buffer of at least @p cnt * @c SECTOR_SIZE bytes.
 * @param cnt  Number of sectors to write.
 */
void ide_write(struct ide_hd *hd, uint32_t lba, void *src, uint32_t cnt);

/**
 * @brief Initialise the IDE subsystem; call once during kernel startup.
 */
void ide_init(void);

#endif