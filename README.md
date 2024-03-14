# libgdb

## Description

This is an experimental library that aims to provide debugger support for seL4 based systems. It is being developed
primarily for use with the seL4 microkit but has been implemented as a library that can be used on other
seL4-based systems. It has only been tested on the odroidc2 but should work on any aarch64 platform.

## Dependencies

This library depends on seL4 and microkit changes that have not yet been upstreamed. As such, a custom version of the
microkit will have to be built using the following sources.

microkit: https://github.com/alwin-joshy/microkit/tree/dev_gdb

seL4: https://github.com/alwin-joshy/seL4/tree/microkit_gdb_final

NOTE: These kernel and microkit changes may break other configurations. Use at your own risk.

## Using libgdb

libgdb currently has an external dependency on microkit which it depends on for seL4 related information.
To use it in your project, you just need to include the library, ensuring that it has been added to your
base CMakeLists.txt and ensuring that it has access to the microkit library. An example of this can be 
seen in examples/microkit_sddf_serial.
