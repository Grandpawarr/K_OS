#ifndef __KERNEL_INC_MEMORY_H
#define __KERNEL_INC_MEMORY_H
#include "stdint.h"
#include "bitmap.h"
#include "list.h"

//=========================
// debugging
//=========================
#define DEBUG (1)
#define TRACE_STR(x) do {if(DEBUG) put_str(x);} while(0)
#define TRACE_INT(x) do {if(DEBUG) put_int(x);} while(0)

//=========================
// define
// *pte = paddr | PG_US_U | PG_RW_W | PG_P_1
//=========================
#define PG_SIZE (4096)
#define PG_P_0  (0x0 << 0)  // page not present
#define PG_P_1  (0x1 << 0)  // page is present
#define PG_RW_R (0x0 << 1)  // read only
#define PG_RW_W (0x1 << 1)  // write only
#define PG_US_S (0x0 << 2)  // supervisor
#define PG_US_U (0x1 << 2)  // user

enum pool_flags{
    PF_KERNEL = 1,
    PF_USER = 2
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
#define PADDR_BITMAP_BASE   (0xC0076000)

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
#define K_HEAP_START        (0xC0100000)

/*
 * addr: [ 31        22 | 21         12 | 11          0 ]
 *         PD index(10)     PT index(10)  offset(12)
 */
#define PDE_IDX(addr) ((addr & 0xFFC00000) >> 22)
#define PTE_IDX(addr) ((addr & 0x003FF000) >> 12)

/* For 1024/512/256/128/64/32/16 byte block types. */
#define MEM_BLOCK_CNT   (7)

//=========================
// struct
//=========================
/**
 * @brief Single physical memory pool managing all available physical pages.
 * 
 * The system contains only one single physical pool.
 */
struct p_pool{
    uint32_t paddr_start;
    uint32_t size;
    struct bitmap paddr_bitmap; 
};

/**
 * @brief Virtual address pool, one per process/kernel for managing virtual address space.
 *
 * @note No size field is needed because the managed range is implicitly encoded
 *       in the bitmap itself: bitmap.bits * 4KB = total managed virtual range.
 */
struct v_pool {
    uint32_t      vaddr_start; 
    struct bitmap vaddr_bitmap; 
};

struct mem_block_desc {
    uint32_t block_size;
    uint32_t blocks_per_arena;
    struct list free_list;
};

//=========================
// function
//=========================
void*   page_malloc(enum pool_flags pf, void* vaddr, int pg_cnt);
void    page_free(enum pool_flags pf, void* vaddr, int pg_cnt);
void*   sys_malloc(uint32_t size);
void    sys_free(void* vaddr);
void    mem_init(void);
void    mem_block_init(struct mem_block_desc* p_mem_block);
#endif