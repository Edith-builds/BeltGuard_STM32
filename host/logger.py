#!/usr/bin/env python3
"""CRC-checked feature logger. No motor commands or fabricated health score."""
import argparse
import json
import time
from pathlib import Path

def crc16(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc

def decode(line):
    payload, checksum = line.strip().rsplit(b"*", 1)
    if len(checksum) != 4 or int(checksum, 16) != crc16(payload):
        raise ValueError("CRC mismatch")
    obj = json.loads(payload)
    if obj.get("v") != 1 or not isinstance(obj.get("node"), int):
        raise ValueError("unsupported packet")
    return obj

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("port", help="COM5 or /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--out", type=Path, default=Path("beltguard.jsonl"))
    args = parser.parse_args()
    import serial  # pip install pyserial
    last = None
    with serial.Serial(args.port, args.baud, timeout=2) as port, args.out.open("a", encoding="utf8") as out:
        while True:
            line = port.read_until(b"\n", 4096)
            if not line:
                print("STALE: no node packet for 2 s", flush=True)
                continue
            try:
                packet = decode(line)
            except (ValueError, TypeError, UnicodeDecodeError) as exc:
                print(f"Rejected packet: {exc}", flush=True)
                continue
            sequence = packet.get("seq")
            if last is not None and sequence != ((last + 1) & 0xFFFFFFFF):
                print(f"Sequence gap or reset: {last} -> {sequence}")
            last = sequence
            packet["host_unix_ns"] = time.time_ns()
            out.write(json.dumps(packet, allow_nan=False) + "\n")
            out.flush()
            print(f"node={packet['node']} section={packet.get('section')} "
                  f"gated={packet.get('gated')} RFID-warning={packet.get('rf_warning_mask')} "
                  f"vibration-valid={packet.get('vib_valid')}", flush=True)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
