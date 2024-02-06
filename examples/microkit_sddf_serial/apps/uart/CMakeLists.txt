add_executable(uart.elf uart.c include/uart.h include/uart_config.h)
target_include_directories(uart.elf PRIVATE include/)
target_link_libraries(uart.elf microkit sddf)