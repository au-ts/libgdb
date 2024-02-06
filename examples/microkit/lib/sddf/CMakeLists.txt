add_library(sddf STATIC src/serialsharedringbuffer/shared_ringbuffer.c
						include/serial/shared_ringbuffer.h
						include/util/fence.h
						)
target_include_directories(sddf PUBLIC include)