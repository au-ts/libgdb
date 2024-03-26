/*
 * Copyright 2024, UNSW
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

use std::{env, thread};
use std::io::{self, Read, Write};
use serialport::{SerialPort, TTYPort};

fn main() {
    let args : Vec<String> = env::args().collect();
    let real_serial = &args[1];
    let baud_rate = &args[2];

    let (mut master_tx, slave) = TTYPort::pair().expect("Unable to create a pseudo-terminal");
    let mut master_rx = master_tx.try_clone().expect("Failed to clone");

    println!("Virtual serial port created. GDB should connect to the file {:?}", slave.name().expect("This should have a name"));

    let mut port_rx = serialport::new(real_serial, baud_rate.parse().unwrap()).open().expect("Failed to open port");
    let mut port_tx = port_rx.try_clone().expect("Failed to clone");

    /* Handle the port -> gdb direction */
    let handle = thread::spawn(move || {
        let mut in_gdb_packet = false;
        let mut gdb_checksum_counter = 0;
        let mut buffer: [u8; 1] = [0];
        loop {
            match port_rx.read(&mut buffer) {
                Ok(n) => {
                    if n == 1 {
                        if buffer[0] as char == '$' {
                            in_gdb_packet = true;
                        } else if buffer[0] as char == '#' && in_gdb_packet {
                            in_gdb_packet = false;
                            /* @alwin: this is kinda ugly*/
                            gdb_checksum_counter = 3;
                        }

                        if in_gdb_packet || gdb_checksum_counter > 0 || buffer[0] as char == '+' || buffer[0] as char == '-' {
                            master_tx.write(&buffer).expect("Could not write to master");
                            if gdb_checksum_counter > 0 {
                                gdb_checksum_counter -= 1;
                            }
                        } else {
                            print!("{}", buffer[0] as char);

                        }
                    }
                },
                Err(ref e) if e.kind() == io::ErrorKind::TimedOut => (),
                Err(e) => println!("port -> gdb: Failed with: {:?}", e),
            }
        }
    });

    /* Handle the gdb -> port direction */
    thread::spawn(move || {
        let mut buffer: [u8; 1] = [0];
        loop {
            match master_rx.read(&mut buffer) {
                Ok(n) => {
                    if n == 1 {
                        port_tx.write(&buffer).expect("Could not write to port\n");
                    }
                }
                Err(ref e) if e.kind() == io::ErrorKind::TimedOut => (),
                Err(e) => println!("gdb -> port: Failed with: {:?}", e),
            }
        }
    });

    handle.join().expect("Could not join thread");
}
