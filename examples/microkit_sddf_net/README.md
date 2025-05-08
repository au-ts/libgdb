# Microkit sDDF net example

## Description

This example uses the sDDF net subsystem to allow the debugger component
to communicate with GDB. Like other sDDF net clients, the debugger
component includes the lwIP stack. The debugger component is configured
to use port 1234 for communication with GDB.

## How to build

```
make MICROKIT_SDK=PATH_TO_SDK MICROKIT_BOARD=BOARD_NAME
```

This will create `build/loader.img`, which is a microkit binary image that
can be run on the target machine.

## Currently supported boards

The boards that have been tested to work are`[odroidc4, odroidc2, qemu_arm_virt]`.

### QEMU booting instructions

To start running the image on qemu, simply run `make qemu`.

This will build the example and run it using the following command:

```
$(QEMU) -machine virt,virtualization=on \
		-cpu cortex-a53 \
		-serial mon:stdio \
		-device loader,file=$(IMAGE_FILE),addr=0x70000000,cpu-num=0 \
		-m size=2G \
		-nographic \
		-device virtio-net-device,netdev=netdev0 \
		-netdev user,id=netdev0,hostfwd=tcp::1234-:1234 \
		-global virtio-mmio.force-legacy=false \
		-d guest_errors
````

### Getting it working on real hardware

To get the example working on real hardware, it must be attached to a network with
a DHCP server. The debugger will attempt to obtain an IP address using lwIP and
print this out. The host machine (running GDB) should be on the same network and
use this IP address when connecting to the remote.

## How to use (GDB CLI)

`tools/config/.gdbinit`contains a GDB configuration script that may be useful. This will work
as-is for QEMU, but the IP address will need to be changed when working with real hardware.
These instructions assume you have correctly set up this configuration file. We also assume
you have aarch64-none-elf-gdb, as the system is by default compiled using the aarch64-none-elf
toolchain. If you used a different compiler, you should use the corresponding version of GDB.

You should first boot the system and wait until you see text that says something like
"Awaiting GDB input..."

Then, on a different console, you should enter the following commands:
```
cd build
aarch64-none-elf-gdb
aarch64-none-elf-gdb> connect_net
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
are completely independent and communicate on separate connections, which we currently don't support.
This should be fairly simple to implement, requiring a unique debugger component per component
to be debugged that uses a different IP address/port for communication. Please reach out if this
might be useful to you. 
