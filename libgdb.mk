#
# Copyright 2024, UNSW (ABN 57 195 873 179)
#
# SPDX-License-Identifier: BSD-2-Clause
#

ifeq ($(strip $(MICROKIT_SDK)),)
    $(error libGDB requires a MICROKIT_SDK)
endif

ifeq ($(strip $(BOARD)),)
    $(error libGDB requires a BOARD)
endif

ifeq ($(strip $(MICROKIT_CONFIG)),)
    $(error libGDB requires a MICROKIT_CONFIG)
endif


AARCH64_FILES := $(LIBGDB_DIR)/src/arch/arm/64/gdb.c
ARCH_INDEP_FILES := $(addprefix $(LIBGDB_DIR)/src/, gdb.c util.c printf.c)
C_FILES := $(AARCH64_FILES) $(ARCH_INDEP_FILES)

CFLAGS += -I$(MICROKIT_SDK)/board/$(BOARD)/$(MICROKIT_CONFIG)/include \
		  -Iinclude \
		  -Iarch_include

LIBGDB_OBJECTS := $(C_FILES:.c=.o)

all: libgdb.a clean

libgdb.a: $(LIBGDB_OBJECTS)
	${AR} rvcs $@ $(LIBGDB_OBJECTS)

clean:
	rm -f $(LIBGDB_OBJECTS)
