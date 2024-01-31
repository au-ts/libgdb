//
// Created by Alwin Joshy on 23/1/2024.
//

#include <gdb.h>
#include <arch/arm/64/gdb.h>
#include <util.h>
#include <sel4/constants.h>

//#define DEBUG_PRINTS 1
#define BUFSIZE 1024
#define MAX_PDS 64
#define MAX_ID 255
#define INITIAL_INFERIOR_POS 0

/* Input buffer */
static char input[BUFSIZE];

/* Output buffer */
static char output[BUFSIZE];

int num_threads = 0;
inferior_t inferiors[MAX_PDS];
inferior_t *target_inferior = NULL;

static char *gdb_get_packet(void) {
    char c;
    int count;
    /* Checksum and expected checksum */
    unsigned char cksum, xcksum;
    char *buf = input;
    (void) buf;

    while (1) {
        /* Wait for the start character - ignoring all other characters */
        while ((c = uart_get_char()) != '$')
#ifndef DEBUG_PRINTS
            ;
#else
        {
            uart_put_char(c);
        }
        uart_put_char(c);
#endif
        retry:
        /* Initialize cksum variables */
        cksum = 0;
        xcksum = -1;
        count = 0;
        (void) xcksum;

        /* Read until we see a # or the buffer is full */
        while (count < BUFSIZE - 1) {
            c = uart_get_char();
#ifdef DEBUG_PRINTS
            uart_put_char(c);
#endif
            if (c == '$') {
                goto retry;
            } else if (c == '#') {
                break;
            }
            cksum += c;
            buf[count++] = c;
        }

        /* Null terminate the string */
        buf[count] = 0;

#ifdef DEBUG_PRINTS
        printf("\nThe value of the command so far is %s. The checksum you should enter is %x\n", buf, cksum);
#endif

        if (c == '#') {
            c = uart_get_char();
            xcksum = hexchar_to_int(c) << 4;
            c = uart_get_char();
            xcksum += hexchar_to_int(c);

            if (cksum != xcksum) {
                uart_put_char('-');   /* checksum failed */
            } else {
                uart_put_char('+');   /* checksum success, ack*/

                if (buf[2] == ':') {
                    uart_put_char(buf[0]);
                    uart_put_char(buf[1]);

                    return &buf[3];
                }

                return buf;
            }
        }
    }

    return NULL;
}

/*
 * Send a packet, computing it's checksum, waiting for it's acknoledge.
 * If there is not ack, packet will be resent.
 */
static void gdb_put_packet(char *buf)
{
    uint8_t cksum;
    for (;;) {
        uart_put_char('$');
        for (cksum = 0; *buf; buf++) {
            cksum += *buf;
            uart_put_char(*buf);
        }
        uart_put_char('#');
        uart_put_char(int_to_hexchar(cksum >> 4));
        uart_put_char(int_to_hexchar(cksum % 16));
        char c = uart_get_char();
        if (c == '+') break;
    }
}

/* Read registers */
static void handle_read_regs(void) {
    seL4_UserContext context;
    int error = seL4_TCB_ReadRegisters(target_inferior->tcb, false, 0,
                                       sizeof(seL4_UserContext) / sizeof(seL4_Word), &context);
    regs2hex(&context, output);
}

/* Write registers */
static void handle_write_regs(char *ptr) {
    assert(*ptr++ == 'G');

    seL4_UserContext context;
    hex2regs(&context, ptr);
    int error = seL4_TCB_WriteRegisters(target_inferior->tcb, false, 0,
                                        sizeof(seL4_UserContext) / sizeof(seL4_Word), &context);
    strlcpy(output, "OK", sizeof(output));
}

