# Microkit sDDF serial example

## Description

This example uses the sDDF serial subsystem to allow the debugger component 
to send/recieve information between the target and GDB.

## How to build

```
make MICROKIT_SDK=PATH_TO_SDK BOARD=BOARD_NAME
```

This will create `build/loader.img`, which is a microkit image that can be run on
the target machine.

## Currently supported boards

The boards that have been tested to work are`[odroidc4, odroidc2, qemu_arm_virt]`. To switch between 
examples, you will need to adjust the `example.system` file as necessary. 

### QEMU booting instructions

```
qemu-system-aarch64 \
       -machine virt,\
       -cpu cortex-a53 \
       -m size=2048M \
       -nographic \
       -serial mon:pty \
       -device loader,file=bin/loader.img,addr=0x70000000,cpu-num=0
```

This configuration will redirect the output to a virtual console, which you can connect GDB to.
Note: The serial demux tool does not currently allow you to operate on virtual consoles.

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

The IDs of the inferiors show in GDB correspond to the IDs they have been assigned in
`example.system`. The inferior IDs are consecutive, beginning at 1 with exactly as
many inferiors as child protection domains in the debugger PD, with lower inferior IDs given to lower
system description IDs. That is, inferior 1 corresponds to the `ping` protection domain, while
inferior 2 corresponds to `pong`. So, the process to load symbols for the protection domains looks
like the following

```
aarch64-none-elf-gdb> file bin/ping.elf
aarch64-none-elf-gdb> inferior 2
aarch64-none-elf-gdb> file bin/pong.elf
```

The debugger component currently assumes that the IDs for debugee protection domains start
at zero and are contigious, with NUM_DEBUGEES total PDs being debugged (this is defined in `apps/debugger.c`).
The is not a limitation of libGDB, but more to do with the difficulty of architecure-dependent configuration
in microkit-based systems. As microkit is extended with more powerful run-time configuration options,
this will likely be changed.

One important note about GDB is that you must call continue from the protection domain that caused
an interrupt into GDB. Since the system is initialized with the current inferior being inferior 1, this
means that you must switch to inferior 1 before calling continue.

The stub implements the semantics imposed by `scheduler-locking step`, meainng that a continue resumes
all protection domains in the system, whereas a step only resumes the protection domain that is being
stepped.

Note that this implementation currently demands exclusive write-access to the serial port of the system.
It does not require exclusive read-access to the system, and you can use the tool found in 
`tools/serial_demux` to share the serial output stream.
