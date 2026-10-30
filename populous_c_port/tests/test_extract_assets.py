"""Synthetic graphics only. Expected pixels are hand-derived, not decoder round trips."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import os
import zlib

from test_extract_land import make_disk, make_file_disk, checksum

PORT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PORT))
TOOL = PORT / "extract_assets.py"


class AssetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.module = None
        if TOOL.exists():
            spec = importlib.util.spec_from_file_location("extract_assets", TOOL)
            cls.module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(cls.module)

    def setUp(self):
        self.assertIsNotNone(self.module, "asset extractor missing")

    def test_small_sprite_plane_order_bit_order_and_mask(self):
        data = struct.pack(">5H", 0x0fff, 0x8000, 0x4000, 0x2000, 0x1000) + bytes(150)
        frames = self.module.decode_masked(data, 16, 16, 16)
        self.assertEqual(len(frames), 1)
        self.assertEqual(frames[0]["indices"][:16], bytes([1, 2, 4, 8] + [0] * 12))
        self.assertEqual(frames[0]["keep"][:16], bytes([0] * 4 + [1] * 12))
        self.assertEqual(frames[0]["mask_conflicts"], 0)

    def test_land_uses_be32_not_two_be16_groups(self):
        data = struct.pack(">5I", 0x7ffffffe, 0x80000000, 1, 0, 0) + bytes(460)
        frame = self.module.decode_masked(data, 32, 24, 32)[0]
        self.assertEqual(frame["indices"][:32], bytes([1] + [0] * 30 + [2]))
        self.assertEqual(frame["keep"][:32], bytes([0] + [1] * 30 + [0]))

    def test_large_sprite_left_and_right_word_groups(self):
        row = struct.pack(">10H", 0, 0x8000, 0, 0, 0, 0, 0, 1, 0, 0)
        frame = self.module.decode_masked(row + bytes(620), 32, 32, 16)[0]
        self.assertEqual(frame["indices"][:32], bytes([1] + [0] * 30 + [2]))

    def test_conflicting_keep_bits_are_not_silently_transparent(self):
        data = struct.pack(">5H", 0xffff, 0x8000, 0, 0, 0) + bytes(150)
        frame = self.module.decode_masked(data, 16, 16, 16)[0]
        self.assertEqual(frame["mask_conflicts"], 1)
        pixels = self.module.rgba(frame["indices"], [(0, 0, 0), (17, 34, 51)],
                                  frame["keep"], faithful_alpha=False)
        self.assertEqual(pixels[:8], bytes([17, 34, 51, 255, 0, 0, 0, 255]))
        faithful = self.module.rgba(bytes([0, 0]), [(0, 0, 0)], bytes([0, 1]))
        self.assertEqual(faithful, bytes([0, 0, 0, 255, 0, 0, 0, 0]))

    def test_planar_picture_planes_rows_and_msb_first(self):
        self.assertEqual(self.module.decode_planar(bytes([0x80, 1, 0x40, 2]), 8, 2, 2),
                         bytes([1, 2] + [0] * 6 + [0] * 6 + [2, 1]))
        for data in (bytes(3), bytes(5)):
            with self.assertRaises(ValueError):
                self.module.decode_planar(data, 8, 2, 2)
        with self.assertRaises(ValueError):
            self.module.decode_masked(bytes(159), 16, 16, 16)

    def test_rgb12_and_values_are_big_endian(self):
        self.assertEqual(self.module.rgb12([0x123, 0xabc]), [(17, 34, 51), (170, 187, 204)])
        with self.assertRaises(ValueError):
            self.module.rgb12([0xf123])
        payload = bytearray(33714)
        struct.pack_into(">H", payload, 0, 0x1234)
        struct.pack_into(">H", payload, 2, 0x0102)
        struct.pack_into(">H", payload, 24, 0x0304)
        struct.pack_into(">H", payload, 46, 0x0506)
        struct.pack_into(">H", payload, 68, 0x0708)
        struct.pack_into(">H", payload, 90, 0x090a)
        payload[96:112] = bytes(range(16))
        struct.pack_into(">H", payload, 112, 3)
        rules = self.module.land_values(payload)
        self.assertEqual(rules["walk_death"], 0x1234)
        for name, first in (("population_add", 0x0102), ("mana_add", 0x0304),
                            ("weapons_add", 0x0506), ("battle_add1", 0x0708), ("battle_add2", 0x090a)):
            self.assertEqual(rules[name][0], first)
        self.assertEqual(rules["map_colour"], list(range(16)))
        self.assertEqual(rules["sprite_set"], 3)
        levels = self.module.level_values(bytes(range(8)) + b"\x12\x34")
        self.assertEqual(levels[0], {"record": 0, "offset": 0, "bytes_0_7": list(range(8)),
                                     "word_8_be": 0x1234})
        with self.assertRaises(ValueError):
            self.module.level_values(bytes(9))

    def test_png_chunks_crc_and_exact_scanlines(self):
        pixels = bytes([17, 34, 51, 255, 0, 0, 0, 0])
        data = self.module.png_rgba(2, 1, pixels)
        self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
        chunks = {}
        offset = 8
        while offset < len(data):
            size = struct.unpack_from(">I", data, offset)[0]
            kind = data[offset + 4:offset + 8]
            content = data[offset + 8:offset + 8 + size]
            crc = struct.unpack_from(">I", data, offset + 8 + size)[0]
            self.assertEqual(crc, zlib.crc32(kind + content))
            chunks[kind] = content
            offset += size + 12
        self.assertEqual(struct.unpack(">IIBBBBB", chunks[b"IHDR"]), (2, 1, 8, 6, 0, 0, 0))
        self.assertEqual(zlib.decompress(chunks[b"IDAT"]), b"\0" + pixels)
        self.assertEqual(chunks[b"IEND"], b"")

    def test_export_raw_integrity_manifest_and_no_overwrite(self):
        image, expected = make_disk()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            adf = root / "source.adf"
            adf.write_bytes(image)
            output = root / "export"
            manifest = self.module.export_assets(adf, output)
            self.assertEqual(adf.read_bytes(), image)
            self.assertEqual(manifest["source"]["sha256"], hashlib.sha256(image).hexdigest())
            self.assertEqual(set(manifest["present"]), set(expected))
            self.assertIn("load.pic", manifest["missing"])
            for name, payload in expected.items():
                self.assertEqual((output / "raw" / name).read_bytes(), payload)
                self.assertEqual(manifest["present"][name]["frame_count"], 70)
            for name, metadata in manifest["artifacts"].items():
                payload = (output / name).read_bytes()
                self.assertEqual(hashlib.sha256(payload).hexdigest(), metadata["sha256"])
            before = (output / "manifest.json").read_bytes()
            with self.assertRaises(FileExistsError):
                self.module.export_assets(adf, output)
            self.assertEqual((output / "manifest.json").read_bytes(), before)
            link = root / "link"
            link.symlink_to(output, target_is_directory=True)
            with self.assertRaises(FileExistsError):
                self.module.export_assets(adf, link)
            with self.assertRaises(FileNotFoundError):
                self.module.export_assets(adf, root / "missing" / "child")

    def test_directory_swaps_cannot_redirect_export(self):
        # Inject one real rename/symlink at a known mkdir boundary, not a race loop.
        for boundary in ("output", "parent", "raw", "after_directories"):
            with self.subTest(boundary=boundary), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                adf = root / "source.adf"
                image = make_file_disk(b"original raw bytes")
                image[900 * 512 + 433:900 * 512 + 441] = b"load.pic"
                checksum(image, 900)
                adf.write_bytes(image)
                parent = root / "parent"
                parent.mkdir()
                output = parent / "export"
                outside = root / "outside"
                outside.mkdir()
                if boundary == "parent":
                    (outside / "export").mkdir()
                elif boundary == "after_directories":
                    for name in ("raw", "graphics", "pixels"):
                        (outside / name).mkdir()
                saved = root / "saved"
                before = sorted(str(path.relative_to(outside)) for path in outside.rglob("*"))
                original_mkdir, original_open = os.mkdir, os.open
                swapped = False
                directory_fds = []

                def swap_after_mkdir(path, *args, **kwargs):
                    nonlocal swapped
                    original_mkdir(path, *args, **kwargs)
                    trigger = {"output": "export", "parent": "export",
                               "raw": "raw", "after_directories": "pixels"}[boundary]
                    if not swapped and Path(path).name == trigger:
                        swapped = True
                        victim = parent if boundary == "parent" else output / "raw" if boundary == "raw" else output
                        victim.rename(saved)
                        victim.symlink_to(outside, target_is_directory=True)

                def record_directory_fd(path, flags, *args, **kwargs):
                    descriptor = original_open(path, flags, *args, **kwargs)
                    if flags & os.O_DIRECTORY:
                        directory_fds.append(descriptor)
                    return descriptor

                with mock.patch.object(os, "mkdir", side_effect=swap_after_mkdir), \
                     mock.patch.object(os, "open", side_effect=record_directory_fd):
                    try:
                        self.module.export_assets(adf, output)
                    except OSError:
                        pass  # Refusal or continuing inside the anchored directory is safe.
                self.assertTrue(swapped, "the deterministic swap must actually run")
                self.assertEqual(sorted(str(path.relative_to(outside)) for path in outside.rglob("*")), before)
                for descriptor in directory_fds:
                    with self.assertRaises(OSError):
                        os.fstat(descriptor)

    def test_symlink_ancestor_is_refused_before_creating_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            adf = root / "source.adf"
            adf.write_bytes(make_file_disk(b"unselected"))
            outside = root / "outside"
            (outside / "nested").mkdir(parents=True)
            link = root / "link"
            link.symlink_to(outside, target_is_directory=True)
            with self.assertRaises(OSError):
                self.module.export_assets(adf, link / "nested" / "export")
            self.assertFalse((outside / "nested" / "export").exists())

    def test_cli_unknown_picture_variant_keeps_raw_and_marks_unsupported(self):
        image = make_file_disk(b"unknown")
        image[900 * 512 + 433:900 * 512 + 441] = b"load.pic"
        checksum(image, 900)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            adf = root / "source.adf"
            adf.write_bytes(image)
            result = subprocess.run([sys.executable, str(TOOL), str(adf), str(root / "out")],
                                    capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            manifest = json.loads((root / "out" / "manifest.json").read_text())
            self.assertEqual(manifest["present"]["load.pic"]["status"], "unsupported")
            self.assertEqual((root / "out" / "raw" / "load.pic").read_bytes(), b"unknown")
            self.assertFalse(list((root / "out" / "graphics").glob("*.png")))


if __name__ == "__main__":
    unittest.main()
