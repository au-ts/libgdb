ifeq ($(strip $(MICROKIT_SDK)),)
    $(error libGDB requires a MICROKIT_SDK)
endif

ifeq ($(strip $(BOARD)),)
    $(error libGDB requires a BOARD)
endif

ifeq ($(strip $(BUILD_DIR)),)
    $(error libGDB requires a BUILD_DIR)
endif

$(echo "hello $BOARD")

ifeq ("$(BOARD)", "odroidc2")
    CPU := "cortex-a53"
    ARCH := "arm"
    MODE := "64"
    UART_DRIVER := "meson"
else ifeq ("$(BOARD)", "odroidc4")
    CPU := "cortex-a55"
    ARCH := "arm"
    MODE := "64"
    UART_DRIVER := "meson"
else ifeq ("$(BOARD)", "qemu_arm_virt")
    CPU := "cortex-a53"
    ARCH := "arm"
    MODE := "64"
    UART_DRIVER := "arm"
else
    $(error Unsupported BOARD ($(BOARD)))
endif

MICROKIT_CONFIG := debug

CC := aarch64-none-elf-gcc
LD := aarch64-none-elf-ld
AR := aarch64-none-elf-ar

MICROKIT_TOOL := $(MICROKIT_SDK)/bin/microkit
BOARD_DIR := $(MICROKIT_SDK)/board/$(BOARD)/$(MICROKIT_CONFIG)

IMAGES := debugger.elf ping.elf pong.elf uart_driver.elf serial_rx_virt.elf serial_tx_virt.elf
CFLAGS += \
    -mtune=$(CPU) \
    -mstrict-align \
    -ffreestanding \
    -nostdlib \
    -g3 \
    -Wall \
    -Werror \
    -Wno-array-bounds \
    -Wno-unused-variable \
    -Wno-unused-function \
    -DBOARD_${BOARD} \
    -I$(BOARD_DIR)/include \
    -I$(SDDF)/include \
    -I$(SDDF)/libco \
    -I$(LIBGDB_DIR)/include \
    -I$(LIBGDB_DIR)/arch_include/ \
    -I$(LIBGDB_DIR)/lib/libco/include \

LDFLAGS := -L$(BOARD_DIR)/lib
LIBS := -lmicrokit -Tmicrokit.ld

IMAGE_FILE := $(BUILD_DIR)/loader.img
REPORT_FILE := $(BUILD_DIR)/report.txt

SERIAL_NUM_CLIENTS := -DSERIAL_NUM_CLIENTS=1

all: $(IMAGE_FILE)

# Make the debugger PD
debugger.o: $(EXAMPLE_DIR)/apps/debugger/debugger.c
	$(CC) -c $(CFLAGS) $^ -o $@

debugger.elf: debugger.o libgdb.a libco.a sddf_libutil.a 
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

# Make the ping PD
ping.o: $(EXAMPLE_DIR)/apps/ping/ping.c
	$(CC) -c $(CFLAGS) $^ -o $@

ping.elf : ping.o sddf_libutil.a
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

# Make the pong PD

pong.o: $(EXAMPLE_DIR)/apps/pong/pong.c
	$(CC) -c $(CFLAGS) $^ -o $@

pong.elf : pong.o sddf_libutil.a
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

# Make the

example.system: ${EXAMPLE_DIR}/example.system
	cp ${EXAMPLE_DIR}/example.system example.system

$(IMAGE_FILE) $(REPORT_FILE): $(IMAGES) example.system
	$(MICROKIT_TOOL) example.system --search-path $(BUILD_DIR) --board $(BOARD) --config $(MICROKIT_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)

FORCE:

clean::
	rm -f debugger.o ping.o pong.o uart_driver.o serial_tx_virt.o serial_rx_virt.o 

include $(LIBGDB_DIR)/libgdb.mk
include $(SDDF)/libco/libco.mk
include $(SDDF)/util/util.mk
include $(SDDF)/drivers/serial/meson/uart.mk
include $(SDDF)/serial/components/serial_components.mk