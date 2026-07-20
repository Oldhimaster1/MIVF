/*
 * m2y2_transcode.c - lossless M2Y1 -> M2Y2 container transcoder.
 *
 * M2Y2 is an ADDITIVE codec tag: it is byte-for-byte the same M2Y1 token
 * stream, but each video packet's concatenated plane payload is compressed
 * with the shared order-1 range coder (source/mivf_rc.h). The model is reset
 * per packet, so every video packet is independently decodable (random-access
 * safe, no cross-packet state). The player range-decodes the payload back to
 * the exact M2Y1 bytes and feeds them to the unchanged dec_m2y1_plane, so the
 * decoded pixels are identical to M2Y1 -> identical quality, smaller file.
 *
 * This tool SELF-VERIFIES: every converted packet is immediately range-decoded
 * and compared byte-for-byte against the original payload. If any packet fails,
 * the tool reports NO and exits non-zero.
 *
 * Build: gcc -O2 -o tools/m2y2_transcode.exe tools/m2y2_transcode.c
 * Usage: tools/m2y2_transcode.exe in.mivf out.mivf
 *
 * M2Y2 video body layout (32-byte header, then the compressed blob):
 *   +0  "M2Y2"
 *   +4  w   u16        (copied from M2Y1)
 *   +6  h   u16        (copied from M2Y1)
 *   +8  frame u32      (copied)
 *   +12 keyframe u8    (copied)   +13 0   +14 u16 0
 *   +16 y_raw  u32     (decompressed Y plane payload size = M2Y1 y_payload)
 *   +20 cb_raw u32
 *   +24 cr_raw u32
 *   +28 comp   u32     (compressed size of concatenated [Y][Cb][Cr])
 *   +32 [comp bytes range-coded]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../source/mivf_rc.h"

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

static u16 le16(const u8 *b) { return (u16)(b[0] | (b[1] << 8)); }
static u32 le32(const u8 *b) {
    return (u32)b[0] | ((u32)b[1] << 8) | ((u32)b[2] << 16) | ((u32)b[3] << 24);
}
static u64 le64(const u8 *b) {
    u64 v = 0;
    for (int i = 0; i < 8; i++) v |= (u64)b[i] << (8 * i);
    return v;
}
static void wr32(u8 *b, u32 v) { for (int i = 0; i < 4; i++) b[i] = (u8)((v >> (8 * i)) & 255); }

/* CRC32 (same polynomial as the encoder's write_page). Player ignores it, but
 * we keep it correct so the file stays valid for every other tool. */
static u32 crc_tbl[256];
static int crc_ready = 0;
static void crc_init(void) {
    for (u32 i = 0; i < 256; i++) {
        u32 c = i;
        for (int j = 0; j < 8; j++) c = (c & 1) ? 0xedb88320u ^ (c >> 1) : (c >> 1);
        crc_tbl[i] = c;
    }
    crc_ready = 1;
}
static u32 crc32_calc(const u8 *d, size_t n) {
    if (!crc_ready) crc_init();
    u32 c = 0xffffffffu;
    for (size_t i = 0; i < n; i++) c = crc_tbl[(c ^ d[i]) & 255] ^ (c >> 8);
    return c ^ 0xffffffffu;
}

