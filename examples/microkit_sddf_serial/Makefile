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

include ../../libgdb.mk
gdb: libgdb.a

#apps/debugger/debugger.mk

IMAGE_FILE := $(BUILD_DIR)/loader.img
REPORT_FILE := $(BUILD_DIR)/report.txt

all: $(BUILD_DIR)/debugger.elf
