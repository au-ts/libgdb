#include <util.h>

static char hexchars[] = "0123456789abcdef";

// seL4_Word strlcpy(char *dest, const char *src, seL4_Word size)
// {
//     seL4_Word len;
//     for (len = 0; len + 1 < size && src[len]; len++) {
//         dest[len] = src[len];
//     }
//     dest[len] = '\0';
//     return len;
// }

// int PURE strncmp(const char *s1, const char *s2, int n)
// {
//     seL4_Word i;
//     int diff;

//     for (i = 0; i < n; i++) {
//         diff = ((unsigned char *)s1)[i] - ((unsigned char *)s2)[i];
//         if (diff != 0 || s1[i] == '\0') {
//             return diff;
//         }
//     }

//     return 0;
// }

// seL4_Word strnlen(const char *s, seL4_Word maxlen)
// {
//     seL4_Word len;
//     for (len = 0; len < maxlen && s[len]; len++);
//     return len;
// }


#define UCHAR_MAX 255
#define SS (sizeof(seL4_Word))
#define ALIGN_MEMCHR (sizeof(seL4_Word)-1)
#define ONES ((seL4_Word)-1/UCHAR_MAX)
#define HIGHS (ONES * (UCHAR_MAX/2+1))
#define HASZERO(x) (((x)-ONES) & ~(x) & HIGHS)

void *memchr(const void *src, int c, seL4_Word n)
{
    const unsigned char *s = src;
    c = (unsigned char)c;
    for (; ((seL4_Word)s & ALIGN_MEMCHR) && n && *s != c; s++, n--);
    if (n && *s != c) {
        const seL4_Word *w;
        seL4_Word k = ONES * c;
        for (w = (const void *)s; n>=SS && !HASZERO(*w ^ k); w++, n-=SS);
        for (s = (const void *)w; n && *s != c; s++, n--);
    }
    return n ? (void *)s : 0;
}

/* Convert a character (representing a hexadecimal) to its integer equivalent */
int hexchar_to_int(unsigned char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

unsigned char int_to_hexchar(int i) {
    if (i < 0 || i > 15) {
        return (unsigned char) -1;
    }

    return hexchars[i];
}

char *hexstr_to_int(char *hex_str, int max_bytes, seL4_Word *val)
{
    int curr_bytes = 0;
    while (*hex_str && curr_bytes < max_bytes) {
        uint8_t byte = *hex_str;
        byte = hexchar_to_int(byte);
        if (byte == (uint8_t) -1) {
            return hex_str;
        }
        *val = (*val << 4) | (byte & 0xF);
        curr_bytes++;
        hex_str++;
    }
    return hex_str;
}

/* Convert a buffer to a hexadecimal string */
char *mem2hex(char *mem, char *buf, int size) {
    int i;
    unsigned char c;
    for (i = 0; i < size; i++, mem++) {
        c = *mem;
        *buf++ = int_to_hexchar(c >> 4);
        *buf++ = int_to_hexchar(c % 16);
    }
    *buf = 0;
    return buf;
}

/* Fill a buffer based with the contents of a hex string */
char *hex2mem(char *buf, char *mem, int size) {
    int i;
    unsigned char c;

    for (i = 0; i < size; i++, mem++) {
        c = hexchar_to_int(*buf++) << 4;
        c += hexchar_to_int(*buf++);
        *mem = c;
    }
    return buf;
}
