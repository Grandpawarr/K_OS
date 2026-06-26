#ifndef __LIB_INC_STRING_H
#define __LIB_INC_STRING_H
#include "stdint.h"

/**
 * @brief Fill the first @p n bytes of a memory area with a constant byte.
 *
 * @param str Pointer to the memory area to fill.
 * @param ch  Byte value written to each location.
 * @param n   Number of bytes to fill.
 */
void memset(void *str, uint8_t ch, uint32_t n);

/**
 * @brief Copy @p n bytes from @p src to @p dst.
 *
 * The memory areas must not overlap.
 *
 * @param dst Destination buffer.
 * @param src Source buffer.
 * @param n   Number of bytes to copy.
 */
void memcpy(void *dst, const void *src, uint32_t n);

/**
 * @brief Compare the first @p n bytes of two memory areas.
 *
 * @param s1 First memory area.
 * @param s2 Second memory area.
 * @param n  Number of bytes to compare.
 * @return 0 if equal; 1 if @p s1 > @p s2; -1 if @p s1 < @p s2 at the first
 *         differing byte.
 */
int memcmp(const void *s1, const void *s2, uint32_t n);

/**
 * @brief Copy the NUL-terminated string @p src into @p dst.
 *
 * @param dst Destination buffer, must be large enough to hold @p src.
 * @param src Source string.
 * @return Pointer to @p dst.
 */
char *strcpy(char *dst, const char *src);

/**
 * @brief Compute the length of a NUL-terminated string.
 *
 * @param str String to measure.
 * @return Number of bytes before the terminating NUL.
 */
uint32_t strlen(const char *str);

/**
 * @brief Compare two NUL-terminated strings.
 *
 * @param s1 First string.
 * @param s2 Second string.
 * @return 0 if equal; 1 if @p s1 > @p s2; -1 if @p s1 < @p s2.
 */
int8_t strcmp(const char *s1, const char *s2);

/**
 * @brief Find the first occurrence of @p ch in @p str.
 *
 * @param str String to search.
 * @param ch  Character to look for.
 * @return Pointer to the first match, or NULL if not found.
 */
char *strchr(const char *str, const uint8_t ch);

/**
 * @brief Find the last occurrence of @p ch in @p str.
 *
 * @param str String to search.
 * @param ch  Character to look for.
 * @return Pointer to the last match, or NULL if not found.
 */
char *strrchr(const char *str, const uint8_t ch);

/**
 * @brief Append @p src to the end of @p dst.
 *
 * Overwrites the terminating NUL of @p dst and re-terminates the result.
 *
 * @param dst Destination string, must be large enough for the result.
 * @param src String to append.
 * @return Pointer to @p dst.
 */
char *strcat(char *dst, const char *src);

/**
 * @brief Count how many times @p ch appears in @p str.
 *
 * @param str String to scan.
 * @param ch  Character to count.
 * @return Number of occurrences of @p ch.
 */
uint32_t strchrs(const char *str, uint8_t ch);

#endif
