/*
 * Copyright 2024, UNSW
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sel4/sel4_arch/types.h>
#include <microkit.h>
#include <gdb.h>
#include <util.h>
#include <libco.h>
#include <stddef.h>
#include <sddf/serial/util.h>
#include <sddf/serial/queue.h>

typedef enum event_state {
    eventState_none = 0,
    eventState_waitingForInputEventLoop,
    eventState_waitingForInputFault
} event_state_t;

cothread_t t_event, t_main, t_fault;

#define NUM_DEBUGEES 2

#define STACK_SIZE 4096
static char t_main_stack[STACK_SIZE];
static char t_fault_stack[STACK_SIZE];

#define PRINT_CHANNEL 9
#define GETCHAR_CHANNEL 11

/* Input buffer */
static char input[BUFSIZE];

/* Output buffer */
static char output[BUFSIZE];

uintptr_t rx_free;
uintptr_t rx_active;
uintptr_t tx_free;
uintptr_t tx_active;

uintptr_t rx_data;
uintptr_t tx_data;

serial_queue_handle_t rx_queue;
serial_queue_handle_t tx_queue;

/* The current event state and phase */
event_state_t state = eventState_none;

void _putchar(char character) {
    microkit_dbg_putc(character);
}

// @alwin: I think this could be made cleaner with less copying if we just pass in one of these buffers as the output ptr all the time.
void gdb_puts(char *str) {
    uintptr_t buffer = 0;
    unsigned int buffer_len = 0;
    void *cookie = 0;

    int err = serial_dequeue_free(&tx_queue, &buffer, &buffer_len);
    if (err) {
        microkit_dbg_puts("Unable to dq from tx free ring");
        return;
    }

    int print_len = strnlen(str, BUFSIZE) + 1;
    if (print_len > BUFFER_SIZE) {
        microkit_dbg_puts("String too long\n");
        return;
    }

    memcpy((char *) buffer, str, print_len);

    bool is_empty = serial_queue_empty(tx_queue.active);
    err = serial_enqueue_active(&tx_queue, buffer, print_len);
    if (err) {
        microkit_dbg_puts("Unable to enq to tx used ring");
    }

    if (is_empty) {
        microkit_notify(PRINT_CHANNEL);
    }

    return;
}

/* @alwin: surely there is a less disgusting way of doing this */
void gdb_put_char(char c) {
    char tmp[2] = {c, 0};
    gdb_puts(tmp);
}

char gdb_get_char(event_state_t new_state) {
    microkit_notify(GETCHAR_CHANNEL);

    uintptr_t buffer = 0;
    unsigned int buffer_len = 0;
    void *cookie = 0;

    // @alwin: check dis
    state = new_state;

    // Wait for the mxu to tell us some input has come through
    co_switch(t_event);

    int err = serial_dequeue_active(&rx_queue, &buffer, &buffer_len);
    if (err) {
        microkit_dbg_puts("Failed to get buffer in gdb_get_char()\n");
        return -1;
    }

    char c = ((char *) buffer)[0];

    err = serial_enqueue_free(&rx_queue, buffer, buffer_len);
    if (err) {
        microkit_dbg_puts("Failed to put used buffer back into free ring\n");
    }

    return c;
}

