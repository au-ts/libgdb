#include <microkit.h>
#include <sel4/sel4.h>
#include <stdint.h>
#include "vspace.h"
#include <sddf/util/printf.h>
#include <sddf/util/util.h>

#define AARCH64_SMALL_PAGE_SIZE 0x1000
#define AARCH64_LARGE_PAGE_SIZE 0x200000

#define ALIGN_4K(x)     (((uintptr_t)(x) + (uintptr_t)(0xFFF)) & ~(uintptr_t)(0xFFF))
#define ALIGN_2MB(x)    (((uintptr_t)(x) + (uintptr_t)(0x1FFFFF)) & ~(uintptr_t)(0x1FFFFF))
#define MIN(i, j) (((i) < (j)) ? (i) : (j))
#define MAX(i, j) (((i) > (j)) ? (i) : (j))

uint64_t small_mapping_region = 0;
uint64_t large_mapping_region = 0;

bool libvspace_initialised = false;

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

    return walk_table((uint64_t *) (table_metadata.table_data_base + start[index]), addr, lvl + 1, page_size);
}

seL4_Word get_page(uint8_t child_id, uintptr_t addr, uint64_t *page_size)
{
    uint64_t page = walk_table((uint64_t *) (table_metadata.table_data_base + table_metadata.pgd[child_id]), addr, 0, page_size);
    return page;
}

uint32_t libvspace_read_bytes(uint16_t client, uintptr_t addr, char *buff, uint64_t nbytes) {
    if (!libvspace_initialised) {
        sddf_dprintf("'libvspace_read_bytes': ERROR libvspace not initialised!\n");
        return 1;
    }
    // These values will be set in the fist iteration of the read loop
    seL4_Word map_addr = 0;
    uint64_t page_size = 0;
    size_t page_aligned_up_curr_addr = 0;

    size_t bytes_to_read = nbytes;
    uintptr_t curr_addr = addr;
    char *curr_buff_addr = buff;

    bool mapped_first_page = false;

    while (bytes_to_read != 0) {
        if (!mapped_first_page || (uintptr_t) curr_addr % page_size == 0) {
            seL4_Word page = get_page(client, (uintptr_t)addr, &page_size);
            if ((page == 0 || page == 0xffffffffffffffff) || page_size == 0) {
                sddf_dprintf("Failed to read byte at addr: 0x%p after remap\n", addr);
                return 1;
            }
            if (page_size == AARCH64_LARGE_PAGE_SIZE) {
                map_addr = large_mapping_region;
                page_aligned_up_curr_addr = ALIGN_2MB(curr_addr + 1);
            } else {
                map_addr = small_mapping_region;
                // This macro aligns up. We add 1 to curr addr as we want to get
                // to the the next page boundary if we are currently at the start of a 
                // page boundary.
                page_aligned_up_curr_addr = ALIGN_4K(curr_addr + 1);
            }
            int err = seL4_ARM_Page_Map(page, VSPACE_CAP, map_addr, seL4_AllRights, seL4_ARM_Default_VMAttributes | seL4_ARM_ExecuteNever);

            if (err) {
                sddf_dprintf("We got an error when mapping page in read_word() at map addr: 0x%lx\n", map_addr);
                return err;
            }
            mapped_first_page = true;
        }

        // Get the amount of space remaining in this page.
        size_t space = page_aligned_up_curr_addr - curr_addr;
        // We can only read at most the space remaining in the page. We
        // will read whatever the minimum of these two values are.
        size_t current_read = MIN(bytes_to_read, space);
        // Calculate the offset into the map addr page
        char *ptr_to_page = (char *) (map_addr + (curr_addr & (page_size - 1)));
        curr_addr += current_read;
        bytes_to_read = bytes_to_read - current_read;

        memcpy(curr_buff_addr, ptr_to_page, current_read);
    }
    return 0;
}

uint32_t libvspace_read_word(uint16_t client, uintptr_t addr, char *val)
{
    if (!libvspace_initialised) {
        sddf_dprintf("'libvspace_read_word': ERROR libvspace not initialised!\n");
        return 1;
    }
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
        sddf_dprintf("We got an error when mapping page in read_word() at map addr: 0x%lx\n", map_addr);
        return err;
    }

    char *ptr_to_page = (char *) (map_addr + (addr & (page_size - 1)));
    memcpy((char *) val, ptr_to_page, sizeof(seL4_Word));
    return 0;
}

