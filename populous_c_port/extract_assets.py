#!/usr/bin/env python3
"""Export original Populous data and source-documented graphics from a local OFS ADF.

Python standard library only. Output must not exist and its parent must exist.
No executable extraction, rendering changes, mounting, or writes to the ADF.
"""
import argparse
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import zlib

from extract_land import ADF_SIZE, LAND_NAMES, LAND_SIZE, extract_ofs_files

# src/populous_prg.asm:24264-24268; also populous_main.asm:5492-5495.
PALETTE = (0x000, 0x444, 0x666, 0x888, 0xaaa, 0xccc, 0x620, 0x840,
           0xa00, 0xa60, 0xaa0, 0x4a0, 0x280, 0x262, 0x248, 0x26c)
ASSET_NAMES = (LAND_NAMES + tuple(f"sprites{index}.dat" for index in range(10)) +
               tuple(f"spr_32{index}.dat" for index in range(10)) +
               ("qaz.pic", "demo.pic", "lord.pic", "load.pic", "level.dat", "font.dat", "mouths.pic"))
PICTURES = {
    "qaz.pic": (4, False, "src/populous_prg.asm:16359-16402,24264-24268"),
    "demo.pic": (4, False, "src/populous_main.asm:1754-1780,5492-5495"),
    "lord.pic": (4, True, "src/populous_prg.asm:16631-16663"),
    "load.pic": (5, True, "src/populous_main.asm:383-395,1842-1858"),
}


def rgb12(words):
    if any(not 0 <= word <= 0xfff for word in words):
        raise ValueError("unsupported non-RGB12 palette words")
    return [tuple(((word >> shift) & 15) * 17 for shift in (8, 4, 0)) for word in words]


def decode_masked(data, width, height, word_bits):
    """Frame-major rows of keep-mask,p0,p1,p2,p3, MSB pixel first.

    LAND: one BE32 group per row. Small sprites: one BE16 group.
    Large sprites: two BE16 groups per row, left then right.
    """
    if word_bits not in (16, 32) or width <= 0 or height <= 0 or width % word_bits:
        raise ValueError("invalid masked frame dimensions")
    word_bytes = word_bits // 8
    frame_size = width * height * 5 // 8
    if not data or len(data) % frame_size:
        raise ValueError(f"masked data must contain complete {frame_size}-byte frames")
    frames = []
    for start in range(0, len(data), frame_size):
        indices, keep = bytearray(), bytearray()
        for offset in range(start, start + frame_size, 5 * word_bytes):
            words = struct.unpack_from(">5" + ("H" if word_bits == 16 else "I"), data, offset)
            for bit in range(word_bits - 1, -1, -1):
                keep.append((words[0] >> bit) & 1)
                indices.append(sum(((words[plane + 1] >> bit) & 1) << plane for plane in range(4)))
        frames.append({"indices": bytes(indices), "keep": bytes(keep),
                       "mask_conflicts": sum(bool(mask and index) for mask, index in zip(keep, indices))})
    return frames


