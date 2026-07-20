# MIVF Format (observed)

The following describes fields and sizes observed in the current implementation (encode_mivf.py and the native helpers). Fields are recorded as observed; where uncertain the note "observed" is used.

Global constants

- MIVF header size: 64 bytes (HEADER_SIZE = 64)
- Stream descriptor size: 32 bytes (STREAM_DESC_SIZE = 32)
- Page header size: 32 bytes (PAGE_HEADER_SIZE = 32)
- Packet header size: 16 bytes (PACKET_HEADER_SIZE = 16)
- Page magic: ASCII `MP`
- Header magic: ASCII `MIVF`

Header

- Bytes 0–3: magic `MIVF`
- Offset 12 (4 bytes LE): number of streams (written/read by code)
- Offset 20 (8 bytes LE): duration / or other timestamp fields (observed usage varies)
- Offset 36 (8 bytes LE): first page offset (first page start after header + stream descriptors)

Stream descriptor

- Stream descriptors follow the 64-byte header. Each descriptor has an observed base size of 32 bytes plus `extra` bytes appended (the front-end writes STREAM_DESC_SIZE + len(extra) into the descriptor header length field).

Pages

- Each page starts with `MP`, a 32-byte page header, then the page payload of `payload_size` bytes.
- Page header contains (observed fields): page size, frame sequence (seq), PTS-like value, payload byte count, number of packets in page, CRC32.

Packets

- Packets live inside page payloads. Packet header size is 16 bytes (PACKET_HEADER_SIZE). Each packet contains stream id, type, header size, and packet payload length, followed by payload.

CRC/flags

- Pages may include a CRC32; flags exist for PAGE_CRC and PAGE_HAS_KEYFRAME.

Notes & cautions

- This is an implementation-derived spec. Do not treat this as a stable, formal standard; it documents what the current code reads/writes.
- If you implement a third-party parser, write defensive checks for header lengths, descriptor lengths, page payloads and CRC verification.
