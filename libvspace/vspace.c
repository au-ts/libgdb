#include <microkit.h>
#include <sel4/sel4.h>
#include <stdint.h>
#include "vspace.h"

microkit_uint64_t walk_table(microkit_uint64_t *start, uintptr_t addr, int lvl)
{
    // Get the 9 bit index for this level.
    uintptr_t index = 0;
    if (lvl == 1) {
        // Index into the PUD
        index = (addr & ((uintptr_t)0x1ff << 30)) >> 30;
    } else if (lvl == 2) {
        // Index into the PD
        index = (addr & ((uintptr_t)0x1ff << 21)) >> 21;
    } else if (lvl == 3) {
        // Index into the PT
        index = (addr & ((uintptr_t)0x1ff << 12)) >> 12;
    }
    if (lvl == 3) {
       return start[index];
    } else {
        walk_table(&table_data[start[index]/8], addr, lvl + 1);
    }
}

seL4_Word get_page(uint8_t child_id, uintptr_t addr) {
    microkit_uint64_t page = walk_table(&table_data[table_metadata[child_id]/8], addr, 0);
    return page;
}

uint32_t read_word(uint16_t client, uintptr_t addr, seL4_Word *val, seL4_Word map_addr) {
    seL4_Word page = get_page(client, addr);
    if (page == 0 || page == 0xffffffffffffffff) {
        return 1;
    }

    // Mapping into vaddr 0x900000, this corresponds to a memory region created in the metaprogram
    // at the same address and 1 page in size.
    int err = seL4_ARM_Page_Map(page, VSPACE_CAP, map_addr, seL4_AllRights, seL4_ARM_Default_VMAttributes | seL4_ARM_ExecuteNever);

    if (err) {
        microkit_dbg_puts("We got an error when mapping page in read_word()");
    }

    uint64_t *ptr_to_page = (uint64_t *) (map_addr + (addr & 0xfff));
    *val = *ptr_to_page;
    return 0;
}

uint32_t write_word(uint16_t client, uintptr_t addr, seL4_Word val, seL4_Word map_addr) {
    seL4_Word page = get_page(client, addr);
    if (page == 0 || page == 0xffffffffffffffff) {
        return 1;
    }

    // Mapping into vaddr 0x900000, this corresponds to a memory region created in the metaprogram
    // at the same address and 1 page in size.
    int err = seL4_ARM_Page_Map(page, VSPACE_CAP, map_addr, seL4_AllRights, seL4_ARM_Default_VMAttributes | seL4_ARM_ExecuteNever);

    if (err) {
        microkit_dbg_puts("We got an error when mapping page in write_word()");
    }

    uint64_t *ptr_to_page = (uint64_t *) (map_addr + (addr & 0xfff));
    *ptr_to_page = val;
    return 0;
}
