#include "ide.h"
#include "assert.h"
#include "interrupt.h"
#include "io.h"
#include "lock.h"
#include "memory.h"
#include "printk.h"
#include "stdio.h"
#include "string.h"
#include "thread.h"
#include "timer.h"

/**
 * @file ide.c
 * @brief PIO-mode driver for ATA/IDE hard disks.
 *
 * Implements drive selection, LBA28 addressing, sector data transfer and the
 * IDENTIFY command. Access to each channel is serialised by a mutex, and the
 * IRQ handler signals transfer completion through a per-channel semaphore.
 */

//=========================
// debugging
//=========================
// #define DEBUG

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
 * @brief One 16-byte MBR/EBR partition-table entry.
 *
 * Mirrors the on-disk layout exactly; @c packed suppresses alignment padding.
 */
struct ptn_table {
    uint8_t boot_flag;
    uint8_t start_head;
    uint8_t start_sector;
    uint8_t start_cylinder;
    uint8_t type;
    uint8_t end_head;
    uint8_t end_sector;
    uint8_t end_cylinder;
    uint32_t start_lba;
    uint32_t sec_cnt;
} __attribute__((packed)); // avoid data alignment

/**
 * @brief A 512-byte boot sector (MBR or EBR).
 *
 * Carries the boot code, four primary partition entries and the 0x55AA
 * boot signature.
 */
struct boot_sector {
    uint8_t boot_code[446];
    struct ptn_table ptable[4];
    uint16_t signature;    // 0x55AA
} __attribute__((packed)); // avoid data alignment

//=========================
// global variable
//=========================
/** @brief Every IDE channel on the system. */
struct ide_ch g_ide_ch[IDE_CH_NR];

/** @brief List of partitions discovered during init (@ref ide_ptn). */
struct list ptn_list;

//=========================
// internal functions
//=========================
/**
 * @brief Setting the master or slave drive on a channel in LBA mode.
 *
 * @param hd  Target disk; @c dev_nr selects master (0) or slave (1).
 */
static void ide_set_drive(struct ide_hd *hd) {
    uint16_t port_base = hd->ch->port_base;
    uint16_t port;

    /* Set drive */
    port = port_base + IDE_DRIVE_SET;
    if (0 == hd->dev_nr) {
        outb(port, DRIVE_DEFAULT | DRIVE_MASTER | DRIVE_LBA);
    } else {
        outb(port, DRIVE_DEFAULT | DRIVE_SLAVE | DRIVE_LBA);
    }
}

/**
 * @brief Issue a command to the drive.
 *
 * @param hd   Target disk.
 * @param cmd  ATA command byte (e.g. @c CMD_IDENTIFY, @c CMD_READ_SEC).
 */
static void ide_set_cmd(struct ide_hd *hd, uint8_t cmd) {
    uint16_t port_base = hd->ch->port_base;
    uint16_t port;

    /* Set command */
    port = port_base + IDE_STA_CMD;
    outb(port, cmd);
}

/**
 * @brief Program the starting LBA and sector count for a transfer.
 *
 * Splits the 28-bit @p start across the LBA low/mid/high registers and the
 * low nibble of the drive/head register, then writes @p cnt to the
 * sector-count register.
 *
 * @param hd     Target disk.
 * @param start  28-bit starting LBA.
 * @param cnt    Sector count to transfer; 256 is encoded as 0.
 */
static void ide_set_lba(struct ide_hd *hd, uint32_t start, uint32_t cnt) {
    uint16_t port_base = hd->ch->port_base;
    uint16_t port;
    uint8_t reg;

    /* Set start lba */
    port = port_base + IDE_LBA_LOW;
    outb(port, start);

    port = port_base + IDE_LBA_MID;
    outb(port, start >> 8);

    port = port_base + IDE_LBA_HIGH;
    outb(port, start >> 16);

    port = port_base + IDE_DRIVE_SET;
    reg = inb(port);
    outb(port,
         reg | ((start >> 24) &
                0xF)); // Bits 0 to 3 are used for the highest four bits of LBA.

    /* Set number of sectors, max is 256 */
    if (256 <= cnt) {
        cnt = 0; // 0 is a special value for 256
    }
    port = port_base + IDE_SEC_CNT;
    outb(port, cnt);
}

