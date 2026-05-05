#include "bitmap.h"
#include "string.h"

void bitmap_init(struct bitmap* btmp, uint8_t* btmp_addr, uint32_t len)
{
    btmp->bits = btmp_addr;
    btmp->len = len;
}

void bitmap_reset(struct bitmap* btmp)
{
    memset(btmp->bits, 0, btmp->len);
}

void bitmap_set(struct bitmap* btmp, uint32_t btmp_idx, bool value)
{
    uint32_t byte_idx = btmp_idx / 8;
    uint32_t bit_idx = btmp_idx % 8;

    if(true == value)
    {
        btmp->bits[byte_idx] |= (1 <<bit_idx);
    }else{
        btmp->bits[byte_idx] &= ~(1<<bit_idx);
    }
}

bool bitmap_check(struct bitmap* btmp, uint32_t btmp_idx)
{
    uint32_t byte_idx = btmp_idx / 8;
    uint32_t bit_idx = btmp_idx % 8;
    return (btmp->bits[byte_idx] & (1 << bit_idx)) ? true : false; 
}

int bitmap_acquire(struct bitmap* btmp, uint32_t cnt)
{
    uint32_t byte_idx = 0;

    /* 1. Search by byte */
    while((0xff == btmp->bits[byte_idx]) & (byte_idx < btmp->len))
    {
        byte_idx++;
    }

    // bitmap is full
    if(byte_idx == btmp->len)
    {
        return -1;
    }

    /* 2. Search by bit */
    int bit_idx = 0;
    while((uint8_t)(1<<bit_idx) & btmp->bits[byte_idx])
    {
        bit_idx++;
    }

    /* A. Get first available index */
    int free_idx = byte_idx * 8 + bit_idx;
    if(1 == cnt)
    {
        bitmap_set(btmp, free_idx, true);
        return free_idx;
    }
    
    /* B. Keep searching available contiguous bitmap */
    int free_cnt = 1;
    uint32_t total = btmp->len * 8;
    uint32_t idx = free_idx + 1;
    free_idx = -1; // reset free_idx
    while(idx < total)
    {
        if(bitmap_check(btmp, idx))
        {
            free_cnt = 0;
        }else{
            free_cnt++;
        }

        // check if find the n free bitmap
        if(free_cnt == cnt)
        {
            free_idx = idx - free_cnt + 1;
            break;
        }
        idx++;
    }

    // set the bitmap if successful
    if(-1 != free_idx)
    {
        for(int i = 0; i < cnt;i++)
        {
            bitmap_set(btmp, free_idx + i , true);
        }
    }

    return free_idx;
}


void bitmap_release(struct bitmap* btmp, uint32_t btmp_idx, uint32_t cnt)
{
    for(int n = 0 ; n < cnt; n++)
    {
        bitmap_set(btmp, btmp_idx + n, false);
    }
}