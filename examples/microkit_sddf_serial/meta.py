# Copyright 2025, UNSW
# SPDX-License-Identifier: BSD-2-Clause
import argparse
from typing import List
from dataclasses import dataclass
from sdfgen import SystemDescription, Sddf, DeviceTree
from importlib.metadata import version

assert version('sdfgen').split(".")[1] == "24", "Unexpected sdfgen version"

ProtectionDomain = SystemDescription.ProtectionDomain
Channel = SystemDescription.Channel
MemoryRegion = SystemDescription.MemoryRegion
Map = SystemDescription.Map

@dataclass
class Board:
    name: str
    arch: SystemDescription.Arch
    paddr_top: int
    serial: str

BOARDS: List[Board] = [
    Board(
        name="qemu_virt_aarch64",
        arch=SystemDescription.Arch.AARCH64,
        paddr_top=0x6_0000_000,
        serial="virtio_mmio@a003e00"
    ),
    Board(
        name="odroidc4",
        arch=SystemDescription.Arch.AARCH64,
        paddr_top=0x80000000,
        serial="soc/bus@ff800000/serial@3000"
    ),
    Board(
        name="odroidc2",
        arch=SystemDescription.Arch.AARCH64,
        paddr_top=0x80000000,
        serial="soc/bus@c8100000/serial@4c0"
    ),
]

def generate(sdf_file: str, output_dir: str, dtb: DeviceTree):
    serial_driver = ProtectionDomain("serial_driver", "serial_driver.elf", priority=100)
    serial_virt_tx = ProtectionDomain("serial_virt_tx", "serial_virt_tx.elf", priority=99)
    serial_virt_rx = ProtectionDomain("serial_virt_rx", "serial_virt_rx.elf", priority=99)
    debugger = ProtectionDomain("debugger", "debugger.elf", priority=97)

    mr = MemoryRegion("mr1", 0x1000)
    sdf.add_mr(mr)
    map = Map(mr, 0x600000, "rw")
    debugger.add_map(map)

    ping = ProtectionDomain("ping", "ping.elf", priority=96)
    pong = ProtectionDomain("pong", "pong.elf", priority=96)
    sdf.add_channel(Channel(ping, pong))

    assert debugger.add_child_pd(ping) == 0
    assert debugger.add_child_pd(pong) == 1

    serial_node = dtb.node(board.serial)
    assert serial_node is not None

    serial_system = Sddf.Serial(sdf, serial_node, serial_driver, serial_virt_tx, virt_rx=serial_virt_rx, enable_color=False)
    serial_system.add_client(debugger)

    pds = [
        serial_driver,
        serial_virt_tx,
        serial_virt_rx,
        debugger,
    ]

    for pd in pds:
        sdf.add_pd(pd)

    assert serial_system.connect()
    assert serial_system.serialise_config(output_dir)

    with open(f"{output_dir}/{sdf_file}", "w+") as f:
        f.write(sdf.render())


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--dtb', required=True)
    parser.add_argument('--sddf', required=True)
    parser.add_argument('--board', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--sdf', required=True)

    args = parser.parse_args()

    board = next(filter(lambda b: b.name == args.board, BOARDS))

    sdf = SystemDescription(board.arch, board.paddr_top)
    sddf = Sddf(args.sddf)

    with open(args.dtb, "rb") as f:
        dtb = DeviceTree(f.read())

    generate(args.sdf, args.output, dtb)