static void handle_query(char *ptr) {
    if (strncmp(ptr, "qSupported", 10) == 0) {
        /* TODO: This may eventually support more features */
        snprintf(output, sizeof(output),
                 "qSupported:PacketSize=%lx;QThreadEvents+;swbreak+;hwbreak+;vContSupported+;fork-events+;exec-events+;multiprocess+;", sizeof(input));
    } else if (strncmp(ptr, "qfThreadInfo", 12) == 0) {
        char *out_ptr = output;
        *out_ptr++ = 'm';
        for (uint8_t i = 0; i < 64; i++) {
            if (inferiors[i].tcb != 0) {
                if (i != 0) {
                    *out_ptr++ = ',';
                }
                *out_ptr++ = 'p';
                out_ptr = mem2hex((char *) &inferiors[i].gdb_id, out_ptr, sizeof(uint8_t));
                strlcpy(out_ptr, ".1", 3);
                /* @alwin: this is stupid, be better */
                out_ptr += 2;
            } else {
                break;
            }
        }
    } else if (strncmp(ptr, "qsThreadInfo", 12) == 0) {
        strlcpy(output, "l", sizeof(output));
    } else if (strncmp(ptr, "qC", 2) == 0) {
        strlcpy(output, "QCp1.1", sizeof(output));
    } else if (strncmp(ptr, "qSymbol", 7) == 0) {
        strlcpy(output, "OK", sizeof(output));
    } else if (strncmp(ptr, "qTStatus", 8) == 0) {
        /* TODO: THis should eventually work in the non startup case */
        strlcpy(output, "T0", sizeof(output));
    } else if (strncmp(ptr, "qAttached", 9) == 0) {
        strlcpy(output, "1", sizeof(output));
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

    /* @alwin: whats this about again? */
    ptr = hexstr_to_int(ptr, sizeof(seL4_Word) * 2, kind);
    if (*kind != 4) {
        return false;
    }

    return true;
}


static void handle_configure_debug_events(char *ptr) {
    /* Precondition: ptr[0] is always 'z' or 'Z' */
    seL4_Word addr, size;
    bool success = false;

    if (!parse_breakpoint_format(ptr, &addr, &size)) {
        strlcpy(output, "E01", sizeof(output));
        return;
    }

    /* Breakpoints and watchpoints */

    if (strncmp(ptr, "Z0", 2) == 0) {
        /* Set a software breakpoint using binary rewriting */
        success = set_software_breakpoint(target_inferior, addr);
    } else if (strncmp(ptr, "z0", 2) == 0) {
        /* Unset a software breakpoint */
        success = unset_software_breakpoint(target_inferior, addr);
    } else if (strncmp(ptr, "Z1", 2) == 0) {
        /* Set a hardware breakpoint */
        success = set_hardware_breakpoint(target_inferior, addr);
    } else if (strncmp(ptr, "z1", 2) == 0) {
        /* Unset a hardware breakpoint */
        success = unset_hardware_breakpoint(target_inferior, addr);
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
                strlcpy(output, "E01", sizeof(output));
                return;
        }

        if (ptr[0] == 'Z') {
            success = set_hardware_watchpoint(target_inferior, addr, watchpoint_type);
        } else {
            success = unset_hardware_watchpoint(target_inferior, addr, watchpoint_type);
        }
    }

    if (!success) {
        strlcpy(output, "E01", sizeof(output));
    } else {
        strlcpy(output, "OK", sizeof(output));
    }
}

int gdb_register_initial(uint8_t id, char* elf_name, seL4_CPtr tcb, seL4_CPtr vspace) {
    /* If this isn't the first thread that was initialized */
    if (num_threads != 0 || inferiors[INITIAL_INFERIOR_POS].tcb != 0) {
        return -1;
    }

    /* If the provided id is greater than expected */
    // @alwin: is this right?
    if (id > MAX_ID) {
        return -1;
    }

    inferiors[INITIAL_INFERIOR_POS].id = id;
    inferiors[INITIAL_INFERIOR_POS].gdb_id = 1;
    inferiors[INITIAL_INFERIOR_POS].tcb = tcb;
    inferiors[INITIAL_INFERIOR_POS].vspace = vspace;
    strlcpy(inferiors[INITIAL_INFERIOR_POS].elf_name, elf_name, MAX_ELF_NAME);
    target_inferior = &inferiors[INITIAL_INFERIOR_POS];
    num_threads = 1;
    return 0;
}

int gdb_register_inferior(uint8_t id, char *elf_name, seL4_CPtr tcb, seL4_CPtr vspace) {
    /* Must already have one thread that has been registered */
    if (num_threads < 1 || inferiors[INITIAL_INFERIOR_POS].tcb == 0) {
        return -1;
    }

    /* We have too many PDs */
    if (num_threads >= MAX_PDS) {
        return -1;
    }

    uint8_t idx = num_threads++;
    inferiors[idx].id = id;
    inferiors[idx].gdb_id = idx + 1;
    strlcpy(inferiors[idx].elf_name, elf_name, MAX_ELF_NAME);

    /* Indicate that the initial thread has forked */
    inferiors[idx].tcb = inferiors[INITIAL_INFERIOR_POS].tcb;
    inferiors[idx].vspace = inferiors[INITIAL_INFERIOR_POS].vspace;
    strlcpy(output, "T05fork:p", sizeof(output));
    char *buf = mem2hex((char *) &inferiors[idx].gdb_id, output + strnlen(output, BUFSIZE), sizeof(uint8_t));
    strlcpy(buf, ".1;", BUFSIZE - strnlen(output, BUFSIZE));
    gdb_put_packet(output);
    gdb_event_loop();

    /* Indicate that the new thread is execing something */
    strlcpy(output, "T05exec:", BUFSIZE);
    buf = mem2hex(elf_name, output + strnlen(output, BUFSIZE), strnlen(elf_name, BUFSIZE - strnlen(output, BUFSIZE)));
    strlcpy(buf, ";", BUFSIZE - strnlen(output, BUFSIZE));
    inferiors[idx].tcb = tcb;
    inferiors[idx].vspace = vspace;
    gdb_put_packet(output);
    gdb_event_loop();

    return 0;
}

