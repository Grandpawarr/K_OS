#ifndef __KERNEL_INC_FS_H
#define __KERNEL_INC_FS_H
#include "ide.h"
#include "list.h"
#include "lock.h"
#include "stdint.h"

//=========================
// define
//=========================
#define INODE_CNT_MAX  (SECTOR_SIZE * 8) /**< Inodes per ptn (a 1-sector bitmap).*/
#define INODE_PER_SEC  (SECTOR_SIZE / sizeof(struct inode_hd)) /**< On-disk inodes/sector.*/
#define BLOCK_SIZE     (4096)         /**< Data block size in bytes (8 sectors). */
#define DIR_PER_BLK    (BLOCK_SIZE / sizeof(struct dirent)) /**< Dir entries per block.*/
#define FS_MAGIC       (0x19890604)   /**< Super-block magic of a formatted ptn.  */
#define FILE_NAME_MAX  (16)           /**< Max file-name length.                  */
#define PATH_DEPTH_MAX (16)           /**< Max path nesting depth.                */
#define ROOT_DIR_MAX   (10)           /**< Max entries in the in-memory root dir. */

enum file_types { FT_UNKNOWN, FT_FILE, FT_DIR }; /**< Directory-entry file types. */

//=========================
// struct
//=========================
/** @brief On-disk inode: number, byte size and 13 direct block pointers. */
struct inode_hd {
    uint32_t i_no;        /**< Inode number.                          */
    uint32_t i_size;      /**< File size in bytes.                    */
    uint32_t i_block[13]; /**< Direct data-block LBAs (no indirect).  */
};

/** @brief In-memory inode: an @ref inode_hd plus open-file bookkeeping. */
struct inode_sys {
    uint32_t i_no;        /**< Inode number.                          */
    uint32_t i_size;      /**< File size in bytes.                    */
    uint32_t i_block[13]; /**< Direct data-block LBAs.                */

    uint32_t open_cnt;          /**< Number of open handles.          */
    struct list_elem inode_tag; /**< Link node for ptn->open_inodes.  */
};

/** @brief On-disk directory entry (24 bytes). */
struct dirent {
    uint32_t i_no;                /**< Inode this entry points to.    */
    char filename[FILE_NAME_MAX]; /**< Entry name.                    */
    enum file_types f_type;       /**< File type (@ref file_types).   */
};

/** @brief In-memory open-directory handle. */
struct dirstream {
    struct inode_sys *inode;       /**< Directory's inode.            */
    uint32_t path[PATH_DEPTH_MAX]; /**< Inode path from the root.     */
    struct dirent dir_entry;       /**< Scratch entry for readdir.    */
    uint32_t pos;                  /**< Read cursor within the dir.   */
};

//=========================
// external variable
//=========================
extern struct dirstream root_dir;            /**< The in-memory root directory. */
extern struct dirent root_blk[ROOT_DIR_MAX]; /**< Root directory's data block.  */

//=========================
// function
//=========================
/** @brief Lay out an empty file system on @p ptn. */
void fs_format(struct ide_ptn *ptn);
/** @brief Mount a formatted partition under the root directory. */
void fs_mount(struct ide_ptn *ptn);
/** @brief Remove a partition's entry from the root directory. */
void fs_unmount(struct ide_ptn *ptn);
/** @brief Initialise the file system; call once at startup. */
void fs_init(void);

#endif