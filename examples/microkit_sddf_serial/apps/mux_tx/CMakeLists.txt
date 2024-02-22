# @alwin: Referencing UART, which is another app, seems like an antipattern here
add_executable(mux_tx.elf mux_tx.c)

if (BOARD STREQUAL "odroidc2" OR BOARD STREQUAL "odroidc4")
	target_sources(mux_tx.elf PRIVATE ../uart_meson/include/uart.h ../uart_meson/include/uart_config.h)
elseif (BOARD STREQUAL "qemu-arm-virt")
	target_sources(mux_tx.elf PRIVATE ../uart_pl11/include/uart.h ../uart_pl11/include/uart_config.h)
endif()

# set_target_properties(mux_tx.elf PROPERTIES COMPILE_FLAGS "-DSERIAL_NUM_CLIENTS=1 -DSERIAL_TRANSFER_WITH_COLOUR")
set_target_properties(mux_tx.elf PROPERTIES COMPILE_FLAGS "-DSERIAL_NUM_CLIENTS=1")

if (BOARD STREQUAL "odroidc2" OR BOARD STREQUAL "odroidc4")
	target_include_directories(mux_tx.elf PRIVATE ../uart_meson/include/)
elseif (BOARD STREQUAL "qemu-arm-virt")
	target_include_directories(mux_tx.elf PRIVATE ../uart_pl11/include/)
endif()

target_link_libraries(mux_tx.elf microkit sddf)