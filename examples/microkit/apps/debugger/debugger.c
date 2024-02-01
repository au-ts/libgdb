#include <uart.h>
#include <sel4/sel4_arch/types.h>
#include <microkit.h>
#include <gdb.h>

typedef enum event_state {
    none = 0,
    waitingForInput
} event_state_t;

typedef enum phase {
    init = 0,
    init_p2,
    standard_event_loop
} phase_t;

/* Input buffer */
static char input[BUFSIZE];

/* Output buffer */
static char output[BUFSIZE];

event_state_t state = none;

char get_char() {
    assert(state == none);

    state = waitingForInput;
    micrkit_notify(UART_CONN);
    co_switch(t_event);

    /* Read the character that was returned and return it */
}

void put_char(char c) {
    assert(state == none);

    microkit_notify(UART_CONN);

    /* Maybe a co-switch if we need to wait for something here? idk? */
}


void get_packet() {
    char c;
    int count;
    /* Checksum and expected checksum */
    unsigned char cksum, xcksum;
    char *buf = input;
    (void) buf;

    while (1) {
        /* Wait for the start character - ignoring all other characters */
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
                uart_put_char('-');   /* checksum failed */
            } else {
                uart_put_char('+');   /* checksum success, ack*/

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
        char c = uart_get_char();
        if (c == '+') break;
    }
}

void event_loop(phase_t phase) {
    cont_type_t ctype = ctype_dont;
    while (ctype == ctype_dont || phase == standard_event_loop) {
        char *input = get_packet();
        ctype = gdb_handle_packet(input, output);
        put_packet(output);
    }

    if (phase == init) {
        init_phase2();
    } else if (phase == init_p2) {
        return;
    }
}

void init() {
	uart_init();

    /* Register the first protection domain */
    if (gdb_register_initial(0, "ping.elf", BASE_TCB_CAP, BASE_VSPACE_CAP)) {
        uart_put_str("Failed to initialize initial inferior");
        return;
    }

    /* This should actually use the co_spawn() thing so that we can get to the event loop and handle the events that let us get packets */
    event_loop(init)
}

void init_phase2() {
    /* Register any remaining protection domains */
    gdb_register_inferior_part1(1, output);
    put_packet(output);

    event_loop(init_p2);

    gdb_register_inferior_part2(1, "pong.elf", BASE_TCB_CAP + 1, BASE_VSPACE_CAP + 1, output);
    put_packet(output)

    /* When you have more PDs, keep doing the above pattern for all of them. Do the below call as the last event_loop() */

    event_loop(standard_event_loop);
}

void fault(microkit_channel ch, microkit_msginfo msginfo) {
    seL4_Word reply_mr = 0;
    int n_reply = gdb_handle_fault(ch, microkit_msginfo_get_label(msginfo), &reply_mr);

    co_switch(t_main);

    if (n_reply >= 0) {
        if (n_reply == 1) {
            microkit_mr_set(0, reply_mr);
        }
        microkit_fault_reply(microkit_msginfo_new(0, n_reply));
    }
}

void notified(microkit_channel ch) {
    if (state == waitingForInput) {
        state = none;
        co_switch(t_main);
    }
    // @alwin: when the user enters ctrl-c in GDB, the ctrl-c character (ascii 3) is sent through UART. We should capture this and enter the event loop
    gdb_event_loop();
}