#include <microkit.h>
#include <stddef.h>

#define PINGPONG_CHANNEL 1

// int a = 1;

#define UART_OFFSET 0x4c0
#define UART_WFIFO  0x0
#define UART_RFIFO  0x4
#define UART_CTRL 0x8
#define UART_STATUS 0xC

#define UART_TX_FULL        (1 << 21)
#define UART_RX_EMPTY       (1 << 20)
#define UART_CONTROL_TX_ENABLE   (1 << 12)

#define REG_PTR(base, offset) ((volatile uint32_t *)((base) + (offset)))

uintptr_t uart_base;

void uart_init() {
    *REG_PTR(uart_base + UART_OFFSET, UART_CTRL) |= UART_CONTROL_TX_ENABLE;
}

int uart_get_char() {
    while ((*REG_PTR(uart_base + UART_OFFSET, UART_STATUS) & UART_RX_EMPTY));

    return *REG_PTR(uart_base + UART_OFFSET, UART_RFIFO);
}

void uart_put_char(int ch) {
    while ((*REG_PTR(uart_base + UART_OFFSET, UART_STATUS) & UART_TX_FULL));

    /* Add character to the buffer. */
    *REG_PTR(uart_base + UART_OFFSET, UART_WFIFO) = ch;
    if (ch == '\r') {
        uart_put_char('\n');
    }
}

void uart_put_str(char *str) {
    while (*str) {
        uart_put_char(*str);
        str++;
    }
    return;
}



void init() {
	int a = 1;
	int *b = NULL;

	// uart_init();
	// seL4_DebugEnterKGDB();
	// uart_put_str("Hi! I'm PING!\n");
	uart_put_str("ping\n");
	// *b = 10;
	a = a + 1;
	// while (true) {
	// 	uart_put_str("ping\n");
	// }

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
		// uart_put_str("Ping!\n");
		microkit_notify(PINGPONG_CHANNEL);
		break;
	}
}
