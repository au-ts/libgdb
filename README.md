# libgdb

## Description

This is an experimental library that aims to provide debugger support for seL4-based systems. It is being developed
primarily for use with the seL4 microkit but has been implemented as a library that can be used on other
seL4-based systems. It has been used most on the `odroidc2` and `qemu_virt_aarch64` boards, but
should work with any aarch64 platform.

This library is designed to be used in combination with a *debugger* component. This holds thread
control block (TCB) and VSpace capabilities of the threads which are to be debugged and is expected
to be the fault handler for these threads. The main responsibilities of the debuger component are to
initialize system state in libGDB, handle communication between the host machine (running GDB) and
libGDB on the target machine, and pass faults to libGDB. 

This repository contains three examples - `microkit_sddf_serial`, `microkit_sddf_net`, `microkit_integrated_serial`.

These examples show different ways that a debugger can be implemented.

`microkit_integrated_serial` uses an internal polling UART driver for IO. This approach might be
used for extremely minimal systems.

`microkit_sddf_serial` uses an [sDDF](https://github.com/au-ts/sddf)-serial subsystem. This approach
might be used if you don't have access to networking.

`microkit_sddf_net` uses an sDDF-net subsystem and provides debugging functionality over TCP. This
is probably the most common way that libGDB might be used, as it is the most flexible configuration.

## Dependencies

This library depends on seL4 and microkit changes that have not been upstreamed. As such, a custom version of the
microkit will have to be built using the following sources.

microkit: [https://github.com/alwin-joshy/microkit/tree/gdb_2025](https://github.com/alwin-joshy/microkit/tree/gdb_2025)

seL4: [https://github.com/alwin-joshy/seL4/tree/gdb_2025](https://github.com/alwin-joshy/seL4/tree/gdb_2025)

The examples depend on the `aarch64-none-elf` toolchain for compilation, and libGDB has only been tested with `aarch64-none-elf-gdb`,
though any GDB that has architecture support for aarch64 should suffice.

The serial-demux tool depends on the Rust toolchain.

NOTE: These kernel and microkit changes may break other configurations. Use at your own risk.

## Using libgdb

We provide both a CMakeLists.txt and libgdb.mk Makefile snippet. To see how either can be
used, look at the `microkit_integrated_serial` and `microkit_sddf_serial` examples respectively.

libGDB was primarily designed to be used with the seL4 microkit, but is actually OS framework agnostic.
In our examples, we use the microkit and as such, pass in -DMICROKIT in the CFLAGS. If this is not provided,
the library will not look for "microkit.h" and will instead try and find the definitions it requires from
the standard "sel4/seL4.h" as in other seL4-based systems.

