add_library(libco STATIC aarch64.c amd64.c arm.c libco.c  include/libco.h include/settings.h)
target_include_directories(libco PUBLIC include)