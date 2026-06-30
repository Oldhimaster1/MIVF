/* HFIX54A_LUMA_PRIORITY_ENCODER */
/* HFIX32_DYNAMIC_QP_ENCODER */
/*
    miv2y_fastest_c.c

    HFIX26 native C M2Y1 encoder.

    Features:
      - Raw planar yuv420p input.
      - M2Y1 video-only MIVF output.
      - Closed-loop reconstructed references.
      - Modes:
          0 SKIP
          1 RAW
          2 DELTA
          3 SOLID
          4 RUN_SKIP
          5 QRES
          6 MVCOPY
      - QRES:
          mode + global signed delta + 16 signed 2x2 residuals.
      - MVCOPY:
          mode + signed mx + signed my.
      - Center-biased, early-exit full-pel motion search.
      - One MIVF page per frame, one video packet per page.

    Build:
      gcc -O3 -std=c11 -Wall -Wextra -o miv2y_ultra_fast_c.exe miv2y_fastest_c.c

    Example:
      ./miv2y_ultra_fast_c.exe \
        --input m2y0_raw_256x144_10s.yuv \
        --output native_m2y1_10s.mivf \
        --width 256 \
        --height 144 \
        --fps 30 \
        --keyint 60 \
        --y-skip 8 \
        --y-delta 16 \
        --y-solid 10 \
        --y-qres 360 \
        --c-skip 12 \
        --c-delta 22 \
        --c-solid 32 \
        --c-qres 300 \
        --mv-range 8 \
        --y-mv-thresh 768 \
        --c-mv-thresh 1024
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;

enum {
    M_SKIP     = 0,
    M_RAW      = 1,
    M_DELTA    = 2,
    M_SOLID    = 3,
    M_RUN_SKIP = 4,
    M_QRES     = 5,
    M_MVCOPY   = 6,
    M_MVQRES  = 7,
    M_TRANSFORM   = 8,
    M_MVTRANSFORM = 9,
    M_QRESZ         = 10,
    M_MVQRESZ       = 11,
    M_TRANSFORMZ    = 12,
    M_MVTRANSFORMZ  = 13,
    M_MVCOPYP       = 14,
    M_MVQRESP       = 15,
    M_MVTRANSFORMP  = 16,
    M_MVQRESZP      = 17,
    M_MVTRANSFORMZP = 18,
    M_GMVCOPY       = 19,
    M_SET_BASE_DQP = 20,
    M_GMVCOPY_INTERNAL = 1001
};

enum {
    HEADER_SIZE = 64,
    STREAM_DESC_SIZE = 32,
    PAGE_HEADER_SIZE = 32,
    PACKET_HEADER_SIZE = 16,

    PAGE_CRC = 1,
    PAGE_HAS_KEYFRAME = 2,

    PKT_KEYFRAME = 1,
    PKT_FRAME_START = 2,
    PKT_FRAME_END = 4
};

typedef struct {
    u8 *data;
    size_t size;
    size_t cap;
} Buf;

typedef struct {
    u32 skip;
    u32 delta;
    u32 solid;
    u32 mv;
    u32 mvqres;
    u32 qres;
    u32 tr;
    u32 mvtr;
    u32 qz;
    u32 mvqz;
    u32 trz;
    u32 mvtrz;
    u32 raw;
    u32 run_tokens;
    u32 run_blocks;
} PlaneStats;

typedef struct {
    Buf payload;
    PlaneStats stats;
    int pending_skip;
} PlaneEnc;

typedef struct {
    int count;
    s8 *mx;
    s8 *my;
} MVList;

typedef struct {
    int w;
    int h;
    int fps;
    int keyint;

    int y_skip;
    int y_delta;
    int y_solid;
    int y_qres;

    int c_skip;
    int c_delta;
    int c_solid;
    int c_qres;

    int mv_range;
    int y_mv_thresh;
    int c_mv_thresh;

    double lambda;
    double c_lambda;
    int qp;
    int c_qp_offset;
} EncParams;

static void die(const char *msg) {
    fprintf(stderr, "ERROR: %s\n", msg);
    exit(1);
}

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static u8 clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (u8)v;
}

static s8 clamp_s8(int v) {
    if (v < -128) return -128;
    if (v > 127) return 127;
    return (s8)v;
}

static int round_div_int(int num, int den) {
    if (den <= 0) return 0;

    if (num >= 0) {
        return (num + den / 2) / den;
    } else {
        return -((-num + den / 2) / den);
    }
}

static void buf_init(Buf *b, size_t cap) {
    b->data = (u8*)malloc(cap ? cap : 1);
    if (!b->data) die("out of memory");
    b->size = 0;
    b->cap = cap ? cap : 1;
}

static void buf_free(Buf *b) {
    free(b->data);
    b->data = NULL;
    b->size = 0;
    b->cap = 0;
}

static void buf_reserve(Buf *b, size_t need) {
    if (need <= b->cap) return;

    size_t nc = b->cap;
    while (nc < need) {
        nc *= 2;
        if (nc < b->cap) die("buffer overflow");
    }

    u8 *p = (u8*)realloc(b->data, nc);
    if (!p) die("out of memory");

    b->data = p;
    b->cap = nc;
}

static void buf_put_u8(Buf *b, u8 v) {
    buf_reserve(b, b->size + 1);
    b->data[b->size++] = v;
}

static void buf_put_bytes(Buf *b, const void *p, size_t n) {
    buf_reserve(b, b->size + n);
    memcpy(b->data + b->size, p, n);
    b->size += n;
}

static void wr_u16le(u8 *p, u16 v) {
    p[0] = (u8)(v & 255);
    p[1] = (u8)((v >> 8) & 255);
}

static void wr_u32le(u8 *p, u32 v) {
    p[0] = (u8)(v & 255);
    p[1] = (u8)((v >> 8) & 255);
    p[2] = (u8)((v >> 16) & 255);
    p[3] = (u8)((v >> 24) & 255);
}

static void wr_u64le(u8 *p, u64 v) {
    wr_u32le(p, (u32)(v & 0xffffffffu));
    wr_u32le(p + 4, (u32)(v >> 32));
}

/* ------------------------------------------------------------------------- */
/* CRC32                                                                     */
/* ------------------------------------------------------------------------- */

static u32 crc32_table[256];
static bool crc32_ready = false;

static void crc32_init(void) {
    if (crc32_ready) return;

    for (u32 i = 0; i < 256; i++) {
        u32 c = i;

        for (int j = 0; j < 8; j++) {
            if (c & 1) {
                c = 0xedb88320u ^ (c >> 1);
            } else {
                c >>= 1;
            }
        }

        crc32_table[i] = c;
    }

    crc32_ready = true;
}

static u32 crc32_calc(const u8 *data, size_t n) {
    crc32_init();

    u32 c = 0xffffffffu;

    for (size_t i = 0; i < n; i++) {
        c = crc32_table[(c ^ data[i]) & 255] ^ (c >> 8);
    }

    return c ^ 0xffffffffu;
}

/* ------------------------------------------------------------------------- */
/* CLI                                                                       */
/* ------------------------------------------------------------------------- */

static const char *arg_str(int argc, char **argv, const char *name, const char *def) {
    for (int i = 1; i + 1 < argc; i++) {
        if (!strcmp(argv[i], name)) {
            return argv[i + 1];
        }
    }

    return def;
}

static int arg_int(int argc, char **argv, const char *name, int def) {
    const char *s = arg_str(argc, argv, name, NULL);
    if (!s) return def;
    return atoi(s);
}

static double arg_double(int argc, char **argv, const char *name, double def) {
    const char *v = arg_str(argc, argv, name, NULL);

    if (!v) {
        return def;
    }

    return atof(v);
}


static void usage(void) {
    printf(
        "usage:\n"
        "  miv2y_ultra_fast_c.exe --input in.yuv --output out.mivf --width 256 --height 144 [options]\n"
        "\n"
        "options:\n"
        "  --fps N              default 30\n"
        "  --keyint N           default 60\n"
        "  --y-skip N           default 8\n"
        "  --y-delta N          default 16\n"
        "  --y-solid N          default 10\n"
        "  --y-qres N           default 360\n"
        "  --c-skip N           default 12\n"
        "  --c-delta N          default 22\n"
        "  --c-solid N          default 32\n"
        "  --c-qres N           default 300\n"
        "  --mv-range N         default 8\n"
        "  --y-mv-thresh N      default 768\n"
        "  --c-mv-thresh N      default 1024\n"
        "  --lambda N          RDO lambda, default 4.0; higher = smaller/lossier\n"
        "  --c-lambda N        chroma RDO lambda, default matches luma lambda\n"
        "  --qp N              transform quantizer, 1..51, default 28\n  --c-qp-offset N     chroma transform QP offset, default 4 in HFIX54A\n"
    );
}

/* ------------------------------------------------------------------------- */
/* MIVF writer                                                               */
/* ------------------------------------------------------------------------- */

