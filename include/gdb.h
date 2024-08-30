/*
 * Copyright 2024, UNSW
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

// @alwin: Ideally this shouldn't depend on microkit
#ifdef MICROKIT
#include <microkit.h>
#else
#include <sel4/sel4.h>
#include <sel4/constants.h>
#include <stdint.h>
#include <stdbool.h>
#endif /* MICROKIT */
#include <sel4/sel4_arch/types.h>

#define MAX_PDS 64
#define MAX_THREADS 256
#define MAX_ELF_NAME 32
#define MAX_SW_BREAKS 32

// @alwin: All the output strclpy things use this #define. This is quite likely a bad design choice.
#define BUFSIZE 2048

/* Bookkeeping for watchpoints */
typedef struct watchpoint {
    uint64_t addr;
    seL4_Word size; //@alwin: No real reason for this to be so big
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

struct inferior;
typedef struct inferior gdb_inferior_t;

/* Each inferior can also have multiple threads within it */
typedef struct thread {
    bool enabled;
    bool wakeup;
    bool ss_enabled;
    gdb_inferior_t *inferior;
    /* The id is something provided by the remote and is used to identify a thread when something such
       as a fault occurs */
    uint64_t id;
    /* The gdb_id is internal to GDB and is used for ease of implementation and efficiency reasons.
       This is the id that is told to GDB. */
    uint16_t gdb_id;
    seL4_CPtr tcb;
    sw_break_t software_breakpoints[MAX_SW_BREAKS];
    hw_break_t hardware_breakpoints[seL4_NumExclusiveBreakpoints];
    hw_watch_t hardware_watchpoints[seL4_NumExclusiveWatchpoints];
} gdb_thread_t;

/* GDB uses 'inferiors' to distinguish between different processes (in our case PDs) */
struct inferior {
    bool enabled;
    /* The id is something provided by the remote and is used to identify a thread when something such
       as a fault occurs */
    uint64_t id;
    /* The gdb_id is internal to GDB and is used for ease of implementation and efficiency reasons.
       This is the id that is told to GDB. */
    uint16_t gdb_id;
    seL4_CPtr vspace;
    int curr_thread_idx;
    gdb_thread_t threads[MAX_THREADS];
};

/* We expose the current target inferior and list of inferiors to users of the library */

/* @alwin: The reason I did this originally was because I thought that the user might have
 * a more efficient way of linking an id to an inferior than libGDB does, but I think this is no
 * longer the case with the thread allocation strategy used in libGDB. It would be better for
 * users to just provide inferior/target id and have libGDB use a hashmap or something internally.
 */
extern gdb_thread_t *target_thread;
extern gdb_inferior_t inferiors[MAX_PDS];

typedef enum continue_type {
    ctype_dont = 0,
    ctype_continue,
    ctype_ss,
} cont_type_t;

bool set_software_breakpoint(gdb_thread_t *thread, seL4_Word address);
bool unset_software_breakpoint(gdb_thread_t *thread, seL4_Word address);

bool set_hardware_breakpoint(gdb_thread_t *thread, seL4_Word address);
bool unset_hardware_breakpoint(gdb_thread_t *thread, seL4_Word address);

bool set_hardware_watchpoint(gdb_thread_t *thread, seL4_Word address,
                             seL4_BreakpointAccess type, seL4_Word size);
bool unset_hardware_watchpoint(gdb_thread_t *thread, seL4_Word address,
                               seL4_BreakpointAccess type, seL4_Word size);

bool enable_single_step(gdb_thread_t *thread);
bool disable_single_step(gdb_thread_t *thread);

/* Convert registers to a hex string */
char *regs2hex(seL4_UserContext *regs, char *buf);

/* Convert registers to a hex string */
char *hex2regs(seL4_UserContext *regs, char *buf);

char *inf_mem2hex(gdb_thread_t *inferior, seL4_Word mem, char *buf, int size, seL4_Word *error);
seL4_Word inf_hex2mem(gdb_thread_t *inferior, char *buf, seL4_Word mem, int size);

gdb_inferior_t *gdb_register_inferior(uint64_t id, seL4_CPtr vspace);
gdb_thread_t *gdb_register_thread(gdb_inferior_t *inferior, uint64_t id, seL4_CPtr tcb);
void gdb_thread_spawn(gdb_thread_t *thread, char *output);
void gdb_thread_exit(gdb_thread_t *thread, char *output);

// int gdb_register_inferior_fork(uint8_t id, char *output);
// int gdb_register_inferior_exec(uint8_t id, char *elf_name, seL4_CPtr tcb, seL4_CPtr vspace, char *output);
bool gdb_handle_fault(gdb_thread_t *thread, seL4_Word exception_reason, seL4_Word *reply_mr, char *output);

bool gdb_handle_packet(char *input, char *output, bool *detached);