static void handle_read_mem(char *ptr) {
    seL4_Word addr, size, error;

    if (!parse_mem_format(ptr, &addr, &size)) {
        /* Error parsing input */
        strlcpy(output, "E01", sizeof(output));
    } else if (size * 2 > sizeof(output) - 1) {
        /* Buffer too small? Don't really get this */
        strlcpy(output, "E01", sizeof(output));
    } else {
        if (inf_mem2hex(target_inferior, addr, output, size, &error) == NULL) {
            /* Failed to read the memory at the location */
           strlcpy(output, "E04", sizeof(output));
        }
    }
}

static void handle_write_mem(char *ptr) {
    seL4_Word addr, size;

    if (!parse_mem_format(ptr, &addr, &size)) {
        strlcpy(output, "E02", sizeof(output));
    } else {
        if ((ptr = memchr(input, ':', BUFSIZE))) {
            ptr++;
            if (inf_hex2mem(target_inferior, ptr, addr, size) == 0) {
                strlcpy(output, "E03", sizeof(output));
            } else {
                strlcpy(output, "OK", sizeof(output));
            }
        }
    }
}

static int parse_thread_id(char *ptr, int *proc_id) {
    int size = 0;
    if (*ptr++ != 'p') {
        return -1;
    }

    char *ptr_tmp = ptr;
    if (strncmp(ptr, "-1", 3) == 0) {
        *proc_id = -1;
        return 0;
    }

    while (*ptr_tmp++ != '.') {
        size++;
    }

    /* 2 hex chars per byte */
    size /= 2;

    if (size == 0) {
        // @alwin: Double check this
        *proc_id = hexchar_to_int(*ptr);
    } else {
        hex2mem(ptr, (char *) proc_id, size);
    }

    ptr = ptr_tmp;
    if (*ptr != '0' && *ptr != '1') {
        return -1;
    }

    return 0;
}

static void handle_set_inferior(char *ptr) {
    int proc_id = 0;

    assert(*ptr++ = 'H');

    if (*ptr != 'g' && *ptr != 'c') {
        strlcpy(output, "E01", sizeof(output));
        return;
    }
    ptr++;

    if (parse_thread_id(ptr, &proc_id) == -1) {
        strlcpy(output, "E01", sizeof(output));
        return;
    }

    if (proc_id != -1 && proc_id != 0) {
        // @alwin: this is not a good assertion
        assert(inferiors[proc_id - 1].gdb_id == proc_id);
        target_inferior = &inferiors[proc_id - 1];
    }

    /* @alwin : figure out what to do when proc_id is 0 */

    if (target_inferior->tcb == 0) {
        /* @alwin: todo, figure this out */
        (void) proc_id;
    }

    strlcpy(output, "OK", sizeof(output));
}


cont_type_t gdb_event_loop() {
    char *ptr;
    int stepping;

    while (1) {
        ptr = gdb_get_packet();
        output[0] = 0;
        if (*ptr == 'g') {
            handle_read_regs();
        } else if (*ptr == 'G') {
            handle_write_regs(ptr);
        } else if (*ptr == 'm') {
            handle_read_mem(ptr);
        } else if (*ptr == 'M') {
            handle_write_mem(ptr);
        } else if (*ptr == 'c' || *ptr == 's') {
            stepping = *ptr == 's' ? 1 : 0;
            ptr++;

            if (stepping) {
                enable_single_step(target_inferior);
            } else {
                disable_single_step(target_inferior);
            }
            /* TODO: Support continue from an address and single step */

            break;
        } else if (*ptr == 'q') {
            handle_query(ptr);
        } else if (*ptr == 'H') {
            handle_set_inferior(ptr);
        } else if (*ptr == '?') {
            /* TODO: This should eventually report more reasons than swbreak */
            strlcpy(output, "T05swbreak:;", sizeof(output));
        } else if (*ptr == 'v') {
            if (strncmp(ptr, "vCont?", 7) == 0) {
                strlcpy(output, "vCont;c", sizeof(output));
            } else if (strncmp(ptr, "vCont;c", 7) == 0) {
                break;
            }
        } else if (*ptr == 'z' || *ptr == 'Z') {
            handle_configure_debug_events(ptr);
        }

        gdb_put_packet(output);
    }

    return stepping;
}

