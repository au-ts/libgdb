add_executable(debugger.elf debugger.c)
target_link_libraries(debugger.elf microkit uart gdb)