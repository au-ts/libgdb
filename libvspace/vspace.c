#include <microkit.h>
#include <sel4/sel4.h>
#include <stdint.h>
#include "vspace.h"
#include <sddf/util/printf.h>

#define AARCH64_SMALL_PAGE_SIZE 0x1000
#define AARCH64_LARGE_PAGE_SIZE 0x200000

uint64_t small_mapping_region = 0;
uint64_t large_mapping_region = 0;

typedef struct table_meta_data {
    uint64_t table_data_base;
    uint64_t pgd[64];
} table_metadata_t;

table_metadata_t table_metadata;

uint64_t walk_table(uint64_t *start, uintptr_t addr, int lvl, uint64_t *page_size)
{
    // Get the 9 bit index for this level.
    uintptr_t index = 0;
    if (lvl == 0) {
        // Index into the PGD
        index = (addr & ((uintptr_t)0x1ff << 39)) >> 39;
    } else if (lvl == 1) {
        // Index into the PUD
        index = (addr & ((uintptr_t)0x1ff << 30)) >> 30;
    } else if (lvl == 2) {
        // Index into the PD
        index = (addr & ((uintptr_t)0x1ff << 21)) >> 21;
    } else if (lvl == 3) {
        // Index into the PT
        index = (addr & ((uintptr_t)0x1ff << 12)) >> 12;
    }

    if (start[index] == 0xffffffffffffffff) {
        return 0xffffffffffffffff;
    }

    if (lvl == 3) {
        *page_size = AARCH64_SMALL_PAGE_SIZE;
        return start[index];
    } else if (lvl == 2) {
        // Check if we have a large page.
        if (start[index] & ((uint64_t) 1 << 63)) {
            *page_size = AARCH64_LARGE_PAGE_SIZE;
            return (start[index] << 1) >> 1;
        }
    }

    walk_table((uint64_t *) (table_metadata.table_data_base + start[index]), addr, lvl + 1, page_size);
}

seL4_Word get_page(uint8_t child_id, uintptr_t addr, uint64_t *page_size)
{
    uint64_t page = walk_table((uint64_t *) (table_metadata.table_data_base + table_metadata.pgd[child_id]), addr, 0, page_size);
    return page;
}

uint32_t libvspace_read_word(uint16_t client, uintptr_t addr, seL4_Word *val)
{
    uint64_t page_size = 0;
    seL4_Word page = get_page(client, addr, &page_size);
    if ((page == 0 || page == 0xffffffffffffffff) || page_size == 0) {
        return 1;
    }

    seL4_Word map_addr;

    if (page_size == AARCH64_LARGE_PAGE_SIZE) {
        map_addr = large_mapping_region;
    } else {
        map_addr = small_mapping_region;
    }

    // Mapping into vaddr 0x900000, this corresponds to a memory region created in the metaprogram
    // at the same address and 1 page in size.
    int err = seL4_ARM_Page_Map(page, VSPACE_CAP, map_addr, seL4_AllRights, seL4_ARM_Default_VMAttributes | seL4_ARM_ExecuteNever);

    if (err) {
        sddf_dprintf("We got an error when mapping page in read_word()");
    }

    uint64_t *ptr_to_page = (uint64_t *) (map_addr + (addr & (page_size - 1)));
    *val = *ptr_to_page;
    return 0;
}

uint32_t libvspace_write_word(uint16_t client, uintptr_t addr, seL4_Word val)
{
    uint64_t page_size = 0;
    seL4_Word page = get_page(client, addr, &page_size);
    if ((page == 0 || page == 0xffffffffffffffff) || page_size == 0) {
        return 1;
    }

    seL4_Word map_addr;

    if (page_size == AARCH64_LARGE_PAGE_SIZE) {
        map_addr = large_mapping_region;
    } else {
        map_addr = small_mapping_region;
    }

    // Mapping into vaddr 0x900000, this corresponds to a memory region created in the metaprogram
    // at the same address and 1 page in size.
    int err = seL4_ARM_Page_Map(page, VSPACE_CAP, map_addr, seL4_AllRights, seL4_ARM_Default_VMAttributes);

    if (err) {
        sddf_dprintf("We got an error when mapping page in write_word()");
    }

    uint64_t *ptr_to_page = (uint64_t *) (map_addr + (addr & (page_size - 1)));
    *ptr_to_page = val;

    asm("dmb sy");
    seL4_ARM_VSpace_CleanInvalidate_Data(VSPACE_CAP, map_addr + (addr & (page_size - 1)), map_addr + (addr & (page_size - 1)) + sizeof(uint64_t));
    seL4_ARM_VSpace_Unify_Instruction(BASE_VSPACE_CAP + client, addr , addr + sizeof(uint64_t));

    return 0;
}

void libvspace_set_small_mapping_region(uint64_t vaddr)
{
    small_mapping_region = vaddr;
}

void libvspace_set_large_mapping_region(uint64_t vaddr)
{
    large_mapping_region = vaddr;
}