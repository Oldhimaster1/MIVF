import struct
import tempfile
import unittest
from pathlib import Path

import encode_mivf


HEADER_SIZE = 64
STREAM_DESC_SIZE = 32
PAGE_HEADER_SIZE = 32


def make_test_mivf(path: Path, frame_count: int = 2) -> bytes:
    desc = struct.pack(
        "<BBH4sIIHHHHIBBH",
        0,
        1,
        STREAM_DESC_SIZE,
        b"M2Y0",
        1,
        30,
        16,
        16,
        30,
        1,
        0,
        0,
        0,
        0,
    )
    first_page_offset = HEADER_SIZE + len(desc)
    header = struct.pack(
        "<4sHHIIIQIIQQIII",
        b"MIVF",
        0,
        12,
        HEADER_SIZE,
        1,
        30000,
        frame_count * 30000 // 30,
        1,
        4096,
        first_page_offset,
        0,
        0,
        0,
        0,
    )

    pages = bytearray()
    for frame_no in range(frame_count):
        payload = b"payload" + str(frame_no).encode()
        page = struct.pack(
            "<2sBBIQIHHII",
            b"MP",
            PAGE_HEADER_SIZE,
            0,
            frame_no,
            frame_no * 30000 // 30,
            len(payload),
            1,
            0,
            0x1234,
            0,
        )
        pages += page + payload

    return header + desc + pages


class EncodeMivfTests(unittest.TestCase):
    def test_count_mivf_frames_and_first_page_offset(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / "tiny.mivf"
            path.write_bytes(make_test_mivf(path, frame_count=3))

            self.assertEqual(encode_mivf.read_mivf_first_page_offset(path), HEADER_SIZE + STREAM_DESC_SIZE)
            self.assertEqual(encode_mivf.count_mivf_frames(path), 3)


if __name__ == "__main__":
    unittest.main()
