//
// Created by Alwin Joshy on 23/1/2024.
//

#pragma once

#include <microkit.h>

static inline seL4_Word arch_to_big_endian(seL4_Word vaddr) {
    return __builtin_bswap64(vaddr);
}