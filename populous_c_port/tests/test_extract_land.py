"""Synthetic OFS fixtures only; no commercial disk bytes belong in this test."""
import importlib.util
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

TOOL = Path(__file__).resolve().parents[1] / "extract_land.py"


def put(image, block, offset, value):
    struct.pack_into(">I", image, block * 512 + offset, value)


def checksum(image, block):
    put(image, block, 20, 0)
    total = sum(struct.unpack_from(">128I", image, block * 512))
    put(image, block, 20, -total & 0xffffffff)


def make_disk():
    image = bytearray(901120)
    image[:4] = b"DOS\0"
    put(image, 0, 8, 880)
    put(image, 880, 0, 2)
    put(image, 880, 12, 72)
    put(image, 880, 508, 1)
    expected = {}
    for land in range(4):
        header = 900 + land
        put(image, 880, 24 + 4 * land, header)
        payload = bytes([land + 1]) * 33714
        expected[f"land{land}"] = payload
        put(image, header, 0, 2)
        put(image, header, 4, header)
        put(image, header, 8, 70)
        put(image, header, 16, 1000 + land * 100)
        put(image, header, 324, len(payload))
        image[header * 512 + 432:header * 512 + 438] = bytes([5]) + f"LaNd{land}".encode()
        put(image, header, 500, 880)
        put(image, header, 508, 0xfffffffd)
        for index in range(70):
            block = 1000 + land * 100 + index
            piece = payload[index * 488:(index + 1) * 488]
            put(image, header, 308 - index * 4, block)
            for offset, value in [(0, 8), (4, header), (8, index + 1),
                                  (12, len(piece)), (16, block + 1 if index < 69 else 0)]:
                put(image, block, offset, value)
            image[block * 512 + 24:block * 512 + 24 + len(piece)] = piece
            checksum(image, block)
        checksum(image, header)
    checksum(image, 880)
    return image, expected


class ExtractLandTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if TOOL.exists():
            spec = importlib.util.spec_from_file_location("extract_land", TOOL)
            cls.module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(cls.module)
        else:
            cls.module = None

    def setUp(self):
        self.assertIsNotNone(self.module, "OFS extractor is not implemented")
        self.image, self.expected = make_disk()

    def reject(self):
        with self.assertRaises(ValueError):
            self.module.extract_land_files(bytes(self.image))

    def test_noncontiguous_data_and_case_insensitive_names(self):
        self.assertEqual(self.module.extract_land_files(bytes(self.image)), self.expected)

    def test_checksums_and_format(self):
        for block in (880, 900, 1000, 1069):
            with self.subTest(block=block):
                self.image, _ = make_disk()
                self.image[block * 512 + 100] ^= 1
                self.reject()
        self.image, _ = make_disk()
        self.image[3] = 1
        self.reject()
        self.image, _ = make_disk()
        self.image.pop()
        self.reject()

    def test_metadata_validation_even_with_valid_checksum(self):
        cases = [(880, 0, 3), (880, 12, 71), (880, 508, 2),
                 (880, 24, 1760), (900, 4, 901), (900, 500, 42),
                 (900, 508, 2), (900, 8, 69), (900, 324, 33713),
                 (900, 504, 1500), (900, 308, 1001),
                 (1000, 0, 7), (1000, 4, 901), (1000, 8, 2),
                 (1000, 12, 489), (1000, 12, 0), (1000, 16, 0),
                 (1000, 16, 1760), (1069, 12, 41)]
        for block, offset, value in cases:
            with self.subTest(block=block, offset=offset, value=value):
                self.image, _ = make_disk()
                put(self.image, block, offset, value)
                checksum(self.image, block)
                self.reject()

    def test_directory_and_data_cycles(self):
        for block, offset, value in [(900, 496, 900), (1000, 16, 1000), (1069, 16, 1000)]:
            with self.subTest(block=block):
                self.image, _ = make_disk()
                put(self.image, block, offset, value)
                checksum(self.image, block)
                self.reject()

    def test_missing_duplicate_and_unsafe_names(self):
        for name in (b"land9", b"land1", b"../x0", b"la/d0", b"la\\d0"):
            with self.subTest(name=name):
                self.image, _ = make_disk()
                self.image[900 * 512 + 433:900 * 512 + 438] = name
                checksum(self.image, 900)
                self.reject()

    def test_cli_extracts_only_four_files_and_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            adf = root / "synthetic.adf"
            adf.write_bytes(self.image)
            output = root / "output"
            command = [sys.executable, str(TOOL), str(adf), str(output)]
            result = subprocess.run(command, capture_output=True, text=True, timeout=5)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual({path.name: path.read_bytes() for path in output.iterdir()}, self.expected)
            result = subprocess.run(command, capture_output=True, text=True, timeout=5)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual((output / "land0").read_bytes(), self.expected["land0"])
            (output / "land0").unlink()
            outside = root / "outside"
            outside.write_bytes(b"do not overwrite")
            (output / "land0").symlink_to(outside)
            result = subprocess.run(command, capture_output=True, text=True, timeout=5)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(outside.read_bytes(), b"do not overwrite")


if __name__ == "__main__":
    unittest.main()