static void write_header(FILE *f, int streams, u64 duration, u64 first) {
    u8 h[64];
    memset(h, 0, sizeof(h));

    memcpy(h + 0, "MIVF", 4);
    wr_u16le(h + 4, 0);
    wr_u16le(h + 6, 12);
    wr_u32le(h + 8, HEADER_SIZE);
    wr_u32le(h + 12, (u32)streams);
    wr_u32le(h + 16, 30000);
    wr_u64le(h + 20, duration);
    wr_u32le(h + 28, 1);
    wr_u32le(h + 32, 4096);
    wr_u64le(h + 36, first);
    wr_u64le(h + 44, 0);
    wr_u32le(h + 52, 0);
    wr_u32le(h + 56, 0);
    wr_u32le(h + 60, 0);

    if (fwrite(h, 1, sizeof(h), f) != sizeof(h)) {
        die("failed to write header");
    }
}

static void write_stream_desc_m2y1(FILE *f, int w, int h, int fps) {
    u8 d[48];
    memset(d, 0, sizeof(d));

    d[0] = 0;                /* stream id */
    d[1] = 1;                /* video */
    wr_u16le(d + 2, 48);
    memcpy(d + 4, "M2Y1", 4);
    wr_u32le(d + 8, 1);
    wr_u32le(d + 12, (u32)fps);
    wr_u16le(d + 16, (u16)w);
    wr_u16le(d + 18, (u16)h);
    wr_u16le(d + 20, (u16)fps);
    wr_u16le(d + 22, 1);
    wr_u32le(d + 24, 0);
    d[28] = 0;
    d[29] = 0;
    wr_u16le(d + 30, 16);

    memcpy(d + 32, "M2Y1", 4);
    wr_u16le(d + 36, (u16)w);
    wr_u16le(d + 38, (u16)h);
    wr_u32le(d + 40, (u32)(w * h));
    wr_u32le(d + 44, (u32)((w / 2) * (h / 2)));

    if (fwrite(d, 1, sizeof(d), f) != sizeof(d)) {
        die("failed to write stream descriptor");
    }
}

static void write_page(FILE *f, u32 frame_no, u64 pts, const u8 *payload, u32 payload_size) {
    u8 h[32];
    memset(h, 0, sizeof(h));

    u32 crc = crc32_calc(payload, payload_size);

    memcpy(h + 0, "MP", 2);
    h[2] = PAGE_HEADER_SIZE;
    h[3] = PAGE_CRC | PAGE_HAS_KEYFRAME;
    wr_u32le(h + 4, frame_no);
    wr_u64le(h + 8, pts);
    wr_u32le(h + 16, payload_size);
    wr_u16le(h + 20, 1);
    wr_u16le(h + 22, 0);
    wr_u32le(h + 24, crc);
    wr_u32le(h + 28, 0);

    if (fwrite(h, 1, sizeof(h), f) != sizeof(h)) {
        die("failed to write page header");
    }

    if (fwrite(payload, 1, payload_size, f) != payload_size) {
        die("failed to write page payload");
    }
}

/* ------------------------------------------------------------------------- */
/* Block helpers                                                             */
/* ------------------------------------------------------------------------- */

static void get_block(const u8 *plane, int w, int bx, int by, u8 block[64]) {
    int x0 = bx * 8;
    int y0 = by * 8;

    for (int y = 0; y < 8; y++) {
        memcpy(block + y * 8, plane + (y0 + y) * w + x0, 8);
    }
}

static void put_block(u8 *plane, int w, int bx, int by, const u8 block[64]) {
    int x0 = bx * 8;
    int y0 = by * 8;

    for (int y = 0; y < 8; y++) {
        memcpy(plane + (y0 + y) * w + x0, block + y * 8, 8);
    }
}

static int avg_u8_64(const u8 b[64]) {
    int sum = 0;
    for (int i = 0; i < 64; i++) sum += b[i];
    return sum / 64;
}

static int max_abs_to_value_64(const u8 b[64], int value) {
    int m = 0;

    for (int i = 0; i < 64; i++) {
        int e = (int)b[i] - value;
        if (e < 0) e = -e;
        if (e > m) m = e;
    }

    return m;
}

static int avg_delta_64(const u8 cur[64], const u8 prev[64]) {
    int sum = 0;

    for (int i = 0; i < 64; i++) {
        sum += (int)cur[i] - (int)prev[i];
    }

    return clamp_int(round_div_int(sum, 64), -128, 127);
}

static int max_abs_delta_error_64(const u8 cur[64], const u8 prev[64], int d) {
    int m = 0;

    for (int i = 0; i < 64; i++) {
        int v = clamp_u8((int)prev[i] + d);
        int e = (int)cur[i] - v;
        if (e < 0) e = -e;
        if (e > m) m = e;
    }

    return m;
}

static int max_abs_diff_64(const u8 a[64], const u8 b[64]) {
    int m = 0;

    for (int i = 0; i < 64; i++) {
        int e = (int)a[i] - (int)b[i];
        if (e < 0) e = -e;
        if (e > m) m = e;
    }

    return m;
}

static void recon_delta_64(const u8 prev[64], int d, u8 out[64]) {
    for (int i = 0; i < 64; i++) {
        out[i] = clamp_u8((int)prev[i] + d);
    }
}

static void recon_solid_64(int value, u8 out[64]) {
    memset(out, value, 64);
}

/* ------------------------------------------------------------------------- */
/* QRES                                                                      */
/* ------------------------------------------------------------------------- */

static bool try_qres_64(
    const u8 cur[64],
    const u8 prev[64],
    int qres_thresh,
    u8 token[18],
    u8 recon[64]
) {
    if (qres_thresh <= 0) return false;

    int global_delta = avg_delta_64(cur, prev);
    s8 residuals[16];

    for (int cy = 0; cy < 4; cy++) {
        for (int cx = 0; cx < 4; cx++) {
            int err_sum = 0;

            for (int yy = 0; yy < 2; yy++) {
                for (int xx = 0; xx < 2; xx++) {
                    int x = cx * 2 + xx;
                    int y = cy * 2 + yy;
                    int idx = y * 8 + x;

                    int pred = clamp_u8((int)prev[idx] + global_delta);
                    err_sum += (int)cur[idx] - pred;
                }
            }

            residuals[cy * 4 + cx] = clamp_s8(round_div_int(err_sum, 4));
        }
    }

    int sad = 0;

    for (int y = 0; y < 8; y++) {
        int cell_y = (y >> 1) * 4;

        for (int x = 0; x < 8; x++) {
            int idx = y * 8 + x;
            int cell = cell_y + (x >> 1);
            int v = clamp_u8((int)prev[idx] + global_delta + (int)residuals[cell]);

            recon[idx] = (u8)v;

            int e = (int)cur[idx] - v;
            if (e < 0) e = -e;
            sad += e;
        }
    }

    if (sad > qres_thresh) {
        return false;
    }

    token[0] = M_QRES;
    token[1] = (u8)(s8)global_delta;

    for (int i = 0; i < 16; i++) {
        token[2 + i] = (u8)residuals[i];
    }

    return true;
}

/* ------------------------------------------------------------------------- */
/* Motion search                                                             */
/* ------------------------------------------------------------------------- */

static void mvlist_free(MVList *m) {
    free(m->mx);
    free(m->my);
    m->mx = NULL;
    m->my = NULL;
    m->count = 0;
}

static void mvlist_add(MVList *m, bool *seen, int side, int range, int mx, int my) {
    if (mx < -range || mx > range || my < -range || my > range) return;

    int idx = (my + range) * side + (mx + range);

    if (seen[idx]) return;

    seen[idx] = true;

    m->mx[m->count] = (s8)mx;
    m->my[m->count] = (s8)my;
    m->count++;
}

static MVList build_mv_list(int range) {
    if (range < 0) range = 0;
    if (range > 127) range = 127;

    int side = range * 2 + 1;
    int max_count = side * side;

    MVList m;
    m.count = 0;
    m.mx = (s8*)malloc(max_count);
    m.my = (s8*)malloc(max_count);

    if (!m.mx || !m.my) die("out of memory building mv list");

    bool *seen = (bool*)calloc((size_t)max_count, sizeof(bool));
    if (!seen) die("out of memory building mv seen");

    mvlist_add(&m, seen, side, range, 0, 0);

    for (int r = 1; r <= range; r++) {
        mvlist_add(&m, seen, side, range, -r, 0);
        mvlist_add(&m, seen, side, range,  r, 0);
    }

    for (int r = 1; r <= range; r++) {
        mvlist_add(&m, seen, side, range, 0, -r);
        mvlist_add(&m, seen, side, range, 0,  r);
    }

    for (int r = 1; r <= range; r++) {
        mvlist_add(&m, seen, side, range, -r, -r);
        mvlist_add(&m, seen, side, range,  r, -r);
        mvlist_add(&m, seen, side, range, -r,  r);
        mvlist_add(&m, seen, side, range,  r,  r);

        for (int mx = -r + 1; mx < r; mx++) {
            mvlist_add(&m, seen, side, range, mx, -r);
            mvlist_add(&m, seen, side, range, mx,  r);
        }

        for (int my = -r + 1; my < r; my++) {
            mvlist_add(&m, seen, side, range, -r, my);
            mvlist_add(&m, seen, side, range,  r, my);
        }
    }

    free(seen);

    return m;
}

