#
# Copyright 2024, UNSW (ABN 57 195 873 179)
#
# SPDX-License-Identifier: BSD-2-Clause
#

AARCH64_FILES := aarch64.c
AARCH32_FILES := arm.c
X86_FILES := amd64.c
ARCH_INDEP_FILES := libco.c
C_FILES := $(AARCH64_FILES) $(AARCH32_FILES) $(X86_FILES) $(ARCH_INDEP_FILES)

OBJECTS := $(C_FILES:.c=.o)
NAME := $(BUILD_DIR)/libco.a

all: $(NAME) clean

$(BUILD_DIR)/libco.a: $(OBJECTS)
	ar rv $@ $(OBJECTS)

clean:
	rm -f $(OBJECTS)