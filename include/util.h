/*
 * Copyright 2024, UNSW Sydney
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once
#include <microkit.h>

static void assert_fail(
    const char  *assertion,
    const char  *file,
    unsigned int line,
    const char  *function)
{
    // @alwin: what to do about this?
    // printf("Failed assertion '%s' at %s:%u in function %s\n", assertion, file, line, function);
    while (1) {}
}

#define assert(expr) \
    do { \
        if (!(expr)) { \
            assert_fail(#expr, __FILE__, __LINE__, __FUNCTION__); \
        } \
    } while(0)

seL4_Word strlcpy(char *dest, const char *src, seL4_Word size);
int PURE strncmp(const char *s1, const char *s2, int n);
seL4_Word strnlen(const char *s, seL4_Word maxlen);
void *memchr (const void *s, int c, seL4_Word size);

int hexchar_to_int(unsigned char c);
unsigned char int_to_hexchar(int i);
char *hexstr_to_int(char *hex_str, int max_bytes, seL4_Word *val);
char *mem2hex(char *mem, char *buf, int size);
char *hex2mem(char *buf, char *mem, int size);