/*
 * Copyright 2024, UNSW
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <arch/arm/64/gdb.h>
#include <util.h>
#include <sel4/constants.h>
#include <gdb.h>
#include <stddef.h>
#ifndef MICROKIT
#include <assert.h>
#endif /* MICROKIT */


/* Software breakpoint related stuff */
#define AARCH64_BREAK_MON   0xd4200000
#define KGDB_DYN_DBG_BRK_IMM        0x400
#define AARCH64_BREAK_KGDB_DYN_DBG  \
    (AARCH64_BREAK_MON | (KGDB_DYN_DBG_BRK_IMM << 5))

/* Convert registers to a hex string */
// @alwin: This is rather unpleasant, but the way the seL4_UserContext struct is formatted is annoying
char *regs2hex(seL4_UserContext *regs, char *buf)
{
    /* First we handle the 64 bit general purpose registers*/
    buf = mem2hex((char *) &regs->x0, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x1, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x2, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x3, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x4, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x5, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x6, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x7, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x8, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x9, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x10, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x11, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x12, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x13, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x14, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x15, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x16, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x17, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x18, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x19, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x20, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x21, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x22, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x23, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x24, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x25, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x26, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x27, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x28, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x29, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->x30, buf, sizeof(seL4_Word));

    /* Now the stack pointer and the instruction pointer */
    buf = mem2hex((char *) &regs->sp, buf, sizeof(seL4_Word));
    buf = mem2hex((char *) &regs->pc, buf, sizeof(seL4_Word));

    /* Finally the cpsr */
    return mem2hex((char *) &regs->spsr, buf, sizeof(seL4_Word) / 2);
}

/* Convert registers to a hex string */
// @alwin: This is rather unpleasant, but the way the seL4_UserContext struct is formatted is annoying
char *hex2regs(seL4_UserContext *regs, char *buf)
{
    /* First we handle the 64 bit general purpose registers*/
    buf = hex2mem(buf, (char *) &regs->x0, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x1, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x2, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x3, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x4, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x5, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x6, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x7, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x8, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x9, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x10, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x11, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x12, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x13, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x14, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x15, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x16, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x17, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x18, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x19, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x20, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x21, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x22, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x23, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x24, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x25, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x26, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x27, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x28, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x29, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->x30, sizeof(seL4_Word));

    /* Now the stack pointer and the instruction pointer */
    buf = hex2mem(buf, (char *) &regs->sp, sizeof(seL4_Word));
    buf = hex2mem(buf, (char *) &regs->pc, sizeof(seL4_Word));

    /* Finally the cpsr */
    buf = hex2mem(buf, (char *) &regs->spsr, sizeof(seL4_Word) / 2);

    return buf;
}

bool set_software_breakpoint(gdb_inferior_t *inferior, seL4_Word address) {
    sw_break_t tmp;
    tmp.addr = address;

    seL4_ARM_VSpace_Read_Word_t ret = seL4_ARM_VSpace_Read_Word(inferior->vspace, address);
    if (ret.error) {
        return false;
    }
    tmp.orig_word = ret.value;

    /* Overwrite the address with the instruction but preserve everything else */
    ret.value = (seL4_Word) AARCH64_BREAK_KGDB_DYN_DBG | (0xFFFFFFFF00000000 & ret.value);

    if (seL4_ARM_VSpace_Write_Word(inferior->vspace, address, ret.value)) {
        return false;
    }

    int i = 0;
    for (; i < MAX_SW_BREAKS; i++) {
        if (inferior->software_breakpoints[i].addr == 0) {
            inferior->software_breakpoints[i] = tmp;
            return true;
        }
    }

    /* Too many sw breakpoints have been set */
    // @alwin: return value
    seL4_ARM_VSpace_Write_Word(inferior->vspace, address, tmp.orig_word);
    return false;
}

