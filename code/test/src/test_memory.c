#include "stddef.h"
#include "memory.h"
#include "list.h"
#include "print.h"

/*===== Bitmap state verification ======
 *
 * Break point: b 0x1036
 *
 * Physical bitmap (x 0xC007_6000):
 *   => 0x0000_007F = 111_1111
 *   => 7 pages allocated: vaddr1[3] + vaddr3[3] + sys_malloc[1]
 *
 * Kernel virtual bitmap (x 0xC009_6000):
 *   => 0x0000_038F = 11_1000_1111
 *   => bit 0~2  : vaddr1[3]
 *   => bit 3    : sys_malloc[1]
 *   => bit 4~6  : None
 *   => bit 7~9  : vaddr3[3]
 *
 * Note: virtual bitmap has 3 extra bits vs physical bitmap
 *       because vaddr2 allocation failed after marking virtual pages,
 *       leaving a hole at bit 4~6 with no corresponding physical pages.
 */
void test_memory(bool test_free, bool test_list_flag)
{
    /*===== 1. Test page malloc ======*/
    /* Test auto-allocation of 3 pages in kernel space. */
    put_str("test page_malloc(PF_KERNEL, 3): ");
    void *vaddr_1 = page_malloc(PF_KERNEL, NULL, 3);
    put_int((uint32_t)vaddr_1);
    put_str("\n");

    /* Test fixed-address allocation at 0xC0102000.
     * Expected: NULL, address already occupied by vaddr_1.
     */
    put_str("test page_malloc(0xC0102000, 3): ");
    void *vaddr_2 = page_malloc(PF_KERNEL, (void *)0xC0102000, 3);
    put_int((uint32_t)vaddr_2);
    put_str("\n");

    /* Test fixed-address allocation at 0xC0107000.
     * Expected: success, address is unoccupied.
     */
    put_str("test page_malloc(0xC0107000, 3): ");
    void *vaddr_3 = page_malloc(PF_KERNEL, (void *)0xC0107000, 3);
    put_int((uint32_t)vaddr_3);
    put_str("\n");

    /*===== 2. Test page free ======*/
    if (test_free)
    {
        put_str("test page_free\n");
        page_free(PF_KERNEL, vaddr_1, 2);
    }

    /*===== 3. Test system malloc ======
     * Test return 0xC010_300c
     * 0xC010_0000 -> page 0 1 (used, page_malloc vaddr_1)
     * 0xC010_1000 -> page 1 1 (used, page_malloc vaddr_1)
     * 0xC010_2000 -> page 2 1 (used, page_malloc vaddr_1)
     * 0xC010_3000 -> page 3 0 <- sys_malloc starts here
     * c is arena space 12 byte
     */
    put_str("test sys_malloc(31): ");
    void *vaddr_4 = sys_malloc(31);
    put_int((uint32_t)vaddr_4);
    put_str("\n");

    /* Test return 0xC010_302c
     * 20 is 32 byte
     */
    put_str("test sys_malloc(29): ");
    void *vaddr_5 = sys_malloc(29);
    put_int((uint32_t)vaddr_5);
    put_str("\n");

    /*===== 4. Test mem_block free list ======
     *
     * Traverse all 7 mem_block_desc free lists and print
     * the number of available blocks for each size class.
     *
     * Expected output (after sys_malloc(31) + sys_malloc(29)):
     *   16  byte free list: 0   (no arena allocated for this size)
     *   32  byte free list: 7D  (one arena allocated, 2 blocks used)
     *   64  byte free list: 0
     *   128 byte free list: 0
     *   256 byte free list: 0
     *   512 byte free list: 0
     *   1024 byte free list: 0
     *
     * Note: sys_malloc(31) and sys_malloc(29) both fit into 32-byte class.
     *       One arena (1 page = 4KB) was created for the 32-byte class.
     *       blocks_per_arena = (4096 - sizeof(arena)) / 32 = 127 = 0x7F
     *       After 2 allocations: 127 - 2 = 125 = 0x7D free blocks remaining.
     */
    if (test_list_flag)
    {
        extern struct mem_block_desc k_mem_block[MEM_BLOCK_CNT];
        put_str("test mem_block free list\n");
        struct list *plist;
        struct list_elem *cur_elem;
        int idx;
        for (idx = 0; idx < MEM_BLOCK_CNT; idx++)
        {
            put_int(k_mem_block[idx].block_size);
            put_str(" byte free list: ");

            plist = &k_mem_block[idx].free_list;
            cur_elem = plist->head.next;
            int cnt = 0;
            while (cur_elem != &plist->tail)
            {
                cnt++;
                cur_elem = cur_elem->next;
            }
            put_int(cnt);
            put_str("\n");
        }
    }

    /*===== 5. Test system free ======*/
    put_str("test sys_free\n");
    sys_free(vaddr_4);
}
