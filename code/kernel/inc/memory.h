#ifndef __KERNEL_INC_MEMORY_H
#define __KERNEL_INC_MEMORY_H
#include "bitmap.h"
#include "list.h"
#include "lock.h"
#include "stdint.h"

//=========================
// define
// *pte = paddr | PG_US_U | PG_RW_W | PG_P_1
//=========================
#define PG_SIZE (4096)
#define PG_P_0 (0x0 << 0)  // page not present
#define PG_P_1 (0x1 << 0)  // page is present
#define PG_RW_R (0x0 << 1) // read only
#define PG_RW_W (0x1 << 1) // write only
#define PG_US_S (0x0 << 2) // supervisor
#define PG_US_U (0x1 << 2) // user

/**
 * @brief Selects which memory pool an allocation or release operates on.
 */
enum pool_flags {
    PF_KERNEL = 1, /**< Kernel physical + virtual pool. */
    PF_USER =
        2 /**< User-process physical + virtual pool (not yet implemented). */
};

/**
 * @brief Physical memory size stored by BIOS during boot,
 * read by OS at initialization
 *
 */
#define MEM_SIZE_ADDR (0x900)

/**
 * @brief Bitmap physical base address (128KB).
 *
 * Each bit manages 4KB, 128KB can maitain 4GB.
 */
#define PADDR_BITMAP_BASE (0xC0076000)

/**
 * @brief Bitmap virtual base address (32 KB).
 *
 * The size of the kernel virtual memory is 1 GB (0xC0000000 ~ 0xFFFFFFFF).
 * Calculation: 1 GB / 4 KB (page size) = 2 ^ 18 pages
 * = 2 ^ 18 bits = 32 KB bitmap(each bit manages 4KB).
 *
 * Note on memory layout:
 * 0xC0096000 - 0xC0076000 = 0x20000 (128 KB).
 * This 128 KB gap is reserved for the physical memory bitmap,
 * which provides enough space to manage up to 4 GB of physical memory.
 */
#define K_VADDR_BITMAP_BASE (0xC0096000)

/**
 * @brief kernel heap Virtual base address of = 0xC0000000 + 1MB
 * Lower 1MB reserved for kernel code / hardware MMIO
 *
 * Virtual address space configuration:
 * 0xC0000000 ~ 0xC00FFFFF → kernel code + hardware MMIO (1MB)
 * 0xC0100000 ~ → kernel heap starts here
 *
 */
#define K_HEAP_START (0xC0100000)

/*
 * addr: [ 31        22 | 21         12 | 11          0 ]
 *         PD index(10)     PT index(10)  offset(12)
 */
#define PDE_IDX(addr) ((addr & 0xFFC00000) >> 22)
#define PTE_IDX(addr) ((addr & 0x003FF000) >> 12)

/* For 1024/512/256/128/64/32/16 byte block types. */
#define MEM_BLOCK_CNT (7)

//=========================
// struct
//=========================
/**
 * @brief Single physical memory pool managing all available physical pages.
 *
 * The system contains only one single physical pool.
 */
struct p_pool {
    uint32_t paddr_start;
    uint32_t size;
    struct bitmap
        paddr_bitmap; // Bitmap tracking page usage(0 = free, 1 = used).
    struct mutex mlock;
};

/**
 * @brief Virtual address pool, one per process/kernel for managing virtual
 * address space.
 *
 * @note No size field is needed because the managed range is implicitly encoded
 *       in the bitmap itself: bitmap.bits * 4KB = total managed virtual range.
 */
struct v_pool {
    uint32_t vaddr_start;
    struct bitmap vaddr_bitmap;
    struct mutex mlock;
};

/**
 * @brief Descriptor for one fixed-size block class used by the slab-style
 * allocator.
 *
 * Each descriptor represents one block size (16, 32, 64, 128, 256, 512, or 1024
 * bytes). Arenas are carved into @p blocks_per_arena blocks of @p block_size
 * bytes; free blocks are linked through @p free_list.
 */
struct mem_block_desc {
    uint32_t block_size; /**< Size in bytes of each block managed by this
                            descriptor. */
    uint32_t
        blocks_per_arena;  /**< Number of blocks that fit in one arena page. */
    struct list free_list; /**< Intrusive list of currently free blocks. */
    struct mutex mlock;
};