bool unset_software_breakpoint(gdb_inferior_t *inferior, seL4_Word address) {
    for (int i = 0; i < MAX_SW_BREAKS; i++) {
        if (inferior->software_breakpoints[i].addr == address) {
            int err = seL4_ARM_VSpace_Write_Word(inferior->vspace,
                                                 address,
                                                 inferior->software_breakpoints[i].orig_word);
            if (!err) {
                inferior->software_breakpoints[i].addr = 0;
            }

            /* If err == 0, we want to return true (success), else return false (failiure) */
            return !err;
        }
    }

    return true;
}

bool set_hardware_breakpoint(gdb_inferior_t *inferior, seL4_Word address) {
    int i = 0;
    for (; i < seL4_NumExclusiveBreakpoints; i++) {
        if (!inferior->hardware_breakpoints[i].addr) break;
    }

    if (i == seL4_NumExclusiveBreakpoints) return false;

    for (int j = 0; j < MAX_THREADS; j++) {
        if (inferior->threads[j].enabled) {
            seL4_Error err = seL4_TCB_SetBreakpoint(inferior->threads[j].tcb, seL4_FirstBreakpoint + i, address,
                                                    seL4_InstructionBreakpoint, 0, seL4_BreakOnRead);
            if (err) {
                // @alwin: Clean up properly.
                return false;
            }
        }
    }

    inferior->hardware_breakpoints[i].addr = address;
    return true;
}

bool thread_enable_nth_hw_breakpoint(gdb_thread_t *thread, int n) {
    assert(n >= 0 && n < seL4_NumExclusiveBreakpoints);

    if (thread->inferior->hardware_breakpoints[n].addr == 0) return true;
    hw_break_t *bp = &thread->inferior->hardware_breakpoints[n];

    seL4_Error err = seL4_TCB_SetBreakpoint(thread->tcb, seL4_FirstBreakpoint + n, bp->addr,
                                            seL4_InstructionBreakpoint, 0, seL4_BreakOnRead);
    if (err) {
        return false;
    }

    return true;
}

bool unset_hardware_breakpoint(gdb_inferior_t *inferior, seL4_Word address) {
    int i = 0;
    for (; i < seL4_NumExclusiveBreakpoints; i++) {
        if (inferior->hardware_breakpoints[i].addr == address) {
            inferior->hardware_breakpoints[i].addr = 0;
            break;
        }
    }

    if (i == seL4_NumExclusiveBreakpoints) return false;

    for (int j = 0; j < MAX_THREADS; j++) {
        if (inferior->threads[j].enabled) {
            seL4_TCB_UnsetBreakpoint(inferior->threads[j].tcb, seL4_FirstBreakpoint + i);
        }
    }
    return true;
}

bool set_hardware_watchpoint(gdb_inferior_t *inferior, seL4_Word address,
                             seL4_BreakpointAccess type, seL4_Word size) {
    int i = 0;
    for (; i < seL4_NumExclusiveWatchpoints; i++) {
        if (!inferior->hardware_watchpoints[i].addr) break;
    }

    if (i == seL4_NumExclusiveWatchpoints) return false;

    for (int j = 0; j < MAX_THREADS; j++) {
        if (inferior->threads[j].enabled) {
            seL4_Error err = seL4_TCB_SetBreakpoint(inferior->threads[j].tcb, seL4_FirstWatchpoint + i,
                                                    address, seL4_DataBreakpoint, size, type);
            if (err) {
                // @alwin: Clean up properly.
                return false;
            }
        }
    }

    inferior->hardware_watchpoints[i].addr = address;
    inferior->hardware_watchpoints[i].size = size;
    inferior->hardware_watchpoints[i].type = type;


    return true;
}

bool thread_enable_nth_hw_watchpoint(gdb_thread_t *thread, int n) {
    assert(n >= 0 && n < seL4_NumExclusiveWatchpoints);

    if (thread->inferior->hardware_watchpoints[n].addr == 0) return true;
    hw_watch_t *wp = &thread->inferior->hardware_watchpoints[n];

    seL4_Error err = seL4_TCB_SetBreakpoint(thread->tcb, seL4_FirstWatchpoint + n,
                                            wp->addr, seL4_DataBreakpoint, wp->size, wp->type);
    if (err) {
        return false;
    }

    return true;
}

