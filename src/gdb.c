/*
 * Copyright 2024, UNSW
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <gdb.h>
#include <arch/arm/64/gdb.h>
#include <util.h>
#include <sel4/constants.h>
#include <printf.h>
#include <string.h>
#ifndef MICROKIT
#include <assert.h>
#endif /* MICROKIT */

//#define DEBUG_PRINTS 1
// @alwin: increase this
#define GDB_INFERIOR_ID_TO_IDX(x) ((x - 1) % MAX_PDS)
#define GDB_THREAD_ID_TO_IDX(x) ((x - 1) % MAX_THREADS)

#define PROC_ID_ALL (-1)
#define PROC_ID_ANY 0

#define THREAD_ID_ALL (-1)
#define THREAD_ID_ANY 0

int curr_inferior_idx = 0;
gdb_inferior_t inferiors[MAX_PDS] = {0};
gdb_thread_t *target_thread = NULL;

/* Read registers */
static void handle_read_regs(char *output) {
    seL4_UserContext context;
    int error = seL4_TCB_ReadRegisters(target_thread->tcb, true, 0,
                                       sizeof(seL4_UserContext) / sizeof(seL4_Word), &context);
    regs2hex(&context, output);
}

/* Write registers */
static void handle_write_regs(char *ptr, char* output) {
    assert(*ptr++ == 'G');

    seL4_UserContext context;
    hex2regs(&context, ptr);
    int error = seL4_TCB_WriteRegisters(target_thread->tcb, true, 0,
                                        sizeof(seL4_UserContext) / sizeof(seL4_Word), &context);
    strlcpy(output, "OK", BUFSIZE);
}

// @alwin: Make this safe
static char *write_thread_id(gdb_thread_t *thread, char *ptr, int len) {
    *(ptr++) = 'p';
    ptr = mem2hex((char *) &thread->inferior->gdb_id, ptr, sizeof(uint8_t));
    *(ptr++) = '.';
    return mem2hex((char *) &thread->gdb_id, ptr, sizeof(uint8_t));
}

static void handle_query(char *ptr, char *output) {
    if (strncmp(ptr, "qSupported", 10) == 0) {
        /* TODO: This may eventually support more features */
        snprintf(output, BUFSIZE,
                 "qSupported:PacketSize=%lx;QThreadEvents+;swbreak+;hwbreak+;vContSupported+;fork-events+;exec-events+;multiprocess+;", BUFSIZE);
    } else if (strncmp(ptr, "qfThreadInfo", 12) == 0) {
        char *out_ptr = output;
        *out_ptr++ = 'm';
        int num_printed = 0;
        for (int i = 0; i < MAX_PDS; i++) {
            if (inferiors[i].enabled) {
                for (int j = 0; j < MAX_THREADS; j++) {
                    if (inferiors[i].threads[j].enabled) {
                        if (num_printed > 0) {
                            *out_ptr++ = ',';
                        }
                        out_ptr = write_thread_id(&inferiors[i].threads[j], out_ptr, 0);
                        num_printed++;
                    }
                }
            }
        }
    } else if (strncmp(ptr, "qsThreadInfo", 12) == 0) {
        strlcpy(output, "l", BUFSIZE);
    } else if (strncmp(ptr, "qC", 2) == 0) {
        strlcpy(output, "QCp1.1", BUFSIZE);
    } else if (strncmp(ptr, "qSymbol", 7) == 0) {
        strlcpy(output, "OK", BUFSIZE);
    } else if (strncmp(ptr, "qTStatus", 8) == 0) {
        /* TODO: THis should eventually work in the non startup case */
        strlcpy(output, "T0", BUFSIZE);
    } else if (strncmp(ptr, "qAttached", 9) == 0) {
        strlcpy(output, "1", BUFSIZE);
    } else if (strncmp(ptr, "QThreadEvents:1", 15) == 0) {
        strlcpy(output, "OK", BUFSIZE);
    } else if (strncmp(ptr, "QThreadEvents:0", 15) == 0) {
        strlcpy(output, "OK", BUFSIZE);
    }
}

