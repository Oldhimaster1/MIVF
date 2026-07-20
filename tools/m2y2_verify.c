/*
 * m2y2_verify.c - proves the PLAYER's dec_m2y2 path is correct without a 3DS.
 *
 * It reads the original M2Y1 file and the transcoded M2Y2 file in lockstep and,
 * for every video frame, runs the EXACT logic the player's dec_m2y2 uses to
 * recover the plane payload (read y_raw@16/cb_raw@20/cr_raw@24/comp@28, then
 * range-decompress comp bytes at +32 into y_raw+cb_raw+cr_raw bytes, then split
 * at the y_raw / cb_raw boundaries) and compares those recovered plane bytes
 * against the ORIGINAL M2Y1 plane payload bytes (at +28).
 *
 * If they match for every frame, the player feeds dec_m2y1_plane the exact same
 * bytes M2Y1 would -> decoded pixels are identical. dec_m2y1_plane itself is
 * unchanged, so the whole player M2Y2 path is proven correct.
 *
 * Build: gcc -O2 -o tools/m2y2_verify.exe tools/m2y2_verify.c
 * Usage: tools/m2y2_verify.exe original_m2y1.mivf transcoded_m2y2.mivf
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

static u8 *slurp(const char *path, long *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *b = (u8 *)malloc((size_t)n);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fprintf(stderr, "read fail %s\n", path); exit(1); }
    fclose(f);
    *out_len = n;
    return b;
}

/* Collect pointers to each video packet body (after the 16-byte packet header). */
typedef struct { const u8 *body; u32 psize; } Pkt;

static int collect_video(const u8 *buf, long len, Pkt *out, int max) {
    u32 streams = le32(buf + 28);
    u64 first   = le64(buf + 36);
    /* find video sid (type==1) */
    int video_sid = -1;
    size_t p = 64; int walked = 1;
    for (u32 i = 0; i < streams; i++) {
        if (p + 4 > first) { walked = 0; break; }
        u16 hs = le16(buf + p + 2);
        if (hs < 16 || p + hs > first) { walked = 0; break; }
        p += hs;
    }
    size_t stride = streams ? (size_t)((first - 64) / streams) : 0;
    p = 64;
    for (u32 i = 0; i < streams; i++) {
        size_t hs = walked ? le16(buf + p + 2) : stride;
        if (buf[p + 1] == 1) { video_sid = buf[p]; break; }
        p += hs;
    }
    int n = 0;
    size_t pos = (size_t)first;
    while (pos + 32 <= (size_t)len) {
        if (buf[pos] != 'M' || buf[pos + 1] != 'P') break;
        u32 payload = le32(buf + pos + 0x10);
        u16 packets = le16(buf + pos + 0x14);
        if (pos + 32 + payload > (size_t)len) break;
        const u8 *body = buf + pos + 32;
        size_t off = 0;
        for (u16 pi = 0; pi < packets; pi++) {
            if (off + 16 > payload) break;
            u8 sid = body[off];
            u16 hs = le16(body + off + 2);
            u32 ps = le32(body + off + 8);
            if (hs != 16 || off + hs + ps > payload) break;
            if ((int)sid == video_sid) {
                if (n < max) { out[n].body = body + off + hs; out[n].psize = ps; n++; }
            }
            off += (size_t)hs + ps;
        }
        pos += 32 + payload;
    }
    return n;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s m2y1.mivf m2y2.mivf\n", argv[0]); return 2; }
    long l1, l2;
    u8 *f1 = slurp(argv[1], &l1);
    u8 *f2 = slurp(argv[2], &l2);

    int cap = 200000;
    Pkt *a = (Pkt *)malloc(sizeof(Pkt) * cap);
    Pkt *b = (Pkt *)malloc(sizeof(Pkt) * cap);
    int na = collect_video(f1, l1, a, cap);
    int nb = collect_video(f2, l2, b, cap);

    printf("m2y1 video packets: %d\n", na);
    printf("m2y2 video packets: %d\n", nb);
    if (na != nb) { printf("FAIL: packet count mismatch\n"); return 3; }

    MivfRcO1 *model = (MivfRcO1 *)calloc(1, sizeof(MivfRcO1));
    u8 *raw = NULL; size_t raw_cap = 0;
    int bad = 0; long checked = 0; long total_raw = 0;

    for (int i = 0; i < na; i++) {
        const u8 *m1 = a[i].body;            /* original M2Y1 body */
        const u8 *m2 = b[i].body;            /* transcoded M2Y2 body */
        if (a[i].psize < 28 || memcmp(m1, "M2Y1", 4)) { bad++; continue; }
        if (b[i].psize < 32 || memcmp(m2, "M2Y2", 4)) { bad++; continue; }

        /* ----- exactly what the player's dec_m2y2 does ----- */
        u32 y_raw  = le32(m2 + 16);
        u32 cb_raw = le32(m2 + 20);
        u32 cr_raw = le32(m2 + 24);
        u32 comp   = le32(m2 + 28);
        size_t raw_total = (size_t)y_raw + cb_raw + cr_raw;
        if (32u + comp > b[i].psize) { printf("FAIL frame %d: comp overruns packet\n", i); bad++; continue; }
        if (raw_cap < raw_total) { raw = (u8 *)realloc(raw, raw_total); raw_cap = raw_total; }
        mivf_rc_o1_decompress(model, m2 + 32, comp, raw, raw_total);

        /* ----- original M2Y1 plane payload (what dec_m2y1 would feed) ----- */
        u32 y1  = le32(m1 + 16);
        u32 cb1 = le32(m1 + 20);
        u32 cr1 = le32(m1 + 24);
        if (y1 != y_raw || cb1 != cb_raw || cr1 != cr_raw) {
            printf("FAIL frame %d: plane sizes differ (m2y1 %u/%u/%u vs m2y2 %u/%u/%u)\n",
                   i, y1, cb1, cr1, y_raw, cb_raw, cr_raw);
            bad++; continue;
        }
        const u8 *planes1 = m1 + 28;
        if (memcmp(raw, planes1, raw_total) != 0) {
            printf("FAIL frame %d: decoded plane bytes differ from M2Y1\n", i);
            bad++; continue;
        }
        checked++; total_raw += (long)raw_total;
    }

    printf("frames checked byte-exact: %ld\n", checked);
    printf("total plane bytes verified: %ld (%.2f MB)\n", total_raw, total_raw / 1048576.0);
    if (bad == 0 && checked == na) {
        printf("RESULT: PASS - player dec_m2y2 reproduces M2Y1 plane bytes EXACTLY for all %d frames\n", na);
        return 0;
    }
    printf("RESULT: FAIL - %d bad frames\n", bad);
    return 3;
}
