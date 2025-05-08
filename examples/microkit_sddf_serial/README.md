# Microkit sDDF serial example

## Description

This example uses the sDDF serial subsystem to allow the debugger component 
to send/recieve information between the target and GDB.

## How to build

```
make MICROKIT_SDK=PATH_TO_SDK MICROKIT_BOARD=BOARD_NAME
```

This will create `build/loader.img`, which is a microkit image that can be run on
the target machine.

## Currently supported boards

The boards that have been tested to work are`[odroidc4, odroidc2, qemu_arm_virt]`.

### QEMU booting instructions

```
$(QEMU) -machine virt,virtualization=on \
              -cpu cortex-a53 \
              -serial mon:stdio \
              -device loader,file=$(IMAGE_FILE),addr=0x70000000,cpu-num=0 \
              -m size=2G \
              -nographic \
              -device virtio-serial-device \
              -chardev pty,id=virtcon \
              -device virtconsole,chardev=virtcon \
              -global virtio-mmio.force-legacy=false \
              -d guest_errors
```

This configuration enables the use of two serial ports. Debug printing will go to
stdout, while GDB will use a virtual console. The virtual console that is used is chosen at
runtime, and you will see output like the following at the beginning of the QEMU log:

```
char device redirected to /dev/ttys004 (label virtcon)
```

You can connect to this with GDB as you would any other serial device.


### Getting it working on real hardware

In all of the hardware examples, GDB and debug printing use the same serial port. This
means that GDB's output will be mixed up with the output of other things on the system. GDB is
quite good at filtering all of this out and ignoring unrelated output from the board, but as a
user, you will not be able to see any output from the board if GDB is connected to the serial.
The best solution is to have GDB use a different serial port that it has exclusive access to, but
if this is not possible, the serial demux tool can be used to split output streams. It will forward
all GDB-related output to a virtual console, which GDB can connect to, and print non-GDB related
output to its stdout. Note that even with this, GDB must have exclusive write-access to the serial
port.

## How to use 

`tools/config/.gdbinit`contains a GDB configuration script that may be useful. Note that this file
is incomplete, and you will have to modify it so that you are connecting to the appropriate serial
device and with the correct baud rate.These instructions assume you have correctly set up this
configuration file. We also assume you have aarch64-none-elf-gdb, as the system is by default compiled
using the aarch64-none-elf toolchain. If you used a different compiler, you should use the corresponding
version of GDB.

You should first boot the system and wait until you see text that says something like
"Awaiting GDB input..."

Then, on a different console, you should enter the following commands:
```
cd build
aarch64-none-elf-gdb
aarch64-none-elf-gdb> connect_serial
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
at zero and are contiguous, with NUM_DEBUGEES total PDs being debugged (this is defined in `apps/debugger.c`).
The is not a limitation of libGDB, but more to do with the difficulty of architecture-dependent configuration
in microkit-based systems. As microkit is extended with more powerful run-time configuration options,
this will likely be changed.

One important note about GDB is that you must call continue from the protection domain that caused
an interrupt into GDB. Since the system is initialized with the current inferior being inferior 1, this
means that you must switch to inferior 1 before calling continue.

The stub implements the semantics imposed by `scheduler-locking step`, meaning that a continue resumes
all protection domains in the system, whereas a step only resumes the protection domain that is being
stepped.

## How to use (VSCode)

1. Install the [C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
2. Set up a launch.json file like the one in .vscode/launch.json
3. Start the target system and wait till it reaches the "Awating GDB connection..." prompt.
4. Launch a debug session using the VSCode sidebar.

This will allow you to use GDB within VSCode for debugging. For more information on how to do this,
see [https://code.visualstudio.com/docs/debugtest/debugging](https://code.visualstudio.com/docs/debugtest/debugging).
We have tested breakpoints, watchpoints, and single-stepping, as well as the usual ability to read
memory and registers. If there are any features you need that don't work, please make an issue
on this repository.

The main limitation of this approach is that the VSCode debugger seems to really only be designed
for a single inferior. There is a multi-target mode, but this seems to assume that the two targets
are completely independent and communicate on separate connections, which is not ideal for serial
as systems only have a limited number of serial ports. Using net would likely work better in this
case, see the microkit_sddf_net example's README for more information.
