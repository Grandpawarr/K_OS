#include "memory.h"
#include "stddef.h"
#include "string.h"
#include "print.h"
#include "math.h"
#include "list.h"

//=========================
// internal struct
//=========================
struct arean{
    struct mem_block_desc *p_mem_block;
    /**
     * If is_page_cnt is true, cnt represents the number of pages.
     * Otherwise, cnt represents the number of memory blocks.
     */
    uint32_t cnt;
    bool is_page_cnt;
};


/* For each meory block in arena */
struct mem_block{
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
struct mem_block_desc   k_mem_block[MEM_BLOCK_CNT];

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
    uint32_t *pde = (uint32_t*)((0xfffff000) + PDE_IDX(vaddr_val) * 4);
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
    uint32_t *pte = (uint32_t)(0xffc00000 + \
                ((vaddr_val & 0xffc0000) >> 10) + \
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
static void* vaddr_acquire(struct v_pool* vp, void* vaddr, int pg_cnt)
{
    void* vaddr_ret = NULL;
    uint32_t vaddr_val = (uint32_t)vaddr;
    int btmp_idx = -1;

    if (NULL == vaddr) {
        btmp_idx  = bitmap_acquire(&vp->vaddr_bitmap, pg_cnt);
        if (-1 == btmp_idx) {
            return NULL;
        }
        vaddr_ret = (void*)vp->vaddr_start + (btmp_idx * PG_SIZE);
    } else {
        btmp_idx = (vaddr_val - vp->vaddr_start) / PG_SIZE;

        /* check and set each virtual address bitmap */
        int cnt = 0;
        while (cnt < pg_cnt) {
            if (bitmap_check(&vp->vaddr_bitmap, btmp_idx + cnt)) {
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
static void vaddr_release(struct v_pool* vp, void* vaddr, int pg_cnt)
{
    uint32_t vaddr_val = (uint32_t)vaddr;
    int btmp_idx = 0;

    btmp_idx = (vaddr_val - vp->vaddr_start) / PG_SIZE;
    bitmap_release(&vp->vaddr_bitmap, btmp_idx, pg_cnt);
}