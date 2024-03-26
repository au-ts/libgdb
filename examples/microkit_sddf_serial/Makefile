#
# Copyright 2023, UNSW
#
# SPDX-License-Identifier: BSD-2-Clause
#

ifeq ($(strip $(MICROKIT_SDK)),)
$(error MICROKIT_SDK must be specified)
endif

ifeq ($(strip $(LIBGCC)),)
LIBGCC := $(shell dirname $$(aarch64-none-elf-gcc --print-file-name libgcc.a))
endif

ifeq ($(strip $(BOARD)),)
    $(error libGDB requires a BOARD)
endif

export MICROKIT_SDK := $(realpath ${MICROKIT_SDK})
export BUILD_DIR ?= $(abspath build)
export BOARD ?= $(BOARD)
export LIBGDB_DIR := $(abspath ../..)
export SDDF := $(abspath ../../sddf)
export EXAMPLE_DIR := $(abspath .)

CC := aarch64-none-elf-gcc
LD := aarch64-none-elf-ld
MICROKIT_TOOL ?= $(MICROKIT_SDK)/bin/microkit

IMAGE_FILE := $(BUILD_DIR)/loader.img
REPORT_FILE := $(BUILD_DIR)/report.txt

all: $(IMAGE_FILE)

${REPORT_FILE} ${IMAGE_FILE}: ${BUILD_DIR}/Makefile example.system FORCE
	${MAKE} -C ${BUILD_DIR}


${BUILD_DIR}/Makefile: sddf_serial_example.mk
	mkdir -p ${BUILD_DIR}
	cp $< ${BUILD_DIR}/Makefile

clean:
	${MAKE} -C ${BUILD_DIR} $@
clobber:
	rm -rf ${BUILD_DIR}

FORCE: ;