/**
 * @brief Write sector data to the drive's data register (PIO).
 *
 * @param hd   Target disk.
 * @param src  Source buffer of at least @p cnt * @c SECTOR_SIZE bytes.
 * @param cnt  Number of sectors to write.
 */
static void ide_set_data(struct ide_hd *hd, void *src, uint32_t cnt) {
    uint16_t port_base = hd->ch->port_base;
    uint16_t port;
    uint32_t size = cnt * SECTOR_SIZE;

    port = port_base + IDE_DATA_REG;
    outsw(port, src, size / 2);
}

/**
 * @brief Read sector data from the drive's data register (PIO).
 *
 * @param hd   Target disk.
 * @param dst  Destination buffer with room for @p cnt * @c SECTOR_SIZE bytes.
 * @param cnt  Number of sectors to read.
 */
static void ide_get_data(struct ide_hd *hd, void *dst, uint32_t cnt) {
    uint16_t port_base = hd->ch->port_base;
    uint16_t port;
    uint32_t size = cnt * SECTOR_SIZE;

    port = port_base + IDE_DATA_REG;
    insw(port, dst, size / 2);
}

/**
 * @brief Read the drive's status register.
 *
 * @param hd  Target disk.
 * @return    Status byte (see @c STAT_BSY, @c STAT_DRQ).
 */
static uint8_t ide_get_status(struct ide_hd *hd) {
    uint16_t port_base = hd->ch->port_base;
    uint16_t port;

    port = port_base + IDE_STA_CMD;
    return inb(port);
}

/**
 * @brief IDE interrupt handler.
 *
 * Locates the channel whose IRQ fired and wakes the waiter on its semaphore.
 *
 * @param irq  IRQ number reported by the interrupt subsystem.
 */
static void ide_handler(uint8_t irq) {
    for (uint32_t idx = 0; idx < IDE_CH_NR; idx++) {
        if (g_ide_ch[idx].irq == irq) {
            // pr_debug("sema_up, irq=0x%x\n", irq);
            sema_up(&g_ide_ch[idx].sema);
            break;
        }
    }
}

/**
 * @brief Issue IDENTIFY and read the drive's information block.
 *
 * Selects the drive, sends @c CMD_IDENTIFY, polls until it clears @c STAT_BSY,
 * then reads the 512-byte identification sector and logs sector count and
 * capacity.
 *
 * @param hd  Target disk.
 * @return    @c true if the drive responded, @c false if absent (status 0).
 */
static bool ide_get_identify(struct ide_hd *hd) {
    uint8_t id_info[512] = {0};
    uint8_t stat;

    /* Set drive */
    ide_set_drive(hd);

    /* Set command */
    ide_set_cmd(hd, CMD_IDENTIFY);

    // check status
    do {
        msleep(10);
        stat = ide_get_status(hd);
    } while (stat & STAT_BSY);

    // print debug log
    pr_debug("%s", hd->name);
    pr_debug("    stat: 0x%x", stat);
    pr_debug("    channel: %s\n", hd->ch->name);

    // check result
    if (0 == stat) {
        return false;
    }

    /* clear semaphore */
    sema_down(&hd->ch->sema);

    /* Read data */
    ide_get_data(hd, id_info, 1);

    // printf info
    uint32_t sec_num = *(uint32_t *)&id_info[ID_SEC_NUM];
    pr_debug("    Sectors: %d", sec_num);
    pr_debug("    Capacity: %d MB\n", sec_num * SECTOR_SIZE / 1024 / 1024);

    return true;
}

/**
 * @brief Scan a boot sector's partition table and record real partitions.
 *
 * Reads the MBR (when @p base is 0) or an EBR, then walks its four entries:
 * extended entries (type @c 0x05) recurse, populated data partitions are
 * appended to @ref ptn_list, and empty entries are skipped.
 *
 * @param hd      Disk to scan.
 * @param base    LBA of the extended partition (0 while scanning the MBR).
 * @param offset  LBA offset of the current EBR within the extended partition.
 */
