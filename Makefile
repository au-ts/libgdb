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

ifeq ($(strip $(BUILD_DIR)),)
    $(error libGDB requires a BUILD_DIR)
endif

ifeq ($(strip $(MICROKIT_CONFIG)),)
    $(error libGDB requires a MICROKIT_CONFIG)
endif


AARCH64_FILES := src/arch/arm/64/gdb.c
ARCH_INDEP_FILES := src/gdb.c \
					src/util.c \
					src/printf.c
C_FILES := $(AARCH64_FILES) $(ARCH_INDEP_FILES)

CFLAGS += -I$(MICROKIT_SDK)/board/$(BOARD)/$(MICROKIT_CONFIG)/include \
		  -Iinclude \
		  -Iarch_include

OBJECTS := $(C_FILES:.c=.o)
NAME := $(BUILD_DIR)/libgdb.a

all: $(NAME) clean

$(BUILD_DIR)/libgdb.a: $(OBJECTS)
	ar rv $@ $(OBJECTS)

clean:
	rm -f $(OBJECTS)
