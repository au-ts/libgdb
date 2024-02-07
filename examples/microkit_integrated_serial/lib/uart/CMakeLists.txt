add_library(uart STATIC uart.c include/uart.h)
target_include_directories(uart PUBLIC include)
target_link_libraries(uart microkit)