static bool try_mvcopy_64(
    const u8 cur[64],
    const u8 *prev_plane,
    int w,
    int h,
    int bx,
    int by,
    const MVList *mvlist,
    int mv_thresh,
    u8 token[3],
    u8 recon[64]
) {
    if (!prev_plane || !mvlist || mv_thresh < 0) return false;

    int dst_x0 = bx * 8;
    int dst_y0 = by * 8;

    int best_sad = -1;
    int best_mx = 0;
    int best_my = 0;
    int best_x0 = dst_x0;
    int best_y0 = dst_y0;

    for (int k = 0; k < mvlist->count; k++) {
        int mx = mvlist->mx[k];
        int my = mvlist->my[k];

        int sx = dst_x0 + mx;
        int sy = dst_y0 + my;

        if (sx < 0 || sy < 0 || sx + 8 > w || sy + 8 > h) {
            continue;
        }

        int limit = (best_sad >= 0) ? best_sad : mv_thresh;
        int sad = 0;
        bool aborted = false;

        for (int yy = 0; yy < 8; yy++) {
            const u8 *p = prev_plane + (sy + yy) * w + sx;
            const u8 *c = cur + yy * 8;

            for (int x = 0; x < 8; x++) {
                int d = (int)c[x] - (int)p[x];
                if (d < 0) d = -d;
                sad += d;

                if (sad > limit) {
                    aborted = true;
                    break;
                }
            }

            if (aborted) break;
        }

        if (aborted) {
            continue;
        }

        if (sad > mv_thresh) {
            continue;
        }

        if (best_sad < 0 || sad < best_sad) {
            best_sad = sad;
            best_mx = mx;
            best_my = my;
            best_x0 = sx;
            best_y0 = sy;

            if (sad == 0) {
                break;
            }
        }
    }

    if (best_sad < 0) {
        return false;
    }

    for (int yy = 0; yy < 8; yy++) {
        memcpy(recon + yy * 8, prev_plane + (best_y0 + yy) * w + best_x0, 8);
    }

    token[0] = M_MVCOPY;
    token[1] = (u8)(s8)best_mx;
    token[2] = (u8)(s8)best_my;

    return true;
}


static bool find_best_mv_64(
    const u8 cur[64],
    const u8 *prev_plane,
    int w,
    int h,
    int bx,
    int by,
    const MVList *mvlist,
    int search_cutoff,
    int *out_mx,
    int *out_my,
    u8 out_pred[64],
    int *out_sad
) {
    if (!prev_plane || !mvlist) {
        return false;
    }

    int dst_x0 = bx * 8;
    int dst_y0 = by * 8;

    int best_sad = -1;
    int best_mx = 0;
    int best_my = 0;
    int best_x0 = dst_x0;
    int best_y0 = dst_y0;

    if (search_cutoff <= 0) {
        search_cutoff = 4096;
    }

    for (int k = 0; k < mvlist->count; k++) {
        int mx = mvlist->mx[k];
        int my = mvlist->my[k];

        int sx = dst_x0 + mx;
        int sy = dst_y0 + my;

        if (sx < 0 || sy < 0 || sx + 8 > w || sy + 8 > h) {
            continue;
        }

        int limit = best_sad >= 0 ? best_sad : search_cutoff;
        int sad = 0;
        bool aborted = false;

        for (int yy = 0; yy < 8; yy++) {
            const u8 *p = prev_plane + (sy + yy) * w + sx;
            const u8 *c = cur + yy * 8;

            for (int x = 0; x < 8; x++) {
                int d = (int)c[x] - (int)p[x];
                if (d < 0) d = -d;

                sad += d;

                if (sad > limit) {
                    aborted = true;
                    break;
                }
            }

            if (aborted) {
                break;
            }
        }

        if (aborted) {
            continue;
        }

        if (best_sad < 0 || sad < best_sad) {
            best_sad = sad;
            best_mx = mx;
            best_my = my;
            best_x0 = sx;
            best_y0 = sy;

            if (sad == 0) {
                break;
            }
        }
    }

    if (best_sad < 0) {
        return false;
    }

    for (int yy = 0; yy < 8; yy++) {
        memcpy(
            out_pred + yy * 8,
            prev_plane + (best_y0 + yy) * w + best_x0,
            8
        );
    }

    *out_mx = best_mx;
    *out_my = best_my;
    *out_sad = best_sad;

    return true;
}

static bool try_mvqres_64(
    const u8 cur[64],
    const u8 pred[64],
    int mx,
    int my,
    int qres_thresh,
    u8 token[20],
    u8 recon[64]
) {
    if (qres_thresh <= 0) {
        return false;
    }

    int global_delta = avg_delta_64(cur, pred);
    s8 residuals[16];

    for (int cy = 0; cy < 4; cy++) {
        for (int cx = 0; cx < 4; cx++) {
            int err_sum = 0;

            for (int yy = 0; yy < 2; yy++) {
                for (int xx = 0; xx < 2; xx++) {
                    int x = cx * 2 + xx;
                    int y = cy * 2 + yy;
                    int idx = y * 8 + x;

                    int predicted = clamp_u8((int)pred[idx] + global_delta);
                    err_sum += (int)cur[idx] - predicted;
                }
            }

            residuals[cy * 4 + cx] = clamp_s8(round_div_int(err_sum, 4));
        }
    }

    int sad = 0;

    for (int y = 0; y < 8; y++) {
        int cell_y = (y >> 1) * 4;

        for (int x = 0; x < 8; x++) {
            int idx = y * 8 + x;
            int cell = cell_y + (x >> 1);

            int v = clamp_u8(
                (int)pred[idx] +
                global_delta +
                (int)residuals[cell]
            );

            recon[idx] = (u8)v;

            int e = (int)cur[idx] - v;
            if (e < 0) e = -e;
            sad += e;
        }
    }

    if (sad > qres_thresh) {
        return false;
    }

    token[0] = M_MVQRES;
    token[1] = (u8)(s8)mx;
    token[2] = (u8)(s8)my;
    token[3] = (u8)(s8)global_delta;

    for (int i = 0; i < 16; i++) {
        token[4 + i] = (u8)residuals[i];
    }

    return true;
}

/* ------------------------------------------------------------------------- */
/* Plane encoder                                                             */
/* ------------------------------------------------------------------------- */

static void plane_enc_init(PlaneEnc *e, size_t cap) {
    buf_init(&e->payload, cap);
    memset(&e->stats, 0, sizeof(e->stats));
    e->pending_skip = 0;
}

static void plane_enc_free(PlaneEnc *e) {
    buf_free(&e->payload);
    e->pending_skip = 0;
}

static void plane_flush_skips(PlaneEnc *e) {
    int n = e->pending_skip;

    while (n > 0) {
        if (n >= 3) {
            int run = n;
            if (run > 256) run = 256;

            buf_put_u8(&e->payload, M_RUN_SKIP);
            buf_put_u8(&e->payload, (u8)(run - 1));

            e->stats.run_tokens++;
            e->stats.run_blocks += (u32)run;

            n -= run;
        } else {
            buf_put_u8(&e->payload, M_SKIP);
            n--;
        }
    }

    e->pending_skip = 0;
}

static void plane_emit_skip(PlaneEnc *e) {
    e->pending_skip++;
}

static void plane_emit_token(PlaneEnc *e, const u8 *p, int n) {
    plane_flush_skips(e);
    buf_put_bytes(&e->payload, p, (size_t)n);
}


/* ------------------------------------------------------------------------- */
/* HFIX28 RDO helpers                                                        */
/* ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
/* HFIX38A: Normalized Sum of Squared Differences visual distortion           */
/*                                                                           */
/* D = (SSD + 4) >> 3                                                        */
/*                                                                           */
/* This keeps the scale near SAD for average per-pixel error around 8, while */
/* penalizing large visible block cliffs much harder than SAD.                */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* HFIX39A: 8x8 block variance for conservative local transform-QP selection  */
/* ------------------------------------------------------------------------- */
static int calculate_block_variance_64(const u8 block[64]) {
    int sum = 0;

    for (int i = 0; i < 64; i++) {
        sum += block[i];
    }

    int mean = sum / 64;
    int variance_sum = 0;

    for (int i = 0; i < 64; i++) {
        int diff = (int)block[i] - mean;
        variance_sum += diff * diff;
    }

    return variance_sum / 64;
}

static int rdo_nssd_64(const u8 a[64], const u8 b[64]) {
    int ssd = 0;

    for (int i = 0; i < 64; i++) {
        int d = (int)a[i] - (int)b[i];
        ssd += d * d;
    }

    return (ssd + 4) >> 3;
}

static int rdo_sad_64(const u8 a[64], const u8 b[64]) {
    int sad = 0;

    for (int i = 0; i < 64; i++) {
        int d = (int)a[i] - (int)b[i];

        if (d < 0) {
            d = -d;
        }

        sad += d;
    }

    return sad;
}

