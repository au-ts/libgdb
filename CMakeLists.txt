add_library(gdb STATIC src/gdb.c src/util.c src/printf.c src/arch/${ARCH}/${MODE}/gdb.c include/gdb.h include/util.h include/printf.h arch_include/arch/${ARCH}/${MODE}/gdb.h)
target_include_directories(gdb
						   PUBLIC include/
						   PRIVATE arch_include/)
target_link_libraries(gdb microkit)