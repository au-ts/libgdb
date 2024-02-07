
#include "include/uart.h"

uintptr_t uart_base_vaddr;

void uart_init() {
    *REG_PTR(uart_base_vaddr + UART_OFFSET, UART_CTRL) |= UART_CONTROL_TX_ENABLE;
}

int uart_get_char() {
    while ((*REG_PTR(uart_base_vaddr + UART_OFFSET, UART_STATUS) & UART_RX_EMPTY));

    return *REG_PTR(uart_base_vaddr + UART_OFFSET, UART_RFIFO);
}

void uart_put_char(int ch) {
    while ((*REG_PTR(uart_base_vaddr + UART_OFFSET, UART_STATUS) & UART_TX_FULL));

    /* Add character to the buffer. */
    *REG_PTR(uart_base_vaddr + UART_OFFSET, UART_WFIFO) = ch;
    if (ch == '\r') {
        uart_put_char('\n');
    }
}

void uart_put_str(char *str) {
    while (*str) {
        uart_put_char(*str);
        str++;
    }
}
