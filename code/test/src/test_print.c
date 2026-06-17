#include "print.h"

void test_put_char(void)
{
    int row = 10;

    /* Test character */
    put_char('C');

    /* Test CR */
    put_char('\r');

    /* Test LF */
    put_char('\n');

    /* Test backspace */
    put_char('B');
    put_char('B');
    put_char('\b');

    /* Test roll screen */
    put_char('\n');
    for (int i = 0; i < 80 * row; i++)
    {
        if (i % 80 == 0)
        {
            put_char('H');
        }
        else if (i % 80 == 79)
        {
            put_char('E');
        }
        else
        {
            put_char('*');
        }
    }
}

void test_put_str(void)
{
    put_str("Test put string\n");
}

void test_put_int(void)
{
    put_int(0);
    put_char('\n');
    put_int(9);
    put_char('\n');
    put_int(0x12345678);
    put_char('\n');
    put_int(9527); // print in 16-bit: 0x2537
    put_char('\n');
    put_int(0x00005678);
    put_char('\n');
    put_int(0x00000000);
    put_char('\n');
}

void test_set_cursor(void)
{
    set_cursor(80 * 25);
}

void test_cls_screen(void)
{
    cls_screen();
}