//=========================
// function
//=========================
/**
 * @brief Allocate @p pg_cnt pages from the pool selected by @p pf.
 *
 * @details
 * Acquires virtual addresses from the corresponding virtual pool and
 * physical frames from the corresponding physical pool, then wires them
 * together in the page table.
 * - If @p vaddr is NULL the allocator picks a free virtual address
 * automatically.
 * - If @p vaddr is non-NULL the allocation starts at that exact address;
 *   fails if any page in the range is already in use.
 *
 * @todo Support PF_USER (user-process pool) and reject invalid pool flags.
 *
 * @param pf      Pool selector: PF_KERNEL or PF_USER.
 * @param vaddr   Desired starting virtual address, or NULL for auto-allocation.
 * @param pg_cnt  Number of consecutive pages to allocate.
 * @return        Starting virtual address of the allocated region,
 *                or NULL on failure.
 */
void *page_malloc(enum pool_flags pf, void *vaddr, int pg_cnt);

/**
 * @brief Release @p pg_cnt pages starting at @p vaddr back to the pool selected
 * by @p pf.
 *
 * @details
 * For each page in [@p vaddr, @p vaddr + @p pg_cnt * PG_SIZE):
 *   1. Resolves the physical address and frees the frame to the physical pool.
 *   2. Clears the PTE and invalidates the TLB entry via @c invlpg.
 * Finally returns the virtual address range to the virtual pool bitmap.
 *
 * @todo Support PF_USER (user-process pool) and reject invalid pool flags.
 *
 * @param pf      Pool selector: PF_KERNEL or PF_USER.
 * @param vaddr   Starting virtual address of the region to free.
 * @param pg_cnt  Number of consecutive pages to free.
 */
void page_free(enum pool_flags pf, void *vaddr, int pg_cnt);

/**
 * @brief Allocate @p size bytes from the kernel heap.
 *
 * @details
 * Two allocation strategies are selected automatically:
 * - **size <= 1024** : Uses the slab-style block allocator.  Finds the
 *   smallest block class whose @c block_size >= @p size, refills the free
 *   list with a fresh arena page if necessary, then pops one block.
 * - **size > 1024**  : Allocates whole pages directly.  Computes
 *   @c DIV_ROUND_UP(size + sizeof(arena), PG_SIZE) pages, places an arena
 *   header at the start, and returns the address immediately after it.
 *
 * The returned memory is zeroed in both cases.
 *
 * @todo Determine whether the caller is in kernel space or user space
 *       and select the appropriate pools accordingly.
 *
 * @param size  Number of bytes to allocate.
 * @return      Pointer to the allocated memory, or NULL on failure.
 */
void *sys_malloc(uint32_t size);

/**
 * @brief Free memory previously allocated by sys_malloc().
 *
 * @details
 * Derives the arena header from @p vaddr and inspects @c arena.is_page_cnt:
 * - **true**  (large allocation) : releases @c arena.cnt whole pages via
 *   page_release(), which also clears the PTEs and frees physical frames.
 * - **false** (block allocation) : returns the block to the arena's free
 *   list.  If all blocks in the arena become free, the entire arena page
 *   is released back to the pool.
 *
 * @todo Determine whether the caller is in kernel space or user space
 *       and select the appropriate pools accordingly.
 *
 * @param vaddr  Pointer previously returned by sys_malloc(); must not be NULL.
 */
void sys_free(void *vaddr);

/**
 * @brief Initialize the entire memory subsystem.
 *
 * @details
 * Reads the total physical memory size stored at MEM_SIZE_ADDR by the
 * bootloader, then calls pool_init() to set up the kernel and user physical
 * pools and the kernel virtual address pool, and finally calls
 * mem_block_init() to prepare the seven kernel slab block descriptors
 * (16 / 32 / 64 / 128 / 256 / 512 / 1024 bytes).
 *
 * Must be called once during kernel initialization before any memory
 * allocation function is used.
 */
void mem_init(void);

/**
 * @brief Initialize an array of MEM_BLOCK_CNT memory block descriptors.
 *
 * @details
 * Fills @p p_mem_block[0..MEM_BLOCK_CNT-1] with block sizes
 * 16, 32, 64, 128, 256, 512, and 1024 bytes respectively.
 * For each descriptor, @c blocks_per_arena is computed as
 * @c (PG_SIZE - sizeof(arena)) / block_size, and the free list is
 * initialized empty.
 *
 * @param p_mem_block  Pointer to an array of at least MEM_BLOCK_CNT
 *                     mem_block_desc entries to initialize.
 */
void mem_block_init(struct mem_block_desc *p_mem_block);
#endif