static bool handle_ss_hwbreak_swbreak_exception(uint8_t id, seL4_Word reason) {
    strlcpy(output, "T05thread:p", sizeof(output));

    // @alwin: is this really necessary?
    uint8_t i = 0;
    for (i = 0; i < MAX_PDS; i++) {
        if (inferiors[i].id == id) break;
    }

    assert(i != MAX_PDS);
    /* @alwin: This is ugly, fix it */
    char *ptr = mem2hex((char *) &inferiors[i].gdb_id, output + strnlen(output, sizeof(output)), sizeof(uint8_t));
    if (reason == seL4_SoftwareBreakRequest) {
        strlcpy(ptr, ".1;swbreak:;", sizeof(output));
    } else {
        strlcpy(ptr, ".1;hwbreak:;", sizeof(output));
    }

    gdb_put_packet(output);

    return (reason == seL4_SingleStep);
}

static void handle_watchpoint_exception(uint8_t id, seL4_Word bp_num, seL4_Word trigger_address) {
    strlcpy(output, "T05thread:p", sizeof(output));

    // @alwin: is this really necessary?
    uint8_t i = 0;
    for (i = 0; i < MAX_PDS; i++) {
        if (inferiors[i].id == id) break;
    }

    assert(i != MAX_PDS);
    char *ptr = mem2hex((char *) &inferiors[i].gdb_id, output + strnlen(output, sizeof(output)), sizeof(uint8_t));
    switch (inferiors[i].hardware_watchpoints[bp_num - seL4_FirstWatchpoint].type) {
        case seL4_BreakOnWrite:
            strlcpy(ptr, ".1;watch:", sizeof(output));
            break;
        case seL4_BreakOnRead:
            strlcpy(ptr, ".1;rwatch:", sizeof(output));
            break;
        case seL4_BreakOnReadWrite:
            strlcpy(ptr, ".1;awatch:", sizeof(output));
            break;
        default:
            assert(0);
    }

    seL4_Word vaddr_be = arch_to_big_endian(trigger_address);
    ptr = mem2hex((char *) &vaddr_be, output + strnlen(output, sizeof(output)), sizeof(seL4_Word));
    strlcpy(ptr, ";", sizeof(output));
    gdb_put_packet(output);
}

static bool handle_debug_exception(uint8_t id, seL4_Word *reply_mr) {
    seL4_Word reason = seL4_GetMR(seL4_DebugException_ExceptionReason);
    seL4_Word fault_ip = seL4_GetMR(seL4_DebugException_FaultIP);
    seL4_Word trigger_address = seL4_GetMR(seL4_DebugException_TriggerAddress);
    seL4_Word bp_num = seL4_GetMR(seL4_DebugException_BreakpointNumber);

    bool single_step_reply = false;
    switch (reason) {
        case seL4_InstructionBreakpoint:
        case seL4_SingleStep:
        case seL4_SoftwareBreakRequest:
            single_step_reply = handle_ss_hwbreak_swbreak_exception(id, reason);
            break;
        case seL4_DataBreakpoint:
            handle_watchpoint_exception(id, bp_num, trigger_address);
            break;
    }

    cont_type_t cont_type = gdb_event_loop();

    if (single_step_reply) {
        if (cont_type == ctype_ss) {
            // @alwin: multi-instruction stepping?
            *reply_mr = 1;
        } else {
            *reply_mr = 0;
        }

        return 1;
    }

    return 0;
}

static void handle_fault(uint8_t id, seL4_Word exception_reason) {
    // #@alwin: we should probably notify gdb that a fault occured so they can debug the thread
}

int gdb_handle_fault(uint8_t id, seL4_Word exception_reason, seL4_Word *reply_mr) {
    if (exception_reason  == seL4_Fault_DebugException) {
        return handle_debug_exception(id, reply_mr);
    } else {
        handle_fault(id, exception_reason);
    }

    return 0;
}