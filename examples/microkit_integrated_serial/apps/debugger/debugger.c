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



cont_type_t gdb_event_loop() {
    cont_type_t ctype = ctype_dont;
    while (ctype == ctype_dont) {
        char *input = get_packet();
        ctype = gdb_handle_packet(input, output);
        put_packet(output);
    }

    return ctype;
}


void suspend_system() {
    // @alwin: this should be less hardcoded
    for (int i = 0; i < 2; i++) {
        seL4_TCB_Suspend(BASE_TCB_CAP + i);
    }
}

void resume_system() {
    for (int i = 0; i < 2; i++) {
        seL4_TCB_Resume(BASE_TCB_CAP + i);
    }
}

void init() {
    suspend_system();

	uart_init();

    /* Register the first protection domain */
    if (gdb_register_initial(0, "ping.elf", BASE_TCB_CAP, BASE_VSPACE_CAP)) {
        uart_put_str("Failed to initialize initial inferior");
        return;
    }

    /* Wait for a connection to be established */
    gdb_event_loop();

    /* Register any remaining protection domains */
    gdb_register_inferior_fork(1, output);
    put_packet(output);

    gdb_event_loop();

    gdb_register_inferior_exec(1, "bin/pong.elf", BASE_TCB_CAP + 1, BASE_VSPACE_CAP + 1, output);
    put_packet(output);

    gdb_event_loop();

    resume_system();
}

void fault(microkit_channel ch, microkit_msginfo msginfo) {
    int n_reply = 0;
    // @alwin: i think this parameter no longer really works with the current model
    seL4_Word reply_mr;

    suspend_system();
    bool reply = gdb_handle_fault(ch, microkit_msginfo_get_label(msginfo), &reply_mr, output);
    put_packet(output);

    cont_type_t c = gdb_event_loop();

    if (reply) {
        if (microkit_msginfo_get_label(msginfo) == seL4_Fault_DebugException && seL4_GetMR(seL4_DebugException_ExceptionReason) == seL4_SingleStep) {
            // @alwin: I don't like how this is done
            n_reply = 1;
            if (c == ctype_ss) {
                microkit_mr_set(0, 1);
            } else {
                microkit_mr_set(0, 0);
            }
        }
        microkit_fault_reply(microkit_msginfo_new(0, n_reply));
    }

    // @alwin: Think more deeply about the difference in semantics between continue and single step. At the moment, they both resume
    // the whole system, but I think the more correct behaviour may be to resume the whole system with a continue but only resume the
    // current thread when single stepping.
    resume_system();
}

void notified(microkit_channel ch) {
    // @alwin: when the user enters ctrl-c in GDB, the ctrl-c character (ascii 3) is sent through UART. We should capture this and enter the event loop
    // This would require making the driver interrupt driven.
    gdb_event_loop();
}