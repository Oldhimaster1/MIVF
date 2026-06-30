/*
 * miv1p_fastest_c.c — very fast MIV1/M1P1 encoder for raw RGB565 input.
 *
 * This implements the current best-looking "fastest" profile:
 *   - no motion search
 *   - same-position SKIP blocks
 *   - same-position AVGDELTA blocks
 *   - RAW fallback
 *
 * Input is one concatenated raw RGB565LE file:
 *   frame_count * width * height * 2 bytes
 *
 * Output is standard MIVF with MIV1/M1P1 packets, compatible with the current player.
 *
 * Build on MSYS2/UCRT64:
 *   gcc -O3 -march=native -flto -o miv1p_fastest_c.exe miv1p_fastest_c.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#define HEADER_SIZE 64
#define STREAM_DESC_SIZE 32
#define PAGE_HEADER_SIZE 32
#define PACKET_HEADER_SIZE 16
#define PAGE_CRC 1
#define PAGE_HAS_KEYFRAME 2
#define PKT_KEYFRAME 1
#define PKT_FRAME_START 2
#define PKT_FRAME_END 4

#define M_SKIP 0
#define M_RAW 1
#define M_SOLID 2
#define M_TWO 3
#define M_AVGDELTA 4
#define M_MVCOPY 5
#define M_MVDELTA 6
#define M_RUN_SKIP 7

typedef struct {
    const char *input;
    const char *output;
    int width, height, frames, fps;
    int keyint;
    int skip;
    int delta;

    /*
        HFIX18:
        Optional safe intra compression.
        0 disables the mode.
    */
    int solid;
    int two;

    /*
        HFIX19 strict rate control.
        If target_frame_bytes > 0, encoder retries each frame with
        increasingly aggressive thresholds until payload <= target.
    */
    int target_frame_bytes;
    int rc_passes;
} Args;

typedef struct {
    unsigned I, P, skip, avg, solid, two, raw;
} Stats;

static uint32_t crc_table[256];
static uint8_t luma_table[65536];

static void init_crc(void) {
    for (uint32_t i=0;i<256;i++) {
        uint32_t c=i;
        for (int j=0;j<8;j++) c=(c&1) ? 0xEDB88320u^(c>>1) : c>>1;
        crc_table[i]=c;
    }
}
static uint32_t crc32_buf(const uint8_t *data, size_t len) {
    uint32_t c=0xffffffffu;
    for (size_t i=0;i<len;i++) c=crc_table[(c^data[i])&255]^(c>>8);
    return c^0xffffffffu;
}
static void init_luma(void) {
    for (int v=0; v<65536; v++) {
        int r5=(v>>11)&31, g6=(v>>5)&63, b5=v&31;
        int r=(r5<<3)|(r5>>2), g=(g6<<2)|(g6>>4), b=(b5<<3)|(b5>>2);
        luma_table[v]=(uint8_t)((77*r+150*g+29*b)>>8);
    }
}
static inline uint16_t rd16(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1]<<8); }
static inline void wr16(uint8_t *p, uint16_t v) { p[0]=v&255; p[1]=v>>8; }
static inline void wr16f(FILE *f, uint16_t v) { fputc(v&255,f); fputc((v>>8)&255,f); }
static inline void wr32f(FILE *f, uint32_t v) { fputc(v&255,f); fputc((v>>8)&255,f); fputc((v>>16)&255,f); fputc((v>>24)&255,f); }
static inline void wr64f(FILE *f, uint64_t v) { wr32f(f,(uint32_t)v); wr32f(f,(uint32_t)(v>>32)); }

static inline void rgb888(uint16_t v, int *r, int *g, int *b) {
    int r5=(v>>11)&31, g6=(v>>5)&63, b5=v&31;
    *r=(r5<<3)|(r5>>2); *g=(g6<<2)|(g6>>4); *b=(b5<<3)|(b5>>2);
}
static inline int clamp8(int x) { return x<0?0:(x>255?255:x); }
static inline int clamp_s8(int x) { return x<-128?-128:(x>127?127:x); }
static inline uint16_t rgb565(int r, int g, int b) {
    return (uint16_t)(((clamp8(r)>>3)<<11)|((clamp8(g)>>2)<<5)|(clamp8(b)>>3));
}
static inline uint16_t add_delta565(uint16_t v, int dr, int dg, int db) {
    int r,g,b; rgb888(v,&r,&g,&b); return rgb565(r+dr,g+dg,b+db);
}

