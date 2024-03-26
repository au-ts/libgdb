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
else ifeq ("$(BOARD)", "odroidc4")
    CPU := "cortex-a55"
    ARCH := "arm"
    MODE := "64"
else ifeq ("$(BOARD)", "qemu_arm_virt")
    CPU := "cortex-a53"
    ARCH := "arm"
    MODE := "64"
else
    $(error Unsupported BOARD ($(BOARD)))
endif

MICROKIT_CONFIG := debug

CC := aarch64-none-elf-gcc
LD := ld

MICROKIT_TOOL := $(MICROKIT_SDK)/bin/microkit
BOARD_DIR := $(MICROKIT_SDK)/board/$(BOARD)/$(MICROKIT_CONFIG)

# IMAGES := ping.elf pong.elf debugger.elf uart.elf mux_rx.elf mux_tx.elf
IMAGES := debugger.elf
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

all: $(IMAGE_FILE)

# Make the debugger PD
DEBUGGER_OBJS := debugger.o

debugger.o: $(EXAMPLE_DIR)/apps/debugger/debugger.c
	$(CC) $(CFLAGS) $^ -o $@

debugger.elf: libgdb.a libco.a sddf_libutil.a $(DEBUGGER_OBJS)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

# Make the ping PD
# PING_OBJS := ping.o

# ping.o: $(EXAMPLE_DIR)/apps/ping/ping.c
# 	$(CC) $(CFLAGS) $^ -o $@

# ping.elf : $(PING_OBJS) sddf_libutil.a
# 	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

# Make the pong PD
# PONG_OBJS := ping.o

# pong.o: $(EXAMPLE_DIR)/apps/pong/pong.c
# 	$(CC) $(CFLAGS) $^ -o $@

# pong.elf : $(PONG_OBJS) sddf_libutil.a
# 	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

# Make the

example.system: ${EXAMPLE_DIR}/example.system
	cp ${EXAMPLE_DIR}/example.system example.system

$(IMAGE_FILE) $(REPORT_FILE): $(IMAGES) example.system
	$(MICROKIT_TOOL) example.system --search-path $(BUILD_DIR) --board $(MICROKIT_BOARD) --config $(MICROKIT_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)



include $(LIBGDB_DIR)/libgdb.mk
include $(SDDF)/libco/libco.mk
include $(SDDF)/util/util.mk