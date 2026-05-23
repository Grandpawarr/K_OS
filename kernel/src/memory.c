#include "memory.h"
#include "stddef.h"
#include "string.h"
#include "print.h"
#include "math.h"
#include "list.h"

//=========================
// internal struct
//=========================
struct arean
{
    struct mem_block_desc *p_mem_block;
    /**
     * If is_page_cnt is true, cnt represents the number of pages.
     * Otherwise, cnt represents the number of memory blocks.
     */
    uint32_t cnt;
    bool is_page_cnt;
};

/* For each meory block in arena */
struct mem_block
{
    struct list_elem free_elem;
};

//=========================
// global variable
//=========================
// for physical memory management
struct p_pool k_p_pool, u_p_pool;

// for kernel virtual address
struct v_pool k_v_pool;

// for memory block management
struct mem_block_desc k_mem_block[MEM_BLOCK_CNT];

//=========================
// internal functions
//=========================
/**
 * @brief Returns the virtual address of the Page Directory Entry (PDE) for a given virtual address.
 *
 * @details
 * Uses recursive page directory mapping, where PDE[1023] points back
 * to the page directory itself, making 0xFFFFF000 the virtual address
 * of the page directory base.
 *
 * Page dictory size is 4 byte
 *
 * PDE = 0xFFC0,0000 + 0x3FF * 0x1000 + vaddr[31:22] * 4
 *     = 0xFFFF,F000 + vaddr[31:22] * 4
 *
 * 0xffc0,0000 = Virtual starting address of all Page Table mapping regions
 * 0x3ff => PTE[1023] point to page dircetory
 *
 * @param vaddr  Virtual address to look up.
 * @return       Pointer to the PDE corresponding to vaddr.
 */
static uint32_t *pde_ptr(void *vaddr)
{
    uint32_t vaddr_val = (uint32_t)vaddr;
    uint32_t *pde = (uint32_t *)((0xfffff000) + PDE_IDX(vaddr_val) * 4);
    return pde;
}

/**
 * @brief Returns the virtual address of the Page Table Entry (PTE) for a given virtual address.
 *
 * Formula: pde = 0xFFC0,0000 + vaddr[31:22] * 0x1000 + vaddr[21:12] * 4
 *
 * @param vaddr Virtual address to look up.
 * @return Pointer to the PTE corresponding to vaddr.
 */
static uint32_t *pte_ptr(void *vaddr)
{
    uint32_t vaddr_val = (uint32_t)vaddr;
    uint32_t *pte = (uint32_t)(0xffc00000 +
                               ((vaddr_val & 0xffc0000) >> 10) +
                               PTE_IDX(vaddr_val) * 4);

    return pte;
}

/**
 * @brief Returns the virtual address of the Page Physical Address for a given virtual address.
 *
 * Formula: physical address = pte[31:12] + vaddr[11:0]
 *
 * @param vaddr
 * @return uint32_t*
 */
static uint32_t *addr_v2p(void *vaddr)
{
    uint32_t *pte = pte_ptr(vaddr);
    uint32_t vaddr_val = (uint32_t)vaddr;
    return ((*pte & 0xfffff000) + (vaddr_val & 0x00000fff));
}

/**
 * @brief Allocate pg_cnt consecutive virtual pages from a virtual address pool.
 *
 * @details
 * Two allocation modes depending on whether vaddr is NULL:
 *   - vaddr == NULL : auto-allocate; searches the bitmap for pg_cnt consecutive
 *                     free bits and returns the corresponding virtual address.
 *   - vaddr != NULL : fixed-address allocation; verifies that every page in
 *                     [vaddr, vaddr + pg_cnt * PG_SIZE) is free, then marks
 *                     them as used and returns vaddr.
 *
 * @param vp      Virtual address pool to allocate from.
 * @param vaddr   Desired starting virtual address, or NULL for auto-allocation.
 * @param pg_cnt  Number of consecutive pages to allocate.
 * @return void*  Starting virtual address of the allocated range,
 *                or NULL if allocation fails (no space or pages already in use).
 */
static void *vaddr_acquire(struct v_pool *vp, void *vaddr, int pg_cnt)
{
    void *vaddr_ret = NULL;
    uint32_t vaddr_val = (uint32_t)vaddr;
    int btmp_idx = -1;

    if (NULL == vaddr)
    {
        btmp_idx = bitmap_acquire(&vp->vaddr_bitmap, pg_cnt);
        if (-1 == btmp_idx)
        {
            return NULL;
        }
        vaddr_ret = (void *)vp->vaddr_start + (btmp_idx * PG_SIZE);
    }
    else
    {
        btmp_idx = (vaddr_val - vp->vaddr_start) / PG_SIZE;

        /* check and set each virtual address bitmap */
        int cnt = 0;
        while (cnt < pg_cnt)
        {
            if (bitmap_check(&vp->vaddr_bitmap, btmp_idx + cnt))
            {
                return NULL;
            }
            bitmap_set(&vp->vaddr_bitmap, btmp_idx + cnt, true);
            cnt++;
        }
        vaddr_ret = vaddr;
    }
    return vaddr_ret;
}

