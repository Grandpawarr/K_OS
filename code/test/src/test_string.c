#include "string.h"
#include "print.h"

void test_string(void)
{
    put_str("test string\n");
    char src[10] = {'\0'};
    char dst[10] = {'\0'};

    // test memset()
    put_str("\n   test memset\n");
    int i;
    for (i = 0; i < 9; i++)
    {
        memset(src + i, '0' + i + 1, 1);
    }
    memset(src + 9, '\0', 1);
    put_str("src:");
    put_str(src);
    put_str("\n");
    put_str("dst:");
    put_str(dst);
    put_str("\n");

    // test memcpy()
    put_str("\n   test memcpy\n");
    memcpy(dst, src, 10);
    put_str("src:");
    put_str(src);
    put_str("\n");
    put_str("dst:");
    put_str(dst);
    put_str("\n");

    // test memcmp()
    put_str("\n   test memcmp\n");
    src[5] = '0';
    if (0 == memcmp(src, dst, 10))
    {
        put_str("The same\n");
    }
    else
    {
        put_str("Different\n");
    }
    put_str("src:");
    put_str(src);
    put_str("\n");
    put_str("dst:");
    put_str(dst);
    put_str("\n");

    // test strcpy()
    put_str("\n   test strcpy\n");
    strcpy(dst, src);
    put_str("src:");
    put_str(src);
    put_str("\n");
    put_str("dst:");
    put_str(dst);
    put_str("\n");

    // test strlen()
    put_str("\n   test strlen\n");
    uint32_t src_n = strlen(src);
    uint32_t dst_n = strlen(dst);
    put_str("src length:");
    put_int(src_n);
    put_str("\n");
    put_str("dst length:");
    put_int(dst_n);
    put_str("\n");

    // test strcmp()
    put_str("\n   test strcmp\n");
    src[5] = '6';
    if (0 == strcmp(src, dst))
    {
        put_str("The same\n");
    }
    else
    {
        put_str("Different\n");
    }
    put_str("src:");
    put_str(src);
    put_str("\n");
    put_str("dst:");
    put_str(dst);
    put_str("\n");

    // test strchr()
    put_str("\n   test strchr\n");
    char *str = "no pain no gain";
    char *first = strchr(str, 'a');
    put_str(str);
    put_str("\n");
    put_str("first 'a' is at:");
    put_int(first - str);
    put_str("\n");

    // test strrchr()
    put_str("\n   test strrchr\n");
    char *last = strrchr(str, 'a');
    put_str("last 'a' is at:");
    put_int(last - str);
    put_str("\n");

    // test strcat()
    put_str("\n   test strcat\n");
    char str1[50] = "no pain no gain, \0";
    char *str2 = "no sweat no sweet";
    strcat(str1, str2);
    put_str(str1);
    put_str("\n");

    // test strchrs()
    put_str("\n   test strchrs\n");
    int n = strchrs(str1, 'n');
    put_str("count of 'n':"); // 6
    put_int(n);
    put_str("\n");
}
