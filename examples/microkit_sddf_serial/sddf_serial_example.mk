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
else ifeq ("$(BOARD)", "odroidc4")
	CPU := cortex-a55
	ARCH := arm
	MODE := 64
	SERIAL_DRIVER := meson
else ifeq ("$(BOARD)", "qemu_virt_aarch64")
	CPU := cortex-a53
	ARCH := arm
	MODE := 64
   	SERIAL_DRIVER := virtio
else
	$(error Unsupported BOARD ($(BOARD)))
endif

MICROKIT_CONFIG := debug

CC := aarch64-none-elf-gcc
LD := aarch64-none-elf-ld
AR := aarch64-none-elf-ar
OBJCOPY := aarch64-none-elf-objcopy
RANLIB := aarch64-none-elf-ranlib
PYTHON ?= python3

MICROKIT_TOOL := $(MICROKIT_SDK)/bin/microkit
BOARD_DIR := $(MICROKIT_SDK)/board/$(BOARD)/$(MICROKIT_CONFIG)

IMAGES := debugger.elf ping.elf pong.elf serial_driver.elf serial_virt_rx.elf serial_virt_tx.elf
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

IMAGE_FILE := loader.img
REPORT_FILE := report.txt
SYSTEM_FILE := serial.system
METAPROGRAM := $(EXAMPLE_DIR)/meta.py
DTB := $(BOARD).dtb
DTS := $(SDDF)/dts/$(BOARD).dts
SYSTEM_FILE := example.system


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

pong.elf: pong.o libsddf_util_debug.a
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(DTB): $(DTS)
	dtc -q -I dts -O dtb $(DTS) > $(DTB)

$(SYSTEM_FILE): $(METAPROGRAM) $(IMAGES) $(DTB)
	$(PYTHON) $(METAPROGRAM) --sddf $(SDDF) --board $(BOARD) --dtb $(DTB) --output . --sdf $(SYSTEM_FILE)
	$(OBJCOPY) --update-section .device_resources=serial_driver_device_resources.data serial_driver.elf
	$(OBJCOPY) --update-section .serial_driver_config=serial_driver_config.data serial_driver.elf
	$(OBJCOPY) --update-section .serial_virt_rx_config=serial_virt_rx.data serial_virt_rx.elf
	$(OBJCOPY) --update-section .serial_virt_tx_config=serial_virt_tx.data serial_virt_tx.elf
	$(OBJCOPY) --update-section .serial_client_config=serial_client_debugger.data debugger.elf


$(IMAGE_FILE) $(REPORT_FILE): $(IMAGES) example.system
	$(MICROKIT_TOOL) $(SYSTEM_FILE) --search-path $(BUILD_DIR) --board $(BOARD) --config $(MICROKIT_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)

FORCE:

clean::
	rm -f debugger.o ping.o pong.o serial_driver.o serial_virt_tx.o serial_virt_rx.o

include $(LIBGDB_DIR)/libgdb.mk
include $(SDDF)/libco/libco.mk
include $(SDDF)/util/util.mk
include $(SDDF)/drivers/serial/$(SERIAL_DRIVER)/serial_driver.mk
include $(SDDF)/serial/components/serial_components.mk
include $(SDDF)/util/util.mk
