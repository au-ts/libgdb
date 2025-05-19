#include <microkit.h>

#define NUM_DEBUGEES 2

microkit_uint64_t table_metadata[64];
microkit_uint64_t table_data[0x800 * (NUM_DEBUGEES + 100)];

uint32_t read_word(uint16_t client, uintptr_t addr, seL4_Word *val, seL4_Word map_addr);
uint32_t write_word(uint16_t client, uintptr_t addr, seL4_Word val, seL4_Word map_addr);