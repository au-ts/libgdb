#include <microkit.h>
#include "../../lib/uart/include/uart.h"

#define PINGPONG_CHANNEL 1

// int a = 1;

void init() {
	int a = 1;
	uart_init();
	// seL4_DebugEnterKGDB();
	// uart_put_str("Hi! I'm PING!\n");
	a = a + 1;
	// int i = 0;
	// while (true) {
		// i++;
	// }
    // arm_sys_null(seL4_SysDebugEnterKGDB);
//	uart_put_str("Ping!\n");
	microkit_notify(PINGPONG_CHANNEL);
}

void notified(microkit_channel ch) {
	switch (ch) {
	case PINGPONG_CHANNEL:
		uart_put_str("Ping!\n");
		microkit_notify(PINGPONG_CHANNEL);
		break;
	}
}
