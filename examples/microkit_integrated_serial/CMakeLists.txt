cmake_minimum_required(VERSION 3.26)

set(CMAKE_SYSTEM_NAME Generic)

if (NOT DEFINED MICROKIT_SDK)
    message(FATAL_ERROR "MICROKIT_SDK is not set")
endif (NOT DEFINED MICROKIT_SDK)

if (NOT DEFINED BOARD)
    message(FATAL_ERROR "BOARD is not set}")
endif (NOT DEFINED BOARD)

if (BOARD STREQUAL "odroidc2")
    set(CPU "cortex-a53")
elseif (BOARD STREQUAL "odroidc4")
    set(CPU "cortex-a55")
else ()
    message(FATAL_ERROR "board (${BOARD}) is not supported")
endif (BOARD STREQUAL "odroidc2")

# Microkit setup
set(MICROKIT_CONFIG debug)
set(BOARD_DIR ${MICROKIT_SDK}/board/${BOARD}/${MICROKIT_CONFIG})
set(MICROKIT_TOOL ${MICROKIT_SDK}/bin/microkit)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(IMAGE_FILE ${EXECUTABLE_OUTPUT_PATH}/loader.img)
set(REPORT_FILE ${EXECUTABLE_OUTPUT_PATH}/report.txt)

# Compiler setup
set(CMAKE_C_COMPILER aarch64-none-elf-gcc)
set(CMAKE_C_FLAGS "-mcpu=${CPU} -mstrict-align -nostdlib -ffreestanding -g3 -Wall -Wno-array-bounds -Wno-unused-variable -Wno-unused-function -Werror -DBOARD_${BOARD}")
set(CMAKE_EXE_LINKER_FLAGS "-T ${BOARD_DIR}/lib/microkit.ld")

project(microkit_example C)

include_directories(${BOARD_DIR}/include)
link_directories(${BOARD_DIR}/lib)

add_library(microkit STATIC IMPORTED)
set_target_properties(microkit PROPERTIES IMPORTED_LOCATION ${BOARD_DIR}/lib/libmicrokit.a)

add_subdirectory(../../ lib/gdb/)

add_subdirectory(lib/uart)

add_subdirectory(apps/ping)
add_subdirectory(apps/pong)
add_subdirectory(apps/debugger)

add_custom_target(image ALL
                   COMMAND ${MICROKIT_TOOL} ${CMAKE_SOURCE_DIR}/example.system --search-path ${EXECUTABLE_OUTPUT_PATH}
                            --board ${BOARD} --config ${MICROKIT_CONFIG} -o ${IMAGE_FILE} -r ${REPORT_FILE}
                   DEPENDS ping.elf pong.elf debugger.elf
)