uint32_t libvspace_write_word(uint16_t client, uintptr_t addr, seL4_Word val)
{
    if (!libvspace_initialised) {
        sddf_dprintf("'libvspace_write_word': ERROR libvspace not initialised!\n");
        return 1;
    }
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

    int err = seL4_ARM_Page_Map(page, VSPACE_CAP, map_addr, seL4_AllRights, seL4_ARM_Default_VMAttributes);

    if (err) {
        sddf_dprintf("We got an error when mapping page in write_word() at map addr: 0x%lx\n", map_addr);
        return err;
    }

    char *ptr_to_page = (char *) (map_addr + (addr & (page_size - 1)));
    memcpy(ptr_to_page, (char *) &val, 8);

    asm("dmb sy");
    seL4_ARM_VSpace_CleanInvalidate_Data(VSPACE_CAP, map_addr + (addr & (page_size - 1)), map_addr + (addr & (page_size - 1)) + sizeof(uint64_t));
    seL4_ARM_VSpace_Unify_Instruction(BASE_VSPACE_CAP + client, addr , addr + sizeof(uint64_t));

    return 0;
}


uint32_t libvspace_write_page(uint16_t client, uintptr_t addr, char *bytes, size_t nbytes)
{
    uint64_t page_size = 0;
    seL4_Word page = get_page(client, addr, &page_size);
    // sddf_dprintf("page: %lu\n", page);
    if ((page == 0 || page == 0xffffffffffffffff) || page_size == 0) {
        return 1;
    }

    seL4_Word map_addr;

    if (page_size == AARCH64_LARGE_PAGE_SIZE) {
        map_addr = large_mapping_region;
    } else {
        map_addr = small_mapping_region;
    }

    int err = seL4_ARM_Page_Map(page, VSPACE_CAP, map_addr, seL4_AllRights, seL4_ARM_Default_VMAttributes);

    if (err) {
        sddf_dprintf("We got an error when mapping page in write_word()\n");
        __builtin_trap();
    }

    char *ptr_to_page = (char *) (map_addr + (addr & (page_size - 1)));
    if (bytes != NULL) {
        memcpy(ptr_to_page, bytes, nbytes);
    } else {
        memset(ptr_to_page, 0, nbytes);
    }

    asm("dmb sy");
    seL4_ARM_VSpace_CleanInvalidate_Data(VSPACE_CAP, map_addr + (addr & (page_size - 1)), map_addr + (addr & (page_size - 1)) + sizeof(uint64_t));
    seL4_ARM_VSpace_Unify_Instruction(BASE_VSPACE_CAP + client, addr , addr + sizeof(uint64_t));

    return 0;
}

uint32_t libvspace_write_bytes(uint16_t client, uintptr_t start_addr, char *bytes, uint64_t nbytes) {
    if (!libvspace_initialised) {
        sddf_dprintf("'libvspace_write_bytes': ERROR libvspace not initialised!\n");
        return 1;
    }

    // @kwinter: All this logic assumes we are working with small pages.
    size_t page_size = 0x1000;
    size_t bytes_to_transfer = nbytes;
    size_t page_aligned_start_addr = ALIGN_4K(start_addr);
    size_t space = page_aligned_start_addr - start_addr;

    if (space > 0) {
        if (nbytes >= space) {
            uint32_t ret = libvspace_write_page(client, start_addr, bytes, space);
            assert(!ret);
            bytes_to_transfer -= space;
        } else {
            uint32_t ret = libvspace_write_page(client, start_addr, bytes, nbytes);
            assert(!ret);
            return 0;
        }
    }

    size_t n_pages = (bytes_to_transfer / page_size) + 1;
    for (int i = 0; i < n_pages; i++) {
        size_t transfer_nbytes = 0;
        if (bytes_to_transfer < page_size) {
            // This case should only be for the last page
            transfer_nbytes = bytes_to_transfer;
        } else {
            transfer_nbytes = page_size;
        }
        uint32_t ret;
        if (bytes != NULL) {
            ret = libvspace_write_page(client, page_aligned_start_addr + (i * page_size), bytes + (i * page_size) + (start_addr % page_size), transfer_nbytes);
        } else {
            ret = libvspace_write_page(client, page_aligned_start_addr + (i * page_size), NULL, transfer_nbytes);
        }
        assert(!ret);

        bytes_to_transfer -= transfer_nbytes;
    }
    return nbytes - bytes_to_transfer;
}

void libvspace_init_mapping_regions(uint64_t small_map_vaddr, uint64_t large_map_vaddr)
{
    small_mapping_region = small_map_vaddr;
    large_mapping_region = large_map_vaddr;
    libvspace_initialised = true;
}