/* Expected string is of the form [Mm][a-fA-F0-9]{sizeof(seL4_Word) * 2},[a-fA-F0-9] +*/
static bool parse_mem_format(char *ptr, seL4_Word *addr, seL4_Word *size)
{
    *addr = 0;
    *size = 0;
    bool is_read = true;

    /* Are we dealing with a memory read or a memory write? */
    if (*ptr++ == 'M') {
        is_read = false;
    }

    /* Parse the address */
    ptr = hexstr_to_int(ptr, sizeof(seL4_Word) * 2, addr);
    if (*ptr++ != ',') {
        return false;
    }

    /* Parse the size */
    ptr = hexstr_to_int(ptr, sizeof(seL4_Word) * 2, size);

    /* Check that we have reached the end of the string */
    if (is_read) {
        // strlcpy(output, "E01", 4);
        return (*ptr == 0);
    } else {
        return (*ptr == 0 || *ptr == ':');
    }
}

static bool parse_breakpoint_format(char *ptr, seL4_Word *addr, seL4_Word *kind)
{
    /* Parse the first three characters */
    assert (*ptr == 'Z' || *ptr == 'z');
    ptr++;
    assert (*ptr >= '0' && *ptr <= '4');
    ptr++;
    assert(*ptr++ == ',');

    /* Parse the addr and kind */

    *addr = 0;
    *kind = 0;

    ptr = hexstr_to_int(ptr, sizeof(seL4_Word) * 2, addr);
    if (*ptr++ != ',') {
        return false;
    }

    /* This is generally to do with the size of the breakpoint that is set. This will be 4
       for breakpoints and any of the following for watchpoints */
    ptr = hexstr_to_int(ptr, sizeof(seL4_Word) * 2, kind);
    if (*kind != 1 && *kind != 2 && *kind != 4 && *kind != 8) {
        return false;
    }

    return true;
}


static void handle_configure_debug_events(char *ptr, char *output) {
    /* Precondition: ptr[0] is always 'z' or 'Z' */
    seL4_Word addr, size;
    bool success = false;

    if (!parse_breakpoint_format(ptr, &addr, &size)) {
        strlcpy(output, "E01", BUFSIZE);
        return;
    }

    /* Breakpoints and watchpoints */

    if (strncmp(ptr, "Z0", 2) == 0) {
        /* Set a software breakpoint using binary rewriting */
        success = set_software_breakpoint(target_thread->inferior, addr);
    } else if (strncmp(ptr, "z0", 2) == 0) {
        /* Unset a software breakpoint */
        printf("\nUnsetting a software breakpoint \n");
        success = unset_software_breakpoint(target_thread->inferior, addr);
    } else if (strncmp(ptr, "Z1", 2) == 0) {
        /* Set a hardware breakpoint */
        success = set_hardware_breakpoint(target_thread->inferior, addr);
    } else if (strncmp(ptr, "z1", 2) == 0) {
        /* Unset a hardware breakpoint */
        success = unset_hardware_breakpoint(target_thread->inferior, addr);
    } else {
        seL4_BreakpointAccess watchpoint_type;
        switch (ptr[1]) {
            case '2':
                watchpoint_type = seL4_BreakOnWrite;
                break;
            case '3':
                watchpoint_type = seL4_BreakOnRead;
                break;
            case '4':
                watchpoint_type = seL4_BreakOnReadWrite;
                break;
            default:
                strlcpy(output, "E01", BUFSIZE);
                return;
        }

        if (ptr[0] == 'Z') {
            success = set_hardware_watchpoint(target_thread->inferior, addr, watchpoint_type, size);
        } else {
            success = unset_hardware_watchpoint(target_thread->inferior, addr, watchpoint_type, size);
        }
    }

    if (!success) {
        strlcpy(output, "E01", BUFSIZE);
    } else {
        strlcpy(output, "OK", BUFSIZE);
    }
}


gdb_inferior_t *gdb_register_inferior(uint64_t id, seL4_CPtr vspace) {
    int end_idx = curr_inferior_idx + MAX_PDS;
    gdb_inferior_t *inferior = NULL;

    /* Find a free slot to put this inferior */
    for (; curr_inferior_idx < end_idx; curr_inferior_idx++) {
        if (inferiors[curr_inferior_idx % MAX_PDS].enabled) {
            continue;
        }

        inferior = &inferiors[curr_inferior_idx % MAX_PDS];
        inferior->enabled = true;
        inferior->id = id;
        inferior->gdb_id = ++curr_inferior_idx;
        inferior->vspace = vspace;
        inferior->curr_thread_idx = 0;
        memset(inferior->threads, 0, MAX_THREADS * sizeof(gdb_thread_t));
        memset(inferior->software_breakpoints, 0, MAX_SW_BREAKS * sizeof(sw_break_t));
        memset(inferior->hardware_breakpoints, 0, seL4_NumExclusiveBreakpoints * sizeof(hw_break_t));
        memset(inferior->hardware_watchpoints, 0, seL4_NumExclusiveWatchpoints * sizeof(hw_watch_t));
        break;
    }

    return inferior;
}