static int rdo_make_qres_64(
    const u8 cur[64],
    const u8 prev[64],
    u8 token[18],
    u8 recon[64]
) {
    int global_delta = avg_delta_64(cur, prev);
    s8 residuals[16];

    for (int cy = 0; cy < 4; cy++) {
        for (int cx = 0; cx < 4; cx++) {
            int err_sum = 0;

            for (int yy = 0; yy < 2; yy++) {
                for (int xx = 0; xx < 2; xx++) {
                    int x = cx * 2 + xx;
                    int y = cy * 2 + yy;
                    int idx = y * 8 + x;

                    int pred = clamp_u8((int)prev[idx] + global_delta);
                    err_sum += (int)cur[idx] - pred;
                }
            }

            residuals[cy * 4 + cx] = clamp_s8(round_div_int(err_sum, 4));
        }
    }

    int sad = 0;

    for (int y = 0; y < 8; y++) {
        int cell_y = (y >> 1) * 4;

        for (int x = 0; x < 8; x++) {
            int idx = y * 8 + x;
            int cell = cell_y + (x >> 1);

            int v = clamp_u8(
                (int)prev[idx] +
                global_delta +
                (int)residuals[cell]
            );

            recon[idx] = (u8)v;

            int e = (int)cur[idx] - v;
            if (e < 0) {
                e = -e;
            }

            sad += e;
        }
    }

    token[0] = M_QRES;
    token[1] = (u8)(s8)global_delta;

    for (int i = 0; i < 16; i++) {
        token[2 + i] = (u8)residuals[i];
    }

    return sad;
}

static int rdo_make_mvqres_64(
    const u8 cur[64],
    const u8 pred[64],
    int mx,
    int my,
    u8 token[20],
    u8 recon[64]
) {
    int global_delta = avg_delta_64(cur, pred);
    s8 residuals[16];

    for (int cy = 0; cy < 4; cy++) {
        for (int cx = 0; cx < 4; cx++) {
            int err_sum = 0;

            for (int yy = 0; yy < 2; yy++) {
                for (int xx = 0; xx < 2; xx++) {
                    int x = cx * 2 + xx;
                    int y = cy * 2 + yy;
                    int idx = y * 8 + x;

                    int predicted = clamp_u8((int)pred[idx] + global_delta);
                    err_sum += (int)cur[idx] - predicted;
                }
            }

            residuals[cy * 4 + cx] = clamp_s8(round_div_int(err_sum, 4));
        }
    }

    int sad = 0;

    for (int y = 0; y < 8; y++) {
        int cell_y = (y >> 1) * 4;

        for (int x = 0; x < 8; x++) {
            int idx = y * 8 + x;
            int cell = cell_y + (x >> 1);

            int v = clamp_u8(
                (int)pred[idx] +
                global_delta +
                (int)residuals[cell]
            );

            recon[idx] = (u8)v;

            int e = (int)cur[idx] - v;
            if (e < 0) {
                e = -e;
            }

            sad += e;
        }
    }

    token[0] = M_MVQRES;
    token[1] = (u8)(s8)mx;
    token[2] = (u8)(s8)my;
    token[3] = (u8)(s8)global_delta;

    for (int i = 0; i < 16; i++) {
        token[4 + i] = (u8)residuals[i];
    }

    return sad;
}


/* ------------------------------------------------------------------------- */
/* HFIX29A 4x4 integer transform residuals                                   */
/* ------------------------------------------------------------------------- */

static void transform4_forward(const int16_t input[16], int16_t output[16]) {
    int tmp[16];

    for (int y = 0; y < 4; y++) {
        int x0 = input[y * 4 + 0];
        int x1 = input[y * 4 + 1];
        int x2 = input[y * 4 + 2];
        int x3 = input[y * 4 + 3];

        int a0 = x0 + x3;
        int a1 = x1 + x2;
        int a2 = x1 - x2;
        int a3 = x0 - x3;

        tmp[y * 4 + 0] = a0 + a1;
        tmp[y * 4 + 1] = a3 + a2;
        tmp[y * 4 + 2] = a0 - a1;
        tmp[y * 4 + 3] = a3 - a2;
    }

    for (int x = 0; x < 4; x++) {
        int x0 = tmp[0 * 4 + x];
        int x1 = tmp[1 * 4 + x];
        int x2 = tmp[2 * 4 + x];
        int x3 = tmp[3 * 4 + x];

        int a0 = x0 + x3;
        int a1 = x1 + x2;
        int a2 = x1 - x2;
        int a3 = x0 - x3;

        output[0 * 4 + x] = (int16_t)(a0 + a1);
        output[1 * 4 + x] = (int16_t)(a3 + a2);
        output[2 * 4 + x] = (int16_t)(a0 - a1);
        output[3 * 4 + x] = (int16_t)(a3 - a2);
    }
}

static void transform4_inverse(const int16_t input[16], int16_t output[16]) {
    int tmp[16];

    for (int y = 0; y < 4; y++) {
        int x0 = input[y * 4 + 0];
        int x1 = input[y * 4 + 1];
        int x2 = input[y * 4 + 2];
        int x3 = input[y * 4 + 3];

        int a0 = x0 + x3;
        int a1 = x1 + x2;
        int a2 = x1 - x2;
        int a3 = x0 - x3;

        tmp[y * 4 + 0] = a0 + a1;
        tmp[y * 4 + 1] = a3 + a2;
        tmp[y * 4 + 2] = a0 - a1;
        tmp[y * 4 + 3] = a3 - a2;
    }

    for (int x = 0; x < 4; x++) {
        int x0 = tmp[0 * 4 + x];
        int x1 = tmp[1 * 4 + x];
        int x2 = tmp[2 * 4 + x];
        int x3 = tmp[3 * 4 + x];

        int a0 = x0 + x3;
        int a1 = x1 + x2;
        int a2 = x1 - x2;
        int a3 = x0 - x3;

        output[0 * 4 + x] = (int16_t)((a0 + a1 + 8) >> 4);
        output[1 * 4 + x] = (int16_t)((a3 + a2 + 8) >> 4);
        output[2 * 4 + x] = (int16_t)((a0 - a1 + 8) >> 4);
        output[3 * 4 + x] = (int16_t)((a3 - a2 + 8) >> 4);
    }
}

static int quant_coeff_int(int v, int q) {
    if (q < 1) {
        q = 1;
    }

    if (v >= 0) {
        return (v + q / 2) / q;
    } else {
        return -((-v + q / 2) / q);
    }
}

static int dequant_coeff_int(int v, int q) {
    if (q < 1) {
        q = 1;
    }

    return v * q;
}

static int8_t clamp_s8_coeff(int v) {
    if (v < -128) {
        return -128;
    }

    if (v > 127) {
        return 127;
    }

    return (int8_t)v;
}

static int rdo_make_transform_64(
    const u8 cur[64],
    const u8 pred[64],
    int qp,
    int mv_mode,
    int mx,
    int my,
    u8 token[20],
    u8 recon[64]
) {
    /*
        8x8 block split into four 4x4 transform quadrants.

        Keep four low-frequency coefficients per quadrant:
            0, 1, 4, 5

        M_TRANSFORM:
            mode + reserved + 16 coeffs = 18 bytes.

        M_MVTRANSFORM:
            mode + mx + my + reserved + 16 coeffs = 20 bytes.
    */
    static const int keep_idx[4] = {
        0, 1,
        4, 5
    };

    int coeff_base = mv_mode ? 4 : 2;

    if (mv_mode) {
        token[0] = M_MVTRANSFORM;
        token[1] = (u8)(s8)mx;
        token[2] = (u8)(s8)my;
        token[3] = 0;
    } else {
        token[0] = M_TRANSFORM;
        token[1] = 0;
    }

    memcpy(recon, pred, 64);

    int out_coeff = 0;

    for (int qy = 0; qy < 2; qy++) {
        for (int qx = 0; qx < 2; qx++) {
            int16_t residual[16];
            int16_t coeff[16];
            int16_t deq[16];
            int16_t inv[16];

            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int bx4 = qx * 4 + x;
                    int by4 = qy * 4 + y;
                    int idx8 = by4 * 8 + bx4;
                    int idx4 = y * 4 + x;

                    residual[idx4] = (int16_t)((int)cur[idx8] - (int)pred[idx8]);
                }
            }

            transform4_forward(residual, coeff);
            memset(deq, 0, sizeof(deq));

            for (int k = 0; k < 4; k++) {
                int ci = keep_idx[k];
                int qv = quant_coeff_int((int)coeff[ci], qp);

                qv = (int)clamp_s8_coeff(qv);

                token[coeff_base + out_coeff] = (u8)(int8_t)qv;
                out_coeff++;

                deq[ci] = (int16_t)dequant_coeff_int(qv, qp);
            }

            transform4_inverse(deq, inv);

            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int bx4 = qx * 4 + x;
                    int by4 = qy * 4 + y;
                    int idx8 = by4 * 8 + bx4;
                    int idx4 = y * 4 + x;

                    recon[idx8] = clamp_u8((int)pred[idx8] + (int)inv[idx4]);
                }
            }
        }
    }

    return rdo_sad_64(cur, recon);
}