/**
 * @brief Release pg_cnt consecutive virtual pages back to a virtual address pool.
 *
 * @details
 * Converts vaddr to a bitmap index via (vaddr - vp->vaddr_start) / PG_SIZE,
 * then clears pg_cnt bits starting at that index to mark the pages as free.
 * The caller is responsible for unmapping the corresponding physical pages
 * before calling this function.
 *
 * @param vp      Virtual address pool to release pages back to.
 * @param vaddr   Starting virtual address of the range to release.
 * @param pg_cnt  Number of consecutive pages to release.
 */
static void vaddr_release(struct v_pool *vp, void *vaddr, int pg_cnt)
{
    uint32_t vaddr_val = (uint32_t)vaddr;
    int btmp_idx = 0;

    btmp_idx = (vaddr_val - vp->vaddr_start) / PG_SIZE;
    bitmap_release(&vp->vaddr_bitmap, btmp_idx, pg_cnt);
}

static void *palloc(struct p_pool *pp)
{
    void *paddr;
    int btmp_idx = -1;

    btmp_idx = bitmap_acquire(&pp->paddr_bitmap, 1);
    if (-1 == btmp_idx)
    {
        return NULL;
    }

    paddr = (void *)pp->paddr_start + (btmp_idx * PG_SIZE);
    return paddr;
}

static void pfree(struct p_pool *pp, void *paddr)
{
    uint32_t paddr_val = (uint32_t)paddr;
    int bitmap_indx = 0;

    bitmap_indx = (paddr_val - pp->paddr_start) / PG_SIZE;
    bitmap_release(&pp->paddr_bitmap, bitmap_indx, 1);
}

/**
 * @brief Map a virtual address to a physical address in the page table.
 *
 * If the PDE is not present, allocates a new page table from the kernel
 * physical pool and initializes it to zero before mapping.
 *
 * @param vaddr Virtual address to map.
 * @param paddr Physical address to map to.
 */
static void page_table_add(void *vaddr, void *paddr)
{
    uint32_t vaddr_val = (uint32_t)vaddr;
    uint32_t paddr_val = (uint32_t)paddr;
    uint32_t *pde = pde_ptr(vaddr); /* PDE entry address for vaddr */
    uint32_t *pte = pte_ptr(vaddr); /* PTE entry address for vaddr */
    uint32_t pt_paddr;

    if (!(*pde & 0x00000001)) /* PDE not present */
    {
        /* Allocate a new page table from kernel pool */
        pt_paddr = (uint32_t)palloc(&k_p_pool);
        *pde = (pt_paddr | PG_US_U | PG_RW_W | PG_P_1);

        /* Zero out the new page table (4KB) */
        memset((void *)((int)pte & 0xfffff000), 0, PG_SIZE);
    }

    /* Map virtual address to physical address in PTE */
    *pte = (paddr_val | PG_US_U | PG_RW_W | PG_P_1);
}

/**
 * @brief Removes the page table entry for a virtual address and invalidates
 *        the corresponding TLB entry via the invlpg instruction.
 *
 * @param vaddr Virtual address whose PTE is to be cleared.
 */
static void page_table_remove(void *vaddr)
{
    uint32_t *pte = pte_ptr(vaddr);
    *pte &= !PG_P_1;

    /* Update TLB */
    asm volatile("invlpg %0" ::"m"(vaddr) : "memory");
}

/**
 * @brief Allocates @p pg_cnt pages by acquiring virtual addresses from @p vp,
 *        physical frames from @p pp, and wiring them together in the page table.
 *
 * @param vp      Virtual address pool to allocate from.
 * @param pp      Physical address pool to allocate frames from.
 * @param vaddr   Requested starting virtual address; NULL lets the allocator choose.
 * @param pg_cnt  Number of pages to allocate.
 * @return        Starting virtual address of the allocated region on success,
 *                or NULL if either virtual or physical allocation fails.
 */
static void *page_acquire(struct v_pool *vp, struct p_pool *pp, void *vaddr, int pg_cnt)
{
    void *vaddr_ret = NULL;
    void *paddr = NULL;

    /* Acquire virtual address */
    vaddr_ret = vaddr_acquire(vp, vaddr, pg_cnt);
    if (NULL == vaddr_ret)
    {
        return NULL;
    }

    /* Acquire physical address */
    void *vaddr_idx = vaddr_ret;
    for (int i = 0; i < pg_cnt; vaddr_idx += pg_cnt, i++)
    {
        paddr = palloc(pp);
        if (NULL == paddr)
        {
            return NULL;
        }

        /* add into the page table */
        page_table_add(vaddr_idx, paddr);
    }

    return vaddr_ret;
}

