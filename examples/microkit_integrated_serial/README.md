# Microkit sDDF serial example

## Description

This example uses the sDDF serial subsystem to allow the debugger component 
to send/recieve information between the target and GDB.

## How to build

```
mkdir build && cd build
cmake -DMICROKIT_SDK=PATH_TO_SDK -DBOARD="odroidc2" ..
make
```

This will create `build/loader.img`, which is a microkit image that can be run on
the target machine.

## Currently supported boards

The boards that have been tested to work are`[odroidc2, odroidc4]`. 

## How to use 

Refer to `examples/microkit_sddf_serial/README.md`