/* ------------------------------------------------------------------------- */
/* HFIX30A sparse zero-masked residual helpers                               */
/* ------------------------------------------------------------------------- */

static int pack_sparse16_le(
    const int8_t vals[16],
    u8 *out,
    int out_cap,
    u16 *out_mask
) {
    u16 mask = 0;
    int pos = 2;

    if (out_cap < 18) {
        return -1;
    }

    for (int i = 0; i < 16; i++) {
        if (vals[i] != 0) {
            mask |= (u16)(1u << i);
            out[pos++] = (u8)vals[i];
        }
    }

    out[0] = (u8)(mask & 255);
    out[1] = (u8)((mask >> 8) & 255);

    *out_mask = mask;
    return pos;
}

static int rdo_make_qresz_64(
    const u8 cur[64],
    const u8 pred[64],
    int mv_mode,
    int mx,
    int my,
    u8 token[32],
    u8 recon[64],
    int *out_len
) {
    int global_delta = avg_delta_64(cur, pred);
    int8_t residuals[16];

    for (int cy = 0; cy < 4; cy++) {
        for (int cx = 0; cx < 4; cx++) {
            int err_sum = 0;

            for (int yy = 0; yy < 2; yy++) {
                for (int xx = 0; xx < 2; xx++) {
                    int x = cx * 2 + xx;
                    int y = cy * 2 + yy;
                    int idx = y * 8 + x;

                    int predicted = clamp_u8((int)pred[idx] + global_delta);
                    err_sum += (int)cur[idx] - predicted;
                }
            }

            residuals[cy * 4 + cx] =
                (int8_t)clamp_s8(round_div_int(err_sum, 4));
        }
    }

    int sad = 0;

    for (int y = 0; y < 8; y++) {
        int cell_y = (y >> 1) * 4;

        for (int x = 0; x < 8; x++) {
            int idx = y * 8 + x;
            int cell = cell_y + (x >> 1);

            int v = clamp_u8(
                (int)pred[idx] +
                global_delta +
                (int)residuals[cell]
            );

            recon[idx] = (u8)v;

            int e = (int)cur[idx] - v;
            if (e < 0) {
                e = -e;
            }

            sad += e;
        }
    }

    int pos = 0;

    if (mv_mode) {
        token[pos++] = M_MVQRESZ;
        token[pos++] = (u8)(s8)mx;
        token[pos++] = (u8)(s8)my;
        token[pos++] = (u8)(s8)global_delta;
    } else {
        token[pos++] = M_QRESZ;
        token[pos++] = (u8)(s8)global_delta;
    }

    u16 mask = 0;

    int packed_len = pack_sparse16_le(
        residuals,
        token + pos,
        32 - pos,
        &mask
    );

    if (packed_len < 0) {
        *out_len = 0;
        return 999999;
    }

    pos += packed_len;
    *out_len = pos;

    return sad;
}

static int rdo_make_transformz_64(
    const u8 cur[64],
    const u8 pred[64],
    int qp,
    int mv_mode,
    int mx,
    int my,
    u8 token[32],
    u8 recon[64],
    int *out_len
) {
    static const int keep_idx[4] = {
        0, 1,
        4, 5
    };

    int8_t coeff_out[16];
    int out_coeff = 0;

    memcpy(recon, pred, 64);

    for (int qy = 0; qy < 2; qy++) {
        for (int qx = 0; qx < 2; qx++) {
            int16_t residual[16];
            int16_t coeff[16];
            int16_t deq[16];
            int16_t inv[16];

            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int bx4 = qx * 4 + x;
                    int by4 = qy * 4 + y;
                    int idx8 = by4 * 8 + bx4;
                    int idx4 = y * 4 + x;

                    residual[idx4] =
                        (int16_t)((int)cur[idx8] - (int)pred[idx8]);
                }
            }

            transform4_forward(residual, coeff);
            memset(deq, 0, sizeof(deq));

            for (int k = 0; k < 4; k++) {
                int ci = keep_idx[k];

                int qv = quant_coeff_int((int)coeff[ci], qp);
                qv = (int)clamp_s8_coeff(qv);

                coeff_out[out_coeff++] = (int8_t)qv;
                deq[ci] = (int16_t)dequant_coeff_int(qv, qp);
            }

            transform4_inverse(deq, inv);

            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int bx4 = qx * 4 + x;
                    int by4 = qy * 4 + y;
                    int idx8 = by4 * 8 + bx4;
                    int idx4 = y * 4 + x;

                    recon[idx8] = clamp_u8(
                        (int)pred[idx8] +
                        (int)inv[idx4]
                    );
                }
            }
        }
    }

    int sad = rdo_sad_64(cur, recon);

    int pos = 0;

    if (mv_mode) {
        token[pos++] = M_MVTRANSFORMZ;
        token[pos++] = (u8)(s8)mx;
        token[pos++] = (u8)(s8)my;
        token[pos++] = 0;
    } else {
        token[pos++] = M_TRANSFORMZ;
        token[pos++] = 0;
    }

    u16 mask = 0;

    int packed_len = pack_sparse16_le(
        coeff_out,
        token + pos,
        32 - pos,
        &mask
    );

    if (packed_len < 0) {
        *out_len = 0;
        return 999999;
    }

    pos += packed_len;
    *out_len = pos;

    return sad;
}


/* ------------------------------------------------------------------------- */
/* HFIX36A packed 4-bit motion-vector helpers                                */
/* ------------------------------------------------------------------------- */

static inline int mv_can_pack4(int mx, int my) {
    return mx >= -8 && mx <= 7 && my >= -8 && my <= 7;
}

static inline u8 mv_pack4(int mx, int my) {
    return (u8)(((my + 8) << 4) | ((mx + 8) & 15));
}


/* ------------------------------------------------------------------------- */
/* HFIX37-REDUX single-block Global Motion Vector estimator                   */
/* ------------------------------------------------------------------------- */

static void estimate_global_mv_luma(
    const u8 *cur,
    const u8 *prev,
    int w,
    int h,
    int mv_range,
    int *out_mx,
    int *out_my
) {
    int best_mx = 0;
    int best_my = 0;
    long long best_sad = 0x7fffffffffffffffLL;

    if (!cur || !prev || w <= 0 || h <= 0) {
        *out_mx = 0;
        *out_my = 0;
        return;
    }

    if (mv_range < 0) {
        mv_range = 0;
    }

    if (mv_range > 8) {
        mv_range = 8;
    }

    /*
        Search only integer-pel vectors. Sample every other 8x8 block to keep
        encoder cost reasonable while still capturing camera pans.
    */
    for (int my = -mv_range; my <= mv_range; my++) {
        for (int mx = -mv_range; mx <= mv_range; mx++) {
            long long sad = 0;

            for (int by = 1; by < (h / 8) - 1; by += 2) {
                for (int bx = 1; bx < (w / 8) - 1; bx += 2) {
                    int x0 = bx * 8;
                    int y0 = by * 8;

                    int sx = x0 + mx;
                    int sy = y0 + my;

                    if (sx < 0 || sy < 0 || sx + 8 > w || sy + 8 > h) {
                        sad += 4096;
                        continue;
                    }

                    for (int yy = 0; yy < 8; yy++) {
                        const u8 *c = cur  + (y0 + yy) * w + x0;
                        const u8 *p = prev + (sy + yy) * w + sx;

                        for (int xx = 0; xx < 8; xx++) {
                            int d = (int)c[xx] - (int)p[xx];
                            sad += d < 0 ? -d : d;
                        }
                    }
                }
            }

            if (sad < best_sad) {
                best_sad = sad;
                best_mx = mx;
                best_my = my;
            }
        }
    }

    /*
        Clamp to HFIX36A packed-vector-safe range. This also keeps chroma
        scaling deterministic.
    */
    *out_mx = clamp_int(best_mx, -8, 7);
    *out_my = clamp_int(best_my, -8, 7);
}

