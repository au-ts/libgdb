#include <microkit.h>

#define UART_OFFSET 0x4c0
#define UART_WFIFO  0x0
#define UART_RFIFO  0x4
#define UART_CTRL 0x8
#define UART_STATUS 0xC

#define UART_TX_FULL        (1 << 21)
#define UART_RX_EMPTY       (1 << 20)
#define UART_CONTROL_TX_ENABLE   (1 << 12)

#define REG_PTR(base, offset) ((volatile uint32_t *)((base) + (offset)))

void uart_init();
void uart_put_char(int ch);
void uart_put_str(char *str);
int uart_get_char();