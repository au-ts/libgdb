/*
 * Copyright 2024, UNSW
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

// @alwin: Ideally this shouldn't depend on microkit
#include <microkit.h>
#include <sel4/sel4_arch/types.h>

#define MAX_PDS 64
#define MAX_ELF_NAME 32
#define MAX_SW_BREAKS 10

// @alwin: All the output strclpy things use this #define. This is quite likely a bad design choice.
#define BUFSIZE 1024

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
    bool enabled;
    bool wakeup;
    uint8_t id;
    /* The id in GDB cannot be 0, because this has a special meaning in GDB */
    uint16_t gdb_id;
    seL4_CPtr tcb;
    seL4_CPtr vspace;
    sw_break_t software_breakpoints[MAX_SW_BREAKS];
    hw_break_t hardware_breakpoints[seL4_NumExclusiveBreakpoints];
    hw_watch_t hardware_watchpoints[seL4_NumExclusiveWatchpoints];
    bool ss_enabled;
} inferior_t;

/* We expose the current target inferior to users of the library */
extern inferior_t *target_inferior;
extern inferior_t inferiors[MAX_PDS];

typedef enum continue_type {
    ctype_dont = 0,
    ctype_continue,
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

int gdb_register_inferior(uint8_t id, seL4_CPtr tcb, seL4_CPtr vspace);
// int gdb_register_inferior_fork(uint8_t id, char *output);
// int gdb_register_inferior_exec(uint8_t id, char *elf_name, seL4_CPtr tcb, seL4_CPtr vspace, char *output);
bool gdb_handle_fault(uint8_t id, seL4_Word exception_reason, seL4_Word *reply_mr, char *output);

bool gdb_handle_packet(char *input, char *output);

