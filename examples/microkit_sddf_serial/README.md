# Microkit sDDF serial example

## Description

This example uses sDDF serial to allow the debugger component to send/recieve information.
It was developed and tested on the odroidc2 platform but should also work on the odroidc4
if the example.system file is adjusted to reference the appropriate physical address for the
UART device.

## How to build

```
mkdir build && cd build
cmake -DMICROKIT_SDK=PATH_TO_SDK -DCPU="cortex-a53" -DBOARD="odroidc2" ..
make
```