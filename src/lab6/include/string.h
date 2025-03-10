#ifndef __STRING_H__
#define __STRING_H__

#include "stdint.h"

void *memset(void *, int, uint64_t);
void *memcpy(void *str1, const void *str2, uint64_t n);
int memcmp(const void *cs, const void *ct, uint64_t count);

static inline int strlen(const char *str)
{
    int len = 0;
    while (*str++)
        len++;
    return len;
}
#endif