typedef struct { u8 *d; size_t n, cap; } Buf;
static void bput(Buf *b, const u8 *p, size_t n) {
    if (b->n + n > b->cap) {
        size_t nc = b->cap ? b->cap : (1u << 20);
        while (nc < b->n + n) nc *= 2;
        b->d = (u8 *)realloc(b->d, nc);
        if (!b->d) { fprintf(stderr, "OOM\n"); exit(1); }
        b->cap = nc;
    }
    memcpy(b->d + b->n, p, n);
    b->n += n;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s in.mivf out.mivf\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("open in"); return 1; }
    fseek(f, 0, SEEK_END);
    long fl = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fl < 64) { fprintf(stderr, "too small\n"); return 1; }
    u8 *buf = (u8 *)malloc((size_t)fl);
    if (!buf || fread(buf, 1, (size_t)fl, f) != (size_t)fl) { fprintf(stderr, "read fail\n"); return 1; }
    fclose(f);
    if (memcmp(buf, "MIVF", 4)) { fprintf(stderr, "not a MIVF file\n"); return 1; }

    u32 streams = le32(buf + 28);
    u64 first   = le64(buf + 36);
    if (first < 64 || first > (u64)fl) { fprintf(stderr, "bad first-page offset\n"); return 1; }

    /* Walk stream descriptors; patch the video stream's codec M2Y1 -> M2Y2. */
    int video_sid = -1;
    {
        int walked = 1;
        size_t p = 64;
        for (u32 i = 0; i < streams; i++) {
            if (p + 4 > first) { walked = 0; break; }
            u16 hs = le16(buf + p + 2);
            if (hs < 16 || p + hs > first) { walked = 0; break; }
            p += hs;
        }
        size_t stride = (streams && (first - 64) >= streams) ? (size_t)((first - 64) / streams) : 0;
        p = 64;
        for (u32 i = 0; i < streams; i++) {
            size_t hs = walked ? le16(buf + p + 2) : stride;
            if (hs < 16 || p + hs > first) break;
            u8 type = buf[p + 1];
            if (type == 1 && !memcmp(buf + p + 4, "M2Y1", 4)) {
                video_sid = buf[p];
                memcpy(buf + p + 4, "M2Y2", 4);
                if (hs >= 36 && !memcmp(buf + p + 32, "M2Y1", 4)) memcpy(buf + p + 32, "M2Y2", 4);
            }
            p += hs;
        }
    }
    if (video_sid < 0) { fprintf(stderr, "no M2Y1 video stream found (nothing to do)\n"); return 1; }

    MivfRcO1 *menc = (MivfRcO1 *)calloc(1, sizeof(MivfRcO1));
    MivfRcO1 *mdec = (MivfRcO1 *)calloc(1, sizeof(MivfRcO1));
    if (!menc || !mdec) { fprintf(stderr, "OOM model\n"); return 1; }

    Buf out = {0};
    bput(&out, buf, (size_t)first);   /* file header + descriptors (codec patched) */

    size_t pos = (size_t)first;
    long vin = 0, vout = 0, npk = 0;
    int all_lossless = 1;
    u8 *comp = NULL; size_t comp_cap = 0;
    u8 *verify = NULL; size_t verify_cap = 0;

    while (pos + 32 <= (size_t)fl) {
        if (buf[pos] != 'M' || buf[pos + 1] != 'P') break;
        u32 payload = le32(buf + pos + 0x10);
        u16 packets = le16(buf + pos + 0x14);
        if (pos + 32 + payload > (size_t)fl) break;
        u8 *body = buf + pos + 32;

        Buf nb = {0};
        size_t off = 0;
        for (u16 pi = 0; pi < packets; pi++) {
            if (off + 16 > payload) break;
            u8  sid = body[off];
            u16 hs  = le16(body + off + 2);
            u32 ps  = le32(body + off + 8);
            if (hs != 16 || (size_t)off + hs + ps > payload) break;
            u8 *pp = body + off + hs;

            if ((int)sid == video_sid && ps >= 28 && !memcmp(pp, "M2Y1", 4)) {
                u32 yraw = le32(pp + 16), cbraw = le32(pp + 20), craw = le32(pp + 24);
                size_t rawtot = (size_t)yraw + cbraw + craw;
                if (28 + rawtot > ps) {
                    bput(&nb, body + off, (size_t)hs + ps);   /* malformed; copy as-is */
                } else {
                    const u8 *planes = pp + 28;
                    size_t need = rawtot * 2 + 4096;
                    if (comp_cap < need) { comp = (u8 *)realloc(comp, need); comp_cap = need; }
                    size_t clen = mivf_rc_o1_compress(menc, planes, rawtot, comp, comp_cap);
                    if (verify_cap < rawtot) { verify = (u8 *)realloc(verify, rawtot ? rawtot : 1); verify_cap = rawtot; }
                    mivf_rc_o1_decompress(mdec, comp, clen, verify, rawtot);
                    if (clen > comp_cap || memcmp(verify, planes, rawtot) != 0) all_lossless = 0;

                    u8 ph[16];
                    memcpy(ph, body + off, 16);
                    wr32(ph + 8, (u32)(32 + clen));      /* new packet psize */

                    u8 bh[32];
                    memset(bh, 0, 32);
                    memcpy(bh, "M2Y2", 4);
                    memcpy(bh + 4, pp + 4, 2);            /* w  */
                    memcpy(bh + 6, pp + 6, 2);            /* h  */
                    memcpy(bh + 8, pp + 8, 4);            /* frame */
                    bh[12] = pp[12];                     /* keyframe flag */
                    bh[13] = pp[13];                     /* transform keep-count */
                    wr32(bh + 16, yraw);
                    wr32(bh + 20, cbraw);
                    wr32(bh + 24, craw);
                    wr32(bh + 28, (u32)clen);

                    bput(&nb, ph, 16);
                    bput(&nb, bh, 32);
                    bput(&nb, comp, clen);

                    vin  += (long)ps;
                    vout += (long)(32 + clen);
                    npk++;
                }
            } else {
                bput(&nb, body + off, (size_t)hs + ps);   /* audio / other: verbatim */
            }
            off += (size_t)hs + ps;
        }
        /* preserve any trailing bytes within the page body */
        if (off < payload) bput(&nb, body + off, payload - off);

        u8 pgh[32];
        memcpy(pgh, buf + pos, 32);
        wr32(pgh + 0x10, (u32)nb.n);
        wr32(pgh + 0x18, crc32_calc(nb.d, nb.n));
        bput(&out, pgh, 32);
        bput(&out, nb.d, nb.n);
        free(nb.d);

        pos += 32 + payload;
    }
    /* preserve any trailing bytes after the last page (unlikely) */
    if (pos < (size_t)fl) bput(&out, buf + pos, (size_t)fl - pos);

    FILE *g = fopen(argv[2], "wb");
    if (!g) { perror("open out"); return 1; }
    fwrite(out.d, 1, out.n, g);
    fclose(g);

    printf("M2Y2 transcode: %s -> %s\n", argv[1], argv[2]);
    printf("  video packets : %ld\n", npk);
    printf("  lossless      : %s\n", all_lossless ? "YES (every packet byte-exact)" : "NO  <-- ERROR");
    printf("  video payload : %ld -> %ld bytes  (%.1f%%)\n", vin, vout, vin ? 100.0 * vout / vin : 0.0);
    printf("  file size     : %ld -> %zu bytes  (%.2f MB -> %.2f MB, saved %.1f%%)\n",
           fl, out.n, fl / 1048576.0, out.n / 1048576.0,
           fl ? 100.0 * (fl - (long)out.n) / fl : 0.0);

    free(buf); free(out.d); free(comp); free(verify); free(menc); free(mdec);
    return all_lossless ? 0 : 3;
}
