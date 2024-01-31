add_executable(ping.elf ping.c)
target_link_libraries(ping.elf microkit uart)