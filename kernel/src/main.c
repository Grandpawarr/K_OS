#include "init.h"
#include "print.h"
#include "test.h"

int main()
{
    put_str("\nKernel main()\n");
    kernel_init();

    /* Test function */
    test_all();
    
    while (1)
        ;
    return 0;
}