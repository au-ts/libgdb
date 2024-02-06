# GDB Serial Demultipexer Tool

## Description

GDB provides multiple interfaces through which a host (running GDB) can connect to a remote target. One of these interfaces is a
serial connection, which is often the most convenient and accessible. However, when using the serial connection, GDB assumes
exclusive access to the device, meaning that you lose the ability to utilise the serial port for other things, such as logging,
as any output from the device that does not follow the GDB packet format is discarded and ignored.

This program solves the problem by assuming control of the real serial device and exposing a virtual console that it forwards
GDB related packets to (printing everything else to STDOUT). GDB can then connect to this virtual console instead of the real
serial device, and recieve packets from the remote. The program also allows GDB to write the the virtual console and forwards
these packets to the reak serial device.

## How to use

cargo run -- [path_to_serial_device] [baud_rate]

