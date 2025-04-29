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

AARCH64_OBJ := libgdb/arch/gdb.o
ARCH_INDEP_FILES := $(addprefix libgdb/, gdb.o util.o printf.o)
ALL_OBJS_LIBGDB :=  ${ARCH_INDEP_FILES} ${AARCH64_OBJ}

${ALL_OBJS_LIBGDB}: libgdb

CFLAGS += -I$(MICROKIT_SDK)/board/$(BOARD)/$(MICROKIT_CONFIG)/include \
		  -Iinclude \
		  -Iarch_include

${AARCH64_OBJ}: ${LIBGDB_DIR}/src/arch/arm/64/gdb.c
	${CC} ${CFLAGS} -c -o $@ $<

libgdb/%.o: ${LIBGDB_DIR}/src/%.c
	${CC} ${CFLAGS} -c -o $@ $<

libgdb.a: ${ALL_OBJS_LIBGDB}
	${AR} rvcs $@ $(ALL_OBJS_LIBGDB)

clean::
	rm -f $(ALL_OBJS_LIBGDB)
	rm  ${ALL_OBJS_LIBGDB:.o=.d}
	rmdir libgdb

libgdb:
	mkdir -p $@
	mkdir -p $@/arch

-include ${ALL_OBJS_LIBGDB:.o=.d}