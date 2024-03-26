/*
 * Copyright 2024, UNSW
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <microkit.h>

static inline seL4_Word arch_to_big_endian(seL4_Word vaddr) {
    return __builtin_bswap64(vaddr);
}