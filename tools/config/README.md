# Additional configuration files

## gdbinit

This is an example .gdbinit file that can be used with GDB. It defines a
`connect` command that can be used to initialize the connection with a
remote.

## launch.json

The Visual Studio Code editor supports a number of extensions which aim to
provide a frontend for GDB. The launch.json file provided relies on the
[GDB beyond](https://marketplace.visualstudio.com/items?itemName=coolchyni.beyond-debug)
extension and allows you to connect to the remote with the debugging tab
in VSCode. This is still highly experimental and may not work in all-cases.
In particular, it does not handle multiple threads well, and has only been
shown to function normally when debugging a single inferior. Furthermore, it
does not pause upon connecting to the system, so it works best if all your
breakpoints, watchpoints, etc. are configured prior to connection.