gdb_thread_t *gdb_register_thread(gdb_inferior_t *inferior, uint64_t id, seL4_CPtr tcb) {
    int end_idx = inferior->curr_thread_idx + MAX_THREADS;
    gdb_thread_t *thread = NULL;

    for (; inferior->curr_thread_idx < end_idx; inferior->curr_thread_idx++) {
        if (inferior->threads[inferior->curr_thread_idx % MAX_THREADS].enabled) {
            continue;
        }

        thread = &inferior->threads[inferior->curr_thread_idx % MAX_THREADS];
        thread->enabled = true;
        thread->wakeup = false;
        thread->ss_enabled = false;
        thread->inferior = inferior;
        thread->id = id;
        thread->gdb_id = ++inferior->curr_thread_idx;
        thread->tcb = tcb;

        /* Set all the hardware breakpoints and watchpoints that have already been set
           for this inferior */
        for (int i = 0; i < seL4_NumExclusiveBreakpoints; i++) {
            // @alwin: Deal with error case
            thread_enable_nth_hw_breakpoint(thread, i);
        }

        for (int i = 0; i< seL4_NumExclusiveWatchpoints; i++) {
            // @alwin: Deal with error case
            thread_enable_nth_hw_watchpoint(thread, i);
        }

        break;
    }

    if (!target_thread && thread) {
        target_thread = thread;
    }

    return thread;
}

static void handle_read_mem(char *ptr, char *output) {
    seL4_Word addr, size, error;

    if (!parse_mem_format(ptr, &addr, &size)) {
        /* Error parsing input */
        strlcpy(output, "E01", BUFSIZE);
    } else if (size * 2 > BUFSIZE - 1) {
        /* Buffer too small? Don't really get this */
        strlcpy(output, "E01", BUFSIZE);
    } else {
        if (inf_mem2hex(target_thread, addr, output, size, &error) == NULL) {
            /* Failed to read the memory at the location */
           strlcpy(output, "E04", BUFSIZE);
        }
    }
}

static void handle_write_mem(char *ptr, char *output) {
    seL4_Word addr, size;

    if (!parse_mem_format(ptr, &addr, &size)) {
        strlcpy(output, "E02", BUFSIZE);
    } else {
        if ((ptr = memchr(ptr, ':', BUFSIZE))) {
            ptr++;
            if (inf_hex2mem(target_thread, ptr, addr, size) == 0) {
                strlcpy(output, "E03", BUFSIZE);
            } else {
                strlcpy(output, "OK", BUFSIZE);
            }
        }
    }
}

static char *parse_thread_id(char *ptr, int *proc_id, int* thread_id) {
    int size = 0;
    if (*ptr++ != 'p') {
        return NULL;
    }

    char *ptr_tmp = ptr;
    if (strncmp(ptr, "-1", 3) == 0) {
        *proc_id = -1;
        return ptr + 2;
    }

    while (*ptr_tmp++ != '.') {
        size++;
    }

    /* 2 hex chars per byte */
    size /= 2;

    if (size == 0) {
        *proc_id = hexchar_to_int(*ptr);
    } else {
        hex2mem(ptr, (char *) proc_id, size);
    }

    ptr = ptr_tmp;
    size = 0;
    if (strncmp(ptr, "-1", 3) == 0) {
        *thread_id = -1;
        return ptr + 2;
    }

    while (hexchar_to_int(*ptr_tmp++) != -1) {
        size++;
    }

    /* 2 hex chars per byte */
    size /= 2;

    if (size == 0) {
        *thread_id = hexchar_to_int(*ptr);
    } else {
        hex2mem(ptr, (char *) thread_id, size);
    }

    // Return the last character that was not a hexchar
    return ptr_tmp - 1;
}

static gdb_thread_t *lookup_thread_from_gdb_id(int proc_id, int thread_id) {
    gdb_inferior_t *inferior = &inferiors[GDB_INFERIOR_ID_TO_IDX(proc_id)];
    if (inferior->gdb_id != proc_id) {
        return NULL;
    }

    gdb_thread_t *thread = &inferior->threads[GDB_THREAD_ID_TO_IDX(thread_id)];
    if (thread->gdb_id != thread_id) {
        return NULL;
    }

    return thread;
}