static void ide_get_partition(struct ide_hd *hd, uint32_t base,
                              uint32_t offset) {
    struct boot_sector *bs = sys_malloc(sizeof(struct boot_sector));
    struct ptn_table *ptable = bs->ptable;
    static int ptn_nr = 0;

    /* 1. Get MBR/EBR */
    ide_read(hd, base + offset, bs, 1);

    for (uint32_t idx = 0; idx < 4; idx++) {
        // A. unused
        if (0 == ptable->type) {
            ptable++;
            continue;
        }

        // B. extended partition using recursive
        if (0x05 == ptable->type) {
            // if the base is not 0, it must be in the extended partition.
            if (0 == base) {
                // for first EBR
                ptn_nr = 5;
                ide_get_partition(hd, ptable->start_lba, 0);
            } else {
                ide_get_partition(hd, base, ptable->start_lba);
            }

            // next partition table
            ptable++;
            continue;
        }

        /*  C. Record entity segmentation region: add to list: */
        if (0 != ptable->type) {
            pr_debug("    type:0x%x", ptable->type);

            // set partition table info
            struct ide_ptn *ptn = sys_malloc(sizeof(struct ide_ptn));
            if (0 == base) { // primary partition
                sprintf(ptn->name, "%s_%d", hd->name, idx + 1);
            } else { // extended partition
                sprintf(ptn->name, "%s_%d", hd->name, ptn_nr++);
            }

            ptn->start_lba = base + offset + ptable->start_lba;
            ptn->sec_cnt = ptable->sec_cnt;
            ptn->hd = hd;
            list_append(&ptn_list, &ptn->ptn_tag);

            pr_debug(", name:%s, start:%d, sectors:%d\n", ptn->name,
                     ptn->start_lba, ptn->sec_cnt);
        }

        /* Next partition entry */
        ptable++;
    }

    sys_free(bs);
}

/**
 * @brief Probe every IDE channel and build the device/partition tables.
 *
 * Assigns the PC/AT command-block port base and interrupt vector to each
 * channel, initialises its mutex, semaphore and IRQ handler, then runs
 * IDENTIFY on both possible drives and scans the partition table of every
 * drive that is present.
 *
 * @note The @c irq field holds the remapped IDT vector (0x20 + IRQ line),
 *       not the raw 8259 IRQ number; see @ref register_handler.
 */
static void ide_ch_init(void) {
    pr_debug("%s +++\n", __func__);

    g_ide_ch[0].port_base = 0x1F0;
    g_ide_ch[0].irq = 0x20 + 14;

    g_ide_ch[1].port_base = 0x170;
    g_ide_ch[1].irq = 0x20 + 15;

    g_ide_ch[2].port_base = 0x1E8;
    g_ide_ch[2].irq = 0x20 + 11;

    g_ide_ch[3].port_base = 0x168;
    g_ide_ch[3].irq = 0x20 + 10;

    /* data structure init */
    for (uint32_t ch_idx = 0; ch_idx < IDE_CH_NR; ch_idx++) {
        sprintf(g_ide_ch[ch_idx].name, "ide%d", ch_idx);
        mutex_init(&g_ide_ch[ch_idx].mlock);
        sema_init(&g_ide_ch[ch_idx].sema, 0);
        register_handler(g_ide_ch[ch_idx].irq, ide_handler);

        // for each hard disk
        struct ide_hd *hd;
        for (uint32_t hd_idx = 0; hd_idx < 2; hd_idx++) {
            hd = &g_ide_ch[ch_idx].dev[hd_idx];
            sprintf(hd->name, "sd%c", 'a' + ch_idx * 2 + hd_idx);
            hd->ch = &g_ide_ch[ch_idx];
            hd->dev_nr = hd_idx;

            // check hd
            if (ide_get_identify(hd)) {
                hd->valid = true;
                ide_get_partition(hd, 0, 0);
            } else {
                hd->valid = false;
            }
        }
    }

    // pr_debug("%s ---\n", __func__);
}

