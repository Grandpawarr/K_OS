#include "string.h"
#include "stddef.h"

/**
 * @brief The memset() function fills the first n bytes of the memory area
 * pointed to by str with the constant byte ch
 *
 * @param str Pointer to the memory area to fill.
 * @param ch  Byte value written to each location.
 * @param n   Number of bytes to fill.
 */
void memset(void* str, uint8_t ch, uint32_t n)
{
    uint8_t *pStr = (uint8_t*) str;
    while(n-->0)
    {
        *pStr++=ch;
    }
}

/**
 * @brief The memcpy() function copies n bytes
 * from memory area src to memory area dst.
 *
 * @param dst Destination buffer.
 * @param src Source buffer.
 * @param n   Number of bytes to copy.
 */
void memcpy(void* dst, const void* src, uint32_t n)
{
    uint8_t *pDst = dst;
    const uint8_t *pSrc = src;
    while(n-->0)
    {
        *pDst++ = *pSrc++;
    }
}

/**
 * @brief The memcmp() function compares the first n bytes
 * of the memory areas s1 and s2
 *
 * @param s1 First memory area.
 * @param s2 Second memory area.
 * @param n  Number of bytes to compare.
 * @return 0 if equal; 1 if s1 > s2; -1 if s1 < s2 at the first differing byte.
 */
int memcmp(const void* s1, const void* s2, uint32_t n)
{
    const char *pS1 = s1;
    const char *pS2 = s2;

    while(n-->0)
    {
        if(*pS1 != *pS2)
        {
            return *pS1 > *pS2 ? 1: -1;
        }
        pS1++;
        pS2++;
    }
    return 0;
}

/**
 * @brief The strcpy() function copies the string pointed to by src
 * to the buffer pointed to by dst.
 *
 * @param dst Destination buffer, must be large enough to hold src.
 * @param src Source string.
 * @return Pointer to dst.
 */
char* strcpy(char* dst, const char* src)
{
    char *pDst = dst;
    while((*dst++ = *src++));

    return pDst;
}

/**
 * @brief The strlen() function calculates the length of the string str.
 *
 * @param str String to measure.
 * @return Number of bytes before the terminating NUL.
 */
uint32_t strlen(const char* str)
{
    const char * pStr = str;
    while(*pStr++);

    return (pStr - str - 1);
}

/**
 * @brief  The strcmp() function compares the two strings s1 and s2.
 *
 * @param s1 First string.
 * @param s2 Second string.
 * @return 0 if equal; 1 if s1 > s2; -1 if s1 < s2.
 */
int8_t strcmp (const char* s1, const char* s2)
{
    while(*s1 != 0 && *s1 == *s2)
    {
        s1++;
        s2++;
    }

    return *s1 < *s2 ? -1 : *s1 > *s2;
}

/**
 * @brief The strchr() function returns a pointer to
 * the first occurrence of the character ch in the string str.
 *
 * @param str String to search.
 * @param ch  Character to look for.
 * @return Pointer to the first match, or NULL if not found.
 */
char* strchr(const char* str, const uint8_t ch)
{
    while (*str != 0)
    {
        if (*str == ch)
        {
            return (char*)str;
        }
        str++;
    }
    return NULL;
}

/**
 * @brief The strrchr() function returns a pointer to
 * the last occurrence of the character ch in the string str.
 *
 * @param str String to search.
 * @param ch  Character to look for.
 * @return Pointer to the last match, or NULL if not found.
 */
char* strrchr(const char* str, const uint8_t ch)
{
    const char* last_char = NULL;
    while (*str != 0)
    {
        if (*str == ch)
        {
            last_char = str;
        }
        str++;
    }
    return (char*)last_char;
}

/**
 * @brief The strcat() function appends the src string to the dst string.
 * overwriting the terminating null byte ('\0') at the end of dst.
 *
 * @param dst Destination string, must be large enough for the result.
 * @param src String to append.
 * @return Pointer to dst.
 */
char* strcat(char* dst, const char* src)
{
    char* pDst = dst;
    while (*pDst++);
    --pDst;
    while((*pDst++ = *src++));
    return dst;
}

/**
 * @brief The strchrs() function returns the count of
 * occurrence of the character ch in the string str.
 *
 * @param str String to scan.
 * @param ch  Character to count.
 * @return Number of occurrences of ch.
 */
uint32_t strchrs(const char* str, uint8_t ch)
{
    uint32_t chCnt = 0;
    const char* p = str;
    while(*p != 0)
    {
        if (*p == ch)
        {
            chCnt++;
        }
        p++;
    }
    return chCnt;
}