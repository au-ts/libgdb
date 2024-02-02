#include <sel4/sel4_arch/types.h>
#include <microkit.h>
#include <gdb.h>
#include <util.h>
#include <libco.h>
#include <stddef.h>



typedef enum event_state {
    eventState_none = 0,
    eventState_waitingForInput
} event_state_t;

typedef enum phase {
    phase_init = 0,
    phase_init_p2,
    phase_standard_event_loop
} phase_t;

cothread_t t_event, t_main;

#define STACK_SIZE 4096
static char t_main_stack[STACK_SIZE];

// @alwinL this is kind of a placeholder
#define UART_CONN 0

/* Input buffer */
static char input[BUFSIZE];

/* Output buffer */
static char output[BUFSIZE];

/* The current event state and phase */
event_state_t state = eventState_none;
phase_t phase = phase_init;

char get_char() {
    assert(state == eventState_none);

    state = eventState_waitingForInput;
    microkit_notify(UART_CONN);
    co_switch(t_event);

    /* Read the character that was returned and return it */

    return 'a';
}

void _putchar(char character) {
    microkit_dbg_putc(character);
}

void put_char(char c) {
    assert(state == eventState_none);

    microkit_notify(UART_CONN);

    /* Maybe a co-switch if we need to wait for something here? idk? */
}


char *get_packet() {
    char c;
    int count;
    /* Checksum and expected checksum */
    unsigned char cksum, xcksum;
    char *buf = input;
    (void) buf;

    while (1) {
        /* Wait for the start character - ignoring all other characters */

        c = get_char();
        while (c != '$') {
            /* Ctrl-C character - should result in an interrupt */
            if (c == 3) {
                buf[0] = c;
                break;
            }
            c = get_char();
        }
        while ((c = get_char()) != '$')
            ;
        retry:
        /* Initialize cksum variables */
        cksum = 0;
        xcksum = -1;
        count = 0;
        (void) xcksum;

        /* Read until we see a # or the buffer is full */
        while (count < BUFSIZE - 1) {
            c = get_char();
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
            c = get_char();
            xcksum = hexchar_to_int(c) << 4;
            c = get_char();
            xcksum += hexchar_to_int(c);

            if (cksum != xcksum) {
                put_char('-');   /* checksum failed */
            } else {
                put_char('+');   /* checksum success, ack*/

                if (buf[2] == ':') {
                    put_char(buf[0]);
                    put_char(buf[1]);

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
        put_char('$');
        for (cksum = 0; *buf; buf++) {
            cksum += *buf;
            put_char(*buf);
        }
        put_char('#');
        put_char(int_to_hexchar(cksum >> 4));
        put_char(int_to_hexchar(cksum % 16));
        char c = get_char();
        if (c == '+') break;
    }
}

static void event_loop();
static void init_phase2();

static void event_loop() {
    cont_type_t ctype = ctype_dont;
    while (ctype == ctype_dont || phase == phase_standard_event_loop) {
        char *input = get_packet();
        ctype = gdb_handle_packet(input, output);
        put_packet(output);
    }

    if (phase == phase_init) {
        init_phase2();
    } else if (phase == phase_init_p2) {
        return;
    }
}

static void init_phase2() {
    /* Register any remaining protection domains */
    phase = phase_init_p2;
    gdb_register_inferior_fork(1, output);
    put_packet(output);

    event_loop(phase_init_p2);

    gdb_register_inferior_exec(1, "pong.elf", BASE_TCB_CAP + 1, BASE_VSPACE_CAP + 1, output);
    put_packet(output);

    /* When you have more PDs, keep doing the above pattern for all of them. Do the below call as the last event_loop() */
    phase = phase_standard_event_loop;
    event_loop();
}

void init() {
    /* Register the first protection domain */
    if (gdb_register_initial(0, "ping.elf", BASE_TCB_CAP, BASE_VSPACE_CAP)) {
        return;
    }

    t_event = co_active();

    t_main = co_derive((void *) t_main_stack, STACK_SIZE, event_loop);

    co_switch(t_main);
}

void fault(microkit_channel ch, microkit_msginfo msginfo) {
    seL4_Word reply_mr = 0;
    int n_reply = gdb_handle_fault(ch, microkit_msginfo_get_label(msginfo), &reply_mr, output);

    co_switch(t_main);

    if (n_reply >= 0) {
        if (n_reply == 1) {
            microkit_mr_set(0, reply_mr);
        }
        microkit_fault_reply(microkit_msginfo_new(0, n_reply));
    }
}

void notified(microkit_channel ch) {
    if (state == eventState_waitingForInput) {
        state = eventState_none;
        co_switch(t_main);
    }
    // @alwin: when the user enters ctrl-c in GDB, the ctrl-c character (ascii 3) is sent through UART. We should capture this and enter the event loop
}