static double block_luma_mad_same(const uint8_t *cur, const uint8_t *prev, int w, int bx, int by, int early_limit) {
    int x0=bx*8, y0=by*8;
    int sum=0, cutoff=early_limit*64;
    for (int y=0;y<8;y++) {
        int base=(y0+y)*w+x0;
        for (int x=0;x<8;x++) {
            uint16_t c=rd16(cur+(base+x)*2), p=rd16(prev+(base+x)*2);
            int d=(int)luma_table[c]-(int)luma_table[p]; sum += d<0 ? -d : d;
        }
        if (sum>cutoff) return (double)sum/64.0;
    }
    return (double)sum/64.0;
}

static void avg_delta_block(const uint8_t *cur, const uint8_t *prev, int w, int bx, int by, int *dr, int *dg, int *db) {
    int x0=bx*8, y0=by*8;
    int sr=0, sg=0, sb=0;
    for (int y=0;y<8;y++) {
        int base=(y0+y)*w+x0;
        for (int x=0;x<8;x++) {
            int cr,cg,cb,pr,pg,pb;
            rgb888(rd16(cur+(base+x)*2), &cr,&cg,&cb);
            rgb888(rd16(prev+(base+x)*2), &pr,&pg,&pb);
            sr += cr-pr; sg += cg-pg; sb += cb-pb;
        }
    }
    *dr=clamp_s8((int)lrint((double)sr/64.0));
    *dg=clamp_s8((int)lrint((double)sg/64.0));
    *db=clamp_s8((int)lrint((double)sb/64.0));
}

static double rgb_mad_delta(const uint8_t *cur, const uint8_t *prev, int w, int bx, int by, int dr, int dg, int db, int early_limit) {
    int x0=bx*8, y0=by*8;
    int sum=0, cutoff=early_limit*64*3;
    for (int y=0;y<8;y++) {
        int base=(y0+y)*w+x0;
        for (int x=0;x<8;x++) {
            int cr,cg,cb,rr,rg,rb;
            uint16_t c=rd16(cur+(base+x)*2);
            uint16_t r=add_delta565(rd16(prev+(base+x)*2), dr,dg,db);
            rgb888(c,&cr,&cg,&cb); rgb888(r,&rr,&rg,&rb);
            int a=cr-rr; sum += a<0?-a:a;
            a=cg-rg; sum += a<0?-a:a;
            a=cb-rb; sum += a<0?-a:a;
        }
        if (sum>cutoff) return (double)sum/192.0;
    }
    return (double)sum/192.0;
}

static void write_raw_block(uint8_t **pp, const uint8_t *cur, int w, int bx, int by) {
    uint8_t *p=*pp;
    *p++=M_RAW;
    int x0=bx*8, y0=by*8;
    for (int y=0;y<8;y++) {
        memcpy(p, cur+((y0+y)*w+x0)*2, 16);
        p += 16;
    }
    *pp=p;
}

static void write_solid_block(uint8_t **pp, uint16_t c) {
    uint8_t *p = *pp;

    *p++ = M_SOLID;
    wr16(p, c);
    p += 2;

    *pp = p;
}

static void write_two_block(uint8_t **pp, uint16_t c0, uint16_t c1, const uint8_t bits[8]) {
    uint8_t *p = *pp;

    *p++ = M_TWO;
    wr16(p, c0);
    p += 2;
    wr16(p, c1);
    p += 2;
    memcpy(p, bits, 8);
    p += 8;

    *pp = p;
}

static int block_max_channel_error_to_color(const uint8_t *cur, int w, int bx, int by, uint16_t ref) {
    int rr, rg, rb;
    rgb888(ref, &rr, &rg, &rb);

    int x0 = bx * 8;
    int y0 = by * 8;
    int maxe = 0;

    for (int y = 0; y < 8; y++) {
        int base = (y0 + y) * w + x0;

        for (int x = 0; x < 8; x++) {
            int cr, cg, cb;
            rgb888(rd16(cur + (base + x) * 2), &cr, &cg, &cb);

            int e = abs(cr - rr);
            if (e > maxe) maxe = e;

            e = abs(cg - rg);
            if (e > maxe) maxe = e;

            e = abs(cb - rb);
            if (e > maxe) maxe = e;
        }
    }

    return maxe;
}

