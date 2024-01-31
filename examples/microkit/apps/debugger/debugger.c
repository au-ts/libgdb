#include <uart.h>
#include <sel4/sel4_arch/types.h>
#include <microkit.h>
#include <gdb.h>

void init() {
	uart_init();

    /* Register the first protection domain */
    if (gdb_register_initial(0, "ping.elf", BASE_TCB_CAP, BASE_VSPACE_CAP)) {
        uart_put_str("Failed to initialize initial inferior");
        return;
    }

    /* Wait for a connection to be established */
    gdb_event_loop();

    /* Register any remaining protection domains */
    if (gdb_register_inferior(1, "pong.elf", BASE_TCB_CAP + 1, BASE_VSPACE_CAP + 1)) {
        uart_put_str("Failed to initialize other inferior");
        return;
    }
}

void fault(microkit_channel ch, microkit_msginfo msginfo) {
    seL4_Word reply_mr = 0;
    int n_reply = gdb_handle_fault(ch, microkit_msginfo_get_label(msginfo), &reply_mr);
    if (n_reply >= 0) {
        if (n_reply == 1) {
            microkit_mr_set(0, reply_mr);
        }
        microkit_fault_reply(microkit_msginfo_new(0, n_reply));
    }
}

void notified(microkit_channel ch) {
    gdb_event_loop();
}