static gdb_inferior_t *lookup_inferior_from_gdb_id(int proc_id) {
    gdb_inferior_t *inferior = &inferiors[(proc_id - 1) % MAX_PDS];
    if (inferior->gdb_id != proc_id) {
        return NULL;
    }

    return inferior;
}

static void handle_check_thread_alive(char *ptr, char *output) {
    assert(*ptr++ == 'T');

    int proc_id, thread_id = 0;
    if (parse_thread_id(ptr, &proc_id, &thread_id) == NULL) {
        strlcpy(output, "E02", BUFSIZE);
        return;
    }

    gdb_thread_t *thread = lookup_thread_from_gdb_id(proc_id, thread_id);
    if (thread->gdb_id == thread_id && thread->enabled == true) {
        strlcpy(output, "OK", BUFSIZE);
    }

    return;
}

static void handle_set_inferior(char *ptr, char *output) {
    int proc_id, thread_id = 0;

    assert(*ptr++ = 'H');

    if (*ptr != 'g' && *ptr != 'c') {
        strlcpy(output, "E01", BUFSIZE);
        return;
    }
    ptr++;

    if (*ptr == '-' && *(ptr + 1) == '1') {
        assert(target_thread != NULL);
        strlcpy(output, "OK", BUFSIZE);
        return;
    } else if (parse_thread_id(ptr, &proc_id, &thread_id) == NULL) {
        // @alwin: Do we care about thread_id here?
        strlcpy(output, "E02", BUFSIZE);
        return;
    }

    assert(proc_id != PROC_ID_ALL);
    if (proc_id != PROC_ID_ANY) {
        gdb_inferior_t *inferior = lookup_inferior_from_gdb_id(proc_id);
        if (!inferior) {
            strlcpy(output, "E02", BUFSIZE);
            return;
        }

        if (thread_id != THREAD_ID_ALL) {
            if (thread_id == THREAD_ID_ANY) {
                // @alwin: Is this the behaviour we want? I don't think so
                // because thread 1 could be dead
                target_thread = lookup_thread_from_gdb_id(proc_id, 1);
            } else {
                target_thread = lookup_thread_from_gdb_id(proc_id, thread_id);
            }
        } else {
            // @alwin: Figure out what to do when thread_id == THREAD_ID_ALL
        }
    } else {
        // @alwin: Figure out what to do when proc_id == PROC_ID_ANY
        // For now, we just leave the current thread as the target thread, which seems fairly valid
    }

    assert(target_thread->enabled && target_thread->tcb != 0);
    strlcpy(output, "OK", BUFSIZE);
}

void handle_sig_interrupt(char *output) {
    strlcpy(output, "S02", BUFSIZE);
}

void handle_vcont(char *input, char *output) {
    /* Skip the original vcont prefix */
    input += 5;

    bool handled[MAX_PDS][MAX_THREADS] = {0};

    while (*input != 0) {
        assert(*input++ == ';');

        bool stepping;
        if (*input == 's') {
            /* If we are stepping, only the thing being stepped should continue*/
            stepping = true;
            for (int i = 0; i < MAX_PDS; i++) {
                if (!inferiors[i].enabled) continue;
                for (int j = 0; j < MAX_THREADS; j++) {
                    if (!inferiors[i].threads[j].enabled) continue;
                    inferiors[i].threads[j].wakeup = false;
                }
            }
        } else if (*input == 'c') {
            stepping = false;
            // @alwin: I think this is a bit dodgy. not entirely convinced that this will
            // work when there are both step and continue things in the same package
            for (int i = 0; i < MAX_PDS; i++) {
                if (!inferiors[i].enabled) continue;
                for (int j = 0; j < MAX_THREADS; j++) {
                    if (!inferiors[i].threads[j].enabled) continue;
                    inferiors[i].threads[j].wakeup = true;
                }
            }
        } else {
            /* @alwin: For now only deal with stepping and continuing */
            strlcpy(output, "E04", BUFSIZE);
            return;
        }
        input++;

        do {
            assert(*input++ == ':');
            int proc_id, thread_id;
            input = parse_thread_id(input, &proc_id, &thread_id);

            if (proc_id == PROC_ID_ALL) {
                // @alwin: In this case, we should select all inferiors.
                printf("TODO: proc_id=-1 provided in vCont packet");
                continue;
            }

            gdb_inferior_t *inferior = lookup_inferior_from_gdb_id(proc_id);

            /* If thread_id == 1, we want to apply the action to all the threads in the inferior  */
            int i = 0;
            int n = MAX_THREADS;
            if (thread_id != THREAD_ID_ALL) {
                i = GDB_THREAD_ID_TO_IDX(thread_id);
                n = thread_id;
            }

            for (; i < n; i++) {
                if (!inferior->threads[i].enabled) {
                    continue;
                }

                if (handled[GDB_INFERIOR_ID_TO_IDX(proc_id)][i]) {
                    continue;
                }

                if (stepping) {
                    enable_single_step(&inferior->threads[i]);
                } else {
                    disable_single_step(&inferior->threads[i]);
                }

                handled[GDB_INFERIOR_ID_TO_IDX(proc_id)][i] = true;
                inferior->threads[i].wakeup = true;
            }
        } while (*input == ':');
    }
}