//=========================
// external functions
//=========================
/**
 * @brief Read @p cnt sectors from @p hd starting at @p lba (blocking PIO).
 *
 * Holds the channel mutex for the whole transfer and splits it into chunks of
 * at most 256 sectors (the LBA28 sector-count limit). Each chunk programs the
 * LBA, issues @c CMD_READ_SEC, blocks on the channel semaphore (raised by the
 * IRQ handler), waits for BSY/DRQ to clear, then drains the data register.
 *
 * @param hd   Source disk.
 * @param lba  28-bit starting LBA.
 * @param dst  Destination buffer of at least @p cnt * @c SECTOR_SIZE bytes.
 * @param cnt  Number of sectors to read.
 */
void ide_read(struct ide_hd *hd, uint32_t lba, void *dst, uint32_t cnt) {
    uint8_t stat;
    void *p_dst = dst;

    /* Acquire channel lock */
    mutex_lock(&hd->ch->mlock);

    /* Set drive */
    ide_set_drive(hd);

    for (uint32_t cnt_done = 0, cnt_each; cnt_done < cnt; cnt_done += cnt_each,
                  p_dst = (void *)((uint32_t)p_dst + cnt_each * SECTOR_SIZE)) {
        // max cnt for each run is 256
        if ((cnt - cnt_done) >= 256) {
            cnt_each = 256;
        } else {
            cnt_each = cnt - cnt_done;
        }

        // set sector
        ide_set_lba(hd, lba, cnt_each);

        // set command
        ide_set_cmd(hd, CMD_READ_SEC);

        // block wait for irq
        sema_down(&hd->ch->sema);

        // wait for complete data transfer
        do {
            msleep(10);
            stat = ide_get_status(hd);
        } while ((stat & STAT_BSY) || !(stat & STAT_DRQ));

        // read data
        ide_get_data(hd, p_dst, cnt_each);
    }

    /* Release channel lock */
    mutex_unlock(&hd->ch->mlock);

    // pr_debug("%s ---\n", __func__);
}

/**
 * @brief Write @p cnt sectors to @p hd starting at @p lba (blocking PIO).
 *
 * Mirror of @ref ide_read: same 256-sector chunking under the channel mutex,
 * but each chunk waits for DRQ, pushes the data into the data register, then
 * blocks on the semaphore until the drive signals completion via IRQ.
 *
 * @param hd   Target disk.
 * @param lba  28-bit starting LBA.
 * @param src  Source buffer of at least @p cnt * @c SECTOR_SIZE bytes.
 * @param cnt  Number of sectors to write.
 */
void ide_write(struct ide_hd *hd, uint32_t lba, void *src, uint32_t cnt) {
    uint8_t stat;
    void *p_src = src;

    // pr_debug("%s +++\n", __func__);

    /* Acquire channel lock */
    mutex_lock(&hd->ch->mlock);

    /* Set drive*/
    ide_set_drive(hd);

    for (uint32_t cnt_done = 0, cnt_each; cnt_done < cnt; cnt_done += cnt_each,
                  p_src = (void *)((uint32_t)p_src + cnt_each * SECTOR_SIZE)) {
        // max cnt for each run is 256
        if ((cnt - cnt_done) >= 256) {
            cnt_each = 256;
        } else {
            cnt_each = cnt - cnt_done;
        }

        // set sectors
        ide_set_lba(hd, lba, cnt_each);

        // set command
        ide_set_cmd(hd, CMD_WRITE_SEC);

        // wait for ready data transfer
        do {
            msleep(10);
            stat = ide_get_status(hd);
        } while ((stat & STAT_BSY) || !(stat & STAT_DRQ));

        // write the data
        ide_set_data(hd, p_src, cnt_each);

        // block wait for irq
        sema_down(&hd->ch->sema);

    } // while

    // release channel lock
    mutex_unlock(&hd->ch->mlock);

    // pr_debug("%s ---\n", __func__);
}

/**
 * @brief Initialise the IDE subsystem.
 *
 * Resets the global partition list and probes every channel via
 * @ref ide_ch_init. Call once during kernel startup.
 */
void ide_init(void) {
    pr_debug("ide_init()\n");

    /* partition list init */
    list_init(&ptn_list);

    /* channel init */
    ide_ch_init();
}