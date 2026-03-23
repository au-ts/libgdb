#pragma once

#include <microkit.h>

uint32_t libvspace_read_word(uint16_t client, uintptr_t addr, char *val);
uint32_t libvspace_read_bytes(uint16_t client, uintptr_t start_addr, char *buff, uint64_t nbytes);
uint32_t libvspace_write_word(uint16_t client, uintptr_t addr, seL4_Word val);
uint32_t libvspace_write_bytes(uint16_t client, uintptr_t start_addr, char *bytes, uint64_t nbytes);
void libvspace_init_mapping_regions(uint64_t small_map_vaddr, uint64_t large_map_vaddr);