char *get_packet(event_state_t new_state) {
    char c;
    int count;
    /* Checksum and expected checksum */
    unsigned char cksum, xcksum;
    char *buf = input;
    (void) buf;

    while (1) {
        /* Wait for the start character - ignoring all other characters */
        c = gdb_get_char(new_state);
        while (c != '$') {
            /* Ctrl-C character - should result in an interrupt */
            if (c == 3) {
                buf[0] = c;
                buf[1] = 0;
                return buf;
            }
            c = gdb_get_char(new_state);
        }
        retry:
        /* Initialize cksum variables */
        cksum = 0;
        xcksum = -1;
        count = 0;
        (void) xcksum;

        /* Read until we see a # or the buffer is full */
        while (count < BUFSIZE - 1) {
            c = gdb_get_char(new_state);
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

        if (c == '#') {
            c = gdb_get_char(new_state);
            xcksum = hexchar_to_int(c) << 4;
            c = gdb_get_char(new_state);
            xcksum += hexchar_to_int(c);

            if (cksum != xcksum) {
                gdb_put_char('-');   /* checksum failed */
            } else {
                gdb_put_char('+');   /* checksum success, ack*/

                if (buf[2] == ':') {
                    gdb_put_char(buf[0]);
                    gdb_put_char(buf[1]);

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
static void put_packet(char *buf, event_state_t new_state)
{
    uint8_t cksum;
    for (;;) {
        gdb_put_char('$');
        for (cksum = 0; *buf; buf++) {
            cksum += *buf;
            gdb_put_char(*buf);
        }
        gdb_put_char('#');
        gdb_put_char(int_to_hexchar(cksum >> 4));
        gdb_put_char(int_to_hexchar(cksum % 16));
        char c = gdb_get_char(new_state);
        if (c == '+') break;
    }
}

static void event_loop();
static void init_phase2();

// Suspend all the child protection domains
void suspend_system() {
    // @alwin: There is no guarantee the debugees have consecutive IDs starting
    // from zero
    for (int i = 0; i < NUM_DEBUGEES; i++) {
        seL4_TCB_Suspend(BASE_TCB_CAP + i);
    }
}

// Resume all the child protection domains
void resume_system() {
    for (int i = 0; i < 64; i++) {
        if (!inferiors[i].enabled) break;
        if (inferiors[i].wakeup) {
            seL4_TCB_Resume(BASE_TCB_CAP + i);
        }
    }
}

static void event_loop() {
    bool resume = false;
    /* The event loop runs perpetually if we are in the standard event loop phase */
    while (true) {
        char *input = get_packet(eventState_waitingForInputEventLoop);
        if (input[0] == 3) {
            /* If we got a ctrl-c packet, we should suspend the whole system */
            suspend_system();
        }
        resume = gdb_handle_packet(input, output);
        put_packet(output, eventState_waitingForInputEventLoop);
        /* If it's a ctype_continue or ctype_sss, we whould resume the system (once we are in the standard event loop)*/
        if (resume) {
            resume_system();
        }
    }
}

void init() {
    /* First, we suspend all the debugeee PDs*/
    suspend_system();

    /* Set up sDDF ring buffers */

    /* Setup rx ring buffers */
    serial_queue_init(&rx_queue, (serial_queue_t *) rx_free, (serial_queue_t *) rx_active, 0, 512, 512);
    /* Add buffers to the rx ring */
    for (int i = 0; i < NUM_ENTRIES - 1; i++) {
        int err = serial_enqueue_free(&rx_queue, rx_data + (i * BUFFER_SIZE), BUFFER_SIZE);

        if (err) {
            microkit_dbg_puts("Failed to setup rx ring buffers\n");
        }
    }

    /* Setup tx ring buffers */
    serial_queue_init(&tx_queue, (serial_queue_t *) tx_free, (serial_queue_t *) tx_active, 0, 512, 512);
    /* Add buffers to the tx ring */
    for (int i = 0; i < NUM_ENTRIES - 1; i++) {
        int err = serial_enqueue_free(&tx_queue, tx_data + (i * BUFFER_SIZE), BUFFER_SIZE);

        if (err) {
            microkit_dbg_puts("Failed to setup tx ring buffers\n");
        }
    }

    for (int i = 0; i < NUM_DEBUGEES; i++) {
        gdb_register_inferior(i, BASE_TCB_CAP + i, BASE_VSPACE_CAP + i);
    }

    /* Make a coroutine for the rest of the initialization */
    t_event = co_active();
    t_main = co_derive((void *) t_main_stack, STACK_SIZE, event_loop);

    co_switch(t_main);
}


void fault_message() {
    put_packet(output, eventState_waitingForInputFault);
    // Go back to waiting for normal input after we send the fault packet to the host
    state = eventState_waitingForInputEventLoop;
    co_switch(t_event);
}

void fault(microkit_channel ch, microkit_msginfo msginfo) {
    seL4_Word reply_mr = 0;

    suspend_system();

    microkit_dbg_puts("Got a fault on channel ");
    puthex64(ch);
    microkit_dbg_puts("\n");

    // @alwin: I'm not entirely convinced there is a point having reply_mr here still
    bool reply = gdb_handle_fault(ch, microkit_msginfo_get_label(msginfo), &reply_mr, output);

    // Start a coroutine for dealing with the fault and transmitting a message to the host
    t_event = co_active();
    t_fault = co_derive((void *) t_fault_stack, STACK_SIZE, fault_message);
    co_switch(t_fault);

    if (reply) {
        microkit_fault_reply(microkit_msginfo_new(0, 0));
    }
}

void notified(microkit_channel ch) {
    if (state == eventState_waitingForInputEventLoop) {
        state = eventState_none;
        co_switch(t_main);
    } else if (state == eventState_waitingForInputFault) {
        state = eventState_none;
        co_switch(t_fault);
    }
}