static void encode_plane(
    const u8 *cur,
    const u8 *prev,
    u8 *recon_plane,
    int w,
    int h,
    bool keyframe,
    int skip_thr,
    int delta_thr,
    int solid_thr,
    int qres_thr,
    const MVList *mvlist,
    int mv_thresh,
    double lambda,
    int qp,
    int g_mx,
    int g_my,
    PlaneEnc *out
) {
    /*
        HFIX30A competitive RDO loop:
            cost = SAD + lambda * bytes

        Includes fixed residual modes and sparse zero-masked Z-modes.
    */
    (void)skip_thr;
    (void)delta_thr;
    (void)solid_thr;
    (void)qres_thr;
    (void)mv_thresh;

    /*
        HFIX32:
        Plane payload byte 0 stores active transform QP.
        Decoder consumes this byte before macroblock token parsing.
        This applies independently to Y, Cb, and Cr planes.
    */
    if (out->payload.size == 0) {
        int qp_byte = qp;

        if (qp_byte < 1) {
            qp_byte = 1;
        }

        if (qp_byte > 51) {
            qp_byte = 51;
        }

        buf_put_u8(&out->payload, (u8)qp_byte);
        buf_put_u8(&out->payload, (u8)(s8)g_mx);
        buf_put_u8(&out->payload, (u8)(s8)g_my);
    }

    int bxcount = w / 8;
    int bycount = h / 8;

    u8 b[64];
    u8 pb[64];
    u8 token_tmp[65];
    u8 recon_tmp[64];

    u8 best_token[65];
    u8 best_recon[64];

    /*
        HFIX39A:
        Base-relative DQP state. This is not cumulative in the bitstream:
            active_qp = base_qp + active_dqp
    */
    int active_dqp = 0;

    for (int by = 0; by < bycount; by++) {
        for (int bx = 0; bx < bxcount; bx++) {
            get_block(cur, w, bx, by, b);

            /*
                HFIX39A conservative three-state QP ladder:
                    flat / gradient: QP - 4
                    normal:          QP
                    texture:         QP + 4
            */
            int block_var_dqp = calculate_block_variance_64(b);

            int block_min_dqp = 255;
            int block_max_dqp = 0;

            for (int dqp_i = 0; dqp_i < 64; dqp_i++) {
                int v = (int)b[dqp_i];

                if (v < block_min_dqp) {
                    block_min_dqp = v;
                }

                if (v > block_max_dqp) {
                    block_max_dqp = v;
                }
            }

            int block_range_dqp = block_max_dqp - block_min_dqp;

            /*
                HFIX39B:
                Flat-only local QP.

                HFIX39A used +4 on high-variance texture blocks, but that
                over-starved important edge/detail regions. HFIX39B only
                grants extra precision to smooth/gradient blocks and never
                makes any block coarser than the base QP.
            */
            int target_dqp = 0;

            if (block_var_dqp < 80 && block_range_dqp < 48) {
                target_dqp = -4;
            }

            int local_qp = clamp_int(qp + target_dqp, 18, 48);
            int dqp_rate = (target_dqp != active_dqp) ? 2 : 0;

            int best_kind = -1;
            int best_len = 0;
            int best_dqp = active_dqp;
            int candidate_dqp = active_dqp;
            double best_cost = 1.0e30;

            #define RDO_CONSIDER(kind_value, rate_bytes, token_len, token_ptr, recon_ptr, sad_value) do { \
                if ((token_len) > 0) { \
                    int visual_dist = rdo_nssd_64(b, (recon_ptr)); \
                    double candidate_cost = (double)visual_dist + lambda * (double)(rate_bytes); \
                    if (candidate_cost < best_cost) { \
                        best_cost = candidate_cost; \
                        best_kind = (kind_value); \
                        best_len = (token_len); \
                        memcpy(best_token, (token_ptr), (size_t)(token_len)); \
                        memcpy(best_recon, (recon_ptr), 64); \
                        best_dqp = candidate_dqp; \
                    } \
                } \
            } while (0)

            if (!keyframe && prev) {
                get_block(prev, w, bx, by, pb);

                /*
                    SKIP.
                */
                token_tmp[0] = M_SKIP;
                int sad_skip = rdo_sad_64(b, pb);
                RDO_CONSIDER(M_SKIP, 1, 1, token_tmp, pb, sad_skip);

                /*
                    DELTA.
                */
                int d = avg_delta_64(b, pb);
                token_tmp[0] = M_DELTA;
                token_tmp[1] = (u8)(s8)d;
                recon_delta_64(pb, d, recon_tmp);
                int sad_delta = rdo_sad_64(b, recon_tmp);
                RDO_CONSIDER(M_DELTA, 2, 2, token_tmp, recon_tmp, sad_delta);

                /*
                    QRES fixed spatial residual.
                */
                int sad_qres = rdo_make_qres_64(b, pb, token_tmp, recon_tmp);
                RDO_CONSIDER(M_QRES, 18, 18, token_tmp, recon_tmp, sad_qres);

                /*
                    QRESZ sparse spatial residual.
                    Same reconstruction as QRES, variable payload.
                */
                int qz_len = 0;
                int sad_qz = rdo_make_qresz_64(
                    b,
                    pb,
                    0,
                    0,
                    0,
                    token_tmp,
                    recon_tmp,
                    &qz_len
                );
                RDO_CONSIDER(M_QRESZ, qz_len, qz_len, token_tmp, recon_tmp, sad_qz);

                /*
                    TRANSFORM fixed frequency residual.
                */
                int sad_tr = rdo_make_transform_64(
                    b,
                    pb,
                    local_qp,
                    0,
                    0,
                    0,
                    token_tmp,
                    recon_tmp
                );
                candidate_dqp = target_dqp;
                RDO_CONSIDER(M_TRANSFORM, 18 + dqp_rate, 18, token_tmp, recon_tmp, sad_tr);
                candidate_dqp = active_dqp;

                /*
                    TRANSFORMZ sparse frequency residual.
                */
                int trz_len = 0;
                int sad_trz = rdo_make_transformz_64(
                    b,
                    pb,
                    local_qp,
                    0,
                    0,
                    0,
                    token_tmp,
                    recon_tmp,
                    &trz_len
                );
                candidate_dqp = target_dqp;
                RDO_CONSIDER(M_TRANSFORMZ, trz_len + dqp_rate, trz_len, token_tmp, recon_tmp, sad_trz);
                candidate_dqp = active_dqp;

                /*
                    Motion-predicted candidates.
                */
                if (mvlist) {
                    u8 pred[64];
                    int best_mx = 0;
                    int best_my = 0;
                    int best_mv_sad = 0;
                    int mv_search_cutoff = 4096;

                    if (find_best_mv_64(
                            b,
                            prev,
                            w,
                            h,
                            bx,
                            by,
                            mvlist,
                            mv_search_cutoff,
                            &best_mx,
                            &best_my,
                            pred,
                            &best_mv_sad
                        )) {
                        /*
                            HFIX37-REDUX:
                            Single-block Global Motion Vector copy.

                            This is a normal one-block token. It does not
                            accumulate runs and never changes decoder traversal.
                        */
                        if (prev) {
                            int g_sx = bx * 8 + g_mx;
                            int g_sy = by * 8 + g_my;

                            if (g_sx >= 0 && g_sy >= 0 &&
                                g_sx + 8 <= w && g_sy + 8 <= h) {
                                u8 gmv_pred[64];

                                for (int yy = 0; yy < 8; yy++) {
                                    memcpy(
                                        gmv_pred + yy * 8,
                                        prev + (g_sy + yy) * w + g_sx,
                                        8
                                    );
                                }

                                int sad_gmv = rdo_sad_64(b, gmv_pred);

                                token_tmp[0] = M_GMVCOPY;

                                RDO_CONSIDER(
                                    M_GMVCOPY_INTERNAL,
                                    1,
                                    1,
                                    token_tmp,
                                    gmv_pred,
                                    sad_gmv
                                );
                            }
                        }

                        /*
                            MVCOPY.
                        */
                        token_tmp[0] = M_MVCOPY;
                        token_tmp[1] = (u8)(s8)best_mx;
                        token_tmp[2] = (u8)(s8)best_my;
                        if (mv_can_pack4(best_mx, best_my)) {
                            u8 packed_token[2];
                            packed_token[0] = M_MVCOPYP;
                            packed_token[1] = mv_pack4(best_mx, best_my);
                            RDO_CONSIDER(M_MVCOPYP, 2, 2, packed_token, pred, best_mv_sad);
                        }
                        RDO_CONSIDER(M_MVCOPY, 3, 3, token_tmp, pred, best_mv_sad);

                        /*
                            MVQRES fixed spatial residual.
                        */
                        int sad_mvqres = rdo_make_mvqres_64(
                            b,
                            pred,
                            best_mx,
                            best_my,
                            token_tmp,
                            recon_tmp
                        );
                        RDO_CONSIDER(M_MVQRES, 20, 20, token_tmp, recon_tmp, sad_mvqres);

                        /*
                            MVQRESZ sparse spatial residual.
                        */
                        int mvqz_len = 0;
                        int sad_mvqz = rdo_make_qresz_64(
                            b,
                            pred,
                            1,
                            best_mx,
                            best_my,
                            token_tmp,
                            recon_tmp,
                            &mvqz_len
                        );
                        RDO_CONSIDER(M_MVQRESZ, mvqz_len, mvqz_len, token_tmp, recon_tmp, sad_mvqz);

                        /*
                            MVTRANSFORM fixed frequency residual.
                        */
                        int sad_mvtr = rdo_make_transform_64(
                            b,
                            pred,
                            local_qp,
                            1,
                            best_mx,
                            best_my,
                            token_tmp,
                            recon_tmp
                        );
                        candidate_dqp = target_dqp;
                        RDO_CONSIDER(M_MVTRANSFORM, 20 + dqp_rate, 20, token_tmp, recon_tmp, sad_mvtr);
                        candidate_dqp = active_dqp;

                        /*
                            MVTRANSFORMZ sparse frequency residual.
                        */
                        int mvtrz_len = 0;
                        int sad_mvtrz = rdo_make_transformz_64(
                            b,
                            pred,
                            local_qp,
                            1,
                            best_mx,
                            best_my,
                            token_tmp,
                            recon_tmp,
                            &mvtrz_len
                        );
                        candidate_dqp = target_dqp;
                        RDO_CONSIDER(M_MVTRANSFORMZ, mvtrz_len + dqp_rate, mvtrz_len, token_tmp, recon_tmp, sad_mvtrz);
                        candidate_dqp = active_dqp;
                        if (mv_can_pack4(best_mx, best_my) && mvtrz_len >= 6) {
                            u8 packed_token[64];
                            packed_token[0] = M_MVTRANSFORMZP;
                            packed_token[1] = mv_pack4(best_mx, best_my);
                            packed_token[2] = token_tmp[3]; /* reserved */
                            memcpy(packed_token + 3, token_tmp + 4, (size_t)(mvtrz_len - 4));
                            int packed_len = mvtrz_len - 1;
                            candidate_dqp = target_dqp;
                            RDO_CONSIDER(M_MVTRANSFORMZP, packed_len + dqp_rate, packed_len, packed_token, recon_tmp, sad_mvtrz);
                            candidate_dqp = active_dqp;
                        }
                    }
                }
            }

            /*
                SOLID.
            */
            int av = avg_u8_64(b);
            token_tmp[0] = M_SOLID;
            token_tmp[1] = (u8)av;
            recon_solid_64(av, recon_tmp);
            int sad_solid = rdo_sad_64(b, recon_tmp);
            RDO_CONSIDER(M_SOLID, 2, 2, token_tmp, recon_tmp, sad_solid);

            /*
                RAW.
            */
            token_tmp[0] = M_RAW;
            memcpy(token_tmp + 1, b, 64);
            RDO_CONSIDER(M_RAW, 65, 65, token_tmp, b, 0);

            #undef RDO_CONSIDER

            if (best_kind < 0) {
                best_token[0] = M_RAW;
                memcpy(best_token + 1, b, 64);
                memcpy(best_recon, b, 64);
                best_kind = M_RAW;
                best_len = 65;
            }

            if (best_kind == M_SKIP) {
                plane_emit_skip(out);
                out->stats.skip++;
            } else if (best_kind == M_GMVCOPY_INTERNAL) {
                u8 tok[1];

                tok[0] = M_GMVCOPY;

                plane_emit_token(out, tok, 1);

                out->stats.mv++;
            } else {
                /*
                    HFIX39A emit base-relative DQP state token only when a
                    transform-family mode wins and the target offset differs
                    from the currently active offset.
                */
                if ((best_kind == M_TRANSFORM ||
                     best_kind == M_MVTRANSFORM ||
                     best_kind == M_TRANSFORMZ ||
                     best_kind == M_MVTRANSFORMZ ||
                     best_kind == M_MVTRANSFORMZP) &&
                    best_dqp != active_dqp) {
                    u8 dqp_tok[2];

                    dqp_tok[0] = M_SET_BASE_DQP;
                    dqp_tok[1] = (u8)(s8)best_dqp;

                    plane_emit_token(out, dqp_tok, 2);

                    active_dqp = best_dqp;
                }

                plane_emit_token(out, best_token, best_len);

                if (best_kind == M_DELTA) {
                    out->stats.delta++;
                } else if (best_kind == M_SOLID) {
                    out->stats.solid++;
                } else if ((best_kind == M_MVCOPY || best_kind == M_MVCOPYP)) {
                    out->stats.mv++;
                } else if (best_kind == M_MVQRES) {
                    out->stats.mvqres++;
                } else if (best_kind == M_QRES) {
                    out->stats.qres++;
                } else if (best_kind == M_TRANSFORM) {
                    out->stats.tr++;
                } else if (best_kind == M_MVTRANSFORM) {
                    out->stats.mvtr++;
                } else if (best_kind == M_QRESZ) {
                    out->stats.qz++;
                } else if (best_kind == M_MVQRESZ) {
                    out->stats.mvqz++;
                } else if (best_kind == M_TRANSFORMZ) {
                    out->stats.trz++;
                } else if ((best_kind == M_MVTRANSFORMZ || best_kind == M_MVTRANSFORMZP)) {
                    out->stats.mvtrz++;
                } else if (best_kind == M_RAW) {
                    out->stats.raw++;
                }
            }

            put_block(recon_plane, w, bx, by, best_recon);
        }
    }

    plane_flush_skips(out);
}

