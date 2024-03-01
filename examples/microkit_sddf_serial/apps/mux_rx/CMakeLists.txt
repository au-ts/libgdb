# @alwin: Referencing UART, which is another app, seems like an antipattern here
add_executable(mux_rx.elf mux_rx.c)

if (BOARD STREQUAL "odroidc2" OR BOARD STREQUAL "odroidc4")
	target_sources(mux_rx.elf PRIVATE ../uart_meson/include/uart.h ../uart_meson/include/uart_config.h)
elseif (BOARD STREQUAL "qemu_arm_virt")
	target_sources(mux_rx.elf PRIVATE ../uart_pl11/include/uart.h ../uart_pl11/include/uart_config.h)
endif()

set_target_properties(mux_rx.elf PROPERTIES COMPILE_FLAGS "-DSERIAL_NUM_CLIENTS=1")
# set_target_properties(mux_rx.elf PROPERTIES COMPILE_FLAGS "-DSERIAL_NUM_CLIENTS=1 -DSERIAL_TRANSFER_WITH_COLOUR")

if (BOARD STREQUAL "odroidc2" OR BOARD STREQUAL "odroidc4")
	target_include_directories(mux_rx.elf PRIVATE ../uart_meson/include/)
elseif (BOARD STREQUAL "qemu_arm_virt")
	target_include_directories(mux_rx.elf PRIVATE ../uart_pl11/include/)
endif()

target_link_libraries(mux_rx.elf microkit sddf)