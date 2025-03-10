#include "string.h"
#include "stdint.h"

void *memset(void *dest, int c, uint64_t n) {
    char *s = (char *)dest;
    for (uint64_t i = 0; i < n; ++i) {
        s[i] = c;
    }
    return dest;
}

void *memcpy(void *str1, const void *str2, uint64_t n) {
    for (int i = 0; i < n; i++) {
        ((char*)str1)[i] = ((const char*)str2)[i];
    }
    return str1;
}

int memcmp(const void *cs, const void *ct, uint64_t count)
{
    const unsigned char *su1, *su2;
    int res = 0;
    for (su1 = cs, su2 = ct; 0 < count; ++su1, ++su2, count--)
        if ((res = *su1 - *su2) != 0)
            break;
    return res;
}