static void handle_detach(char *ptr, char *output) {
    /* @alwin: This packet could also be used to detach a single specific process */
    strlcpy(output, "OK", BUFSIZE);

    for (int i = 0; i < MAX_PDS; i++) {
        if (!inferiors[i].enabled) continue;

        gdb_inferior_t *inferior = &inferiors[i];

        /* @alwin: This is kinda stupid and slow */
        /* Clear any breakpoints/watchpoints */
        for (int i = 0; i < MAX_SW_BREAKS; i++) {
            unset_software_breakpoint(inferior, inferior->software_breakpoints[i].addr);
        }
        for (int i = 0; i < seL4_NumExclusiveBreakpoints; i++) {
            unset_hardware_breakpoint(inferior, inferior->hardware_breakpoints[i].addr);
        }
        for (int i = 0; i < seL4_NumExclusiveWatchpoints; i++) {
            unset_hardware_watchpoint(inferior, inferior->hardware_watchpoints[i].addr,
                                      inferior->hardware_watchpoints[i].type,
                                      inferior->hardware_watchpoints[i].size);
        }
        memset(inferior->software_breakpoints, 0, MAX_SW_BREAKS * sizeof(sw_break_t));
        memset(inferior->hardware_breakpoints, 0, seL4_NumExclusiveBreakpoints * sizeof(hw_break_t));
        memset(inferior->hardware_watchpoints, 0, seL4_NumExclusiveWatchpoints * sizeof(hw_watch_t));

        for (int j = 0; j < MAX_THREADS; j++) {
            if (!inferior->threads[j].enabled) continue;

            gdb_thread_t *thread = &inferior->threads[j];
            if (thread->ss_enabled) {
                disable_single_step(thread);
            }
            thread->ss_enabled = false;
            thread->wakeup = true;
        }
    }

    return;
}

bool gdb_handle_packet(char *input, char *output, bool *detached) {
    output[0] = 0;
    if (*input == 'g') {
        handle_read_regs(output);
    } else if (*input == 'G') {
        handle_write_regs(input, output);
    } else if (*input == 'm') {
        handle_read_mem(input, output);
    } else if (*input == 'M') {
        handle_write_mem(input, output);
    } else if (*input == 'q' || *input == 'Q') {
        handle_query(input, output);
    } else if (*input == 'H') {
        handle_set_inferior(input, output);
    } else if (*input == 'D') {
        handle_detach(input, output);
        *detached = true;
        return true;
    } else if (*input == 'T') {
        handle_check_thread_alive(input, output);
    } else if (*input == '?') {
        /* @alwin: This should probably report reasons other than swbreak, though I've only seen
         * this packet be used when first connecting to the system, in which case only swbreak makes
         * any sense.
         */
        strlcpy(output, "T05swbreak:;", BUFSIZE);
    } else if (*input == 'v') {
        if (strncmp(input, "vCont?", 7) == 0) {
            strlcpy(output, "vCont;c;C;s;S", BUFSIZE);
        } else if (strncmp(input, "vCont;", 6) == 0) {
            /* vCont is a substitute for s and c when doing multiprocess stuff */
            handle_vcont(input, output);
            return true;
        }
    } else if (*input == 'z' || *input == 'Z') {
        handle_configure_debug_events(input, output);
    } else if (*input == 3) {
        /* In case the ctrl-C character was entered */
        handle_sig_interrupt(output);
    }

    return false;
}

