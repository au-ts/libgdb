#include <microkit.h>
#include "../../lib/uart/include/uart.h"


#define PINGPONG_CHANNEL 1

void notified(microkit_channel ch) {
    switch (ch) {
    case PINGPONG_CHANNEL: {
        uart_put_str("Pong!\n");
        microkit_notify(PINGPONG_CHANNEL);
        break;
    }
    }
}

void init() {
	uart_init();
//	uart_put_str("Hi! I'm PONG!\n");
    // microkit_notify(300);
    // arm_sys_null(seL4_SysDebugEnterKGDB);
}
