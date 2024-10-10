#include <util.h>
#include <stdint.h>

static char hexchars[] = "0123456789abcdef";

#ifdef MICROKIT

seL4_Word strlcpy(char *dest, const char *src, seL4_Word size)
{
    seL4_Word len;
    for (len = 0; len + 1 < size && src[len]; len++) {
        dest[len] = src[len];
    }
    dest[len] = '\0';
    return len;
}

int PURE strncmp(const char *s1, const char *s2, int n)
{
    seL4_Word i;
    int diff;

    for (i = 0; i < n; i++) {
        diff = ((unsigned char *)s1)[i] - ((unsigned char *)s2)[i];
        if (diff != 0 || s1[i] == '\0') {
            return diff;
        }
    }

    return 0;
}

seL4_Word strnlen(const char *s, seL4_Word maxlen)
{
    seL4_Word len;
    for (len = 0; len < maxlen && s[len]; len++);
    return len;
}


void *memset(void *dest, int c, size_t n)
{
	unsigned char *s = dest;
	size_t k;

	/* Fill head and tail with minimal branching. Each
	 * conditional ensures that all the subsequently used
	 * offsets are well-defined and in the dest region. */

	if (!n) return dest;
	s[0] = s[n-1] = c;
	if (n <= 2) return dest;
	s[1] = s[n-2] = c;
	s[2] = s[n-3] = c;
	if (n <= 6) return dest;
	s[3] = s[n-4] = c;
	if (n <= 8) return dest;

	/* Advance pointer to align it at a 4-byte boundary,
	 * and truncate n to a multiple of 4. The previous code
	 * already took care of any head/tail that get cut off
	 * by the alignment. */

	k = -(uintptr_t)s & 3;
	s += k;
	n -= k;
	n &= -4;

#ifdef __GNUC__
	typedef uint32_t __attribute__((__may_alias__)) u32;
	typedef uint64_t __attribute__((__may_alias__)) u64;

	u32 c32 = ((u32)-1)/255 * (unsigned char)c;

	/* In preparation to copy 32 bytes at a time, aligned on
	 * an 8-byte bounary, fill head/tail up to 28 bytes each.
	 * As in the initial byte-based head/tail fill, each
	 * conditional below ensures that the subsequent offsets
	 * are valid (e.g. !(n<=24) implies n>=28). */

	*(u32 *)(s+0) = c32;
	*(u32 *)(s+n-4) = c32;
	if (n <= 8) return dest;
	*(u32 *)(s+4) = c32;
	*(u32 *)(s+8) = c32;
	*(u32 *)(s+n-12) = c32;
	*(u32 *)(s+n-8) = c32;
	if (n <= 24) return dest;
	*(u32 *)(s+12) = c32;
	*(u32 *)(s+16) = c32;
	*(u32 *)(s+20) = c32;
	*(u32 *)(s+24) = c32;
	*(u32 *)(s+n-28) = c32;
	*(u32 *)(s+n-24) = c32;
	*(u32 *)(s+n-20) = c32;
	*(u32 *)(s+n-16) = c32;

	/* Align to a multiple of 8 so we can fill 64 bits at a time,
	 * and avoid writing the same bytes twice as much as is
	 * practical without introducing additional branching. */

	k = 24 + ((uintptr_t)s & 4);
	s += k;
	n -= k;

	/* If this loop is reached, 28 tail bytes have already been
	 * filled, so any remainder when n drops below 32 can be
	 * safely ignored. */

	u64 c64 = c32 | ((u64)c32 << 32);
	for (; n >= 32; n-=32, s+=32) {
		*(u64 *)(s+0) = c64;
		*(u64 *)(s+8) = c64;
		*(u64 *)(s+16) = c64;
		*(u64 *)(s+24) = c64;
	}
#else
	/* Pure C fallback with no aliasing violations. */
	for (; n; n--, s++) *s = c;
#endif

	return dest;
}

#endif

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