static bool try_solid_block(uint8_t **pp, const uint8_t *cur, int w, int bx, int by, int threshold) {
    if (threshold <= 0) {
        return false;
    }

    int x0 = bx * 8;
    int y0 = by * 8;

    int sr = 0;
    int sg = 0;
    int sb = 0;

    for (int y = 0; y < 8; y++) {
        int base = (y0 + y) * w + x0;

        for (int x = 0; x < 8; x++) {
            int r, g, b;
            rgb888(rd16(cur + (base + x) * 2), &r, &g, &b);
            sr += r;
            sg += g;
            sb += b;
        }
    }

    uint16_t avg = rgb565(sr / 64, sg / 64, sb / 64);

    int maxe = block_max_channel_error_to_color(cur, w, bx, by, avg);

    if (maxe > threshold) {
        return false;
    }

    write_solid_block(pp, avg);
    return true;
}

static bool try_two_block(uint8_t **pp, const uint8_t *cur, int w, int bx, int by, int threshold) {
    if (threshold <= 0) {
        return false;
    }

    int x0 = bx * 8;
    int y0 = by * 8;

    uint16_t dark = 0;
    uint16_t light = 0;
    int dark_luma = 999999;
    int light_luma = -1;

    for (int y = 0; y < 8; y++) {
        int base = (y0 + y) * w + x0;

        for (int x = 0; x < 8; x++) {
            uint16_t c = rd16(cur + (base + x) * 2);
            int l = luma_table[c];

            if (l < dark_luma) {
                dark_luma = l;
                dark = c;
            }

            if (l > light_luma) {
                light_luma = l;
                light = c;
            }
        }
    }

    int threshold_luma = (dark_luma + light_luma) / 2;
    uint8_t bits[8];
    memset(bits, 0, sizeof(bits));

    int maxe = 0;

    for (int i = 0; i < 64; i++) {
        int x = i & 7;
        int y = i >> 3;
        int base = (y0 + y) * w + x0;

        uint16_t c = rd16(cur + (base + x) * 2);
        uint16_t ref;

        if (luma_table[c] >= threshold_luma) {
            bits[i >> 3] |= (uint8_t)(1u << (i & 7));
            ref = light;
        } else {
            ref = dark;
        }

        int cr, cg, cb;
        int rr, rg, rb;

        rgb888(c, &cr, &cg, &cb);
        rgb888(ref, &rr, &rg, &rb);

        int e = abs(cr - rr);
        if (e > maxe) maxe = e;

        e = abs(cg - rg);
        if (e > maxe) maxe = e;

        e = abs(cb - rb);
        if (e > maxe) maxe = e;

        if (maxe > threshold) {
            return false;
        }
    }

    write_two_block(pp, dark, light, bits);
    return true;
}



static void copy_token(uint8_t **pp, const uint8_t *src, size_t n) {
    memcpy(*pp, src, n);
    *pp += n;
}

static size_t m1p_token_size(const uint8_t *p, const uint8_t *end) {
    if (p >= end) return 0;

    switch (p[0]) {
        case M_SKIP:
            return 1;

        case M_RAW:
            return (p + 129 <= end) ? 129 : 0;

        case M_AVGDELTA:
            return (p + 4 <= end) ? 4 : 0;

        /*
            These are included for future compatibility. The current fastest C
            encoder only emits SKIP / AVGDELTA / RAW.
        */
        case M_TWO:
            return (p + 13 <= end) ? 13 : 0;

        case M_SOLID:
            return (p + 3 <= end) ? 3 : 0;

        case M_MVDELTA:
            return (p + 6 <= end) ? 6 : 0;

        case M_MVCOPY:
            return (p + 3 <= end) ? 3 : 0;

        default:
            return 0;
    }
}

static size_t emit_m1p1_rle(uint8_t *dst, const uint8_t *src, size_t src_size) {
    const uint8_t *p = src;
    const uint8_t *end = src + src_size;
    uint8_t *out = dst;

    while (p < end) {
        if (*p == M_SKIP) {
            uint32_t run = 1;

            while (run < 256 &&
                   p + run < end &&
                   p[run] == M_SKIP) {
                run++;
            }

            /*
                M_RUN_SKIP costs 2 bytes:
                    token + run_minus_1

                Individual M_SKIP costs run bytes.
                run >= 3 is the first profitable point.
            */
            if (run >= 3) {
                *out++ = M_RUN_SKIP;
                *out++ = (uint8_t)(run - 1);
                p += run;
                continue;
            }

            /*
                run 1 or 2: emit individual skips.
            */
            for (uint32_t i = 0; i < run; i++) {
                *out++ = M_SKIP;
            }

            p += run;
            continue;
        }

        size_t ts = m1p_token_size(p, end);

        if (ts == 0) {
            fprintf(stderr, "ERROR: invalid token while RLE-emitting M1P1 stream at offset %ld mode=%u\n",
                (long)(p - src),
                (unsigned)*p);
            exit(1);
        }

        copy_token(&out, p, ts);
        p += ts;
    }

    return (size_t)(out - dst);
}

