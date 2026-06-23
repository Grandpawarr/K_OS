#include "init.h"
#include "print.h"
#include "test.h"
#include "thread.h"
#include "sched.h"

int main()
{
    put_str("\nKernel main()\n");
    kernel_init();

    /* Test function */
    test_all();

    kthread_exit();

    // while (1)
    //     ;

    return 0;
}