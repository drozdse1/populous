#!/usr/bin/env python3
"""Extract only root land0..3 from a user-supplied 880 KiB Amiga OFS ADF.

No embedded assets, sector scanning, decompression or filesystem mounting.
LAND payload decoding belongs to land_data.c; this checks the OFS container.
"""
import argparse
import os
from pathlib import Path
import struct
import sys

ADF_SIZE = 901120
LAND_SIZE = 114 + 0x8340
LAND_NAMES = tuple(f"land{index}" for index in range(4))


def extract_ofs_files(image, names):
    """Return present, explicitly named root files; validate all selected chains.

    Missing names are omitted. No recursion, executable discovery or sector scan.
    Supports variable lengths and OFS extension tables, not FFS or hard links.
    """
    names = tuple(dict.fromkeys(name.lower() for name in names))
    if any(not name or len(name) > 30 or name in (".", "..") or
           any(char in name for char in ("\0", "/", "\\")) for name in names):
        raise ValueError("unsafe requested OFS name")
    if len(image) != ADF_SIZE or image[:4] != b"DOS\0":
        raise ValueError("expected an uncompressed 880 KiB DOS0/OFS ADF")

    def block(number):
        if not 2 <= number < ADF_SIZE // 512:
            raise ValueError(f"block pointer out of range: {number}")
        data = image[number * 512:(number + 1) * 512]
        words = struct.unpack(">128I", data)
        if sum(words) & 0xffffffff:
            raise ValueError(f"bad checksum in block {number}")
        return data, words

    root_number = struct.unpack_from(">I", image, 8)[0]
    if root_number != 880:
        raise ValueError("unsupported root block (expected 880)")
    _, root = block(root_number)
    if (root[0], root[3], root[127]) != (2, 72, 1):
        raise ValueError("invalid OFS root header")
    headers = {}
    visited = {root_number}
    for pointer in root[6:78]:
        while pointer:
            if pointer in visited:
                raise ValueError("directory chain cycle or duplicate block")
            visited.add(pointer)
            data, words = block(pointer)
            if (words[0], words[1], words[125]) != (2, pointer, root_number):
                raise ValueError("invalid root entry type, key or parent")
            length = data[432]
            name_bytes = data[433:433 + length]
            if not 1 <= length <= 30 or any(char in name_bytes for char in (0, 47, 92)):
                raise ValueError("unsafe OFS entry name")
            name = name_bytes.decode("latin1").lower()
            if name in (".", ".."):
                raise ValueError("unsafe OFS entry name")
            if name in names:
                if name in headers:
                    raise ValueError(f"duplicate {name}")
                if words[127] != 0xfffffffd or words[81] > ADF_SIZE:
                    raise ValueError(f"{name}: expected a bounded regular file")
                headers[name] = (pointer, words)
            pointer = words[124]

    files = {}
    for name, (owner, header) in headers.items():
        expected_count = (header[81] + 487) // 488
        table = header
        pointers = []
        while True:
            count = min(72, expected_count - len(pointers))
            if table[2] != count:
                raise ValueError(f"{name}: invalid block table count")
            entries = list(reversed(table[6:78]))
            if any(entries[count:]) or any(not entry for entry in entries[:count]):
                raise ValueError(f"{name}: invalid block table entries")
            pointers.extend(entries[:count])
            extension = table[126]
            if len(pointers) == expected_count:
                if extension:
                    raise ValueError(f"{name}: unexpected extension")
                break
            if not extension or extension in visited:
                raise ValueError(f"{name}: missing, shared or cyclic extension")
            visited.add(extension)
            _, table = block(extension)
            if (table[0], table[1], table[125], table[127]) != (16, extension, owner, 0xfffffffd):
                raise ValueError(f"{name}: invalid extension type, key or parent")
        pointer = header[4]
        payload = bytearray()
        for sequence, expected_pointer in enumerate(pointers, 1):
            if pointer != expected_pointer:
                raise ValueError(f"{name}: data chain disagrees with header block table")
            if pointer in visited:
                raise ValueError(f"{name}: data cycle or shared block")
            visited.add(pointer)
            data, words = block(pointer)
            if (words[0], words[1], words[2]) != (8, owner, sequence):
                raise ValueError(f"{name}: invalid data type, owner or sequence")
            size = words[3]
            if size != min(488, header[81] - len(payload)) or size == 0:
                raise ValueError(f"{name}: invalid data payload size")
            payload.extend(data[24:24 + size])
            pointer = words[4]
        if len(payload) != header[81] or pointer:
            raise ValueError(f"{name}: invalid data chain length")
        files[name] = bytes(payload)
    return files


def extract_land_files(image):
    """Compatibility API: require all four original, exact-length LAND files."""
    files = extract_ofs_files(image, LAND_NAMES)
    if set(files) != set(LAND_NAMES):
        raise ValueError("ADF must contain root land0, land1, land2 and land3")
    if any(len(payload) != LAND_SIZE for payload in files.values()):
        raise ValueError(f"LAND requires regular {LAND_SIZE}-byte files")
    return {name: files[name] for name in LAND_NAMES}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("adf", type=Path, help="lawfully supplied local DOS0 ADF")
    parser.add_argument("output", type=Path, help="external output directory (not the source tree)")
    args = parser.parse_args()
    try:
        with args.adf.open("rb") as source:
            image = source.read(ADF_SIZE + 1)
        files = extract_land_files(image)
        args.output.mkdir(mode=0o700, parents=True, exist_ok=True)
        # dir_fd anchors every write; O_EXCL refuses existing files and symlinks.
        directory = os.open(args.output, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        try:
            for name in LAND_NAMES:
                try:
                    os.stat(name, dir_fd=directory, follow_symlinks=False)
                except FileNotFoundError:
                    continue
                raise ValueError(f"refusing to overwrite {args.output / name}")
            for name, payload in files.items():
                descriptor = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                                     0o600, dir_fd=directory)
                with os.fdopen(descriptor, "wb") as target:
                    target.write(payload)
        finally:
            os.close(directory)
    except (OSError, ValueError) as error:
        print(f"LAND extraction failed: {error}", file=sys.stderr)
        return 1
    print(f"Extracted only land0..3 ({LAND_SIZE} bytes each) to {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
