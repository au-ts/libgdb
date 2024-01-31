add_library(gdb STATIC src/gdb.c src/util.c src/arch/arm/64/gdb.c include/gdb.h include/util.h include/arch/arm/64/gdb.h)
target_include_directories(gdb
						   PUBLIC include/
						   PRIVATE arch_include/)