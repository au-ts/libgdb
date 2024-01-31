//
// Created by Alwin Joshy on 22/1/2024.
//
#pragma once

#include <uart.h>
#include <sel4/sel4_arch/types.h>
#include <microkit.h>



#define MAX_ELF_NAME 32
#define MAX_SW_BREAKS 10

/* Bookkeeping for watchpoints */
typedef struct watchpoint {
    uint64_t addr;
    seL4_BreakpointAccess type;
} hw_watch_t;

/* Bookkeeping for hardware breakpoints */
typedef struct hw_breakpoint {
    uint64_t addr;
} hw_break_t;

/* Bookkeeping for software breakpoints */
typedef struct sw_breakpoint {
    uint64_t addr;
    uint64_t orig_word;
} sw_break_t;

/* GDB uses 'inferiors' to distinguish between different processes (in our case PDs) */
typedef struct inferior {
    uint8_t id;
    /* The id in GDB cannot be 0, because this has a special meaning in GDB */
    uint16_t gdb_id;
    seL4_CPtr tcb;
    seL4_CPtr vspace;
    char elf_name[MAX_ELF_NAME];
    sw_break_t software_breakpoints[MAX_SW_BREAKS];
    hw_break_t hardware_breakpoints[seL4_NumExclusiveBreakpoints];
    hw_watch_t hardware_watchpoints[seL4_NumExclusiveWatchpoints];
    bool ss_enabled;
} inferior_t;

typedef enum continue_type {
    ctype_continue = 0,
    ctype_ss,
} cont_type_t;

bool set_software_breakpoint(inferior_t *inferior, seL4_Word address);
bool unset_software_breakpoint(inferior_t *inferior, seL4_Word address);

bool set_hardware_breakpoint(inferior_t *inferior, seL4_Word address);
bool unset_hardware_breakpoint(inferior_t *inferior, seL4_Word address);

bool set_hardware_watchpoint(inferior_t *inferior, seL4_Word address,
                             seL4_BreakpointAccess type);
bool unset_hardware_watchpoint(inferior_t *inferior, seL4_Word address,
                               seL4_BreakpointAccess type);

bool enable_single_step(inferior_t *inferior);
bool disable_single_step(inferior_t *inferior);

/* Convert registers to a hex string */
char *regs2hex(seL4_UserContext *regs, char *buf);

/* Convert registers to a hex string */
char *hex2regs(seL4_UserContext *regs, char *buf);

char *inf_mem2hex(inferior_t *inferior, seL4_Word mem, char *buf, int size, seL4_Word *error);
seL4_Word inf_hex2mem(inferior_t *inferior, char *buf, seL4_Word mem, int size);

int gdb_register_initial(uint8_t id, char* elf_name, seL4_CPtr tcb, seL4_CPtr vspace);
int gdb_register_inferior(uint8_t id, char *elf_name, seL4_CPtr tcb, seL4_CPtr vspace);
int gdb_handle_fault(uint8_t id, seL4_Word exception_reason, seL4_Word *reply_mr);

cont_type_t gdb_event_loop();

