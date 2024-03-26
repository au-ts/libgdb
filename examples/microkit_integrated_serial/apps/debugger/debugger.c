/*
 * Copyright 2024, UNSW
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <uart.h>
#include <sel4/sel4_arch/types.h>
#include <microkit.h>
#include <stddef.h>
#include <gdb.h>
#include <util.h>

/* Input buffer */
static char input[BUFSIZE];

/* Output buffer */
static char output[BUFSIZE];

#define NUM_DEBUGEES 2

// @alwin: Do we really want this in here? The GDB library relies on printf, but I think that dependency should be removed
void _putchar(char character) {
    microkit_dbg_putc(character);
}

static char *get_packet(void) {
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
static void put_packet(char *buf)
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

// Suspend all the child protection domains
static void suspend_system() {
    // @alwin: There is no guarantee the debugees have consecutive IDs starting
    // from zero
    for (int i = 0; i < NUM_DEBUGEES; i++) {
        seL4_TCB_Suspend(BASE_TCB_CAP + i);
    }
}

// Resume all the child protection domains
static void resume_system() {
    for (int i = 0; i < 64; i++) {
        if (!inferiors[i].enabled) break;
        if (inferiors[i].wakeup) {
            seL4_TCB_Resume(BASE_TCB_CAP + i);
        }
    }
}

void gdb_event_loop() {
    cont_type_t ctype = ctype_dont;
    while (ctype == ctype_dont) {
        char *input = get_packet();
        ctype = gdb_handle_packet(input, output);
        if (ctype != ctype_dont) break;
        put_packet(output);
    }

    resume_system();
}

void init() {
    suspend_system();

	uart_init();

    for (int i = 0; i < NUM_DEBUGEES; i++) {
        gdb_register_inferior(i, BASE_TCB_CAP + i, BASE_VSPACE_CAP + i);
    }

    /* Wait for a connection to be established */
    gdb_event_loop();
}

void fault(microkit_channel ch, microkit_msginfo msginfo) {
    int n_reply = 0;
    // @alwin: i think this parameter no longer really works with the current model
    seL4_Word reply_mr;

    suspend_system();
    bool reply = gdb_handle_fault(ch, microkit_msginfo_get_label(msginfo), &reply_mr, output);
    put_packet(output);
    if (reply) {
        microkit_fault_reply(microkit_msginfo_new(0, 0));
    }

    gdb_event_loop();
}

void notified(microkit_channel ch) {
    // @alwin: when the user enters ctrl-c in GDB, the ctrl-c character (ascii 3) is sent through UART. We should capture this and enter the event loop
    // This would require making the driver interrupt driven.
    gdb_event_loop();
}