"""Synthetic graphics only. Expected pixels are hand-derived, not decoder round trips."""
from contextlib import contextmanager
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


@contextmanager
def real_temporary_directory():
    """Yield a temp directory whose path has no symlink components.

    macOS puts temp files under /var/folders, and /var is a symlink.
    export_assets opens every output component with O_NOFOLLOW.
    """
    with tempfile.TemporaryDirectory() as temporary:
        yield os.path.realpath(temporary)


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
        with real_temporary_directory() as temporary:
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
            with self.subTest(boundary=boundary), real_temporary_directory() as temporary:
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
        with real_temporary_directory() as temporary:
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

    @staticmethod
    def sound_bank(measure=0, measure_length=0x1000, kit_sample=1, size_delta=0):
        """Hand-built _load_sound file: measln, 1 sequence entry, 2 kits, 2 samples."""
        data = struct.pack(">64H", *([48] + [64] * 63)) + struct.pack(">H2H", 1, measure, 1)
        data += struct.pack(">I", measure_length) + b"B" + b" " * (measure_length - 1)
        data += struct.pack(">H", 2) + struct.pack(">HHbbH", 400, 6, 64, 0, 0)
        data += struct.pack(">HHbbH", 300, 4, 20, kit_sample, 2)
        data += struct.pack(">H", 2) + struct.pack(">HHI", 6, 6, 0xdeadbeef) + struct.pack(">HHI", 4, 0, 0)
        data += struct.pack(">I", 10) + bytes(range(10)) + b"\x1b\x39"
        return data + bytes(size_delta) if size_delta >= 0 else data[:size_delta]

    def test_sound_bank_layout_and_strict_rejections(self):
        bank = self.module.sound_values(self.sound_bank())
        self.assertEqual(bank["measln"][0], 48)
        self.assertEqual(bank["sequence"], [[0, 1]])
        self.assertEqual(bank["measures_offset"], 128 + 2 + 4 + 4)
        self.assertEqual(bank["no_sounds"], 2)
        self.assertEqual(bank["drumkit"][1], {"sound": 1, "period": 300, "length": 4, "volume": 20,
                                              "sample": 1, "repeat_word": 2})
        # File pointers are ignored; offsets follow the lengths (asm:20823-20843).
        self.assertEqual([sam["offset"] for sam in bank["samples"]], [0, 6])
        self.assertEqual(bank["all_sample_length"], 10)
        self.assertEqual(self.module.sound_values(self.sound_bank(measure_length=0x1800))["measure_length"], 0x1800)
        for broken in (self.sound_bank(size_delta=1), self.sound_bank(size_delta=-1),
                       self.sound_bank(measure=16), self.sound_bank(measure_length=0x800),
                       self.sound_bank(kit_sample=2), self.sound_bank()[:100]):
            with self.assertRaises(ValueError):
                self.module.sound_values(broken)

    @staticmethod
    def popsam(rate=8000, size_delta=0):
        """Hand-built title sample file: 8 x (BE32 length, BE16 rate, PCM)."""
        data = b"".join(struct.pack(">IH", 2 + index, 10000 if index == 0 else rate) + bytes(range(2 + index))
                        for index in range(8))
        return data + bytes(size_delta) if size_delta >= 0 else data[:size_delta]

    def test_popsam_layout_and_strict_rejections(self):
        title = self.module.popsam_values(self.popsam())
        # SUB_2F9C asm:2650-2662: data follows each 6-byte header; DIVU 0x369e99/rate.
        self.assertEqual([sample["offset"] for sample in title["samples"]][:3], [6, 14, 23])
        self.assertEqual(title["samples"][0], {"sample": 0, "offset": 6, "length": 2, "rate_hz": 10000,
                                               "period_base": 357})
        self.assertEqual(title["samples"][7]["period_base"], 0x369e99 // 8000)
        for broken in (self.popsam(size_delta=1), self.popsam(size_delta=-1), self.popsam(rate=0),
                       self.popsam(rate=54), self.popsam()[:20], self.popsam(size_delta=0x1c01e)):
            with self.assertRaises(ValueError):
                self.module.popsam_values(broken)

    def test_export_popsam_raw_and_values(self):
        payload = self.popsam()
        image = make_file_disk(payload)
        image[900 * 512 + 432:900 * 512 + 440] = b"\x06popsam\0"
        checksum(image, 900)
        with real_temporary_directory() as temporary:
            root = Path(temporary)
            adf = root / "source.adf"
            adf.write_bytes(image)
            manifest = self.module.export_assets(adf, root / "out")
            entry = manifest["present"]["popsam"]
            self.assertEqual((entry["status"], entry["sample_count"]), ("verified", 8))
            self.assertEqual((root / "out" / "raw" / "popsam").read_bytes(), payload)
            values = json.loads((root / "out" / "values.json").read_text())
            self.assertEqual(values["sounds"]["popsam"]["samples"][1]["rate_hz"], 8000)

    def test_export_sound_bank_raw_and_values(self):
        payload = self.sound_bank()
        image = make_file_disk(payload)
        image[900 * 512 + 432:900 * 512 + 441] = b"\x07gmusic1\0"
        checksum(image, 900)
        with real_temporary_directory() as temporary:
            root = Path(temporary)
            adf = root / "source.adf"
            adf.write_bytes(image)
            manifest = self.module.export_assets(adf, root / "out")
            entry = manifest["present"]["gmusic1"]
            self.assertEqual((entry["status"], entry["no_sounds"], entry["sample_count"]), ("verified", 2, 2))
            self.assertEqual((root / "out" / "raw" / "gmusic1").read_bytes(), payload)
            values = json.loads((root / "out" / "values.json").read_text())
            self.assertEqual(values["sounds"]["gmusic1"]["drumkit"][0]["period"], 400)
            self.assertIn("gwords", manifest["missing"])

    def test_mouth_frames_rows_planes_and_msb_first(self):
        # _draw_mouth: frame*0x348, per row 6 bytes of plane 0, 1, 2, 3.
        data = bytearray(5040)
        data[0] = 0x80                      # frame 0 row 0 plane 0 pixel 0
        data[18 + 5] = 0x01                 # frame 0 row 0 plane 3 pixel 47
        data[840 + 2 * 24 + 6 + 2] = 0x40   # frame 1 row 2 plane 1 pixel 17
        data[5 * 840 + 34 * 24 + 12] = 0xff  # frame 5 row 34 plane 2 pixels 0..7
        frames = self.module.decode_mouths(bytes(data))
        self.assertEqual(len(frames), 6)
        self.assertTrue(all(len(frame) == 48 * 35 for frame in frames))
        self.assertEqual((frames[0][0], frames[0][47], frames[0][1]), (1, 8, 0))
        self.assertEqual((frames[1][2 * 48 + 17], frames[1][2 * 48 + 16]), (2, 0))
        self.assertEqual(frames[5][34 * 48:34 * 48 + 9], bytes([4] * 8 + [0]))
        self.assertEqual(sum(map(any, frames)), 3)
        for size in (5039, 5041):
            with self.assertRaises(ValueError):
                self.module.decode_mouths(bytes(size))

    def test_export_mouths_without_lord_palette(self):
        payload = bytearray(5040)
        payload[0] = 0xc0
        image = make_file_disk(bytes(payload))
        image[900 * 512 + 432:900 * 512 + 443] = b"\x0amouths.pic"
        checksum(image, 900)
        with real_temporary_directory() as temporary:
            root = Path(temporary)
            adf = root / "source.adf"
            adf.write_bytes(image)
            manifest = self.module.export_assets(adf, root / "out")
            entry = manifest["present"]["mouths.pic"]
            self.assertEqual((entry["status"], entry["frame_count"], entry["frame_width"], entry["frame_height"]),
                             ("verified", 6, 48, 35))
            self.assertIn("lord.pic palette", entry["limitations"][0])
            indices = (root / "out" / "pixels" / "mouths.pic.indices.u8").read_bytes()
            self.assertEqual(indices[:3], bytes([1, 1, 0]))
            self.assertEqual(len(indices), 6 * 48 * 35)
            self.assertFalse((root / "out" / "graphics" / "mouths.pic.png").exists())

    def test_cli_unknown_picture_variant_keeps_raw_and_marks_unsupported(self):
        image = make_file_disk(b"unknown")
        image[900 * 512 + 433:900 * 512 + 441] = b"load.pic"
        checksum(image, 900)
        with real_temporary_directory() as temporary:
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