static size_t encode_frame(uint8_t *out, const uint8_t *cur, const uint8_t *prev, int w, int h, int frame_no, const Args *a, Stats *st) {
    int bxcount = w / 8;
    int bycount = h / 8;
    uint32_t blocks = (uint32_t)(bxcount * bycount);
    bool iframe = (!prev) || (a->keyint > 0 && frame_no % a->keyint == 0);

    /*
        HFIX19 strict rate control.

        Important:
        - This does NOT preserve perfect quality.
        - This enforces a byte budget using progressively more lossy block choices.
        - It is an emergency path for hitting a hard file-size target.
    */
    size_t block_cap = (size_t)blocks * 129u;
    uint8_t *block_tmp = (uint8_t*)malloc(block_cap);
    uint8_t *best_blocks = (uint8_t*)malloc(block_cap);

    if (!block_tmp || !best_blocks) {
        fprintf(stderr, "OOM HFIX19 block buffers\n");
        exit(1);
    }

    size_t best_size = (size_t)-1;
    Stats best_stats = {0};

    int max_passes = a->rc_passes > 0 ? a->rc_passes : 20;

    if (max_passes < 1) {
        max_passes = 1;
    }

    for (int pass = 0; pass < max_passes; pass++) {
        /*
            Threshold schedule.

            Pass 0 = requested settings.
            Later passes aggressively raise thresholds.
            Final passes can force SOLID, guaranteeing small size.
        */
        int skip_t  = a->skip  + pass * 2;
        int delta_t = a->delta + pass * 2;

        int solid_t = a->solid;
        int two_t   = a->two;

        if (pass >= 2) {
            solid_t = solid_t > 0 ? solid_t + pass * 5 : pass * 5;
            two_t   = two_t   > 0 ? two_t   + pass * 7 : pass * 7;
        }

        /*
            Panic zone:
            If earlier passes cannot fit the budget, force cheap modes.
            255 means every block can be represented by SOLID if needed.
        */
        if (pass >= max_passes - 2) {
            solid_t = 255;
            two_t = 0;
            skip_t = 255;
            delta_t = 255;
        } else if (pass >= max_passes - 5) {
            solid_t = 96;
            two_t = 160;
            skip_t = 80;
            delta_t = 80;
        }

        uint8_t *bp = block_tmp;
        Stats fs = {0};

        if (iframe) {
            fs.I++;
        } else {
            fs.P++;
        }

        for (int by = 0; by < bycount; by++) {
            for (int bx = 0; bx < bxcount; bx++) {
                if (!iframe) {
                    double lm = block_luma_mad_same(cur, prev, w, bx, by, skip_t);

                    if (lm <= skip_t) {
                        *bp++ = M_SKIP;
                        fs.skip++;
                        continue;
                    }

                    int dr, dg, db;
                    avg_delta_block(cur, prev, w, bx, by, &dr, &dg, &db);

                    double rm = rgb_mad_delta(cur, prev, w, bx, by, dr, dg, db, delta_t);

                    if (rm <= delta_t) {
                        *bp++ = M_AVGDELTA;
                        *bp++ = (uint8_t)(int8_t)dr;
                        *bp++ = (uint8_t)(int8_t)dg;
                        *bp++ = (uint8_t)(int8_t)db;
                        fs.avg++;
                        continue;
                    }
                }

                /*
                    Try cheap intra modes before RAW.
                    In high RC passes these become intentionally aggressive.
                */
                if (try_solid_block(&bp, cur, w, bx, by, solid_t)) {
                    fs.solid++;
                    continue;
                }

                if (try_two_block(&bp, cur, w, bx, by, two_t)) {
                    fs.two++;
                    continue;
                }

                write_raw_block(&bp, cur, w, bx, by);
                fs.raw++;
            }
        }

        size_t raw_block_bytes = (size_t)(bp - block_tmp);

        /*
            Worst-case M1P1 RLE output cannot exceed raw block bytes here,
            because only SKIP runs shrink.
        */
        uint8_t *tmp_out = out + 20;
        size_t rle_block_bytes = emit_m1p1_rle(tmp_out, block_tmp, raw_block_bytes);
        size_t payload_size = 20 + rle_block_bytes;

        if (payload_size < best_size) {
            best_size = payload_size;
            memcpy(best_blocks, tmp_out, rle_block_bytes);
            best_stats = fs;
        }

        if (a->target_frame_bytes <= 0 || payload_size <= (size_t)a->target_frame_bytes) {
            best_size = payload_size;
            memcpy(best_blocks, tmp_out, rle_block_bytes);
            best_stats = fs;
            break;
        }
    }

    /*
        Write final selected frame header + selected block stream.
    */
    uint8_t *p = out;

    memcpy(p, "M1P1", 4);
    p += 4;

    wr16(p, w);
    p += 2;

    wr16(p, h);
    p += 2;

    p[0] = frame_no & 255;
    p[1] = (frame_no >> 8) & 255;
    p[2] = (frame_no >> 16) & 255;
    p[3] = (frame_no >> 24) & 255;
    p += 4;

    *p++ = iframe ? 1 : 2;
    *p++ = 8;
    *p++ = 8;
    *p++ = 0;

    p[0] = blocks & 255;
    p[1] = (blocks >> 8) & 255;
    p[2] = (blocks >> 16) & 255;
    p[3] = (blocks >> 24) & 255;
    p += 4;

    memcpy(p, best_blocks, best_size - 20);
    p += best_size - 20;

    st->I     += best_stats.I;
    st->P     += best_stats.P;
    st->skip  += best_stats.skip;
    st->avg   += best_stats.avg;
    st->solid += best_stats.solid;
    st->two   += best_stats.two;
    st->raw   += best_stats.raw;

    free(block_tmp);
    free(best_blocks);

    return (size_t)(p - out);
}