/**
 * @brief Releases @p pg_cnt pages starting at @p vaddr: frees each physical
 *        frame back to @p pp, removes the corresponding page table entries,
 *        and returns the virtual address range to @p vp.
 *
 * @param vp      Virtual address pool the region belongs to.
 * @param pp      Physical address pool the frames were allocated from.
 * @param vaddr   Starting virtual address of the region to release.
 * @param pg_cnt  Number of pages to release.
 */
static void page_release(struct v_pool *vp, struct p_pool *pp, void *vaddr, int pg_cnt)
{
    void *vaddr_start = vaddr;
    void *paddr = NULL;

    void *vaddr_idx = vaddr_start;
    for (int i = 0; i < pg_cnt; vaddr_idx += PG_SIZE, i++)
    {
        /* free physical memory */
        paddr = (void *)addr_v2p(vaddr_idx);
        pfree(pp, paddr);

        /* remove page table */
        page_table_remove(vaddr_idx);
    }

    /* Release virtuall memory */
    vaddr_release(vp, vaddr_start, pg_cnt);
}

/**
 * @brief Initializes the kernel and user physical memory pools as well as the
 *        kernel virtual address pool, including their backing bitmaps.
 *
 *        Physical memory layout assumed:
 *          [0, 1MB)              — reserved (BIOS, page tables, etc.)
 *          [1MB, 1MB+256*PG_SIZE) — page table area (256 pages)
 *          remainder             — split evenly between kernel and user pools,
 *                                  with the kernel pool capped at 1 GB.
 *
 * @param mem_total_size Total installed physical memory in bytes.
 */
static void pool_init(uint32_t mem_total_size)
{
    TRACE_STR("memory total size: ");
    TRACE_INT(mem_total_size);
    TRACE_STR("\n");

    /* 1. Alculate the physical memory size for kernel and user. */
    uint32_t page_table_size = PG_SIZE * 256;
    uint32_t used_mem = page_table_size + 0x100000;
    uint32_t free_mem = mem_total_size - used_mem;
    uint32_t all_free_pages = free_mem / PG_SIZE;
    uint32_t kernel_free_pages = all_free_pages / 2;
    // The kernel physical space is limited to 1 GB = 0x40000 pages.
    if (kernel_free_pages > 0x40000)
    {
        kernel_free_pages = 0x40000;
    }
    uint32_t user_free_pages = all_free_pages - kernel_free_pages;

    TRACE_STR("kernel free pages: ");
    TRACE_INT(kernel_free_pages);
    TRACE_STR("\n");
    TRACE_STR("user free pages: ");
    TRACE_INT(user_free_pages);
    TRACE_STR("\n");

    /* 2a. Physical pool */
    uint32_t kp_start = used_mem;
    uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;
    k_p_pool.paddr_start = kp_start;
    u_p_pool.paddr_start = up_start;
    k_p_pool.size = kernel_free_pages * PG_SIZE;
    u_p_pool.size = user_free_pages * PG_SIZE;

    TRACE_STR("kernel physical pool start at ");
    TRACE_INT(k_p_pool.paddr_start);
    TRACE_STR("\n");
    TRACE_STR("user physical pool start at ");
    TRACE_INT(u_p_pool.paddr_start);
    TRACE_STR("\n");

    /* 2b. Physical pool bitmap */
    uint32_t kbm_length = kernel_free_pages / 8;
    uint32_t ubm_length = user_free_pages / 8;
    struct bitmap *btmp;
    void *btmp_addr;
    btmp = &k_p_pool.paddr_bitmap;
    btmp_addr = (void *)PADDR_BITMAP_BASE;
    bitmap_init(btmp, btmp_addr, kbm_length);
    bitmap_reset(btmp);

    btmp = &u_p_pool.paddr_bitmap;
    btmp_addr = (void *)PADDR_BITMAP_BASE + kbm_length;
    bitmap_init(btmp, btmp_addr, ubm_length);
    bitmap_reset(btmp);

    TRACE_STR("kernel physical pool bitmap: ");
    TRACE_INT((uint32_t)k_p_pool.paddr_bitmap.bits);
    TRACE_STR("\n");
    TRACE_STR("user physical pool bitmap: ");
    TRACE_INT((uint32_t)u_p_pool.paddr_bitmap.bits);
    TRACE_STR("\n");

    /* 3a. Virtual pool */
    k_v_pool.vaddr_start = K_HEAP_START;

    TRACE_STR("kernel virtual pool start at ");
    TRACE_INT(k_v_pool.vaddr_start);
    TRACE_STR("\n");

    /* 3b. Virtual pool bitmap */
    btmp = &k_v_pool.vaddr_bitmap;
    btmp_addr = (void *)K_VADDR_BITMAP_BASE;
    // 32KB virtual bitmap is sufficient to map the entire 1GB kernel space.
    bitmap_init(btmp, btmp_addr, 0x8000);
    bitmap_reset(btmp);

    TRACE_STR("kernel virtual pool bitmap: ");
    TRACE_INT((uint32_t)k_v_pool.vaddr_bitmap.bits);
    TRACE_STR("\n");
}