static bool handle_ss_hwbreak_swbreak_exception(gdb_thread_t *thread, seL4_Word reason, char *output) {
    strlcpy(output, "T05thread:", BUFSIZE);
    char *ptr = write_thread_id(thread, output + strlen(output), BUFSIZE - strlen(output));
    if (reason == seL4_SoftwareBreakRequest) {
        strlcpy(ptr, ";swbreak:;", BUFSIZE);
    } else {
        strlcpy(ptr, ";hwbreak:;", BUFSIZE);
    }

    /* As we include a thread-id, GDB expects the target inferior to be the thread that we set */
    target_thread = thread;

    return (reason == seL4_SingleStep);
}

static void handle_watchpoint_exception(gdb_thread_t *thread, seL4_Word bp_num, seL4_Word trigger_address, char *output) {

    strlcpy(output, "T05thread:", BUFSIZE);
    char *ptr = write_thread_id(thread, output + strlen(output), BUFSIZE - strlen(output));
    switch (thread->inferior->hardware_watchpoints[bp_num - seL4_FirstWatchpoint].type) {
        case seL4_BreakOnWrite:
            strlcpy(ptr, ";watch:", BUFSIZE);
            break;
        case seL4_BreakOnRead:
            strlcpy(ptr, ";rwatch:", BUFSIZE);
            break;
        case seL4_BreakOnReadWrite:
            strlcpy(ptr, ";awatch:", BUFSIZE);
            break;
        default:
            assert(0);
    }

    seL4_Word vaddr_be = arch_to_big_endian(trigger_address);
    ptr = mem2hex((char *) &vaddr_be, output + strnlen(output, BUFSIZE), sizeof(seL4_Word));
    strlcpy(ptr, ";", BUFSIZE);

    /* As we include a thread-id, GDB expects the target inferior to be the thread that we set */
    target_thread = thread;
}

static bool handle_debug_exception(gdb_thread_t *thread, seL4_Word *reply_mr, char *output) {
    seL4_Word reason = seL4_GetMR(seL4_DebugException_ExceptionReason);
    seL4_Word fault_ip = seL4_GetMR(seL4_DebugException_FaultIP);
    seL4_Word trigger_address = seL4_GetMR(seL4_DebugException_TriggerAddress);
    seL4_Word bp_num = seL4_GetMR(seL4_DebugException_BreakpointNumber);

    switch (reason) {
        case seL4_InstructionBreakpoint:
        case seL4_SingleStep:
        case seL4_SoftwareBreakRequest:
            handle_ss_hwbreak_swbreak_exception(thread, reason, output);
            break;
        case seL4_DataBreakpoint:
            handle_watchpoint_exception(thread, bp_num, trigger_address, output);
            break;
    }

    // @alwin: i think you always want to resume for a debug exception. Think about this more.
    return true;
}

static bool handle_fault(gdb_thread_t *thread, seL4_Word exception_reason, char *output) {
    // @alwin: I'm pretty sure there is no fault here that should reawaken the thread. Think about this more.
    // @alwin: Currentlywe just doing SIGABRT for every kind of fault that happens, this probably could be better?

    strlcpy(output, "T06thread:", BUFSIZE);
    char *ptr = write_thread_id(thread, output + strlen(output), BUFSIZE - strlen(output));
    strlcpy(ptr, ";", BUFSIZE);

    /* As we include a thread-id, GDB expects the target inferior to be the thread that we set */
    target_thread = thread;

    return false;
}

bool gdb_handle_fault(gdb_thread_t *thread, seL4_Word exception_reason, seL4_Word *reply_mr, char *output) {
    if (exception_reason  == seL4_Fault_DebugException) {
        return handle_debug_exception(thread, reply_mr, output);
    }
    return handle_fault(thread, exception_reason, output);
}

void gdb_thread_spawn(gdb_thread_t *thread, char *output) {
    strlcpy(output, "T05clone:", BUFSIZE);
    char *ptr = write_thread_id(thread, output + strlen(output), BUFSIZE - strlen(output));
    strlcpy(ptr, ";thread:", BUFSIZE);
    ptr = write_thread_id(target_thread    , output + strlen(output), BUFSIZE - strlen(output));
    strlcpy(ptr, ";", BUFSIZE);

    return;
}

void gdb_thread_exit(gdb_thread_t *thread, char* output) {
    thread->enabled = false;

    strlcpy(output, "w00;", BUFSIZE);
    char *ptr = write_thread_id(thread, output + strlen(output), BUFSIZE - strlen(output));

    return;
}

