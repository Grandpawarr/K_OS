#include "test.h"

void test_timer()
{
    put_str("100 ms to jiffies: 0x");
    put_int(msecs_to_jiffies(100));
    put_str("\n");

    put_str("100 us to jiffies: 0x");
    put_int(usecs_to_jiffies(100));
    put_str("\n");

    put_str("5000 jiffies to ms: 0x");
    put_int(jiffies_to_msecs(5000));
    put_str("\n");

    put_str("50 jiffies to us: 0x");
    put_int(jiffies_to_usecs(50));
    put_str("\n");

    put_str("jiffies: 0x");
    put_int(jiffies);
    put_str("\n");
}