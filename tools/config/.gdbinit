define connect_serial
# set debug remote 1 # Uncomment for more logging
# replace [BAUD] with desired baud rate
set serial baud [BAUD]
set detach-on-fork off
set follow-fork-mode child
# replace [PATH] with path to serial device or virt console
target remote [PATH]
set scheduler-locking step
end