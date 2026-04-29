#include "print.h"
#include "test.h"

int main()
{
    put_str("\nKernel main()\n");

    /* Test code */
    test_all();
    
    while (1)
        ;
    return 0;
}