bool unset_hardware_watchpoint(gdb_inferior_t *inferior, seL4_Word address,
                               seL4_BreakpointAccess type, seL4_Word size) {
    int i = 0;
    for (; i < seL4_NumExclusiveWatchpoints; i++) {
        if (inferior->hardware_watchpoints[i].addr == address &&
            inferior->hardware_watchpoints[i].type == type &&
            inferior->hardware_watchpoints[i].size == size) {

            inferior->hardware_watchpoints[i].addr = 0;

            break;
        }
    }

    if (i == seL4_NumExclusiveWatchpoints) return false;


    for (int j = 0; j < MAX_THREADS; j++) {
        if (inferior->threads[j].enabled) {
            seL4_TCB_UnsetBreakpoint(inferior->threads[j].tcb, seL4_FirstWatchpoint + i);
        }
    }

    return true;
}

bool enable_single_step(gdb_thread_t *thread) {
    // @alwin: Remove this check so we can override a thread we already replied to. Is this safe?
    // if (inferior->ss_enabled) {
    //     return false;
    // }

    thread->ss_enabled = true;
    seL4_TCB_ConfigureSingleStepping(thread->tcb, 0, 1);
    return true;
}

bool disable_single_step(gdb_thread_t *thread) {
    // @alwin: Remove this check so we can override a thread we already replied to. Is this safe?
    // if (!inferior->ss_enabled) {
    //     return false;
    // }

    seL4_TCB_ConfigureSingleStepping(thread->tcb, 0, 0);
    return true;
}

 char *inf_mem2hex(gdb_thread_t *thread, seL4_Word mem, char *buf, int size, seL4_Word *error)
{
    int i;
    unsigned char c;

    seL4_Word curr_word = 0;
    for (i = 0; i < size; i++) {
        if (i % sizeof(seL4_Word) == 0) {
            seL4_ARM_VSpace_Read_Word_t ret = seL4_ARM_VSpace_Read_Word(thread->inferior->vspace, mem);
            if (ret.error) {
                *error = ret.error;
                return NULL;
            }

            curr_word = ret.value;
            mem += sizeof(seL4_Word);
        }

        c = *(((char *) &curr_word) + (i % sizeof(seL4_Word)));
        *buf++ = int_to_hexchar(c >> 4);
        *buf++ = int_to_hexchar(c % 16);
    }

    *buf = 0;
    return buf;
}

/*
 * Returns a ptr to the char after last memory byte written
 * or NULL on error (cannot write memory)
 */
seL4_Word inf_hex2mem(gdb_thread_t *thread, char *buf, seL4_Word mem, int size)
{
    int i;
    unsigned char c;

    seL4_Word curr_word = 0;
    for (i = 0; i < size; i++, mem++) {
        if (i % sizeof(seL4_Word) == 0) {
            seL4_ARM_VSpace_Read_Word_t ret = seL4_ARM_VSpace_Read_Word(thread->inferior->vspace, mem);
            if (ret.error) {
                return (mem + i);
            }

            curr_word = ret.value;
        }

        c = hexchar_to_int(*buf++) << 4;
        c += hexchar_to_int(*buf++);
        *(((char *) &curr_word) + (i % sizeof(seL4_Word))) = c;

        if (i % sizeof(seL4_Word) == sizeof(seL4_Word) - 1 || i == size - 1) {
            int err = seL4_ARM_VSpace_Write_Word(thread->inferior->vspace, mem + (i/sizeof(seL4_Word)), curr_word);
            if (err) {
                return (mem + i);
            }
            mem += sizeof(seL4_Word);
        }
    }

    return (mem + i);
}