static void write_header(FILE *f, const Args *a, uint64_t first) {
    fwrite("MIVF",1,4,f); wr16f(f,0); wr16f(f,12); wr32f(f,HEADER_SIZE); wr32f(f,1); wr32f(f,30000);
    wr64f(f,(uint64_t)a->frames*30000/(uint32_t)a->fps); wr32f(f,1); wr32f(f,4096); wr64f(f,first); wr32f(f,0); wr32f(f,0); wr32f(f,0); wr32f(f,0); wr32f(f,0);
    uint8_t extra[16]; memcpy(extra,"M1P1",4); extra[4]=8; extra[5]=8; extra[6]=(uint8_t)a->keyint; extra[7]=0; extra[8]=a->skip&255; extra[9]=a->skip>>8; extra[10]=0; extra[11]=0; extra[12]=a->delta&255; extra[13]=a->delta>>8; extra[14]=0; extra[15]=0;
    fputc(0,f); fputc(1,f); wr16f(f,STREAM_DESC_SIZE+16); fwrite("MIV1",1,4,f); wr32f(f,1); wr32f(f,a->fps);
    wr16f(f,(uint16_t)a->width); wr16f(f,(uint16_t)a->height); wr16f(f,(uint16_t)a->fps); wr16f(f,1); wr32f(f,0); fputc(0,f); fputc(0,f); wr16f(f,16); fwrite(extra,1,16,f);
}

static void write_page(FILE *f, uint32_t frame_no, const uint8_t *payload, uint32_t payload_size, int fps) {
    uint32_t pkt_size = PACKET_HEADER_SIZE + payload_size;
    uint8_t *pkt=(uint8_t*)malloc(pkt_size);
    pkt[0]=0; pkt[1]=PKT_KEYFRAME|PKT_FRAME_START|PKT_FRAME_END; pkt[2]=PACKET_HEADER_SIZE; pkt[3]=0;
    memset(pkt+4,0,4);
    pkt[8]=payload_size&255; pkt[9]=(payload_size>>8)&255; pkt[10]=(payload_size>>16)&255; pkt[11]=(payload_size>>24)&255;
    pkt[12]=frame_no&255; pkt[13]=(frame_no>>8)&255; pkt[14]=(frame_no>>16)&255; pkt[15]=(frame_no>>24)&255;
    memcpy(pkt+PACKET_HEADER_SIZE,payload,payload_size);
    uint32_t crc=crc32_buf(pkt,pkt_size);
    fwrite("MP",1,2,f); fputc(PAGE_HEADER_SIZE,f); fputc(PAGE_CRC|PAGE_HAS_KEYFRAME,f); wr32f(f,frame_no); wr64f(f,(uint64_t)frame_no*30000/(uint32_t)fps); wr32f(f,pkt_size); wr16f(f,1); wr16f(f,0); wr32f(f,crc); wr32f(f,0);
    fwrite(pkt,1,pkt_size,f);
    free(pkt);
}