def decode_planar(data, width, height, planes):
    if width <= 0 or width % 8 or height <= 0 or not 1 <= planes <= 8:
        raise ValueError("invalid planar dimensions")
    plane_size = width * height // 8
    if len(data) != plane_size * planes:
        raise ValueError("invalid plane-major picture length")
    return bytes(sum(((data[plane * plane_size + pixel // 8] >> (7 - pixel % 8)) & 1) << plane
                     for plane in range(planes)) for pixel in range(width * height))


def rgba(indices, palette, keep=None, faithful_alpha=True):
    if keep is not None and len(keep) != len(indices):
        raise ValueError("mask length does not match indices")
    if any(index >= len(palette) for index in indices):
        raise ValueError("pixel index outside palette")
    pixels = bytearray()
    for pixel, index in enumerate(indices):
        pixels.extend(palette[index])
        pixels.append(0 if faithful_alpha and keep is not None and keep[pixel] else 255)
    return bytes(pixels)


def png_rgba(width, height, pixels):
    if width <= 0 or height <= 0 or len(pixels) != width * height * 4:
        raise ValueError("invalid RGBA dimensions")

    def chunk(kind, payload):
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))

    rows = b"".join(b"\0" + pixels[row * width * 4:(row + 1) * width * 4] for row in range(height))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def land_values(data):
    """Stored values, not runtime-derived duplicates/sorts; land_data.c:23-65."""
    if len(data) != LAND_SIZE:
        raise ValueError("unsupported LAND length")
    rules = {"walk_death": struct.unpack_from(">H", data)[0]}
    for name, offset, count in (("population_add", 2, 11), ("mana_add", 24, 11),
                                ("weapons_add", 46, 11), ("battle_add1", 68, 11),
                                ("battle_add2", 90, 3)):
        rules[name] = list(struct.unpack_from(f">{count}H", data, offset))
    rules["map_colour"] = list(data[96:112])
    rules["sprite_set"] = struct.unpack_from(">H", data, 112)[0]
    return rules


def level_values(data):
    """Source documents 10-byte records; avoid unverified semantic field names."""
    if not data or len(data) % 10:
        raise ValueError("unsupported level record length")
    return [{"record": offset // 10, "offset": offset, "bytes_0_7": list(data[offset:offset + 8]),
             "word_8_be": struct.unpack_from(">H", data, offset + 8)[0]}
            for offset in range(0, len(data), 10)]


def json_bytes(value):
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")


def masked_artifacts(name, data, width, height, word_bits, source_offset):
    frames = decode_masked(data, width, height, word_bits)
    columns = min(10 if width == 32 else 16, len(frames))
    sheet_width = columns * width
    sheet_height = ((len(frames) + columns - 1) // columns) * height
    pixels = bytearray(sheet_width * sheet_height * 4)
    masks = bytearray(len(pixels))
    conflicts = sum(frame["mask_conflicts"] for frame in frames)
    positions = []
    palette = rgb12(PALETTE)
    for number, frame in enumerate(frames):
        # A conflicting keep-bit means OR-compositing, not alpha blending.
        # In that case show the whole sheet on background index 0, never hide bits.
        frame_pixels = rgba(frame["indices"], palette, frame["keep"], not conflicts)
        mask_pixels = rgba(frame["keep"], [(0, 0, 0), (255, 255, 255)])
        left, top = number % columns * width, number // columns * height
        for row in range(height):
            dest = ((top + row) * sheet_width + left) * 4
            src = row * width * 4
            pixels[dest:dest + width * 4] = frame_pixels[src:src + width * 4]
            masks[dest:dest + width * 4] = mask_pixels[src:src + width * 4]
        positions.append({"frame": number, "source_offset": source_offset + number * width * height * 5 // 8,
                          "x": left, "y": top, "mask_conflicts": frame["mask_conflicts"]})
    artifacts = {
        f"graphics/{name}.png": png_rgba(sheet_width, sheet_height, pixels),
        f"graphics/{name}.keep-mask.png": png_rgba(sheet_width, sheet_height, masks),
        f"pixels/{name}.indices.u8": b"".join(frame["indices"] for frame in frames),
        f"pixels/{name}.keep.u8": b"".join(frame["keep"] for frame in frames),
    }
    metadata = {"frame_count": len(frames), "frame_width": width, "frame_height": height,
                "frame_bytes": width * height * 5 // 8, "graphics_offset": source_offset,
                "format": f"row groups: BE{word_bits} keep-mask,p0,p1,p2,p3; MSB left; left group first",
                "sheet_columns": columns, "frames": positions, "palette": "gameplay_rgb12",
                "mask_conflicts": conflicts, "faithful_alpha": not bool(conflicts),
                "preview": "RGBA with keep-mask alpha" if not conflicts else "opaque OR bits on background index 0",
                "pixel_layout": "frame-major, then row-major; one byte per index/keep bit"}
    return artifacts, metadata


def export_assets(adf, output):
    adf, output = Path(adf), Path(output)
    # lexists catches dangling symlinks as well as populated or empty directories.
    if os.path.lexists(output):
        raise FileExistsError(f"refusing existing output: {output}")
    if not output.parent.is_dir():
        raise FileNotFoundError(f"output parent must already exist: {output.parent}")
    with adf.open("rb") as source:
        image = source.read(ADF_SIZE + 1)
    files = extract_ofs_files(image, ASSET_NAMES)
    artifacts, present = {}, {}
    values = {"palettes": {"gameplay_rgb12": list(PALETTE)}, "land": {}, "levels": None,
              "encoding": "LAND words unsigned BE16 as stored; level bytes 0..7 and BE16 at 8",
              "sources": ["src/populous_prg.asm:16483-16529,12321-12362,24264-24268",
                          "src/populous_main.asm:5492-5495", "populous_c_port/land_data.c:23-65"],
              "limitations": ["No guessed level field names or level-to-world mapping.",
                              "Stored LAND values are preserved, not clamped or replaced by port defaults."]}
    for name, data in sorted(files.items()):
        raw_path = f"raw/{name}"
        artifacts[raw_path] = data
        entry = {"raw": raw_path, "size": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                 "status": "verified", "limitations": []}
        present[name] = entry
        try:
            if name in LAND_NAMES:
                rules = land_values(data)
                values["land"][name] = rules
                generated, metadata = masked_artifacts(name, data[114:], 32, 24, 32, 114)
                artifacts.update(generated)
                entry.update(metadata)
                entry["source"] = "src/populous_prg.asm:15766-15789,16483-16529"
            elif name.startswith(("sprites", "spr_32")):
                small = name.startswith("sprites")
                generated, metadata = masked_artifacts(name, data, 16 if small else 32,
                                                       16 if small else 32, 16, 0)
                artifacts.update(generated)
                entry.update(metadata)
                entry["source"] = "src/populous_prg.asm:19312-19357" if small else "src/populous_prg.asm:19397-19449"
                if name not in ("sprites0.dat", "spr_320.dat"):
                    entry["status"] = "inferred"
                    entry["limitations"].append("Numbered variant uses the same source loader; variant content not independently verified.")
                if small:
                    entry["limitations"].append("All complete file frames exported; gameplay reads 143 (0x5960 bytes), intro 147 (0x5be0). Extra frames' gameplay use is unverified.")
            elif name in PICTURES:
                planes, embedded, reference = PICTURES[name]
                pixel_size = planes * 8000
                expected_size = pixel_size + (2 * (1 << planes) if embedded else 0)
                if len(data) != expected_size:
                    raise ValueError(f"expected {expected_size} bytes; unknown picture variant")
                indices = decode_planar(data[:pixel_size], 320, 200, planes)
                artifacts[f"pixels/{name}.indices.u8"] = indices
                palette_words = list(struct.unpack(f">{1 << planes}H", data[pixel_size:])) if embedded else list(PALETTE)
                palette = rgb12(palette_words)
                artifacts[f"graphics/{name}.png"] = png_rgba(320, 200, rgba(indices, palette))
                values["palettes"][name] = palette_words
                entry.update({"width": 320, "height": 200, "planes": planes,
                              "format": "plane-major, 8000 bytes/plane, 40 bytes/row, MSB left",
                              "palette": name, "palette_source": f"file offset {pixel_size}, BE16 RGB12" if embedded else reference,
                              "source": reference, "pixel_layout": "row-major indices, one byte/pixel"})
            elif name == "level.dat":
                values["levels"] = level_values(data)
                entry.update({"record_count": len(values["levels"]), "record_bytes": 10,
                              "source": "src/populous_prg.asm:12321-12362", "format": "8 bytes + BE16 word"})
                if len(data) != 990:
                    entry["status"] = "inferred"
                    entry["limitations"].append("Source documents 99 records; this file has a different complete-record count.")
            else:
                raise ValueError("Original auxiliary graphics preserved raw; font/mouth layout and palette not verified by this exporter")
        except ValueError as error:
            entry["status"] = "unsupported"
            entry["limitations"].append(str(error))
    artifacts["values.json"] = json_bytes(values)
    manifest = {
        "schema_version": 1,
        "source": {"path": str(adf.resolve()), "size": len(image), "sha256": hashlib.sha256(image).hexdigest(),
                   "format": "880 KiB DOS0/OFS, root 880", "modified": False},
        "present": present, "missing": sorted(set(ASSET_NAMES) - set(files)),
        "selection": "Explicit root allowlist: land0..3, sprites0..9.dat, spr_320..329.dat, four pictures, level.dat, font.dat, mouths.pic. No executables or audio.",
        "status_meaning": {"verified": "layout documented in local source and file size validated; not a historical authenticity claim",
                           "inferred": "source-family layout fits but variant interpretation is not independently verified",
                           "unsupported": "raw preserved; decoded interpretation unavailable"},
        "limitations": ["Static sheets are not reconstructed gameplay, animation timing, copper effects or CRT colour simulation.",
                        "RGB12 channels expand by 17; palette variants and dynamic palette changes are not inferred.",
                        "Original compositing: (background AND keep-mask) OR plane bits. Alpha is faithful only with zero bits under every keep bit.",
                        "Mask PNG: white=keep background, black=replace; sheet padding is transparent. Raw masks and indices remain authoritative.",
                        "Missing numbered variants are absent from this disk, not synthesized or claimed necessary.",
                        "OFS checks selected chains, not the full volume bitmap or allocation of unselected files.",
                        "No executable data tables, sounds, saves or campaign maps are decoded."],
        "artifacts": {name: {"size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
                      for name, data in sorted(artifacts.items())},
    }
    # Walk one component at a time: O_NOFOLLOW on a full path protects only
    # its last component. Keep descriptors pinned even if ancestors are renamed.
    # Decode before creating output; a failed write leaves an incomplete export.
    directory_flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
    with ExitStack() as descriptors:
        parent_fd = os.open(output.anchor or ".", directory_flags)
        descriptors.callback(os.close, parent_fd)
        components = output.parts[1:] if output.anchor else output.parts
        for component in components[:-1]:
            parent_fd = os.open(component, directory_flags, dir_fd=parent_fd)
            descriptors.callback(os.close, parent_fd)
        os.mkdir(output.name, mode=0o700, dir_fd=parent_fd)
        output_fd = os.open(output.name, directory_flags, dir_fd=parent_fd)
        descriptors.callback(os.close, output_fd)
        directories = {"": output_fd}
        for directory in ("raw", "graphics", "pixels"):
            os.mkdir(directory, mode=0o700, dir_fd=output_fd)
            descriptor = os.open(directory, directory_flags, dir_fd=output_fd)
            descriptors.callback(os.close, descriptor)
            directories[directory] = descriptor
        # Single-component names relative to pinned fds never resolve ancestors.
        for name, data in (*artifacts.items(), ("manifest.json", json_bytes(manifest))):
            directory, _, filename = name.rpartition("/")
            descriptor = os.open(filename, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                                 0o600, dir_fd=directories[directory])
            try:
                with os.fdopen(descriptor, "wb", closefd=False) as target:
                    target.write(data)
            finally:
                os.close(descriptor)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("adf", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        manifest = export_assets(args.adf, args.output)
    except (OSError, ValueError) as error:
        print(f"Asset extraction failed: {error}", file=sys.stderr)
        return 1
    print(f"Extracted {len(manifest['present'])} original assets to {args.output}")
    print(f"Manifest: {args.output / 'manifest.json'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
