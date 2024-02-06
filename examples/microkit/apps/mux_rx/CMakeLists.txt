# @alwin: Referencing UART, which is another app, seems like an antipattern here
add_executable(mux_rx.elf mux_rx.c ../uart/include/uart.h ../uart/include/uart_config.h)
set_target_properties(mux_rx.elf PROPERTIES COMPILE_FLAGS "-DSERIAL_NUM_CLIENTS=1")
# set_target_properties(mux_rx.elf PROPERTIES COMPILE_FLAGS "-DSERIAL_NUM_CLIENTS=1 -DSERIAL_TRANSFER_WITH_COLOUR")
target_include_directories(mux_rx.elf PRIVATE ../uart/include/)
target_link_libraries(mux_rx.elf microkit sddf)