/* ------------------------------------------------------------------------- */
/* Main encoder                                                              */
/* ------------------------------------------------------------------------- */

static long file_size(FILE *f) {
    long cur = ftell(f);

    if (cur < 0) die("ftell failed");

    if (fseek(f, 0, SEEK_END)) die("fseek end failed");

    long n = ftell(f);
    if (n < 0) die("ftell end failed");

    if (fseek(f, cur, SEEK_SET)) die("fseek restore failed");

    return n;
}

static void print_stats(
    int frames,
    long total_bytes,
    const PlaneStats *ys,
    const PlaneStats *cs,
    int iframes,
    int pframes
) {
    printf(
        "STATS I=%d P=%d "
        "Y_skip=%u Y_delta=%u Y_solid=%u Y_mv=%u Y_mvqres=%u Y_qres=%u Y_tr=%u Y_mvtr=%u Y_qz=%u Y_mvqz=%u Y_trz=%u Y_mvtrz=%u Y_raw=%u Y_run_tokens=%u Y_run_blocks=%u "
        "C_skip=%u C_delta=%u C_solid=%u C_mv=%u C_mvqres=%u C_qres=%u C_tr=%u C_mvtr=%u C_qz=%u C_mvqz=%u C_trz=%u C_mvtrz=%u C_raw=%u C_run_tokens=%u C_run_blocks=%u "
        "frames=%d total_bytes=%ld\n",
        iframes,
        pframes,
        ys->skip,
        ys->delta,
        ys->solid,
        ys->mv,
        ys->mvqres,
        ys->qres,
        ys->tr,
        ys->mvtr,
        ys->qz,
        ys->mvqz,
        ys->trz,
        ys->mvtrz,
        ys->raw,
        ys->run_tokens,
        ys->run_blocks,
        cs->skip,
        cs->delta,
        cs->solid,
        cs->mv,
        cs->mvqres,
        cs->qres,
        cs->tr,
        cs->mvtr,
        cs->qz,
        cs->mvqz,
        cs->trz,
        cs->mvtrz,
        cs->raw,
        cs->run_tokens,
        cs->run_blocks,
        frames,
        total_bytes
    );
}

