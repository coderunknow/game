#!/usr/bin/env python3
"""Append a payload archive to the installer stub (desktop/setup.cpp format).

Archive layout appended after the stub PE image:

    [raw file data (dataBytes)][file table][footer 32 bytes]

    footer: magic[8]="PVZMSTP1" | u64 blobOffset | u64 dataBytes |
            u32 fileCount | u32 reserved

    table entry: u16 pathLen | utf8 path ('/' separators) | u64 size | u64 off
    (off is relative to blobOffset)

Usage:
    make-installer.py --setup PVZM-Setup-raw.exe --payload DIR --out PVZM-Setup-x64.exe
"""

import argparse
import os
import struct
import sys

MAGIC = b"PVZMSTP1"


def collect(payload_dir: str):
    files = []
    for root, _dirs, names in os.walk(payload_dir):
        for name in sorted(names):
            full = os.path.join(root, name)
            rel = os.path.relpath(full, payload_dir).replace(os.sep, "/")
            files.append((rel, full, os.path.getsize(full)))
    files.sort(key=lambda t: t[0])
    return files


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--setup", required=True, help="compiled installer stub (PE)")
    ap.add_argument("--payload", required=True, help="directory to embed")
    ap.add_argument("--out", required=True, help="output installer exe")
    args = ap.parse_args()

    with open(args.setup, "rb") as f:
        stub = f.read()
    if stub[:2] != b"MZ":
        print("error: setup stub is not a PE executable", file=sys.stderr)
        return 1

    files = collect(args.payload)
    if not files:
        print("error: payload is empty", file=sys.stderr)
        return 1

    data = bytearray()
    table = bytearray()
    for rel, full, size in files:
        off = len(data)
        with open(full, "rb") as f:
            data += f.read()
        path = rel.encode("utf-8")
        table += struct.pack("<H", len(path)) + path
        table += struct.pack("<QQ", size, off)

    blob = bytes(data) + bytes(table)
    footer = MAGIC + struct.pack(
        "<QQII", len(stub), len(data), len(files), 0
    )
    assert len(footer) == 32

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "wb") as f:
        f.write(stub)
        f.write(blob)
        f.write(footer)

    total = os.path.getsize(args.out)
    print(f"wrote {args.out}: {len(files)} files, {total} bytes total")
    return 0


if __name__ == "__main__":
    sys.exit(main())