static void usage(void) {
    fprintf(stderr,"Usage: miv1p_fastest_c --input in.rgb565 --output out.mivf --width 400 --height 240 --frames 120 --fps 30 [--keyint 30] [--skip 1] [--delta 1] [--solid 0] [--two 0] [--target-frame-bytes 0] [--rc-passes 20]\n");
}
static int parse(int argc, char **argv, Args *a) {
    memset(a,0,sizeof(*a)); a->fps=30; a->keyint=30; a->skip=1; a->delta=1; a->solid=0; a->two=0; a->target_frame_bytes=0; a->rc_passes=20;
    for (int i=1;i<argc;i++) {
        if (!strcmp(argv[i],"--input") && i+1<argc) a->input=argv[++i];
        else if (!strcmp(argv[i],"--output") && i+1<argc) a->output=argv[++i];
        else if (!strcmp(argv[i],"--width") && i+1<argc) a->width=atoi(argv[++i]);
        else if (!strcmp(argv[i],"--height") && i+1<argc) a->height=atoi(argv[++i]);
        else if (!strcmp(argv[i],"--frames") && i+1<argc) a->frames=atoi(argv[++i]);
        else if (!strcmp(argv[i],"--fps") && i+1<argc) a->fps=atoi(argv[++i]);
        else if (!strcmp(argv[i],"--keyint") && i+1<argc) a->keyint=atoi(argv[++i]);
        else if (!strcmp(argv[i],"--skip") && i+1<argc) a->skip=atoi(argv[++i]);
        else if (!strcmp(argv[i],"--delta") && i+1<argc) a->delta=atoi(argv[++i]);
        else if (!strcmp(argv[i],"--solid") && i+1<argc) a->solid=atoi(argv[++i]);
        else if (!strcmp(argv[i],"--two") && i+1<argc) a->two=atoi(argv[++i]);
        else if (!strcmp(argv[i],"--target-frame-bytes") && i+1<argc) a->target_frame_bytes=atoi(argv[++i]);
        else if (!strcmp(argv[i],"--rc-passes") && i+1<argc) a->rc_passes=atoi(argv[++i]);
        else { usage(); return 0; }
    }
    if (!a->input||!a->output||a->width<=0||a->height<=0||a->frames<=0||a->fps<=0||a->width%8||a->height%8) { usage(); return 0; }
    return 1;
}

int main(int argc, char **argv) {
    Args a; if (!parse(argc,argv,&a)) return 1;
    init_crc(); init_luma();
    FILE *in=fopen(a.input,"rb"); if(!in){perror(a.input); return 1;}
    FILE *out=fopen(a.output,"wb"); if(!out){perror(a.output); fclose(in); return 1;}
    size_t frame_size=(size_t)a.width*a.height*2;
    size_t max_payload=20 + (size_t)(a.width/8)*(a.height/8)*129;
    uint8_t *cur=(uint8_t*)malloc(frame_size), *prev=(uint8_t*)malloc(frame_size), *payload=(uint8_t*)malloc(max_payload);
    if(!cur||!prev||!payload){fprintf(stderr,"OOM\n");return 1;}
    uint64_t first=HEADER_SIZE+STREAM_DESC_SIZE+16;
    write_header(out,&a,first);
    Stats st={0};
    for (int f=0; f<a.frames; f++) {
        if (fread(cur,1,frame_size,in)!=frame_size) { fprintf(stderr,"short input at frame %d\n",f); return 1; }
        size_t ps=encode_frame(payload,cur,(f==0?NULL:prev),a.width,a.height,f,&a,&st);
        write_page(out,(uint32_t)f,payload,(uint32_t)ps,a.fps);
        uint8_t *tmp=prev; prev=cur; cur=tmp;
        if ((f+1)%30==0 || f==a.frames-1) fprintf(stderr,"encoded %d/%d\n",f+1,a.frames);
    }
    fclose(in); fclose(out);
    fprintf(stderr,"STATS I=%u P=%u skip=%u avg=%u solid=%u two=%u raw=%u\n",st.I,st.P,st.skip,st.avg,st.solid,st.two,st.raw);
    fprintf(stderr,"WROTE %s\n",a.output);
    free(cur); free(prev); free(payload);
    return 0;
}