int main(int argc, char **argv) {
    const char *in_path = arg_str(argc, argv, "--input", NULL);
    const char *out_path = arg_str(argc, argv, "--output", NULL);

    if (!in_path || !out_path) {
        usage();
        return 1;
    }

    EncParams ep;
    ep.w = arg_int(argc, argv, "--width", 0);
    ep.h = arg_int(argc, argv, "--height", 0);
    ep.fps = arg_int(argc, argv, "--fps", 30);
    ep.keyint = arg_int(argc, argv, "--keyint", 60);

    ep.y_skip = arg_int(argc, argv, "--y-skip", 8);
    ep.y_delta = arg_int(argc, argv, "--y-delta", 16);
    ep.y_solid = arg_int(argc, argv, "--y-solid", 10);
    ep.y_qres = arg_int(argc, argv, "--y-qres", 360);

    ep.c_skip = arg_int(argc, argv, "--c-skip", 12);
    ep.c_delta = arg_int(argc, argv, "--c-delta", 22);
    ep.c_solid = arg_int(argc, argv, "--c-solid", 32);
    ep.c_qres = arg_int(argc, argv, "--c-qres", 300);

    ep.mv_range = arg_int(argc, argv, "--mv-range", 8);
    ep.y_mv_thresh = arg_int(argc, argv, "--y-mv-thresh", 768);
    ep.c_mv_thresh = arg_int(argc, argv, "--c-mv-thresh", 1024);

    ep.lambda = arg_double(argc, argv, "--lambda", 3.35);
    ep.c_lambda = arg_double(argc, argv, "--c-lambda", ep.lambda * 1.55);
    ep.qp = arg_int(argc, argv, "--qp", 35);
        ep.c_qp_offset = arg_int(argc, argv, "--c-qp-offset", 4);
if (ep.qp < 1) ep.qp = 1;
    if (ep.qp > 51) ep.qp = 51;

    if (ep.w <= 0 || ep.h <= 0 || ep.fps <= 0) {
        die("bad width/height/fps");
    }

    if ((ep.w & 15) || (ep.h & 15)) {
        die("width/height should be multiples of 16 for yuv420 8x8 planes");
    }

    int y_size = ep.w * ep.h;
    int c_w = ep.w / 2;
    int c_h = ep.h / 2;
    int c_size = c_w * c_h;
    int frame_size = y_size + c_size + c_size;

    FILE *fin = fopen(in_path, "rb");
    if (!fin) die("failed to open input");

    long in_bytes = file_size(fin);

    if (in_bytes <= 0 || (in_bytes % frame_size) != 0) {
        die("input size is not a whole number of yuv420p frames");
    }

    int frames = (int)(in_bytes / frame_size);

    FILE *fout = fopen(out_path, "wb");
    if (!fout) die("failed to open output");

    u64 duration = (u64)frames * 30000ull / (u64)ep.fps;
    u64 first = HEADER_SIZE + 48;

    write_header(fout, 1, duration, first);
    write_stream_desc_m2y1(fout, ep.w, ep.h, ep.fps);

    u8 *frame = (u8*)malloc((size_t)frame_size);
    u8 *prev_y = (u8*)calloc((size_t)y_size, 1);
    u8 *prev_cb = (u8*)calloc((size_t)c_size, 1);
    u8 *prev_cr = (u8*)calloc((size_t)c_size, 1);

    u8 *recon_y = (u8*)malloc((size_t)y_size);
    u8 *recon_cb = (u8*)malloc((size_t)c_size);
    u8 *recon_cr = (u8*)malloc((size_t)c_size);

    if (!frame || !prev_y || !prev_cb || !prev_cr || !recon_y || !recon_cb || !recon_cr) {
        die("out of memory allocating frame buffers");
    }

    MVList mvlist = build_mv_list(ep.mv_range);

    PlaneStats total_y;
    PlaneStats total_c;
    memset(&total_y, 0, sizeof(total_y));
    memset(&total_c, 0, sizeof(total_c));

    int iframes = 0;
    int pframes = 0;

    for (int fi = 0; fi < frames; fi++) {
        size_t got = fread(frame, 1, (size_t)frame_size, fin);

        if (got != (size_t)frame_size) {
            die("short read");
        }

        const u8 *cur_y = frame;
        const u8 *cur_cb = frame + y_size;
        const u8 *cur_cr = frame + y_size + c_size;

        bool keyframe = (fi == 0) || (ep.keyint > 0 && (fi % ep.keyint) == 0);

        if (keyframe) {
            iframes++;
        } else {
            pframes++;
        }

        PlaneEnc yenc;
        PlaneEnc cbenc;
        PlaneEnc crenc;

        plane_enc_init(&yenc, (size_t)(y_size + 4096));
        plane_enc_init(&cbenc, (size_t)(c_size + 2048));
        plane_enc_init(&crenc, (size_t)(c_size + 2048));

        int g_mx = 0;
        int g_my = 0;

        if (!keyframe) {
            estimate_global_mv_luma(cur_y, prev_y, ep.w, ep.h, 8, &g_mx, &g_my);
        }

        encode_plane(
            cur_y,
            keyframe ? NULL : prev_y,
            recon_y,
            ep.w,
            ep.h,
            keyframe,
            ep.y_skip,
            ep.y_delta,
            ep.y_solid,
            ep.y_qres,
            &mvlist,
            ep.y_mv_thresh,
            ep.lambda,
            ep.qp,
            g_mx,
            g_my,
            &yenc
        );

        encode_plane(
            cur_cb,
            keyframe ? NULL : prev_cb,
            recon_cb,
            c_w,
            c_h,
            keyframe,
            ep.c_skip,
            ep.c_delta,
            ep.c_solid,
            ep.c_qres,
            &mvlist,
            ep.c_mv_thresh,
            ep.c_lambda,
            clamp_int(ep.qp + ep.c_qp_offset, 1, 51),
            g_mx / 2,
            g_my / 2,
            &cbenc
        );

        encode_plane(
            cur_cr,
            keyframe ? NULL : prev_cr,
            recon_cr,
            c_w,
            c_h,
            keyframe,
            ep.c_skip,
            ep.c_delta,
            ep.c_solid,
            ep.c_qres,
            &mvlist,
            ep.c_mv_thresh,
            ep.c_lambda,
            clamp_int(ep.qp + ep.c_qp_offset, 1, 51),
            g_mx / 2,
            g_my / 2,
            &crenc
        );

        total_y.skip += yenc.stats.skip;
        total_y.delta += yenc.stats.delta;
        total_y.solid += yenc.stats.solid;
        total_y.mv += yenc.stats.mv;
        total_y.mvqres += yenc.stats.mvqres;
        total_y.qres += yenc.stats.qres;
        total_y.tr += yenc.stats.tr;
        total_y.mvtr += yenc.stats.mvtr;
        total_y.qz += yenc.stats.qz;
        total_y.mvqz += yenc.stats.mvqz;
        total_y.trz += yenc.stats.trz;
        total_y.mvtrz += yenc.stats.mvtrz;
        total_y.raw += yenc.stats.raw;
        total_y.run_tokens += yenc.stats.run_tokens;
        total_y.run_blocks += yenc.stats.run_blocks;

        total_c.skip += cbenc.stats.skip + crenc.stats.skip;
        total_c.delta += cbenc.stats.delta + crenc.stats.delta;
        total_c.solid += cbenc.stats.solid + crenc.stats.solid;
        total_c.mv += cbenc.stats.mv + crenc.stats.mv;
        total_c.mvqres += cbenc.stats.mvqres + crenc.stats.mvqres;
        total_c.qres += cbenc.stats.qres + crenc.stats.qres;
        total_c.tr += cbenc.stats.tr + crenc.stats.tr;
        total_c.mvtr += cbenc.stats.mvtr + crenc.stats.mvtr;
        total_c.qz += cbenc.stats.qz + crenc.stats.qz;
        total_c.mvqz += cbenc.stats.mvqz + crenc.stats.mvqz;
        total_c.trz += cbenc.stats.trz + crenc.stats.trz;
        total_c.mvtrz += cbenc.stats.mvtrz + crenc.stats.mvtrz;
        total_c.raw += cbenc.stats.raw + crenc.stats.raw;
        total_c.run_tokens += cbenc.stats.run_tokens + crenc.stats.run_tokens;
        total_c.run_blocks += cbenc.stats.run_blocks + crenc.stats.run_blocks;

        u32 body_size =
            28u +
            (u32)yenc.payload.size +
            (u32)cbenc.payload.size +
            (u32)crenc.payload.size;

        u32 pkt_size = PACKET_HEADER_SIZE + body_size;

        u8 *pkt = (u8*)malloc(pkt_size);
        if (!pkt) die("out of memory packet");

        memset(pkt, 0, pkt_size);

        pkt[0] = 0;
        pkt[1] = PKT_KEYFRAME | PKT_FRAME_START | PKT_FRAME_END;
        wr_u16le(pkt + 2, PACKET_HEADER_SIZE);
        wr_u32le(pkt + 4, 0);
        wr_u32le(pkt + 8, body_size);
        wr_u32le(pkt + 12, (u32)fi);

        u8 *body = pkt + PACKET_HEADER_SIZE;

        memcpy(body + 0, "M2Y1", 4);
        wr_u16le(body + 4, (u16)ep.w);
        wr_u16le(body + 6, (u16)ep.h);
        wr_u32le(body + 8, (u32)fi);
        body[12] = keyframe ? 1 : 2;
        body[13] = 0;
        wr_u16le(body + 14, 0);
        wr_u32le(body + 16, (u32)yenc.payload.size);
        wr_u32le(body + 20, (u32)cbenc.payload.size);
        wr_u32le(body + 24, (u32)crenc.payload.size);

        u8 *q = body + 28;

        memcpy(q, yenc.payload.data, yenc.payload.size);
        q += yenc.payload.size;

        memcpy(q, cbenc.payload.data, cbenc.payload.size);
        q += cbenc.payload.size;

        memcpy(q, crenc.payload.data, crenc.payload.size);

        write_page(
            fout,
            (u32)fi,
            (u64)fi * 30000ull / (u64)ep.fps,
            pkt,
            pkt_size
        );

        free(pkt);

        memcpy(prev_y, recon_y, (size_t)y_size);
        memcpy(prev_cb, recon_cb, (size_t)c_size);
        memcpy(prev_cr, recon_cr, (size_t)c_size);

        plane_enc_free(&yenc);
        plane_enc_free(&cbenc);
        plane_enc_free(&crenc);

        if (((fi + 1) % 30) == 0 || fi + 1 == frames) {
            printf("encoded %d/%d\n", fi + 1, frames);
            fflush(stdout);
        }
    }

    fclose(fin);
    fflush(fout);

    long out_bytes = ftell(fout);
    fclose(fout);

    mvlist_free(&mvlist);

    free(frame);
    free(prev_y);
    free(prev_cb);
    free(prev_cr);
    free(recon_y);
    free(recon_cb);
    free(recon_cr);

    printf("WROTE %s\n", out_path);
    printf("frames=%d bytes=%ld\n", frames, out_bytes);

    print_stats(
        frames,
        out_bytes,
        &total_y,
        &total_c,
        iframes,
        pframes
    );

    return 0;
}
