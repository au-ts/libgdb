#include <microkit.h>

#define NUM_DEBUGEES 2

typedef struct table_meta_data {
    uint64_t table_data_base;
    uint64_t pgd[64];
} table_metadata_t;

table_metadata_t table_metadata;

uint32_t read_word(uint16_t client, uintptr_t addr, seL4_Word *val, seL4_Word map_addr);
uint32_t write_word(uint16_t client, uintptr_t addr, seL4_Word val, seL4_Word map_addr);