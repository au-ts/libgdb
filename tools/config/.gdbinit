define connect_serial
# set debug remote 1 # Uncomment for more logging
# replace [BAUD] with desired baud rate
set serial baud 115200
set detach-on-fork off
set follow-fork-mode child
# replace [PATH] with path to serial device or virt console
target remote /dev/pts/8
set scheduler-locking step
end

define connect_net
# set debug remote 1 # Uncomment for more logging
set detach-on-fork off
set follow-fork-mode child
# replace [PATH] with path to serial device or virt console
target remote localhost:1234
set scheduler-locking step
end