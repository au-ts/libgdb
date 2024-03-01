# Microkit sDDF serial example

## Description

This example uses sDDF serial to allow the debugger component to send/recieve information.
It was developed and tested on the odroidc2 platform but should also work on the odroidc4
if the example.system file is adjusted to reference the appropriate physical address for the
UART device.

## How to build

```
mkdir build && cd build
cmake -DMICROKIT_SDK=PATH_TO_SDK -DBOARD="odroidc2" ..
make
```

## How to use 

`tools/config/.gdbinit`contains a GDB configuration script that may be useful. Note that this file
is incomplete, and you will have to modify it so that you are connecting to the appropriate serial
device and with the correct baud rate.These instructions assume you have correctly set up this
configuration file. We also assume you have aarch64-none-elf-gdb, as the system is by default compiled
using the aarch64-none-elf toolchain. If you used a different compiler, you should use the corresponding
version of GDB.

```
cd build
aarch64-none-elf-gdb
aarch64-none-elf-gdb> connect
```

At this point, you will be connected to the target system, which will be pre-registered with several
_inferiors_, each corresponding to a child protection domain under the debugger. At this stage,
GDB is not aware of the mapping between a protection domain and an executable, as no symbol tables
have been loaded.

The IDs of the inferiors show in GDB correspond directly to the IDs they have been assigned in
`example.system`. That is, inferior 0 corresponds to the `ping` protection domain, while inferior 1
corresponds to `pong`. So, the process to load symbols for the protection domains looks like the
following

```
aarch64-none-elf-gdb> file bin/ping.elf
aarch64-none-elf-gdb> inferior 1
aarch64-none-elf-gdb> file bin/pong.elf
```

One important note about GDB is that you must call continue from the protection domain that caused
an interrupt into GDB. Since the system is initialized with the current inferior being inferior 0, this
means that you must switch to inferior 0 before calling continue.

The stub implements the semantics imposed by `scheduler-locking step`, meainng that a continue resumes
all protection domains in the system, whereas a step only resumes the protection domain that is being
stepped.

Note that this implementation currently demands exclusive write-access to the serial port of the system.
If you would still like to observe other reads in the system, this can be done using the tool found in
tools/serial_demux`.
