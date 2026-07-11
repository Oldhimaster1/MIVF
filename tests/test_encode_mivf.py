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


    def test_parse_rational_frame_rate(self) -> None:
        self.assertEqual(encode_mivf.parse_frame_rate("24"), encode_mivf.Fraction(24, 1))
        self.assertEqual(encode_mivf.parse_frame_rate("24000/1001"), encode_mivf.Fraction(24000, 1001))
        self.assertEqual(encode_mivf.parse_frame_rate("48000/2002"), encode_mivf.Fraction(24000, 1001))

    def test_rational_ticks_and_audio_packet_size(self) -> None:
        self.assertEqual(encode_mivf.frame_to_ticks(24000, 24000, 1001), 30030000)
        self.assertEqual(encode_mivf.fixed_audio_samples_per_frame(48000, 24000, 1001), 2002)
        self.assertEqual(encode_mivf.fixed_audio_samples_per_frame(48000, 24, 1), 2000)
        self.assertEqual(encode_mivf.fixed_audio_samples_per_frame(48000, 30, 1), 1600)
        with self.assertRaises(SystemExit):
            encode_mivf.fixed_audio_samples_per_frame(44100, 24000, 1001)

    def test_patch_video_timing_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / "rational.mivf"
            path.write_bytes(make_test_mivf(path, frame_count=3))
            encode_mivf.patch_video_timing_metadata(path, 24000, 1001)
            data = path.read_bytes()
            first = struct.unpack_from("<Q", data, 36)[0]
            self.assertEqual(struct.unpack_from("<HH", data, HEADER_SIZE + 20), (24000, 1001))
            self.assertEqual(struct.unpack_from("<Q", data, 20)[0], encode_mivf.frame_to_ticks(3, 24000, 1001))
            pos = first
            for frame in range(3):
                self.assertEqual(struct.unpack_from("<Q", data, pos + 8)[0], encode_mivf.frame_to_ticks(frame, 24000, 1001))
                payload = struct.unpack_from("<I", data, pos + 16)[0]
                pos += PAGE_HEADER_SIZE + payload


if __name__ == "__main__":
    unittest.main()
