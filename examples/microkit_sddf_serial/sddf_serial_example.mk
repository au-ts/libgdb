#
# Copyright 2023, UNSW
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

ifeq ("$(BOARD)", "odroidc2")
    CPU := cortex-a53
    ARCH := arm
    MODE := 64
    SERIAL_DRIVER := meson
    SERIAL_DRIVER_MAKEFILE := uart_driver.mk
    SERIAL_DRIVER_ELF := uart_driver.elf
else ifeq ("$(BOARD)", "odroidc4")
    CPU := cortex-a55
    ARCH := arm
    MODE := 64
    SERIAL_DRIVER := meson
    SERIAL_DRIVER_MAKEFILE := uart_driver.mk
    SERIAL_DRIVER_ELF := uart_driver.elf
else ifeq ("$(BOARD)", "qemu_virt_aarch64")
    CPU := cortex-a53
    ARCH := arm
    MODE := 64
    SERIAL_DRIVER := virtio
    SERIAL_DRIVER_MAKEFILE := serial_driver.mk
    SERIAL_DRIVER_ELF := serial_driver.elf
else
    $(error Unsupported BOARD ($(BOARD)))
endif

MICROKIT_CONFIG := debug

CC := aarch64-none-elf-gcc
LD := aarch64-none-elf-ld
AR := aarch64-none-elf-ar
RANLIB := aarch64-none-elf-ranlib

MICROKIT_TOOL := $(MICROKIT_SDK)/bin/microkit
BOARD_DIR := $(MICROKIT_SDK)/board/$(BOARD)/$(MICROKIT_CONFIG)

IMAGES := debugger.elf ping.elf pong.elf $(SERIAL_DRIVER_ELF) serial_virt_rx.elf serial_virt_tx.elf
CFLAGS += \
    -mtune=$(CPU) \
    -mstrict-align \
    -ffreestanding \
    -DMICROKIT \
    -nostdlib \
    -g3 \
    -Wall \
    -Werror \
    -Wno-array-bounds \
    -Wno-unused-variable \
    -Wno-unused-function \
    -Wno-unused-value \
    -Wno-parentheses \
    -DBOARD_${BOARD} \
    -I$(BOARD_DIR)/include \
    -I$(SDDF)/include \
    -I$(SDDF)/libco \
    -I$(LIBGDB_DIR)/include \
    -I$(LIBGDB_DIR)/arch_include/ \
    -I$(LIBGDB_DIR)/lib/libco/include \
    -I../include

LDFLAGS := -L$(BOARD_DIR)/lib
LIBS := -lmicrokit -Tmicrokit.ld

IMAGE_FILE := $(BUILD_DIR)/loader.img
REPORT_FILE := $(BUILD_DIR)/report.txt

# Required by the serial virtualisers
SERIAL_NUM_CLIENTS := -DSERIAL_NUM_CLIENTS=1

all: $(IMAGE_FILE)

# Make the debugger PD
debugger.o: $(EXAMPLE_DIR)/apps/debugger/debugger.c
	$(CC) -c $(CFLAGS) $^ -o $@

debugger.elf: debugger.o libgdb.a libco.a libsddf_util.a
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

# Make the ping PD
ping.o: $(EXAMPLE_DIR)/apps/ping/ping.c
	$(CC) -c $(CFLAGS) $^ -o $@

ping.elf : ping.o libsddf_util_debug.a
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

# Make the pong PD

pong.o: $(EXAMPLE_DIR)/apps/pong/pong.c
	$(CC) -c $(CFLAGS) $^ -o $@

pong.elf : pong.o libsddf_util_debug.a
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

example.system: ${EXAMPLE_DIR}/example.system
	cp ${EXAMPLE_DIR}/example.system example.system

$(IMAGE_FILE) $(REPORT_FILE): $(IMAGES) example.system
	$(MICROKIT_TOOL) example.system --search-path $(BUILD_DIR) --board $(BOARD) --config $(MICROKIT_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)

FORCE:

clean::
	rm -f debugger.o ping.o pong.o uart_driver.o serial_virt_tx.o serial_virt_rx.o

include $(LIBGDB_DIR)/libgdb.mk
include $(SDDF)/libco/libco.mk
include $(SDDF)/util/util.mk
include $(SDDF)/drivers/serial/$(SERIAL_DRIVER)/$(SERIAL_DRIVER_MAKEFILE)
include $(SDDF)/serial/components/serial_components.mk
include $(SDDF)/util/util.mk
