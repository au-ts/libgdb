# @alwin: Referencing UART, which is another app, seems like an antipattern here
add_executable(mux_tx.elf mux_tx.c ../uart/include/uart.h ../uart/include/uart_config.h)
# set_target_properties(mux_tx.elf PROPERTIES COMPILE_FLAGS "-DSERIAL_NUM_CLIENTS=1 -DSERIAL_TRANSFER_WITH_COLOUR")
set_target_properties(mux_tx.elf PROPERTIES COMPILE_FLAGS "-DSERIAL_NUM_CLIENTS=1")
target_include_directories(mux_tx.elf PRIVATE ../uart/include/)
target_link_libraries(mux_tx.elf microkit sddf)