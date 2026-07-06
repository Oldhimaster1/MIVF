#include <3ds.h>
#include <sys/stat.h>

/* ------------------------------------------------------------------------- */
/* HFIX58J_RETAIL_UX_AND_IO                                                   */
/* Version: MIVF Phase 5G Retail UX HFIX58J                                   */
/* ------------------------------------------------------------------------- */

/* HFIX58J-R1 FORWARD PROTOTYPES */
static void hfix58_rect565(u8 *fb, int x, int y, int w, int h, int r, int g, int b);
static void hfix58_draw_text_shadow(u8 *fb, int x, int y, const char *text, int scale, int r, int g, int b);
static const char g_hfix58j_version_tag[] __attribute__((used)) =
    "MIVF Phase 5G Retail UX HFIX58J";

#include <time.h>

/* ------------------------------------------------------------------------- */
/* HFIX58I_R1_PACKET_FIELD_FIX                                                */
/* Version: MIVF Phase 5G Retail UX HFIX58J-R1                               */
/* ------------------------------------------------------------------------- */
static const char g_hfix58i_r1_version_tag[] __attribute__((used)) =
    "MIVF Phase 5G Retail UX HFIX58J-R1";


/* ------------------------------------------------------------------------- */
/* HFIX58I_IO_OPTIMIZATION                                                    */
/* Version: MIVF Phase 5G Retail UX HFIX58J                                  */
/* ------------------------------------------------------------------------- */
static const char g_hfix58i_version_tag[] __attribute__((used)) =
    "MIVF Phase 5G Retail UX HFIX58J";


/* ------------------------------------------------------------------------- */
/* HFIX58H_LATENCY_WAKE_POLISH                                                */
/* Version: MIVF Phase 5G Retail UX HFIX58J                              */
/* ------------------------------------------------------------------------- */
static const char g_hfix58h_version_tag[] __attribute__((used)) =
    "MIVF Phase 5G Retail UX HFIX58J";


/* ------------------------------------------------------------------------- */
/* HFIX58F_R2_ROBUST_COMPILE_REPAIR                                           */
/* Version: MIVF Phase 5G Retail UX HFIX58J-R2                               */
/* ------------------------------------------------------------------------- */
static const char g_hfix58f_r2_version_tag[] __attribute__((used)) =
    "MIVF Phase 5G Retail UX HFIX58J-R2";


/* ------------------------------------------------------------------------- */
/* HFIX58F_SAFE_KEYFRAME_SEEK                                                 */
/* Version: MIVF Phase 5G Retail UX HFIX58J                                  */
/* ------------------------------------------------------------------------- */
static const char g_hfix58f_version_tag[] __attribute__((used)) =
    "MIVF Phase 5G Retail UX HFIX58J";


/* ------------------------------------------------------------------------- */
/* HFIX58E_PERF_OPTIMIZATION                                                  */
/* Version: MIVF Phase 5G Retail UX HFIX58J                                */
/* ------------------------------------------------------------------------- */
static const char g_hfix58e_version_tag[] __attribute__((used)) =
    "MIVF Phase 5G Retail UX HFIX58J";


/* ------------------------------------------------------------------------- */
/* HFIX58D_FLUENT_UI_ANIMATION                                                */
/* Version: MIVF Phase 5G Retail UX HFIX58J                                   */
/* ------------------------------------------------------------------------- */
static const char g_hfix58d_version_tag[] __attribute__((used)) =
    "MIVF Phase 5G Retail UX HFIX58J";

#include <dirent.h>
#include <ctype.h>
#include <3ds/services/y2r.h>
#include <3ds/services/apt.h>
#include <3ds/services/gsplcd.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#include "mivf_stream.h"
#include "mivf_settings.h"
#include "mivf_rc.h"
#include "mivf_transform.h"
#include "moflex/playback/moflex_playback.h"

/* Kept transform coefficients per quadrant for the frame being decoded.
   Read from the M2Y1/M2Y2 body header (reserved byte 13); 0 means legacy 4. */
static int g_m2y1_nkeep = MIVF_T_NKEEP_LEGACY;

/* ------------------------------------------------------------------------- */
/* HFIX58S_SRT_SUBTITLES_PATCH_ONLY                                           */
/* Version: MIVF Phase 5G SRT Subtitles HFIX58S                               */
/* ------------------------------------------------------------------------- */
static const char g_hfix58s_subtitle_version_tag[] __attribute__((used)) =
    "MIVF Phase 5G SRT Subtitles HFIX58S";

#include "mivf_subtitles.h"



#define MIVF_RUNTIME_TELEMETRY 0
#define MIVF_PATH g_hfix58_selected_media

#define MIVF_HEADER_SIZE        64
#define MIVF_STREAM_HEADER_SIZE 64
#define HFIX58_MAX_PATH 512

static u32 g_mivf_stream_stride = MIVF_STREAM_HEADER_SIZE;

#define TOP_W 400
#define TOP_H 240

#define MIVF_SCALE_FULLSCREEN 0

#define AUDIO_BUFS 48
#define AUDIO_MAX_PACKET 8192
static FILE *g_mivf_log = NULL;

static void mivf_log_open(void) {
    if (!g_mivf_log) {
        MIVF_AppDataEnsureLayout();
        g_mivf_log = fopen(MIVF_LOG_PATH, "w");
    }
}

static void mivf_log_close(void) {
    if (g_mivf_log) {
        fflush(g_mivf_log);
        fclose(g_mivf_log);
        g_mivf_log = NULL;
    }
}

static int tee_printf(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    va_end(ap);

    if (g_mivf_log) {
        va_start(ap, fmt);
        vfprintf(g_mivf_log, fmt, ap);
        va_end(ap);
        fflush(g_mivf_log);
    }

    return r;
}

#define printf tee_printf

typedef struct {
    u32 streams;
    u64 duration;
    u64 first;
} Header;

/* HFIX59R2: authoritative global movie timing.
   The MIVF header stores duration at byte offset 20 in 30000 Hz ticks.
   Timeline must not use seek table count as duration. */
static u64 g_hfix59r2_duration_ticks = 0;
static u32 g_hfix59r2_video_fps_num = 30;
static u32 g_hfix59r2_video_fps_den = 1;
static bool g_hfix56_force_stereo = true;
static bool g_hfix59r3_settings_visible = false;
static bool g_hfix59r3_resume_after_settings = false;

static MivfSettings g_mivf_settings;
static bool g_mivf_settings_loaded = false;
static int g_hfix59r3_settings_index = 0;
static u32 g_mivf_idle_frames = 0;
static bool g_mivf_brightness_dimmed = false;
static u32 g_mivf_brightness_active = 5;
static aptHookCookie g_mivf_apt_hook;

/* HFIX60: built-in theme accent (selected by g_mivf_settings.theme_index). */
static u8 g_mivf_theme_r = 70;
static u8 g_mivf_theme_g = 120;
static u8 g_mivf_theme_b = 210;

/* HFIX60: chapter markers loaded from a ".chapters" sidecar next to the video. */
#define MIVF_CHAP_MAX 64
typedef struct {
    u32 frame;
    char label[40];
} MivfChapter;
static MivfChapter g_mivf_chapters[MIVF_CHAP_MAX];
static int g_mivf_chapters_count = 0;

/* Playback-speed table (percent of normal). Index 2 == 100% (1.0x). */
static const u32 g_mivf_speed_table[] = { 50u, 75u, 100u, 125u, 150u, 200u };
#define MIVF_SPEED_COUNT ((int)(sizeof(g_mivf_speed_table) / sizeof(g_mivf_speed_table[0])))

static u32 mivf_speed_pct(void) {
    u32 idx = g_mivf_settings.playback_speed_idx;
    if (idx >= (u32)MIVF_SPEED_COUNT) {
        idx = 2u;
    }
    return g_mivf_speed_table[idx];
}

/* A/B scene looper state. Frame sentinels use 0xFFFFFFFF for "unset". */
#define MIVF_AB_UNSET 0xFFFFFFFFu
static u32 g_mivf_ab_a = MIVF_AB_UNSET;
static u32 g_mivf_ab_b = MIVF_AB_UNSET;
static int g_mivf_ab_state = 0; /* 0 = off, 1 = A set, 2 = looping (A+B) */

/* Sleep timer: absolute deadline in system ticks (0 = disarmed). */
static u64 g_mivf_sleep_deadline_tick = 0;
static bool g_mivf_sleep_fired = false;

/* Clamshell pause/park: remember whether to resume audio after wake. */
static bool g_mivf_park_resume_audio = false;

/* Set true when playback ends because the stream finished (vs. user quit). */
static bool g_mivf_play_reached_eof = false;

static char g_hfix58_selected_media[HFIX58_MAX_PATH] = "sdmc:/test_rawv.mivf";
static bool g_hfix58_has_selected_media = false;

typedef struct {
    u8 id;
    u8 type;
    u16 hsize;

    char codec[5];

    // For video:
    //   w, h, fpsn, fpsd
    //
    // For audio PC16:
    //   w     = sample rate
    //   h     = channels
    //   fpsn  = samples per video frame
    //   fpsd  = usually 1/reserved
    u16 w;
    u16 h;
    u16 fpsn;
    u16 fpsd;
} Stream;

typedef struct {
    u8 sid;
    u8 flags;
    u16 hsize;
    u32 psize;
    u32 frame;
} Packet;

typedef struct {
    bool ready;
    bool ndsp_ready;

    u8 sid;
    u32 rate;
    u8 channels;

    u32 samples_per_frame;
    u32 bytes_per_packet;

    u8 *buf[AUDIO_BUFS];
    ndspWaveBuf wb[AUDIO_BUFS];

    int next;
} AudioState;

static AudioState audio;

static u32 g_audio_submit = 0;
static u32 g_audio_drop = 0;
static u32 g_last_audio_bytes = 0;
static u32 g_last_audio_samples = 0;
static bool g_ndsp_ready = false;
static bool g_ndsp_init_attempted = false;

#ifndef MIVF_DISABLE_AUDIO
#define MIVF_DISABLE_AUDIO 0
#endif

static bool app_audio_system_init(void);
static void app_audio_system_shutdown(void);
static void audio_shutdown(void);

static bool audio_can_use_ndsp(void) {
    return g_ndsp_ready && audio.ready;
}

static bool audio_configure_ndsp_channel(void) {
    if (!g_ndsp_ready) {
        return false;
    }

    ndspChnReset(0);
    ndspSetOutputMode(audio.channels == 1 ? NDSP_OUTPUT_MONO : NDSP_OUTPUT_STEREO);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, (float)audio.rate * (float)mivf_speed_pct() / 100.0f);
    ndspChnSetFormat(0, audio.channels == 1 ? NDSP_FORMAT_MONO_PCM16 : NDSP_FORMAT_STEREO_PCM16);

    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 1.0f;
    mix[1] = audio.channels == 1 ? 0.0f : 1.0f;
    ndspChnSetMix(0, mix);

    return true;
}

static bool audio_dspfirm_available(void) {
    FILE *f = fopen("sdmc:/3ds/dspfirm.cdc", "rb");

    if (!f) {
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }

    long size = ftell(f);
    fclose(f);
    return size > 0;
}

static bool app_audio_system_init(void) {
    if (g_ndsp_init_attempted) {
        return g_ndsp_ready;
    }

    g_ndsp_init_attempted = true;

#if MIVF_DISABLE_AUDIO
    printf("app audio: disabled by build flag\n");
    g_ndsp_ready = false;
    return false;
#else
    printf("app audio: checking DSP firmware\n");

    if (!audio_dspfirm_available()) {
        printf("app audio: dspfirm.cdc missing or empty\n");
        printf("audio disabled\n");
        g_ndsp_ready = false;
        return false;
    }

    Result rc = ndspInit();

    if (R_FAILED(rc)) {
        printf("app audio: ndspInit failed: 0x%08lx\n", (unsigned long)rc);
        printf("audio disabled\n");
        g_ndsp_ready = false;
        return false;
    }

    g_ndsp_ready = true;
    printf("app audio: ndspInit ok\n");
    return true;
#endif
}

static void app_audio_system_shutdown(void) {
    if (g_ndsp_ready) {
        ndspExit();
        g_ndsp_ready = false;
    }
}

/* HFIX33 display-only deblocking state. */
static int g_m2y1_display_qp = 28;
static bool g_m2y1_deblock_this_frame = false;


static u8 file_iobuf[1024 * 1024];

static FILE *g_mivf_diag = NULL;

static inline u64 ticks_to_us(u64 ticks) {
    return (ticks * 1000000ULL) / (u64)SYSCLOCK_ARM11;
}

static void mivf_diag_open(void) {
    if (!g_mivf_diag) {
        g_mivf_diag = fopen("sdmc:/mivf_phase5a_diag.csv", "w");
        if (g_mivf_diag) {
            fprintf(g_mivf_diag,
                "frame,page_no,page_payload,page_packets,ring_kb,page_wait_us,parse_us,blit_us,total_us,video_pkts,audio_pkts,last_audio_bytes,last_audio_samples,audio_submit,audio_drop\n");
            fflush(g_mivf_diag);
        }
    }
}

static void mivf_diag_close(void) {
    if (g_mivf_diag) {
        fflush(g_mivf_diag);
        fclose(g_mivf_diag);
        g_mivf_diag = NULL;
    }
}

static inline u32 le32(const u8 *p) {
    return (u32)p[0] |
           ((u32)p[1] << 8) |
           ((u32)p[2] << 16) |
           ((u32)p[3] << 24);
}

static inline u16 le16(const u8 *p) {
    return (u16)p[0] | ((u16)p[1] << 8);
}

static inline u64 le64(const u8 *p) {
    return (u64)le32(p) | ((u64)le32(p + 4) << 32);
}

static int rd(FILE *f, void *p, size_t n) {
    return fread(p, 1, n, f) == n ? 0 : -1;
}

/*
    MIVF header parsing.

    The generated files used so far have a 64-byte global header,
    followed by fixed 64-byte stream headers, then page data.

    This parser is intentionally slightly defensive because earlier
    tools evolved over multiple phases.
*/
static int read_header(FILE *f, Header *h) {
    u8 probe[8192];

    memset(h, 0, sizeof(*h));
    g_mivf_stream_stride = MIVF_STREAM_HEADER_SIZE;

    if (fseek(f, 0, SEEK_SET)) {
        return -1;
    }

    size_t got = fread(probe, 1, sizeof(probe), f);

    if (got < MIVF_HEADER_SIZE) {
        return -1;
    }

    if (probe[0] != 'M' || probe[1] != 'I' || probe[2] != 'V' || probe[3] != 'F') {
        return -2;
    }

    h->duration = le64(probe + 20);
    g_hfix59r2_duration_ticks = h->duration;
    printf("HFIX59R2 header duration ticks=%llu seconds=%llu\n",
        (unsigned long long)h->duration,
        (unsigned long long)(h->duration / 30000ull));

    h->first = 0;
    h->streams = 0;

    printf("header scan HFIX6: got=%lu pagehdr=%u\n",
        (unsigned long)got,
        (unsigned)MIVF_PAGE_HEADER_SIZE);

    /*
        Find first real page header. Page header is 32 bytes for this file family.
    */
    for (u32 off = MIVF_HEADER_SIZE; off + MIVF_PAGE_HEADER_SIZE + 16 <= got; off += 4) {
        if (probe[off + 0] != 'M' || probe[off + 1] != 'P') {
            continue;
        }

        u32 payload = le32(probe + off + 0x10);
        u16 packets = le16(probe + off + 0x14);

        if (payload == 0 || payload > (512 * 1024)) {
            continue;
        }

        if (packets == 0 || packets > 128) {
            continue;
        }

        u32 packet_off = off + MIVF_PAGE_HEADER_SIZE;

        u16 pkt_hsize = le16(probe + packet_off + 2);
        u32 pkt_psize = le32(probe + packet_off + 8);

        if (pkt_hsize != 16) {
            printf("MP reject off=%lu payload=%lu packets=%u pkt_hsize=%u\n",
                (unsigned long)off,
                (unsigned long)payload,
                packets,
                pkt_hsize);
            continue;
        }

        if ((u64)pkt_hsize + (u64)pkt_psize > (u64)payload) {
            printf("MP reject off=%lu payload=%lu packets=%u pkt_psize=%lu\n",
                (unsigned long)off,
                (unsigned long)payload,
                packets,
                (unsigned long)pkt_psize);
            continue;
        }

        printf("MP accept off=%lu payload=%lu packets=%u pkt_psize=%lu\n",
            (unsigned long)off,
            (unsigned long)payload,
            packets,
            (unsigned long)pkt_psize);

        h->first = off;
        break;
    }

    if (h->first == 0) {
        printf("header scan: no valid MP page found\n");
        return -3;
    }

    /*
        Infer stream count and stride.

        HFIX5 allowed layouts with garbage streams, so count=4/stride=24
        beat count=2/stride=48 by accident. HFIX6 rejects candidate layouts
        unless EVERY inferred stream has a valid type and codec.
    */
    u32 stream_area = (u32)h->first - MIVF_HEADER_SIZE;
    u32 best_count = 0;
    u32 best_stride = 0;
    u32 best_score = 0;

    for (u32 count = 1; count <= 16; count++) {
        if (stream_area % count) {
            continue;
        }

        u32 stride = stream_area / count;

        if (stride < 24 || stride > 4096) {
            continue;
        }

        u32 score = 0;
        bool seen_video = false;
        bool seen_audio = false;
        bool candidate_valid = true;

        for (u32 i = 0; i < count; i++) {
            u32 pos = MIVF_HEADER_SIZE + i * stride;

            if (pos + 24 > h->first) {
                candidate_valid = false;
                break;
            }

            u8 sid = probe[pos + 0];
            u8 type = probe[pos + 1];

            char c0 = (char)probe[pos + 4];
            char c1 = (char)probe[pos + 5];
            char c2 = (char)probe[pos + 6];
            char c3 = (char)probe[pos + 7];

            bool codec_printable =
                c0 >= 32 && c0 <= 126 &&
                c1 >= 32 && c1 <= 126 &&
                c2 >= 32 && c2 <= 126 &&
                c3 >= 32 && c3 <= 126;

            bool codec_known =
                (c0 == 'M' && c1 == 'I' && c2 == 'V') ||
                (c0 == 'M' && c1 == '2' && c2 == 'Y' && c3 == '0') ||
                (c0 == 'M' && c1 == '2' && c2 == 'Y' && c3 == '1') ||
                (c0 == 'M' && c1 == '2' && c2 == 'Y' && c3 == '2') ||
                (c0 == 'R' && c1 == 'A' && c2 == 'W' && c3 == 'V') ||
                (c0 == 'P' && c1 == 'C' && c2 == '1' && c3 == '6') ||
                (c0 == 'I' && c1 == 'A' && c2 == '4' && c3 == 'M');

            if (!codec_printable || !codec_known) {
                candidate_valid = false;
                break;
            }

            if (type == 1) {
                seen_video = true;
                score += 100;
            } else if (type == 2) {
                seen_audio = true;
                score += 100;
            } else {
                candidate_valid = false;
                break;
            }

            if (sid < 16) {
                score += 5;
            }

            if (codec_known) {
                score += 25;
            }
        }

        if (!candidate_valid) {
            printf("stream infer reject: count=%lu stride=%lu\n",
                (unsigned long)count,
                (unsigned long)stride);
            continue;
        }

        if (seen_video) {
            score += 200;
        }

        if (seen_audio) {
            score += 250;
        }

        /*
            Prefer fewer valid streams if score is close. This avoids
            splitting metadata into fake streams.
        */
        score += (32 - count);

        printf("stream infer candidate: count=%lu stride=%lu score=%lu video=%d audio=%d\n",
            (unsigned long)count,
            (unsigned long)stride,
            (unsigned long)score,
            seen_video ? 1 : 0,
            seen_audio ? 1 : 0);

        if (score > best_score) {
            best_score = score;
            best_count = count;
            best_stride = stride;
        }
    }

    if (best_count == 0 || best_stride == 0) {
        printf("header scan: could not infer stream layout first=%lu area=%lu\n",
            (unsigned long)h->first,
            (unsigned long)stream_area);
        return -4;
    }

    h->streams = best_count;
    g_mivf_stream_stride = best_stride;

    printf("header: streams=%lu stride=%lu first=%lu\n",
        (unsigned long)h->streams,
        (unsigned long)g_mivf_stream_stride,
        (unsigned long)h->first);

    if (fseek(f, MIVF_HEADER_SIZE, SEEK_SET)) {
        return -5;
    }

    return 0;
}

static int read_stream(FILE *f, Stream *s) {
    u8 b[MIVF_STREAM_HEADER_SIZE];

    memset(s, 0, sizeof(*s));
    memset(b, 0, sizeof(b));

    long start_pos = ftell(f);

    u32 stride = g_mivf_stream_stride;

    if (stride < 24) {
        return -1;
    }

    u32 to_read = stride;

    if (to_read > MIVF_STREAM_HEADER_SIZE) {
        to_read = MIVF_STREAM_HEADER_SIZE;
    }

    if (rd(f, b, to_read)) {
        return -2;
    }

    if (stride > to_read) {
        if (fseek(f, (long)(stride - to_read), SEEK_CUR)) {
            return -3;
        }
    }

    s->id = b[0];
    s->type = b[1];
    s->hsize = (u16)stride;

    memcpy(s->codec, b + 4, 4);
    s->codec[4] = 0;

    /*
        Video streams:
            0x10 width
            0x12 height
            0x14 fps numerator
            0x16 fps denominator

        Legacy PC16 audio streams:
            0x10 channels
            0x14 sample rate
            0x2A samples per video frame, observed 533 for 16000/30

        Modern MIVF audio streams, including files written by encode_mivf.py:
            0x10 sample rate
            0x12 channels
            0x14 samples per video frame

        We map into Stream as:
            w    = sample rate
            h    = channels
            fpsn = samples per video frame
            fpsd = 1
    */
    if (!strcmp(s->codec, "PC16")) {
        u16 modern_rate = le16(b + 0x10);
        u16 modern_channels = le16(b + 0x12);
        u16 modern_samples_per_frame = le16(b + 0x14);
        u16 legacy_channels = le16(b + 0x10);
        u16 legacy_rate = le16(b + 0x14);
        u16 channels = legacy_channels;
        u16 rate = legacy_rate;
        u16 samples_per_frame = 0;

        if (to_read >= 0x2C) {
            samples_per_frame = le16(b + 0x2A);
        }

        if (modern_rate >= 8000 &&
            modern_channels >= 1 &&
            modern_channels <= 2 &&
            modern_samples_per_frame > 0) {
            rate = modern_rate;
            channels = modern_channels;
            samples_per_frame = modern_samples_per_frame;
        }

        if (channels == 0 || channels > 2) {
            channels = 1;
        }

        if (rate == 0) {
            rate = 16000;
        }

        if (samples_per_frame == 0) {
            samples_per_frame = rate / 30;
        }

        s->w = rate;
        s->h = channels;
        s->fpsn = samples_per_frame;
        s->fpsd = 1;
    } else {
        s->w = le16(b + 0x10);
        s->h = le16(b + 0x12);
        s->fpsn = le16(b + 0x14);
        s->fpsd = le16(b + 0x16);
    }

    printf("stream@%ld: id=%u type=%u stride=%u codec=%s w=%u h=%u fps=%u/%u\n",
        start_pos,
        s->id,
        s->type,
        stride,
        s->codec,
        s->w,
        s->h,
        s->fpsn,
        s->fpsd ? s->fpsd : 1);

    return 0;
}

static int read_packet(const u8 *b, size_t n, Packet *p) {
    if (n < 16) {
        return -1;
    }

    p->sid   = b[0];
    p->flags = b[1];
    p->hsize = le16(b + 2);
    p->psize = le32(b + 8);
    p->frame = le32(b + 12);

    if (p->hsize != 16) {
        return -2;
    }

    if ((u64)p->hsize + (u64)p->psize > (u64)n) {
        return -3;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Video decode helpers                                                       */
/* ------------------------------------------------------------------------- */

/* HFIX13: disable unsafe motion-vector reconstruction for artifact test */
#define MIVF_SAFE_INTER_PRED 0

enum {
    M_SKIP     = 0,
    M_RAW      = 1,
    M_SOLID    = 2,
    M_TWO      = 3,
    M_AVGDELTA = 4,
    M_MVCOPY   = 5,
    M_MVDELTA  = 6,

    /*
        HFIX16 / M1P1 extension:
        M_RUN_SKIP, run_minus_1
        run length = run_minus_1 + 1
        range = 1..256 blocks
    */
    M_RUN_SKIP = 7
};

static inline u16 rgb565_read(const u8 *p) {
    return (u16)p[0] | ((u16)p[1] << 8);
}

static inline void rgb565_write(u8 *p, u16 c) {
    p[0] = c & 255;
    p[1] = c >> 8;
}

static inline int clampi(int x, int lo, int hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static inline u16 rgb565_delta(u16 c, int dr, int dg, int db) {
    int r = (c >> 11) & 31;
    int g = (c >> 5) & 63;
    int b = c & 31;

    r = clampi(r + dr, 0, 31);
    g = clampi(g + dg, 0, 63);
    b = clampi(b + db, 0, 31);

    return (u16)((r << 11) | (g << 5) | b);
}

static void copyblk(u8 *out, const u8 *prev, int w, int bx, int by) {
    for (int y = 0; y < 8; y++) {
        memcpy(
            out  + (((by * 8 + y) * w + bx * 8) * 2),
            prev + (((by * 8 + y) * w + bx * 8) * 2),
            16
        );
    }
}

static void copy_skip_run(
    u8 *out,
    const u8 *prev,
    int w,
    int bxcount,
    int *pbx,
    int *pby,
    u32 *pbi,
    u32 run
) {
    int bx = *pbx;
    int by = *pby;
    u32 bi = *pbi;

    while (run > 0) {
        /*
            Copy as many consecutive 8x8 blocks as possible on the current
            block row.

            Each block row:
                8 pixels/block * 2 bytes/pixel = 16 bytes per block
        */
        u32 row_left = (u32)(bxcount - bx);
        u32 chunk = run < row_left ? run : row_left;
        u32 bytes = chunk * 16;

        for (int y = 0; y < 8; y++) {
            u8 *dst = out + (((by * 8 + y) * w + bx * 8) * 2);
            const u8 *src = prev + (((by * 8 + y) * w + bx * 8) * 2);

            memcpy(dst, src, bytes);
        }

        bx += (int)chunk;
        bi += chunk;
        run -= chunk;

        if (bx >= bxcount) {
            bx = 0;
            by++;
        }
    }

    *pbx = bx;
    *pby = by;
    *pbi = bi;
}

static void copymv(u8 *out, const u8 *prev, int w, int h,
                   int bx, int by, int dx, int dy) {
    int sx0 = bx * 8 + dx;
    int sy0 = by * 8 + dy;

    for (int y = 0; y < 8; y++) {
        int sy = sy0 + y;
        int dy_abs = by * 8 + y;

        if (sy < 0) sy = 0;
        if (sy >= h) sy = h - 1;

        if (dy_abs < 0 || dy_abs >= h) {
            continue;
        }

        for (int x = 0; x < 8; x++) {
            int sx = sx0 + x;
            int dx_abs = bx * 8 + x;

            if (sx < 0) sx = 0;
            if (sx >= w) sx = w - 1;

            if (dx_abs < 0 || dx_abs >= w) {
                continue;
            }

            const u8 *src = prev + ((sy * w + sx) * 2);
            u8 *dst = out + ((dy_abs * w + dx_abs) * 2);

            dst[0] = src[0];
            dst[1] = src[1];
        }
    }
}

static void delta_same(u8 *out, const u8 *prev, int w, int bx, int by, int dr, int dg, int db) {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            u8 *dst = out + (((by * 8 + y) * w + bx * 8 + x) * 2);
            const u8 *src = prev + (((by * 8 + y) * w + bx * 8 + x) * 2);

            u16 c = rgb565_read(src);
            c = rgb565_delta(c, dr, dg, db);
            rgb565_write(dst, c);
        }
    }
}

static void delta_mv(u8 *out, const u8 *prev, int w, int h,
                     int bx, int by, int dx, int dy, int dr, int dg, int db) {
    int sx0 = bx * 8 + dx;
    int sy0 = by * 8 + dy;

    for (int y = 0; y < 8; y++) {
        int sy = sy0 + y;
        int dy_abs = by * 8 + y;

        if (sy < 0) sy = 0;
        if (sy >= h) sy = h - 1;

        if (dy_abs < 0 || dy_abs >= h) {
            continue;
        }

        for (int x = 0; x < 8; x++) {
            int sx = sx0 + x;
            int dx_abs = bx * 8 + x;

            if (sx < 0) sx = 0;
            if (sx >= w) sx = w - 1;

            if (dx_abs < 0 || dx_abs >= w) {
                continue;
            }

            const u8 *src = prev + ((sy * w + sx) * 2);
            u8 *dst = out + ((dy_abs * w + dx_abs) * 2);

            u16 c = rgb565_read(src);
            c = rgb565_delta(c, dr, dg, db);
            rgb565_write(dst, c);
        }
    }
}

static int dec_m1p0(const u8 *p, size_t n, u8 *out, const u8 *prev,
                    bool have_prev, int ew, int eh) {
    if (n < 20 || memcmp(p, "M1P0", 4)) {
        return -1;
    }

    int w = le16(p + 4);
    int h = le16(p + 6);
    u32 bc = le32(p + 16);

    if (w != ew || h != eh || (w & 7) || (h & 7)) {
        return -2;
    }

    int bxcount = w / 8;
    int bycount = h / 8;

    if (bc != (u32)(bxcount * bycount)) {
        return -3;
    }

    size_t off = 20;

    for (int by = 0; by < bycount; by++) {
        for (int bx = 0; bx < bxcount; bx++) {
            if (off >= n) {
                return -4;
            }

            u8 m = p[off++];

            if (m == M_SKIP) {
                if (!have_prev) return -5;
                copyblk(out, prev, w, bx, by);
            } else if (m == M_RAW) {
                if (off + 128 > n) return -6;

                /*
                    HFIX15:
                    Fast RAW block copy. Avoid thousands of tiny memcpy calls
                    per second in all-intra/keyint=1 diagnostic files.
                */
                for (int y = 0; y < 8; y++) {
                    u32 *dst32 = (u32*)(out + (((by * 8 + y) * w + bx * 8) * 2));
                    const u32 *src32 = (const u32*)(p + off + y * 16);

                    dst32[0] = src32[0];
                    dst32[1] = src32[1];
                    dst32[2] = src32[2];
                    dst32[3] = src32[3];
                }

                off += 128;
            } else if (m == M_TWO) {
                if (off + 12 > n) return -7;

                u16 c0 = le16(p + off);
                u16 c1 = le16(p + off + 2);
                const u8 *bits = p + off + 4;

                off += 12;

                for (int i = 0; i < 64; i++) {
                    u16 c = ((bits[i >> 3] >> (i & 7)) & 1) ? c1 : c0;
                    int x = i & 7;
                    int y = i >> 3;

                    u8 *d = out + (((by * 8 + y) * w + bx * 8 + x) * 2);
                    d[0] = c & 255;
                    d[1] = c >> 8;
                }
            } else if (m == M_SOLID) {
                if (off + 2 > n) return -8;

                u16 c = le16(p + off);
                off += 2;

                for (int y = 0; y < 8; y++) {
                    u8 *d = out + (((by * 8 + y) * w + bx * 8) * 2);

                    for (int x = 0; x < 8; x++) {
                        d[x * 2 + 0] = c & 255;
                        d[x * 2 + 1] = c >> 8;
                    }
                }
            } else if (m == M_AVGDELTA) {
                if (!have_prev || off + 3 > n) return -9;

                int8_t dr = (int8_t)p[off + 0];
                int8_t dg = (int8_t)p[off + 1];
                int8_t db = (int8_t)p[off + 2];

                off += 3;

                delta_same(out, prev, w, bx, by, dr, dg, db);
            } else if (m == M_MVCOPY) {
                if (!have_prev || off + 2 > n) return -10;

                int8_t dx = (int8_t)p[off + 0];
                int8_t dy = (int8_t)p[off + 1];

                off += 2;

                copymv(out, prev, w, h, bx, by, dx, dy);
            } else if (m == M_MVDELTA) {
                if (!have_prev || off + 5 > n) return -11;

                int8_t dx = (int8_t)p[off + 0];
                int8_t dy = (int8_t)p[off + 1];
                int8_t dr = (int8_t)p[off + 2];
                int8_t dg = (int8_t)p[off + 3];
                int8_t db = (int8_t)p[off + 4];

                off += 5;

                delta_mv(out, prev, w, h, bx, by, dx, dy, dr, dg, db);
            } else {
                return -12;
            }
        }
    }

    return 0;
}

static int dec_m1p1(const u8 *p, size_t n, u8 *out, const u8 *prev,
                    bool have_prev, int ew, int eh) {
    if (n < 20 || memcmp(p, "M1P1", 4)) {
        return -1;
    }

    int w = le16(p + 4);
    int h = le16(p + 6);
    u32 bc = le32(p + 16);

    if (w != ew || h != eh || (w & 7) || (h & 7)) {
        return -2;
    }

    int bxcount = w / 8;
    int bycount = h / 8;

    if (bc != (u32)(bxcount * bycount)) {
        return -3;
    }

    size_t off = 20;

    /*
        ARM11-friendly linear block tracking.
        Avoid per-block division/modulo.
    */
    u32 bi = 0;
    int bx = 0;
    int by = 0;

    while (bi < bc) {
        if (off >= n) {
            return -4;
        }

        u8 m = p[off++];

        if (m == M_RUN_SKIP) {
            if (!have_prev) {
                return -5;
            }

            if (off >= n) {
                return -6;
            }

            u32 run = (u32)p[off++] + 1;

            if (bi + run > bc) {
                return -7;
            }

            copy_skip_run(out, prev, w, bxcount, &bx, &by, &bi, run);
            continue;
        }

        /*
            Single-block modes. These intentionally mirror dec_m1p0(),
            but advance bx/by/bi linearly instead of nested loops.
        */
        if (m == M_SKIP) {
            if (!have_prev) return -8;
            copyblk(out, prev, w, bx, by);

        } else if (m == M_RAW) {
            if (off + 128 > n) return -9;

            /*
                HFIX15 fast RAW block copy.
            */
            for (int y = 0; y < 8; y++) {
                u32 *dst32 = (u32*)(out + (((by * 8 + y) * w + bx * 8) * 2));
                const u32 *src32 = (const u32*)(p + off + y * 16);

                dst32[0] = src32[0];
                dst32[1] = src32[1];
                dst32[2] = src32[2];
                dst32[3] = src32[3];
            }

            off += 128;

        } else if (m == M_TWO) {
            if (off + 12 > n) return -10;

            u16 c0 = le16(p + off);
            u16 c1 = le16(p + off + 2);
            const u8 *bits = p + off + 4;

            off += 12;

            for (int i = 0; i < 64; i++) {
                u16 c = ((bits[i >> 3] >> (i & 7)) & 1) ? c1 : c0;
                int x = i & 7;
                int y = i >> 3;

                u8 *d = out + (((by * 8 + y) * w + bx * 8 + x) * 2);
                d[0] = c & 255;
                d[1] = c >> 8;
            }

        } else if (m == M_SOLID) {
            if (off + 2 > n) return -11;

            u16 c = le16(p + off);
            off += 2;

            for (int y = 0; y < 8; y++) {
                u8 *d = out + (((by * 8 + y) * w + bx * 8) * 2);

                for (int x = 0; x < 8; x++) {
                    d[x * 2 + 0] = c & 255;
                    d[x * 2 + 1] = c >> 8;
                }
            }

        } else if (m == M_AVGDELTA) {
            if (!have_prev || off + 3 > n) return -12;

            int8_t dr = (int8_t)p[off + 0];
            int8_t dg = (int8_t)p[off + 1];
            int8_t db = (int8_t)p[off + 2];

            off += 3;

            delta_same(out, prev, w, bx, by, dr, dg, db);

        } else if (m == M_MVCOPY) {
            if (!have_prev || off + 2 > n) return -13;

            int8_t dx = (int8_t)p[off + 0];
            int8_t dy = (int8_t)p[off + 1];

            off += 2;

#if defined(MIVF_SAFE_INTER_PRED) && MIVF_SAFE_INTER_PRED
            (void)dx;
            (void)dy;
            copyblk(out, prev, w, bx, by);
#else
            copymv(out, prev, w, h, bx, by, dx, dy);
#endif

        } else if (m == M_MVDELTA) {
            if (!have_prev || off + 5 > n) return -14;

            int8_t dx = (int8_t)p[off + 0];
            int8_t dy = (int8_t)p[off + 1];
            int8_t dr = (int8_t)p[off + 2];
            int8_t dg = (int8_t)p[off + 3];
            int8_t db = (int8_t)p[off + 4];

            off += 5;

#if defined(MIVF_SAFE_INTER_PRED) && MIVF_SAFE_INTER_PRED
            (void)dx;
            (void)dy;
            delta_same(out, prev, w, bx, by, dr, dg, db);
#else
            delta_mv(out, prev, w, h, bx, by, dx, dy, dr, dg, db);
#endif

        } else {
            return -15;
        }

        bx++;

        if (bx >= bxcount) {
            bx = 0;
            by++;
        }

        bi++;
    }

    return 0;
}


/* ------------------------------------------------------------------------- */
/* M2Y0 raw YUV420 chassis                                                    */
/* ------------------------------------------------------------------------- */

typedef struct {
    u16 w;
    u16 h;

    u32 y_size;
    u32 c_size;
    u32 total_size;

    u8 *base;
    u8 *y;
    u8 *cb;
    u8 *cr;
} M2Y0Frame;

static bool m2y0_frame_alloc(M2Y0Frame *f, u16 w, u16 h) {
    memset(f, 0, sizeof(*f));

    if ((w & 1) || (h & 1)) {
        return false;
    }

    f->w = w;
    f->h = h;

    f->y_size = (u32)w * (u32)h;
    f->c_size = ((u32)w / 2) * ((u32)h / 2);
    f->total_size = f->y_size + f->c_size + f->c_size;

    f->base = (u8*)linearAlloc(f->total_size);

    if (!f->base) {
        return false;
    }

    f->y  = f->base;
    f->cb = f->y  + f->y_size;
    f->cr = f->cb + f->c_size;

    memset(f->y, 0, f->y_size);
    memset(f->cb, 128, f->c_size);
    memset(f->cr, 128, f->c_size);

    return true;
}

static void m2y0_frame_free(M2Y0Frame *f) {
    if (f->base) {
        linearFree(f->base);
    }

    memset(f, 0, sizeof(*f));
}

static inline u8 clamp_u8_fast(int x) {
    if (x < 0) return 0;
    if (x > 255) return 255;
    return (u8)x;
}

static inline u16 rgb888_to_rgb565_fast(int r, int g, int b) {
    r = clamp_u8_fast(r);
    g = clamp_u8_fast(g);
    b = clamp_u8_fast(b);

    return (u16)(((r >> 3) << 11) |
                 ((g >> 2) << 5)  |
                 (b >> 3));
}

static inline u16 yuv_to_rgb565_pixel(u8 yy, u8 uu, u8 vv) {
    int c = (int)yy - 16;
    int d = (int)uu - 128;
    int e = (int)vv - 128;

    if (c < 0) {
        c = 0;
    }

    int r = (298 * c + 409 * e + 128) >> 8;
    int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
    int b = (298 * c + 516 * d + 128) >> 8;

    return rgb888_to_rgb565_fast(r, g, b);
}


/* ------------------------------------------------------------------------- */
/* HFIX33 display-only luma deblocking filter                                */
/* ------------------------------------------------------------------------- */

static u8 *g_m2y1_display_y = NULL;
static size_t g_m2y1_display_y_cap = 0;

static inline u8 hfix33_clamp_u8_int(int v) {
    if (v < 0) {
        return 0;
    }

    if (v > 255) {
        return 255;
    }

    return (u8)v;
}


/* HFIX35 clamp helper for advanced deblocking. */
static inline u8 hfix35_clamp_u8(int v) {
    if (v < 0) {
        return 0;
    }

    if (v > 255) {
        return 255;
    }

    return (u8)v;
}


/* HFIX36A Lossless Packed Motion Vector Modes */
#define M2Y1_MVCOPYP       14
#define M2Y1_MVQRESP       15
#define M2Y1_MVTRANSFORMP  16
#define M2Y1_MVQRESZP      17
#define M2Y1_MVTRANSFORMZP 18
#define M2Y1_GMVCOPY       19
#define M2Y1_SET_BASE_DQP 20

static inline void m2y1_unpack_mv4(u8 b, int *mx, int *my) {
    *mx = (int)(b & 15) - 8;
    *my = (int)(b >> 4) - 8;
}


/* ------------------------------------------------------------------------- */
/* HFIX53B_SMART_LUMA_DEBLOCK                                                 */
/*                                                                           */
/* Display-only luma boundary filter.                                         */
/*                                                                           */
/* This function is intentionally signature-compatible with the previous      */
/* hfix53b_m2y1_deblock_plane_luma(src_plane, w, h, qp) call site. It only operates   */
/* on the temporary display Y copy created by m2y1_get_display_y_copy().       */
/* It must never be called on closed-loop reference planes.                    */
/* ------------------------------------------------------------------------- */
static void hfix53b_m2y1_deblock_plane_luma(u8 *src_plane, int w, int h, int qp) {
    if (!src_plane || w < 16 || h < 16) {
        return;
    }

    /*
        QP-scaled thresholds.

        alpha controls whether the boundary jump is small enough to smooth.
        beta rejects true high-detail edges around line art / character edges.
    */
    int alpha = 8 + (qp >> 2);
    int beta  = 4 + (qp >> 3);

    if (alpha < 8) {
        alpha = 8;
    }

    if (alpha > 24) {
        alpha = 24;
    }

    if (beta < 3) {
        beta = 3;
    }

    if (beta > 10) {
        beta = 10;
    }

    /*
        Pass 1: vertical 8x8 block boundaries.

        Boundary model:
            p1 p0 | q0 q1

        Smooth only modest artificial steps and reject real high-contrast
        detail by comparing local gradients on both sides.
    */
    for (int y = 0; y < h; y++) {
        u8 *row = src_plane + y * w;

        /*
            x = 8,16,24...
            Stop before w so q1 is safe.
        */
        for (int x = 8; x < w - 1; x += 8) {
            int p1 = row[x - 2];
            int p0 = row[x - 1];
            int q0 = row[x + 0];
            int q1 = row[x + 1];

            int edge = q0 - p0;
            if (edge < 0) {
                edge = -edge;
            }

            int gp = p0 - p1;
            if (gp < 0) {
                gp = -gp;
            }

            int gq = q1 - q0;
            if (gq < 0) {
                gq = -gq;
            }

            if (edge > 0 && edge < alpha && gp < beta && gq < beta) {
                int np0 = (p1 + (p0 << 1) + q0 + 2) >> 2;
                int nq0 = (p0 + (q0 << 1) + q1 + 2) >> 2;

                row[x - 1] = (u8)np0;
                row[x + 0] = (u8)nq0;
            }
        }
    }

    /*
        Pass 2: horizontal 8x8 block boundaries.

        Boundary model:
            p1
            p0
            --
            q0
            q1
    */
    for (int y = 8; y < h - 1; y += 8) {
        u8 *row_p1 = src_plane + (y - 2) * w;
        u8 *row_p0 = src_plane + (y - 1) * w;
        u8 *row_q0 = src_plane + (y + 0) * w;
        u8 *row_q1 = src_plane + (y + 1) * w;

        for (int x = 0; x < w; x++) {
            int p1 = row_p1[x];
            int p0 = row_p0[x];
            int q0 = row_q0[x];
            int q1 = row_q1[x];

            int edge = q0 - p0;
            if (edge < 0) {
                edge = -edge;
            }

            int gp = p0 - p1;
            if (gp < 0) {
                gp = -gp;
            }

            int gq = q1 - q0;
            if (gq < 0) {
                gq = -gq;
            }

            if (edge > 0 && edge < alpha && gp < beta && gq < beta) {
                int np0 = (p1 + (p0 << 1) + q0 + 2) >> 2;
                int nq0 = (p0 + (q0 << 1) + q1 + 2) >> 2;

                row_p0[x] = (u8)np0;
                row_q0[x] = (u8)nq0;
            }
        }
    }
}

static void m2y1_deblock_plane_luma(u8 *src_plane, int w, int h, int qp) {
    /*
        HFIX35:
        Advanced display-only multi-tap deblocking.

        This function is called only on the temporary display Y copy created
        by m2y1_get_display_y_copy(). It must never operate on the closed-loop
        decoder reference plane.
    */
    if (!src_plane || w < 16 || h < 16) {
        return;
    }

    int alpha = (qp / 2) + 2;

    if (alpha < 4) {
        alpha = 4;
    }

    if (alpha > 32) {
        alpha = 32;
    }

    int beta = (qp / 4) + 1;

    if (beta < 2) {
        beta = 2;
    }

    if (beta > 16) {
        beta = 16;
    }

    /*
        Pass 1:
        Vertical block boundaries at x = 8, 16, 24...
        Context:
            p2 p1 p0 | q0 q1 q2
    */
    for (int y = 0; y < h; y++) {
        int y_offset = y * w;

        for (int bx = 1; bx < (w / 8); bx++) {
            int edge = y_offset + bx * 8;

            /*
                Since bx starts at 1 and block size is 8:
                    edge >= 8
                    p2_idx = edge - 3 is safe.
                Since bx < w/8:
                    edge <= w - 8
                    q2_idx = edge + 2 is safe.
            */
            int p0_idx = edge - 1;
            int p1_idx = edge - 2;
            int p2_idx = edge - 3;
            int q0_idx = edge;
            int q1_idx = edge + 1;
            int q2_idx = edge + 2;

            int p0 = src_plane[p0_idx];
            int p1 = src_plane[p1_idx];
            int p2 = src_plane[p2_idx];
            int q0 = src_plane[q0_idx];
            int q1 = src_plane[q1_idx];
            int q2 = src_plane[q2_idx];

            int d0 = p0 - q0;
            int ad0 = d0 < 0 ? -d0 : d0;

            if (ad0 > 0 && ad0 < alpha) {
                int ap = p1 - p0;
                int aq = q1 - q0;

                if (ap < 0) {
                    ap = -ap;
                }

                if (aq < 0) {
                    aq = -aq;
                }

                if (ap < beta && aq < beta) {
                    /*
                        Strong filter:
                        Smooth p1,p0,q0,q1 across flat-region seams.
                    */
                    src_plane[p0_idx] = hfix35_clamp_u8(
                        (p2 + (p1 << 1) + (p0 << 1) + (q0 << 1) + q1 + 4) >> 3
                    );

                    src_plane[q0_idx] = hfix35_clamp_u8(
                        (p1 + (p0 << 1) + (q0 << 1) + (q1 << 1) + q2 + 4) >> 3
                    );

                    src_plane[p1_idx] = hfix35_clamp_u8(
                        (p2 + p1 + p0 + q0 + 2) >> 2
                    );

                    src_plane[q1_idx] = hfix35_clamp_u8(
                        (p0 + q0 + q1 + q2 + 2) >> 2
                    );
                } else {
                    /*
                        Weak filter:
                        Preserve texture/detail while reducing boundary ridge.
                    */
                    int delta = (3 * d0) >> 3;

                    if (delta > 8) {
                        delta = 8;
                    }

                    if (delta < -8) {
                        delta = -8;
                    }

                    src_plane[p0_idx] = hfix35_clamp_u8(p0 - delta);
                    src_plane[q0_idx] = hfix35_clamp_u8(q0 + delta);
                }
            }
        }
    }

    /*
        Pass 2:
        Horizontal block boundaries at y = 8, 16, 24...
        Context:
            p2
            p1
            p0
            --
            q0
            q1
            q2
    */
    for (int by = 1; by < (h / 8); by++) {
        int edge_row_offset = by * 8 * w;

        for (int x = 0; x < w; x++) {
            int edge = edge_row_offset + x;

            /*
                Since by starts at 1:
                    p2_idx = edge - 3*w is safe.
                Since by < h/8:
                    q2_idx = edge + 2*w is safe.
            */
            int p0_idx = edge - w;
            int p1_idx = edge - (w * 2);
            int p2_idx = edge - (w * 3);
            int q0_idx = edge;
            int q1_idx = edge + w;
            int q2_idx = edge + (w * 2);

            int p0 = src_plane[p0_idx];
            int p1 = src_plane[p1_idx];
            int p2 = src_plane[p2_idx];
            int q0 = src_plane[q0_idx];
            int q1 = src_plane[q1_idx];
            int q2 = src_plane[q2_idx];

            int d0 = p0 - q0;
            int ad0 = d0 < 0 ? -d0 : d0;

            if (ad0 > 0 && ad0 < alpha) {
                int ap = p1 - p0;
                int aq = q1 - q0;

                if (ap < 0) {
                    ap = -ap;
                }

                if (aq < 0) {
                    aq = -aq;
                }

                if (ap < beta && aq < beta) {
                    /*
                        Strong filter.
                    */
                    src_plane[p0_idx] = hfix35_clamp_u8(
                        (p2 + (p1 << 1) + (p0 << 1) + (q0 << 1) + q1 + 4) >> 3
                    );

                    src_plane[q0_idx] = hfix35_clamp_u8(
                        (p1 + (p0 << 1) + (q0 << 1) + (q1 << 1) + q2 + 4) >> 3
                    );

                    src_plane[p1_idx] = hfix35_clamp_u8(
                        (p2 + p1 + p0 + q0 + 2) >> 2
                    );

                    src_plane[q1_idx] = hfix35_clamp_u8(
                        (p0 + q0 + q1 + q2 + 2) >> 2
                    );
                } else {
                    /*
                        Weak filter.
                    */
                    int delta = (3 * d0) >> 3;

                    if (delta > 8) {
                        delta = 8;
                    }

                    if (delta < -8) {
                        delta = -8;
                    }

                    src_plane[p0_idx] = hfix35_clamp_u8(p0 - delta);
                    src_plane[q0_idx] = hfix35_clamp_u8(q0 + delta);
                }
            }
        }
    }
}

static u8 *m2y1_get_display_y_copy(const u8 *src_y, int w, int h, int qp) {
    if (!src_y || w <= 0 || h <= 0) {
        return NULL;
    }

    size_t need = (size_t)w * (size_t)h;

    if (g_m2y1_display_y_cap < need) {
        if (g_m2y1_display_y) {
            linearFree(g_m2y1_display_y);
            g_m2y1_display_y = NULL;
            g_m2y1_display_y_cap = 0;
        }

        g_m2y1_display_y = (u8*)linearAlloc(need);

        if (!g_m2y1_display_y) {
            return NULL;
        }

        g_m2y1_display_y_cap = need;
    }

    memcpy(g_m2y1_display_y, src_y, need);

    /*
        Critical:
        This modifies only the display copy, never the closed-loop reference.
    */
    hfix53b_m2y1_deblock_plane_luma(g_m2y1_display_y, w, h, qp);

    return g_m2y1_display_y;
}


/* ------------------------------------------------------------------------- */
/* HFIX51B_DIRECT_VRAM: Direct YUV420 -> rotated top RGB565 framebuffer        */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* HFIX51C: throttled bottom UI + unified presentation finish                  */
/* ------------------------------------------------------------------------- */
#define HFIX51C_DIRECT_UI 1

typedef enum {
    STATE_STOPPED,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_FAST_FORWARD,
    STATE_REWIND
} MediaPlaybackState;

typedef struct {
    MediaPlaybackState state;
    u32 current_frame_idx;
    u32 total_frames;
    int dummy_seek_state; /* -1 = rewind highlight, 1 = fwd highlight, 0 = idle */
    bool ui_visible;
} MediaPlaybackController;

static MediaPlaybackController g_media_ctl = {
    STATE_PLAYING,
    0,
    1866,
    0,
    true
};

static void hfix51c_draw_rect_bgr8(
    u8 *fb,
    int x0,
    int y0,
    int rw,
    int rh,
    u8 r,
    u8 g,
    u8 b
) {
    if (!fb) {
        return;
    }

    for (int x = x0; x < x0 + rw; x++) {
        if (x < 0 || x >= 320) {
            continue;
        }

        for (int y = y0; y < y0 + rh; y++) {
            if (y < 0 || y >= 240) {
                continue;
            }

            int idx = (x * 240) + (240 - 1 - y);

            fb[idx * 3 + 0] = b;
            fb[idx * 3 + 1] = g;
            fb[idx * 3 + 2] = r;
        }
    }
}


/* ------------------------------------------------------------------------- */
/* HFIX51C_P1_RGB565_BOTTOM_UI                                                */
/*                                                                           */
/* The bottom framebuffer is rendered as RGB565/u16. The previous BGR8-style  */
/* byte writes caused repeated/tiled UI artifacts and extra memory traffic.    */
/* ------------------------------------------------------------------------- */
static inline u16 hfix51c_ui_rgb565(int r, int g, int b) {
    return rgb888_to_rgb565_fast(r, g, b);
}

static inline void hfix51c_bottom_px565(u8 *fb8, int x, int y, u16 c) {
    if (!fb8) {
        return;
    }

    if (x < 0 || x >= 320 || y < 0 || y >= 240) {
        return;
    }

    u16 *fb = (u16*)fb8;

    /*
        3DS framebuffer is rotated.
    */
    fb[x * 240 + (239 - y)] = c;
}

static void hfix51c_draw_rect565(
    u8 *fb8,
    int x0,
    int y0,
    int rw,
    int rh,
    int r,
    int g,
    int b
) {
    if (!fb8) {
        return;
    }

    u16 c = hfix51c_ui_rgb565(r, g, b);

    int x1 = x0 + rw;
    int y1 = y0 + rh;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > 320) x1 = 320;
    if (y1 > 240) y1 = 240;

    for (int x = x0; x < x1; x++) {
        u16 *col = ((u16*)fb8) + x * 240;

        for (int y = y0; y < y1; y++) {
            col[239 - y] = c;
        }
    }
}


/* ------------------------------------------------------------------------- */
/* HFIX57A_TOUCH_TRANSPORT_POLISH                                             */
/*                                                                           */
/* Larger touchscreen hit targets plus a 3DS-style dark transport dock.       */
/* Center touch toggles real pause/play through existing KEY_A path.          */
/* Left/right touch activate existing scan/highlight controls until indexed   */
/* keyframe seeking is implemented.                                           */
/* ------------------------------------------------------------------------- */

typedef enum {
    HFIX57_TOUCH_NONE = 0,
    HFIX57_TOUCH_REWIND = 1,
    HFIX57_TOUCH_PLAY = 2,
    HFIX57_TOUCH_FORWARD = 3
} Hfix57TouchButton;

static Hfix57TouchButton g_hfix57_touch_button = HFIX57_TOUCH_NONE;

/*
    Visual button rectangles.
*/
#define HFIX57_DOCK_X 18
#define HFIX57_DOCK_Y 112
#define HFIX57_DOCK_W 284
#define HFIX57_DOCK_H 78

#define HFIX57_BTN_W 70
#define HFIX57_BTN_H 54
#define HFIX57_BTN_Y 124
#define HFIX57_LEFT_X 34
#define HFIX57_PLAY_X 125
#define HFIX57_RIGHT_X 216

static bool hfix57_point_in_rect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static Hfix57TouchButton hfix57_hit_transport(int px, int py) {
    if (hfix57_point_in_rect(px, py, HFIX57_LEFT_X, HFIX57_BTN_Y, HFIX57_BTN_W, HFIX57_BTN_H)) {
        return HFIX57_TOUCH_REWIND;
    }

    if (hfix57_point_in_rect(px, py, HFIX57_PLAY_X, HFIX57_BTN_Y, HFIX57_BTN_W, HFIX57_BTN_H)) {
        return HFIX57_TOUCH_PLAY;
    }

    if (hfix57_point_in_rect(px, py, HFIX57_RIGHT_X, HFIX57_BTN_Y, HFIX57_BTN_W, HFIX57_BTN_H)) {
        return HFIX57_TOUCH_FORWARD;
    }

    return HFIX57_TOUCH_NONE;
}

/*
    Convert touch presses into the same logical input flags the keyboard path
    already understands. This keeps pause/audio gating centralized in the
    existing KEY_A handler.
*/
static u32 hfix57_touch_transport_to_keys(u32 keys_down, u32 keys_held) {
    if (!(keys_held & KEY_TOUCH)) {
        if (g_hfix57_touch_button != HFIX57_TOUCH_NONE) {
            g_hfix57_touch_button = HFIX57_TOUCH_NONE;
            g_media_ctl.dummy_seek_state = 0;
        }

        return 0;
    }

    touchPosition touch;
    hidTouchRead(&touch);

    Hfix57TouchButton hit = hfix57_hit_transport((int)touch.px, (int)touch.py);
    g_hfix57_touch_button = hit;

    if (hit == HFIX57_TOUCH_REWIND) {
        return KEY_LEFT;
    }

    if (hit == HFIX57_TOUCH_FORWARD) {
        return KEY_RIGHT;
    }

    if (hit == HFIX57_TOUCH_PLAY) {
        if (keys_down & KEY_TOUCH) {
            return KEY_A;
        }

        return 0;
    }

    return 0;
}

static void hfix57_draw_button_frame(
    u8 *fb,
    int x,
    int y,
    int w,
    int h,
    int r,
    int g,
    int b,
    bool pressed
) {
    int dr = pressed ? r / 2 : r;
    int dg = pressed ? g / 2 : g;
    int db = pressed ? b / 2 : b;

    /*
        Faux rounded/beveled rectangle using layered rects.
    */
    hfix51c_draw_rect565(fb, x + 4, y + 0, w - 8, h,     12, 16, 28);
    hfix51c_draw_rect565(fb, x + 0, y + 4, w,     h - 8, 12, 16, 28);

    hfix51c_draw_rect565(fb, x + 5, y + 3, w - 10, h - 6, dr, dg, db);
    hfix51c_draw_rect565(fb, x + 3, y + 6, w - 6,  h - 12, dr, dg, db);

    /*
        Highlight and shadow rails.
    */
    hfix51c_draw_rect565(fb, x + 7, y + 5, w - 14, 2, 120, 170, 230);
    hfix51c_draw_rect565(fb, x + 7, y + h - 7, w - 14, 2, 5, 8, 16);

    if (pressed) {
        hfix51c_draw_rect565(fb, x + 5, y + 3, w - 10, h - 6, 45, 85, 140);
        hfix51c_draw_rect565(fb, x + 8, y + 6, w - 16, 2, 120, 190, 255);
    }
}

static void hfix57_draw_left_tri(u8 *fb, int tip_x, int cy, int half_h, u16 white) {
    for (int dx = 0; dx < half_h; dx++) {
        int x = tip_x + dx;

        for (int y = cy - dx; y <= cy + dx; y++) {
            hfix51c_bottom_px565(fb, x, y, white);
        }
    }
}

static void hfix57_draw_right_tri(u8 *fb, int tip_x, int cy, int half_h, u16 white) {
    for (int dx = 0; dx < half_h; dx++) {
        int x = tip_x - dx;

        for (int y = cy - dx; y <= cy + dx; y++) {
            hfix51c_bottom_px565(fb, x, y, white);
        }
    }
}

static void hfix57_draw_transport_dock(u8 *fb) {
    if (!fb) {
        return;
    }

    /*
        Dark universal 3DS-style dock.
    */
    hfix51c_draw_rect565(fb, HFIX57_DOCK_X + 6, HFIX57_DOCK_Y, HFIX57_DOCK_W - 12, HFIX57_DOCK_H, 4, 8, 18);
    hfix51c_draw_rect565(fb, HFIX57_DOCK_X, HFIX57_DOCK_Y + 6, HFIX57_DOCK_W, HFIX57_DOCK_H - 12, 4, 8, 18);
    hfix51c_draw_rect565(fb, HFIX57_DOCK_X + 3, HFIX57_DOCK_Y + 3, HFIX57_DOCK_W - 6, HFIX57_DOCK_H - 6, 14, 20, 38);
    hfix51c_draw_rect565(fb, HFIX57_DOCK_X + 8, HFIX57_DOCK_Y + 7, HFIX57_DOCK_W - 16, 2, 72, 116, 190);
    hfix51c_draw_rect565(fb, HFIX57_DOCK_X + 8, HFIX57_DOCK_Y + HFIX57_DOCK_H - 9, HFIX57_DOCK_W - 16, 2, 2, 4, 8);

    /*
        Small touch hint rails.
    */
    hfix51c_draw_rect565(fb, 54, 184, 36, 3, 45, 80, 130);
    hfix51c_draw_rect565(fb, 142, 184, 36, 3, 45, 130, 80);
    hfix51c_draw_rect565(fb, 230, 184, 36, 3, 45, 80, 130);

    bool left_pressed =
        (g_media_ctl.dummy_seek_state == -1) ||
        (g_hfix57_touch_button == HFIX57_TOUCH_REWIND);

    bool right_pressed =
        (g_media_ctl.dummy_seek_state == 1) ||
        (g_hfix57_touch_button == HFIX57_TOUCH_FORWARD);

    bool play_pressed =
        (g_hfix57_touch_button == HFIX57_TOUCH_PLAY);

    /*
        Button colors.
    */
    hfix57_draw_button_frame(
        fb,
        HFIX57_LEFT_X,
        HFIX57_BTN_Y,
        HFIX57_BTN_W,
        HFIX57_BTN_H,
        left_pressed ? 20 : 44,
        left_pressed ? 100 : 54,
        left_pressed ? 190 : 76,
        left_pressed
    );

    if (g_media_ctl.state == STATE_PLAYING) {
        hfix57_draw_button_frame(
            fb,
            HFIX57_PLAY_X,
            HFIX57_BTN_Y,
            HFIX57_BTN_W,
            HFIX57_BTN_H,
            play_pressed ? 30 : 40,
            play_pressed ? 130 : 180,
            play_pressed ? 50 : 70,
            play_pressed
        );
    } else {
        hfix57_draw_button_frame(
            fb,
            HFIX57_PLAY_X,
            HFIX57_BTN_Y,
            HFIX57_BTN_W,
            HFIX57_BTN_H,
            play_pressed ? 150 : 235,
            play_pressed ? 90 : 140,
            play_pressed ? 0 : 20,
            play_pressed
        );
    }

    hfix57_draw_button_frame(
        fb,
        HFIX57_RIGHT_X,
        HFIX57_BTN_Y,
        HFIX57_BTN_W,
        HFIX57_BTN_H,
        right_pressed ? 20 : 44,
        right_pressed ? 100 : 54,
        right_pressed ? 190 : 76,
        right_pressed
    );

    u16 white = hfix51c_ui_rgb565(245, 245, 245);

    /*
        Rewind << glyph.
    */
    int lcy = HFIX57_BTN_Y + 27;
    hfix57_draw_left_tri(fb, HFIX57_LEFT_X + 25, lcy, 15, white);
    hfix57_draw_left_tri(fb, HFIX57_LEFT_X + 44, lcy, 15, white);

    /*
        Center play/pause glyph.
        While playing, show pause bars.
        While paused, show play triangle.
    */
    int pcx = HFIX57_PLAY_X;
    int pcy = HFIX57_BTN_Y + 27;

    if (g_media_ctl.state == STATE_PLAYING) {
        hfix51c_draw_rect565(fb, pcx + 24, HFIX57_BTN_Y + 15, 7, 25, 245, 245, 245);
        hfix51c_draw_rect565(fb, pcx + 40, HFIX57_BTN_Y + 15, 7, 25, 245, 245, 245);
    } else {
        for (int dx = 0; dx < 22; dx++) {
            int x = pcx + 26 + dx;

            for (int y = pcy - dx; y <= pcy + dx; y++) {
                hfix51c_bottom_px565(fb, x, y, white);
            }
        }
    }

    /*
        Fast-forward >> glyph.
    */
    int rcy = HFIX57_BTN_Y + 27;
    hfix57_draw_right_tri(fb, HFIX57_RIGHT_X + 45, rcy, 15, white);
    hfix57_draw_right_tri(fb, HFIX57_RIGHT_X + 26, rcy, 15, white);
}



/* ------------------------------------------------------------------------- */
/* HFIX58A_R5_FORWARD_DECLS                                                   */
/* Forward declarations for polished alerts/file browser.                     */
/* ------------------------------------------------------------------------- */
static void hfix58_alert_set(const char *msg, int level);
static void hfix58_draw_alert(u8 *fb);
static bool hfix58_file_browser_select(char *out_path, size_t out_sz);


/* ------------------------------------------------------------------------- */
/* HFIX58B_FORWARD_DECLS                                                      */
/* ------------------------------------------------------------------------- */
static void hfix58b_ui_init_once(void);
static void hfix58b_draw_bottom_glass_ui(u8 *fb);
static void hfix58b_transport_handle_input(u32 down, u32 held);


/* ------------------------------------------------------------------------- */

/*                                                                           */
/* These must appear before hfix51c_draw_bottom_ui_throttled(), because that  */
/* function reads g_mivf_ui_skin.selected_index / play_pause.pressed.         */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* HFIX58B_R3_ACCESSOR_FORWARD_DECLS                                          */
/*                                                                           */
/* hfix51c_draw_bottom_ui_throttled() appears before the full HFIX58B skin    */
/* type/global declarations. Use accessors so the throttler does not need     */
/* to know MivfTransportSkin yet.                                             */
/* ------------------------------------------------------------------------- */
static int hfix58b_get_selected_index(void);
static bool hfix58b_get_play_pressed(void);


/* ------------------------------------------------------------------------- */
/* HFIX58D_FORWARD_DECLS                                                      */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* HFIX58J_SYSTEM_OVERLAY                                                     */
/* ------------------------------------------------------------------------- */
static void hfix58j_draw_system_overlay(u8 *fb, int top_y_offset) {
    time_t unix_time = time(NULL);
    struct tm *ts = gmtime(&unix_time);
    char clock_txt[16];

    if (!ts) {
        snprintf(clock_txt, sizeof(clock_txt), "--:--");
    } else {
        snprintf(clock_txt, sizeof(clock_txt), "%02d:%02d", ts->tm_hour, ts->tm_min);
    }

    int y = 22 + top_y_offset;
    if (y < -12 || y > 240) return;

    u8 battery = 0;
    (void)PTMU_GetBatteryLevel(&battery);
    if (battery > 5) battery = 5;

    /* Move battery slightly up and tightly right */
    int bx = 282;
    int by = y - 2;

    hfix58_rect565(fb, bx, by, 22, 9, 90, 110, 130);
    hfix58_rect565(fb, bx + 22, by + 3, 2, 3, 90, 110, 130);
    hfix58_rect565(fb, bx + 2, by + 2, 18, 5, 3, 6, 14);

    for (u8 i = 0; i < battery; i++) {
        hfix58_rect565(fb, bx + 3 + i * 3, by + 3, 2, 3, 80, 220, 120);
    }

    /* Tuck clock right-aligned directly beneath battery */
    hfix58_draw_text_shadow(fb, 268, y + 10, clock_txt, 1, 220, 235, 250);
}


/* ------------------------------------------------------------------------- */
/* HFIX58S_SUBTITLE_OVERLAY_HELPERS                                           */
/* ------------------------------------------------------------------------- */
static MivfSubtitles g_hfix58s_subtitles;
static bool g_hfix58s_subtitles_initialized = false;
static bool g_hfix58s_subtitles_ready = false;
static uint32_t g_hfix58s_subtitle_now_ms = 0;
static char g_hfix58s_subtitle_current[MIVF_SUBTITLE_MAX_TEXT];

static void hfix58s_subtitles_init_once(void) {
    if (!g_hfix58s_subtitles_initialized) {
        MIVF_SubtitlesInit(&g_hfix58s_subtitles);
        g_hfix58s_subtitles_initialized = true;
        g_hfix58s_subtitle_current[0] = 0;
    }
}

static void hfix58s_subtitles_unload(void) {
    hfix58s_subtitles_init_once();
    MIVF_SubtitlesFree(&g_hfix58s_subtitles);
    MIVF_SubtitlesInit(&g_hfix58s_subtitles);
    g_hfix58s_subtitles_ready = false;
    g_hfix58s_subtitle_now_ms = 0;
    g_hfix58s_subtitle_current[0] = 0;
}

static bool hfix58s_subtitles_load_for_video(const char *video_path) {
    char sidecar[MIVF_SUBTITLE_MAX_PATH];
    char alt_path[MIVF_SUBTITLE_MAX_PATH];
    char *dot;

    hfix58s_subtitles_unload();

    if (!video_path || !*video_path) {
        return false;
    }

    if (!g_mivf_settings.show_subtitle_tracks) {
        return false;
    }

    if (!MIVF_SubtitlesMakeSidecarPath(video_path, sidecar, sizeof(sidecar))) {
        return false;
    }

    if (g_mivf_settings.subtitle_track_index > 0) {
        snprintf(alt_path, sizeof(alt_path), "%s", sidecar);
        dot = strrchr(alt_path, '.');
        if (dot) {
            snprintf(dot, (size_t)(alt_path + sizeof(alt_path) - dot), ".%lu.srt", (unsigned long)g_mivf_settings.subtitle_track_index);
        }

        if (MIVF_SubtitlesLoadSrt(&g_hfix58s_subtitles, alt_path)) {
            g_hfix58s_subtitles_ready = true;
            return true;
        }
    }

    g_hfix58s_subtitles_ready = MIVF_SubtitlesLoadSrt(&g_hfix58s_subtitles, sidecar);

    return g_hfix58s_subtitles_ready;
}

static void hfix58s_subtitles_set_ms(uint32_t now_ms) {
    const char *txt;

    hfix58s_subtitles_init_once();

    if (!g_hfix58s_subtitles_ready) {
        g_hfix58s_subtitle_current[0] = 0;
        return;
    }

    g_hfix58s_subtitle_now_ms = now_ms;

    txt = MIVF_SubtitlesTextAtMs(&g_hfix58s_subtitles, now_ms);

    if (txt && *txt) {
        strncpy(g_hfix58s_subtitle_current, txt, sizeof(g_hfix58s_subtitle_current) - 1);
        g_hfix58s_subtitle_current[sizeof(g_hfix58s_subtitle_current) - 1] = 0;
    } else {
        g_hfix58s_subtitle_current[0] = 0;
    }
}

static void hfix58s_subtitles_set_frame_time(
    uint32_t frame_index,
    uint32_t fps_num,
    uint32_t fps_den
) {
    int64_t now_ms;

    if (fps_num == 0 || fps_den == 0) {
        g_hfix58s_subtitle_current[0] = 0;
        return;
    }

    now_ms =
        (int64_t)(((uint64_t)frame_index * 1000ull * (uint64_t)fps_den) /
                  (uint64_t)fps_num);

    now_ms += (int64_t)g_mivf_settings.subtitle_delay_ms;

    if (now_ms < 0) {
        g_hfix58s_subtitle_current[0] = 0;
        return;
    }

    hfix58s_subtitles_set_ms((uint32_t)now_ms);
}

static void hfix58s_draw_subtitle_overlay(u8 *fb) {
    int box_x = 18;
    int box_y = 184;
    int box_w = 284;
    int box_h = 24;

    if (!fb || !g_hfix58s_subtitles_ready || !g_hfix58s_subtitle_current[0]) {
        return;
    }

    hfix58_rect565(fb, box_x - 2, box_y - 2, box_w + 4, box_h + 4, 0, 1, 4);
    hfix58_rect565(fb, box_x, box_y, box_w, box_h, 3, 8, 18);
    hfix58_rect565(fb, box_x + 2, box_y + 2, box_w - 4, 1, 30, 120, 190);

    hfix58_draw_text_shadow(
        fb,
        box_x + 8,
        box_y + 8,
        g_hfix58s_subtitle_current,
        1,
        235,
        245,
        255
    );
}

/* HFIX60: replace the extension of a .mivf path with another sidecar ext. */
static void hfix60_make_sidecar_path(char *out, size_t out_sz, const char *mivf_path, const char *ext) {
    char *dot;

    if (!out || out_sz == 0) {
        return;
    }

    out[0] = 0;

    if (!mivf_path || !ext) {
        return;
    }

    snprintf(out, out_sz, "%s", mivf_path);

    dot = strrchr(out, '.');
    if (dot && dot > out) {
        snprintf(dot, out_sz - (size_t)(dot - out), "%s", ext);
    } else {
        size_t len = strlen(out);
        if (len + strlen(ext) < out_sz) {
            strcat(out, ext);
        }
    }
}

/* HFIX60: select a built-in UI accent palette from theme_index. */
static void hfix60_apply_theme(u32 idx) {
    switch (idx & 3u) {
        case 0: g_mivf_theme_r = 70;  g_mivf_theme_g = 120; g_mivf_theme_b = 210; break; /* blue   */
        case 1: g_mivf_theme_r = 0;   g_mivf_theme_g = 170; g_mivf_theme_b = 95;  break; /* green  */
        case 2: g_mivf_theme_r = 150; g_mivf_theme_g = 80;  g_mivf_theme_b = 220; break; /* purple */
        case 3: g_mivf_theme_r = 235; g_mivf_theme_g = 140; g_mivf_theme_b = 40;  break; /* amber  */
        default: g_mivf_theme_r = 70; g_mivf_theme_g = 120; g_mivf_theme_b = 210; break;
    }
}

static const char *hfix60_theme_name(u32 idx) {
    switch (idx & 3u) {
        case 0: return "AQUA";
        case 1: return "MINT";
        case 2: return "VIOLET";
        case 3: return "AMBER";
        default: return "AQUA";
    }
}

static const char *hfix60_aspect_name(u32 idx) {
    static const char *names[] = { "FIT", "FILL", "NATIVE" };
    return names[idx % 3u];
}

static const char *hfix60_subtitle_pos_name(u32 idx) {
    static const char *names[] = { "LOW", "MID", "HIGH" };
    return names[idx % 3u];
}

static void hfix59r3_apply_screen_brightness(bool dimmed) {
    u32 brightness = dimmed ? g_mivf_settings.autodim_brightness : g_mivf_brightness_active;

    if (brightness < 1u) brightness = 1u;
    if (brightness > 5u) brightness = 5u;

    GSPLCD_SetBrightness(GSPLCD_SCREEN_BOTH, brightness);
    g_mivf_brightness_dimmed = dimmed;
}

static void hfix59r3_sync_runtime_settings(void) {
    g_hfix56_force_stereo = g_mivf_settings.force_stereo;

    if (g_mivf_settings.autodim_brightness < 1u) {
        g_mivf_settings.autodim_brightness = 1u;
    }

    if (g_mivf_settings.autodim_brightness > 5u) {
        g_mivf_settings.autodim_brightness = 5u;
    }

    if (g_mivf_settings.active_brightness < 1u) {
        g_mivf_settings.active_brightness = 1u;
    }

    if (g_mivf_settings.active_brightness > 5u) {
        g_mivf_settings.active_brightness = 5u;
    }

    g_mivf_brightness_active = g_mivf_settings.active_brightness;

    hfix60_apply_theme(g_mivf_settings.theme_index);

    if (g_mivf_settings.autodim_timeout_frames < 30u) {
        g_mivf_settings.autodim_timeout_frames = 30u;
    }
}

static void hfix59r3_apply_runtime_settings(void) {
    hfix59r3_sync_runtime_settings();
    aptSetSleepAllowed(g_mivf_settings.autosleep_allowed);
}

static void hfix59r3_note_activity(void) {
    g_mivf_idle_frames = 0;
    if (g_mivf_settings.autodim_enabled && g_mivf_brightness_dimmed) {
        hfix59r3_apply_screen_brightness(false);
    }
}

static void hfix59r3_set_settings_open(bool open) {
    if (open) {
        if (!g_hfix59r3_settings_visible) {
            g_hfix59r3_resume_after_settings = (g_media_ctl.state == STATE_PLAYING);
            if (g_media_ctl.state == STATE_PLAYING) {
                g_media_ctl.state = STATE_PAUSED;
                if (audio_can_use_ndsp()) {
                    ndspChnSetPaused(0, true);
                }
            }
        }

        g_hfix59r3_settings_visible = true;
        g_media_ctl.ui_visible = true;
        hfix58_alert_set("SETTINGS", 1);
    } else {
        g_hfix59r3_settings_visible = false;
        if (g_hfix59r3_resume_after_settings) {
            g_media_ctl.state = STATE_PLAYING;
            if (audio_can_use_ndsp()) {
                ndspChnSetPaused(0, false);
            }
        }
        g_hfix59r3_resume_after_settings = false;
        MIVF_SettingsSave(&g_mivf_settings);
        hfix58_alert_set("SETTINGS OFF", 1);
    }

    hfix59r3_note_activity();
}

static void hfix59r3_tick_idle(bool activity) {
    if (activity) {
        hfix59r3_note_activity();
        return;
    }

    if (g_mivf_idle_frames < 0xFFFFFFFFu) {
        g_mivf_idle_frames++;
    }

    if (!g_mivf_settings.autodim_enabled || g_mivf_brightness_dimmed) {
        return;
    }

    if (g_mivf_idle_frames >= g_mivf_settings.autodim_timeout_frames) {
        hfix59r3_apply_screen_brightness(true);
    }
}

static void hfix59r3_apt_hook(APT_HookType hook, void *param) {
    (void)param;

    switch (hook) {
        case APTHOOK_ONSUSPEND:
            /* HOME menu / app suspend: park playback but leave the
               screen on so the HOME menu is visible.  Only real sleep
               (ONSLEEP / lid close) powers off backlights. */
            if (g_media_ctl.state == STATE_PLAYING) {
                g_media_ctl.state = STATE_PAUSED;
                g_mivf_park_resume_audio = true;
                if (audio_can_use_ndsp()) {
                    ndspChnSetPaused(0, true);
                }
            }
            break;
        case APTHOOK_ONSLEEP:
            /* Lid close / real sleep: park playback and power off
               backlights to save power. */
            if (g_media_ctl.state == STATE_PLAYING) {
                g_media_ctl.state = STATE_PAUSED;
                g_mivf_park_resume_audio = true;
                if (audio_can_use_ndsp()) {
                    ndspChnSetPaused(0, true);
                }
            }
            GSPLCD_PowerOffAllBacklights();
            break;
        case APTHOOK_ONWAKEUP:
        case APTHOOK_ONRESTORE:
            GSPLCD_PowerOnAllBacklights();
            hfix59r3_apply_screen_brightness(g_mivf_brightness_dimmed);
            /* Resume only if we were the ones who parked playback. */
            if (g_mivf_park_resume_audio) {
                g_mivf_park_resume_audio = false;
                g_media_ctl.state = STATE_PLAYING;
                if (audio_can_use_ndsp()) {
                    ndspChnSetPaused(0, false);
                }
            }
            break;
        case APTHOOK_ONEXIT:
            GSPLCD_PowerOffAllBacklights();
            break;
        default:
            break;
    }
}

#define HFIX59R3_SETTINGS_COUNT 20
#define HFIX59R3_SETTINGS_VISIBLE 13

static const char *hfix59r3_settings_group(int idx) {
    if (idx <= 3) return "PLAY";
    if (idx <= 8) return "DISPLAY";
    if (idx == 9) return "AUDIO";
    if (idx <= 14) return "SUBS";
    return "ADV";
}

static const char *hfix59r3_settings_label(int idx) {
    static const char *labels[HFIX59R3_SETTINGS_COUNT] = {
        "RESUME",
        "SPEED",
        "AUTO NEXT",
        "SLEEP TIMER",
        "ASPECT",
        "BRIGHTNESS",
        "AUTO DIM",
        "THEME",
        "FONT",
        "STEREO OUT",
        "SUBTITLES",
        "SUB TRACK",
        "SUB DELAY",
        "SUB POS",
        "CHAPTERS",
        "FAVORITES",
        "LID SLEEP",
        "DIM TIME",
        "DIM LEVEL",
        "DEBUG"
    };

    if (idx < 0 || idx >= HFIX59R3_SETTINGS_COUNT) {
        return "";
    }

    return labels[idx];
}

static void hfix59r3_settings_value(int idx, char *out, size_t out_sz) {
    if (!out || out_sz == 0) {
        return;
    }

    out[0] = 0;

    switch (idx) {
        case 0: snprintf(out, out_sz, "%s", g_mivf_settings.resume_enabled ? "ON" : "OFF"); break;
        case 1: {
            u32 pct = mivf_speed_pct();
            snprintf(out, out_sz, "%lu.%02lux", (unsigned long)(pct / 100u), (unsigned long)(pct % 100u));
            break;
        }
        case 2: snprintf(out, out_sz, "%s", g_mivf_settings.auto_advance ? "ON" : "OFF"); break;
        case 3:
            if (g_mivf_settings.sleep_timer_min == 0u) snprintf(out, out_sz, "OFF");
            else snprintf(out, out_sz, "%lum", (unsigned long)g_mivf_settings.sleep_timer_min);
            break;
        case 4: snprintf(out, out_sz, "%s", hfix60_aspect_name(g_mivf_settings.aspect_mode)); break;
        case 5: snprintf(out, out_sz, "%lu", (unsigned long)g_mivf_settings.active_brightness); break;
        case 6: snprintf(out, out_sz, "%s", g_mivf_settings.autodim_enabled ? "ON" : "OFF"); break;
        case 7: snprintf(out, out_sz, "%s", hfix60_theme_name(g_mivf_settings.theme_index)); break;
        case 8: snprintf(out, out_sz, "%lux", (unsigned long)g_mivf_settings.font_scale); break;
        case 9: snprintf(out, out_sz, "%s", g_mivf_settings.force_stereo ? "ON" : "OFF"); break;
        case 10: snprintf(out, out_sz, "%s", g_mivf_settings.show_subtitle_tracks ? "ON" : "OFF"); break;
        case 11: snprintf(out, out_sz, "%lu", (unsigned long)g_mivf_settings.subtitle_track_index); break;
        case 12: snprintf(out, out_sz, "%+dms", g_mivf_settings.subtitle_delay_ms); break;
        case 13: snprintf(out, out_sz, "%s", hfix60_subtitle_pos_name(g_mivf_settings.subtitle_position)); break;
        case 14: snprintf(out, out_sz, "%s", g_mivf_settings.show_chapters ? "ON" : "OFF"); break;
        case 15: snprintf(out, out_sz, "%s", g_mivf_settings.remember_favorites ? "ON" : "OFF"); break;
        case 16: snprintf(out, out_sz, "%s", g_mivf_settings.autosleep_allowed ? "ON" : "OFF"); break;
        case 17: snprintf(out, out_sz, "%lus", (unsigned long)(g_mivf_settings.autodim_timeout_frames / 60u)); break;
        case 18: snprintf(out, out_sz, "%lu", (unsigned long)g_mivf_settings.autodim_brightness); break;
        case 19: snprintf(out, out_sz, "%s", g_mivf_settings.debug_overlay_enabled ? "ON" : "OFF"); break;
        default: break;
    }
}

static bool hfix59r3_handle_settings_menu(u32 down) {
    char value[32];

    value[0] = 0;

    if (!g_hfix59r3_settings_visible) {
        return false;
    }

    if ((down & KEY_B) || (down & KEY_SELECT)) {
        hfix59r3_set_settings_open(false);
        return true;
    }

    if (down & KEY_DUP) {
        g_hfix59r3_settings_index--;
        if (g_hfix59r3_settings_index < 0) {
            g_hfix59r3_settings_index = HFIX59R3_SETTINGS_COUNT - 1;
        }
        hfix59r3_note_activity();
        return true;
    }

    if (down & KEY_DDOWN) {
        g_hfix59r3_settings_index++;
        if (g_hfix59r3_settings_index >= HFIX59R3_SETTINGS_COUNT) {
            g_hfix59r3_settings_index = 0;
        }
        hfix59r3_note_activity();
        return true;
    }

    if (!(down & (KEY_A | KEY_DLEFT | KEY_DRIGHT))) {
        return true;
    }

    switch (g_hfix59r3_settings_index) {
        case 0:
            g_mivf_settings.resume_enabled = !g_mivf_settings.resume_enabled;
            snprintf(value, sizeof(value), "RESUME %s", g_mivf_settings.resume_enabled ? "ON" : "OFF");
            break;
        case 1: {
            u32 pct;
            if (down & KEY_DLEFT) {
                g_mivf_settings.playback_speed_idx =
                    (g_mivf_settings.playback_speed_idx + (u32)MIVF_SPEED_COUNT - 1u) % (u32)MIVF_SPEED_COUNT;
            } else {
                g_mivf_settings.playback_speed_idx =
                    (g_mivf_settings.playback_speed_idx + 1u) % (u32)MIVF_SPEED_COUNT;
            }
            pct = mivf_speed_pct();
            if (audio_can_use_ndsp()) {
                ndspChnSetRate(0, (float)audio.rate * (float)pct / 100.0f);
            }
            snprintf(value, sizeof(value), "SPEED %lu.%02lux",
                (unsigned long)(pct / 100u), (unsigned long)(pct % 100u));
            break;
        }
        case 2:
            g_mivf_settings.auto_advance = !g_mivf_settings.auto_advance;
            snprintf(value, sizeof(value), "AUTO-ADVANCE %s", g_mivf_settings.auto_advance ? "ON" : "OFF");
            break;
        case 3: {
            static const u32 sleep_opts[] = { 0u, 15u, 30u, 45u, 60u, 90u, 120u };
            int n = (int)(sizeof(sleep_opts) / sizeof(sleep_opts[0]));
            int cur = 0;
            int j;
            for (j = 0; j < n; j++) {
                if (sleep_opts[j] == g_mivf_settings.sleep_timer_min) { cur = j; break; }
            }
            cur += (down & KEY_DLEFT) ? (n - 1) : 1;
            cur %= n;
            g_mivf_settings.sleep_timer_min = sleep_opts[cur];
            if (g_mivf_settings.sleep_timer_min == 0u) {
                snprintf(value, sizeof(value), "SLEEP TIMER OFF");
            } else {
                snprintf(value, sizeof(value), "SLEEP %lum", (unsigned long)g_mivf_settings.sleep_timer_min);
            }
            break;
        }
        case 4:
            if (down & KEY_DLEFT) {
                g_mivf_settings.aspect_mode = (g_mivf_settings.aspect_mode + 2u) % 3u;
            } else {
                g_mivf_settings.aspect_mode = (g_mivf_settings.aspect_mode + 1u) % 3u;
            }
            snprintf(value, sizeof(value), "ASPECT %s", hfix60_aspect_name(g_mivf_settings.aspect_mode));
            break;
        case 5: {
            int step = (down & KEY_DLEFT) ? -1 : 1;
            int next = (int)g_mivf_settings.active_brightness + step;
            if (next < 1) next = 1;
            if (next > 5) next = 5;
            g_mivf_settings.active_brightness = (u32)next;
            g_mivf_brightness_active = g_mivf_settings.active_brightness;
            hfix59r3_apply_screen_brightness(false);
            snprintf(value, sizeof(value), "SCREEN %lu", (unsigned long)g_mivf_settings.active_brightness);
            break;
        }
        case 6:
            g_mivf_settings.autodim_enabled = !g_mivf_settings.autodim_enabled;
            snprintf(value, sizeof(value), "AUTO DIM %s", g_mivf_settings.autodim_enabled ? "ON" : "OFF");
            break;
        case 7:
            if (down & KEY_A || down & KEY_DRIGHT) {
                g_mivf_settings.theme_index = (g_mivf_settings.theme_index + 1u) % 4u;
            } else if (down & KEY_DLEFT) {
                g_mivf_settings.theme_index = (g_mivf_settings.theme_index + 3u) % 4u;
            }
            snprintf(value, sizeof(value), "THEME %s", hfix60_theme_name(g_mivf_settings.theme_index));
            break;
        case 8:
            if (down & KEY_A || down & KEY_DRIGHT) {
                g_mivf_settings.font_scale++;
            } else if (g_mivf_settings.font_scale > 1u) {
                g_mivf_settings.font_scale--;
            }
            snprintf(value, sizeof(value), "FONT %lux", (unsigned long)g_mivf_settings.font_scale);
            break;
        case 9:
            g_mivf_settings.force_stereo = !g_mivf_settings.force_stereo;
            g_hfix56_force_stereo = g_mivf_settings.force_stereo;
            snprintf(value, sizeof(value), "STEREO %s", g_mivf_settings.force_stereo ? "ON" : "OFF");
            break;
        case 10:
            g_mivf_settings.show_subtitle_tracks = !g_mivf_settings.show_subtitle_tracks;
            hfix58s_subtitles_load_for_video(MIVF_PATH);
            snprintf(value, sizeof(value), "SUBS %s", g_mivf_settings.show_subtitle_tracks ? "ON" : "OFF");
            break;
        case 11:
            if (down & KEY_A || down & KEY_DRIGHT) {
                g_mivf_settings.subtitle_track_index = (g_mivf_settings.subtitle_track_index + 1u) % 4u;
            } else if (down & KEY_DLEFT) {
                g_mivf_settings.subtitle_track_index = (g_mivf_settings.subtitle_track_index + 3u) % 4u;
            }
            hfix58s_subtitles_load_for_video(MIVF_PATH);
            snprintf(value, sizeof(value), "TRACK %lu", (unsigned long)g_mivf_settings.subtitle_track_index);
            break;
        case 12: {
            int step = (down & KEY_DLEFT) ? -250 : 250;
            g_mivf_settings.subtitle_delay_ms += step;
            snprintf(value, sizeof(value), "SUB %+dms", g_mivf_settings.subtitle_delay_ms);
            break;
        }
        case 13:
            if (down & KEY_DLEFT) {
                g_mivf_settings.subtitle_position = (g_mivf_settings.subtitle_position + 2u) % 3u;
            } else {
                g_mivf_settings.subtitle_position = (g_mivf_settings.subtitle_position + 1u) % 3u;
            }
            snprintf(value, sizeof(value), "SUB %s", hfix60_subtitle_pos_name(g_mivf_settings.subtitle_position));
            break;
        case 14:
            g_mivf_settings.show_chapters = !g_mivf_settings.show_chapters;
            snprintf(value, sizeof(value), "CHAPTERS %s", g_mivf_settings.show_chapters ? "ON" : "OFF");
            break;
        case 15:
            g_mivf_settings.remember_favorites = !g_mivf_settings.remember_favorites;
            snprintf(value, sizeof(value), "FAVORITES %s", g_mivf_settings.remember_favorites ? "ON" : "OFF");
            break;
        case 16:
            g_mivf_settings.autosleep_allowed = !g_mivf_settings.autosleep_allowed;
            aptSetSleepAllowed(g_mivf_settings.autosleep_allowed);
            snprintf(value, sizeof(value), "LID SLEEP %s", g_mivf_settings.autosleep_allowed ? "ON" : "OFF");
            break;
        case 17: {
            int step = (down & KEY_DLEFT) ? -300 : 300;
            int next = (int)g_mivf_settings.autodim_timeout_frames + step;
            if (next < 30) next = 30;
            if (next > 60 * 60 * 10) next = 60 * 60 * 10;
            g_mivf_settings.autodim_timeout_frames = (u32)next;
            snprintf(value, sizeof(value), "DIM %lus", (unsigned long)(g_mivf_settings.autodim_timeout_frames / 60u));
            break;
        }
        case 18: {
            int step = (down & KEY_DLEFT) ? -1 : 1;
            int next = (int)g_mivf_settings.autodim_brightness + step;
            if (next < 1) next = 1;
            if (next > 5) next = 5;
            g_mivf_settings.autodim_brightness = (u32)next;
            snprintf(value, sizeof(value), "DIM LEVEL %lu", (unsigned long)g_mivf_settings.autodim_brightness);
            break;
        }
        case 19:
            g_mivf_settings.debug_overlay_enabled = !g_mivf_settings.debug_overlay_enabled;
            snprintf(value, sizeof(value), "DEBUG %s", g_mivf_settings.debug_overlay_enabled ? "ON" : "OFF");
            break;
        default:
            break;
    }

    if (value[0]) {
        hfix58_alert_set(value, 1);
    }

    MIVF_SettingsClamp(&g_mivf_settings);
    hfix59r3_sync_runtime_settings();
    /* Settings are saved on close (hfix59r3_set_settings_open(false));
       saving on every value change + held-key repeat causes SD I/O stalls
       on Old 3DS hardware. */
    hfix59r3_note_activity();
    return true;
}

static void hfix59r3_draw_settings_overlay(u8 *fb) {
    int first = g_hfix59r3_settings_index - HFIX59R3_SETTINGS_VISIBLE / 2;

    if (!g_hfix59r3_settings_visible) {
        return;
    }

    if (first < 0) {
        first = 0;
    }

    if (first > HFIX59R3_SETTINGS_COUNT - HFIX59R3_SETTINGS_VISIBLE) {
        first = HFIX59R3_SETTINGS_COUNT - HFIX59R3_SETTINGS_VISIBLE;
    }

    if (first < 0) {
        first = 0;
    }

    hfix58_rect565(fb, 14, 6, 292, 228, 8, 13, 22);
    hfix58_rect565(fb, 16, 8, 288, 224, 18, 28, 44);
    hfix58_rect565(fb, 16, 8, 288, 14, g_mivf_theme_r, g_mivf_theme_g, g_mivf_theme_b);
    hfix58_rect565(fb, 16, 23, 288, 1, 40, 95, 135);

    hfix58_draw_text_shadow(fb, 24, 10, "SETTINGS", 1, 245, 255, 255);
    hfix58_draw_text_shadow(fb, 168, 10, "A CHANGE  B CLOSE", 1, 235, 245, 255);

    for (int row = 0; row < HFIX59R3_SETTINGS_VISIBLE; row++) {
        int i = first + row;
        int y = 30 + row * 14;
        bool selected = (i == g_hfix59r3_settings_index);
        char value[32];

        if (i >= HFIX59R3_SETTINGS_COUNT) {
            break;
        }

        if (selected) {
            hfix58_rect565(fb, 22, y - 2, 266, 12, 30, 70, 110);
            hfix58_rect565(fb, 22, y - 2, 4, 12, g_mivf_theme_r, g_mivf_theme_g, g_mivf_theme_b);
        }

        hfix59r3_settings_value(i, value, sizeof(value));

        hfix58_draw_text_shadow(fb, 30, y, hfix59r3_settings_group(i), 1, 130, 165, 205);
        hfix58_draw_text_shadow(fb, 80, y, hfix59r3_settings_label(i), 1, selected ? 255 : 205, selected ? 255 : 220, selected ? 255 : 235);
        hfix58_draw_text_shadow(
            fb,
            222,
            y,
            value,
            1,
            selected ? 210 : 182,
            selected ? 236 : 214,
            selected ? 255 : 244);

        if (selected) {
            hfix58_draw_text_shadow(fb, 210, y, "<", 1, 168, 208, 244);
            hfix58_draw_text_shadow(fb, 286, y, ">", 1, 168, 208, 244);
        }
    }

    if (HFIX59R3_SETTINGS_COUNT > HFIX59R3_SETTINGS_VISIBLE) {
        int track_h = 178;
        int knob_h = (track_h * HFIX59R3_SETTINGS_VISIBLE) / HFIX59R3_SETTINGS_COUNT;
        int max_first = HFIX59R3_SETTINGS_COUNT - HFIX59R3_SETTINGS_VISIBLE;
        int knob_y = 30;
        if (knob_h < 18) knob_h = 18;
        if (max_first > 0) knob_y += ((track_h - knob_h) * first) / max_first;
        hfix58_rect565(fb, 292, 30, 4, track_h, 30, 36, 52);
        hfix58_rect565(fb, 292, knob_y, 4, knob_h, g_mivf_theme_r, g_mivf_theme_g, g_mivf_theme_b);
    }

    hfix58_rect565(fb, 22, 212, 266, 1, 44, 66, 96);
    hfix58_draw_text_shadow(fb, 24, 216, "UP/DOWN MOVE  LEFT/RIGHT CHANGE", 1, 208, 228, 246);
    hfix58_draw_text_shadow(fb, 24, 226, "SELECT OR B CLOSES AND SAVES", 1, 208, 228, 246);
}

static void hfix58d_draw_bottom_fluent_ui(u8 *fb);
static void hfix58s_draw_subtitle_overlay_top(u8 *fb);

/* HFIX58F_R2_FORWARD_DECLS */
static void hfix58f_tick_seek_ui_tail(void);
static void wait_stream_prebuffer(MivfStream *stream);

static void hfix58d_anim_tick(void);
static bool hfix58d_anim_needs_redraw(void);
static void hfix58d_notify_input(u32 down, u32 held);

/* Accessors avoid early throttler depending on late HFIX58B struct layout. */
static int hfix58b_get_selected_index(void);
static bool hfix58b_get_play_pressed(void);


/* HFIX58F_FORWARD_DECLS */
static u32 hfix58f_current_frame(void);
static u32 hfix58f_total_frames(void);
static u32 hfix59r2_frame_to_sec(u32 frame);
static void hfix59r2_format_time(char *out, size_t out_sz, u32 sec);
static bool hfix58f_seek_active(void);
static void hfix58f_request_relative_seek(int delta_frames);
static void hfix58f_draw_timeline(u8 *fb, int panel_y);

static void hfix58j_touch_scrub_update(u32 down, u32 held, u32 up);
static void hfix51c_draw_bottom_ui(void) {
    u16 fw = 0;
    u16 fh = 0;
    u8 *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &fw, &fh);

    (void)fw;
    (void)fh;

    hfix58d_draw_bottom_fluent_ui(fb);
}

static void hfix51c_draw_bottom_ui_throttled(void) {
    static u32 last_frame_idx = 0xFFFFFFFFu;
    static u32 hfix58f_last_rendered_sec = 0xFFFFFFFFu;
    static MediaPlaybackState last_state = STATE_STOPPED;
    static int last_dummy_seek_state = 999;
    static bool last_visible = false;
    static bool last_settings_visible = false;
    static int last_settings_index = -1;
    static int force_redraw_frames = 2;

    /* HFIX58D_THROTTLER_TICK */
    hfix58d_anim_tick();
    if (hfix58d_anim_needs_redraw()) {
        force_redraw_frames = 2;
    }
    static int last_hfix58b_selected_index = -1;
    static bool last_hfix58b_play_pressed = false;
    static Hfix57TouchButton last_hfix57_touch_button = HFIX57_TOUCH_NONE;

    if (!g_media_ctl.ui_visible) {
        last_visible = false;
        last_settings_visible = false;
        return;
    }

    if (g_hfix59r3_settings_visible) {
        force_redraw_frames = 2;
    }

    if (g_hfix59r3_settings_visible != last_settings_visible ||
        g_hfix59r3_settings_index != last_settings_index) {
        force_redraw_frames = 2;
        last_settings_visible = g_hfix59r3_settings_visible;
        last_settings_index = g_hfix59r3_settings_index;
    }

    int hfix58b_cur_selected_index = hfix58b_get_selected_index();
    bool hfix58b_cur_play_pressed = hfix58b_get_play_pressed();

    if (hfix58b_cur_selected_index != last_hfix58b_selected_index ||
        hfix58b_cur_play_pressed != last_hfix58b_play_pressed) {
        force_redraw_frames = 2;
        last_hfix58b_selected_index = hfix58b_cur_selected_index;
        last_hfix58b_play_pressed = hfix58b_cur_play_pressed;
    }

    if (g_hfix57_touch_button != last_hfix57_touch_button) {
        force_redraw_frames = 2;
        last_hfix57_touch_button = g_hfix57_touch_button;
    }

    if (!last_visible ||
        g_media_ctl.state != last_state ||
        g_media_ctl.dummy_seek_state != last_dummy_seek_state) {
        force_redraw_frames = 2;
        last_state = g_media_ctl.state;
        last_dummy_seek_state = g_media_ctl.dummy_seek_state;
        last_visible = true;
    }

    if (g_media_ctl.state == STATE_PLAYING) {
        if (g_media_ctl.current_frame_idx != last_frame_idx &&
            ((g_media_ctl.current_frame_idx & 15u) == 0u)) {
            force_redraw_frames = 2;
            last_frame_idx = g_media_ctl.current_frame_idx;
        }
    } else {
        if (g_media_ctl.current_frame_idx != last_frame_idx) {
            force_redraw_frames = 2;
            last_frame_idx = g_media_ctl.current_frame_idx;
        }
    }

    /* HFIX58F_TIMELINE_THROTTLE */
    if (hfix58f_seek_active()) {
        force_redraw_frames = 2;
    } else {
        u32 hfix58f_sec = hfix59r2_frame_to_sec(hfix58f_current_frame());
        if (hfix58f_sec != hfix58f_last_rendered_sec) {
            force_redraw_frames = 2;
            hfix58f_last_rendered_sec = hfix58f_sec;
        }
    }

    if (force_redraw_frames > 0) {
        hfix51c_draw_bottom_ui();
        force_redraw_frames--;
    }
}

static void hfix51c_present_finish(void) {
    hfix51c_draw_bottom_ui_throttled();
    gfxFlushBuffers();
    gfxSwapBuffers();
}


static void m2y0_to_top_rgb565_direct(const M2Y0Frame *src) {
    if (!src || !src->y || !src->cb || !src->cr) {
        return;
    }

    int w = (int)src->w;
    int h = (int)src->h;

    /*
        HFIX51B diagnostic path is only for native 400x240 top-screen assets.
        Non-native assets must use the legacy RGB565 frame + blit path.
    */
    if (w != TOP_W || h != TOP_H) {
        return;
    }

    u16 fw, fh;
    u8 *fb8 = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &fw, &fh);
    if (!fb8) {
        return;
    }

    u16 *fb = (u16*)fb8;

    /*
        Display-only luma deblocking support.
        This reads from a temporary display copy if enabled and never mutates
        the closed-loop Y/Cb/Cr reference frame.
    */
    const u8 *display_y = src->y;
    if (g_m2y1_deblock_this_frame) {
        u8 *tmp_y = m2y1_get_display_y_copy(src->y, w, h, g_m2y1_display_qp);
        if (tmp_y) {
            display_y = tmp_y;
        }
    }
    g_m2y1_deblock_this_frame = false;

    int cw = w >> 1;

    /*
        Column-major destination traversal.
        x is outer, y is inner, making destination writes contiguous inside 
        each hardware framebuffer column. Inner loop index tracking scales 
        via accumulation (ci += cw) to preserve register cycles.
    */
    for (int x = 0; x < w; x += 2) {
        u16 *dst0 = fb + (x + 0) * TOP_H;
        u16 *dst1 = fb + (x + 1) * TOP_H;

        int chroma_x = x >> 1;
        int ci = chroma_x;

        for (int y = 0; y < h; y += 2) {
            u8 u = src->cb[ci];
            u8 v = src->cr[ci];

            const u8 *y0 = display_y + y * w;
            const u8 *y1 = display_y + (y + 1) * w;

            dst0[TOP_H - 1 - (y + 0)] = yuv_to_rgb565_pixel(y0[x + 0], u, v);
            dst1[TOP_H - 1 - (y + 0)] = yuv_to_rgb565_pixel(y0[x + 1], u, v);
            dst0[TOP_H - 1 - (y + 1)] = yuv_to_rgb565_pixel(y1[x + 0], u, v);
            dst1[TOP_H - 1 - (y + 1)] = yuv_to_rgb565_pixel(y1[x + 1], u, v);

            ci += cw;
        }
    }

    hfix58s_draw_subtitle_overlay_top(fb8);
    hfix51c_present_finish();
}

static void m2y0_to_rgb565(const M2Y0Frame *src, u8 *dst_rgb565) {
    if (!src || !dst_rgb565 || !src->y || !src->cb || !src->cr) {
        return;
    }

    int w = (int)src->w;
    int h = (int)src->h;

    if (w <= 0 || h <= 0) {
        return;
    }

    /*
        HFIX33 display-only deblock hook.

        The compressed M2Y1 path sets g_m2y1_deblock_this_frame=true
        immediately before calling this converter. This function consumes
        that one-shot flag and resets it, so raw M2Y0 conversion remains
        untouched.
    */
    const u8 *display_y = src->y;

    if (g_m2y1_deblock_this_frame) {
        u8 *tmp_y = m2y1_get_display_y_copy(
            src->y,
            w,
            h,
            g_m2y1_display_qp
        );

        if (tmp_y) {
            display_y = tmp_y;
        }
    }

    g_m2y1_deblock_this_frame = false;

    u16 *dst = (u16*)dst_rgb565;

    for (int y = 0; y < h; y += 2) {
        const u8 *y0 = display_y + y * w;
        const u8 *y1 = display_y + (y + 1) * w;

        const u8 *cb = src->cb + (y / 2) * (w / 2);
        const u8 *cr = src->cr + (y / 2) * (w / 2);

        u16 *d0 = dst + y * w;
        u16 *d1 = dst + (y + 1) * w;

        for (int x = 0; x < w; x += 2) {
            u8 u = cb[x / 2];
            u8 v = cr[x / 2];

            d0[x + 0] = yuv_to_rgb565_pixel(y0[x + 0], u, v);
            d0[x + 1] = yuv_to_rgb565_pixel(y0[x + 1], u, v);
            d1[x + 0] = yuv_to_rgb565_pixel(y1[x + 0], u, v);
            d1[x + 1] = yuv_to_rgb565_pixel(y1[x + 1], u, v);
        }
    }
}

static int dec_m2y0_raw(const u8 *p, size_t n, M2Y0Frame *out) {
    if (n < 28 || memcmp(p, "M2Y0", 4)) {
        return -1;
    }

    u16 w = le16(p + 4);
    u16 h = le16(p + 6);

    u32 y_size  = le32(p + 16);
    u32 cb_size = le32(p + 20);
    u32 cr_size = le32(p + 24);

    if (w != out->w || h != out->h) {
        return -2;
    }

    if (y_size != out->y_size ||
        cb_size != out->c_size ||
        cr_size != out->c_size) {
        return -3;
    }

    size_t need = 28u + (size_t)y_size + (size_t)cb_size + (size_t)cr_size;

    if (n < need) {
        return -4;
    }

    const u8 *q = p + 28;

    memcpy(out->y, q, y_size);
    q += y_size;

    memcpy(out->cb, q, cb_size);
    q += cb_size;

    memcpy(out->cr, q, cr_size);

    return 0;
}


static void m2y0_frame_copy(M2Y0Frame *dst, const M2Y0Frame *src) {
    if (!dst || !src || !dst->base || !src->base) {
        return;
    }

    if (dst->total_size != src->total_size) {
        return;
    }

    memcpy(dst->base, src->base, src->total_size);
}

enum {
    M2Y1_SKIP     = 0,
    M2Y1_RAW      = 1,
    M2Y1_DELTA    = 2,
    M2Y1_SOLID    = 3,
    M2Y1_RUN_SKIP = 4,
    M2Y1_QRES     = 5,
    M2Y1_MVCOPY   = 6,
    M2Y1_MVQRES   = 7,
    M2Y1_TRANSFORM   = 8,
    M2Y1_MVTRANSFORM = 9,
    M2Y1_QRESZ         = 10,
    M2Y1_MVQRESZ       = 11,
    M2Y1_TRANSFORMZ    = 12,
    M2Y1_MVTRANSFORMZ  = 13
};

static void m2y1_copy_block(
    u8 *dst,
    const u8 *prev,
    int plane_w,
    int bx,
    int by
) {
    int x0 = bx * 8;
    int y0 = by * 8;

    for (int y = 0; y < 8; y++) {
        memcpy(
            dst  + (y0 + y) * plane_w + x0,
            prev + (y0 + y) * plane_w + x0,
            8
        );
    }
}

static void m2y1_raw_block(
    u8 *dst,
    const u8 *src,
    int plane_w,
    int bx,
    int by
) {
    int x0 = bx * 8;
    int y0 = by * 8;

    for (int y = 0; y < 8; y++) {
        memcpy(dst + (y0 + y) * plane_w + x0, src + y * 8, 8);
    }
}

static void m2y1_solid_block(
    u8 *dst,
    int plane_w,
    int bx,
    int by,
    u8 value
) {
    int x0 = bx * 8;
    int y0 = by * 8;

    for (int y = 0; y < 8; y++) {
        memset(dst + (y0 + y) * plane_w + x0, value, 8);
    }
}

static void m2y1_delta_block(
    u8 *dst,
    const u8 *prev,
    int plane_w,
    int bx,
    int by,
    int delta
) {
    int x0 = bx * 8;
    int y0 = by * 8;

    for (int y = 0; y < 8; y++) {
        u8 *d = dst + (y0 + y) * plane_w + x0;
        const u8 *p = prev + (y0 + y) * plane_w + x0;

        for (int x = 0; x < 8; x++) {
            int v = (int)p[x] + delta;

            if (v < 0) {
                v = 0;
            } else if (v > 255) {
                v = 255;
            }

            d[x] = (u8)v;
        }
    }
}


static inline u8 m2y1_clamp_u8_int(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (u8)v;
}

static void m2y1_qres_block(
    u8 *dst,
    const u8 *prev,
    int plane_w,
    int bx,
    int by,
    int global_delta,
    const int8_t residuals[16]
) {
    int x0 = bx * 8;
    int y0 = by * 8;

    for (int y = 0; y < 8; y++) {
        u8 *d = dst + (y0 + y) * plane_w + x0;
        const u8 *p = prev + (y0 + y) * plane_w + x0;

        int cell_y = (y >> 1) * 4;

        for (int x = 0; x < 8; x++) {
            int cell = cell_y + (x >> 1);
            int v = (int)p[x] + global_delta + (int)residuals[cell];
            d[x] = m2y1_clamp_u8_int(v);
        }
    }
}


static void m2y1_mvcopy_block(
    u8 *dst,
    const u8 *prev,
    int plane_w,
    int plane_h,
    int bx,
    int by,
    int mx,
    int my
) {
    int dst_x0 = bx * 8;
    int dst_y0 = by * 8;

    int src_x0 = dst_x0 + mx;
    int src_y0 = dst_y0 + my;

    /*
        Bounds safety:
        Encoder should never emit invalid vectors, but the decoder must
        never read outside the previous plane.
    */
    if (src_x0 < 0 || src_y0 < 0 ||
        src_x0 + 8 > plane_w ||
        src_y0 + 8 > plane_h) {
        /*
            Fallback to same-position copy rather than crashing.
            This preserves byte sync and prevents off-plane reads.
        */
        src_x0 = dst_x0;
        src_y0 = dst_y0;
    }

    for (int y = 0; y < 8; y++) {
        memcpy(
            dst  + (dst_y0 + y) * plane_w + dst_x0,
            prev + (src_y0 + y) * plane_w + src_x0,
            8
        );
    }
}


static void m2y1_mvqres_block(
    u8 *dst,
    const u8 *prev,
    int plane_w,
    int plane_h,
    int bx,
    int by,
    int mx,
    int my,
    int global_delta,
    const int8_t residuals[16]
) {
    int dst_x0 = bx * 8;
    int dst_y0 = by * 8;

    int src_x0 = dst_x0 + mx;
    int src_y0 = dst_y0 + my;

    if (src_x0 < 0 || src_y0 < 0 ||
        src_x0 + 8 > plane_w ||
        src_y0 + 8 > plane_h) {
        src_x0 = dst_x0;
        src_y0 = dst_y0;
    }

    for (int y = 0; y < 8; y++) {
        u8 *d = dst + (dst_y0 + y) * plane_w + dst_x0;
        const u8 *p = prev + (src_y0 + y) * plane_w + src_x0;

        int cell_y = (y >> 1) * 4;

        for (int x = 0; x < 8; x++) {
            int cell = cell_y + (x >> 1);
            int v = (int)p[x] + global_delta + (int)residuals[cell];

            d[x] = m2y1_clamp_u8_int(v);
        }
    }
}


/* ------------------------------------------------------------------------- */
/* HFIX29B M2Y1 4x4 inverse transform decoder                                */
/* ------------------------------------------------------------------------- */

/* HFIX32: transform QP is read from each plane payload, no fixed define. */

static void m2y1_transform4_inverse(
    const int16_t input[16],
    int16_t output[16]
) {
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

static void m2y1_transform_block(
    u8 *dst,
    const u8 *prev,
    int plane_w,
    int plane_h,
    int bx,
    int by,
    int mx,
    int my,
    int qp,
    const int8_t *coeffs
) {
    /* Reconstruct one 8x8 block via the shared transform codec (keep g_m2y1_nkeep). */
    mivf_t_decode(dst, prev, plane_w, plane_h, bx, by, mx, my, qp, coeffs, g_m2y1_nkeep);
}


/* ------------------------------------------------------------------------- */
/* HFIX30B M2Y1 sparse zero-masked decoder helpers                           */
/* ------------------------------------------------------------------------- */

static int m2y1_read_sparse16_le(
    const u8 *src,
    size_t n,
    size_t *off,
    int8_t vals[16]
) {
    if (*off + 2 > n) {
        return -1;
    }

    u16 mask =
        (u16)src[*off] |
        ((u16)src[*off + 1] << 8);

    *off += 2;

    memset(vals, 0, 16);

    for (int i = 0; i < 16; i++) {
        if (mask & (u16)(1u << i)) {
            if (*off >= n) {
                return -2;
            }

            vals[i] = (int8_t)src[*off];
            *off += 1;
        }
    }

    return 0;
}

static void m2y1_qresz_block(
    u8 *dst,
    const u8 *prev,
    int plane_w,
    int plane_h,
    int bx,
    int by,
    int mx,
    int my,
    int global_delta,
    const int8_t residuals[16]
) {
    int dst_x0 = bx * 8;
    int dst_y0 = by * 8;

    int src_x0 = dst_x0 + mx;
    int src_y0 = dst_y0 + my;

    if (src_x0 < 0 || src_y0 < 0 ||
        src_x0 + 8 > plane_w ||
        src_y0 + 8 > plane_h) {
        src_x0 = dst_x0;
        src_y0 = dst_y0;
    }

    for (int y = 0; y < 8; y++) {
        int cell_y = (y >> 1) * 4;

        for (int x = 0; x < 8; x++) {
            int cell = cell_y + (x >> 1);

            int src_idx = (src_y0 + y) * plane_w + src_x0 + x;
            int dst_idx = (dst_y0 + y) * plane_w + dst_x0 + x;

            int v =
                (int)prev[src_idx] +
                global_delta +
                (int)residuals[cell];

            dst[dst_idx] = m2y1_clamp_u8_int(v);
        }
    }
}


/* ------------------------------------------------------------------------- */
/* HFIX37-REDUX single-block bounds-checked GMV copy                          */
/* ------------------------------------------------------------------------- */

static void m2y1_mvcopyp_block(
    u8 *dst,
    const u8 *prev,
    int plane_w,
    int plane_h,
    int bx,
    int by,
    int mx,
    int my
) {
    int dst_x0 = bx * 8;
    int dst_y0 = by * 8;

    int src_x0 = dst_x0 + mx;
    int src_y0 = dst_y0 + my;

    if (src_x0 < 0 || src_y0 < 0 ||
        src_x0 + 8 > plane_w ||
        src_y0 + 8 > plane_h) {
        src_x0 = dst_x0;
        src_y0 = dst_y0;
    }

    for (int y = 0; y < 8; y++) {
        memcpy(
            dst + (dst_y0 + y) * plane_w + dst_x0,
            prev + (src_y0 + y) * plane_w + src_x0,
            8
        );
    }
}

static int dec_m2y1_plane(
    const u8 *src,
    size_t n,
    u8 *dst,
    const u8 *prev,
    bool have_prev,
    int plane_w,
    int plane_h
) {
    if ((plane_w & 7) || (plane_h & 7)) {
        return -1;
    }

    int bxcount = plane_w / 8;
    int bycount = plane_h / 8;
    u32 block_count = (u32)(bxcount * bycount);

    size_t off = 0;

    /*
        HFIX32:
        Each plane payload begins with one active QP byte.
        This must be consumed before macroblock token parsing.
    */
    if (off >= n) {
        return -50;
    }

    int current_frame_qp = (int)src[off++];
    g_m2y1_display_qp = current_frame_qp;

    if (current_frame_qp < 1 || current_frame_qp > 51) {
        return -51;
    }

    /*
        HFIX37-REDUX:
        Plane-local global motion vector follows QP.
        Decoder applies these values directly and never rescales chroma.
    */
    if (off + 2 > n) {
        return -80;
    }

    int g_mx = (int)(int8_t)src[off++];
    int g_my = (int)(int8_t)src[off++];

    /*
        HFIX39A:
        Base-relative transform QP state. DQP tokens set active_qp from
        base_qp + signed delta. They are not cumulative.
    */
    int base_qp = current_frame_qp;
    int active_qp = base_qp;

    u32 bi = 0;
    int bx = 0;
    int by = 0;

    while (bi < block_count) {
        if (off >= n) {
            return -2;
        }

        u8 m = src[off++];

        if (m == M2Y1_SET_BASE_DQP) {
            if (off >= n) {
                return -90;
            }

            int dqp = (int)(int8_t)src[off++];
            int q = base_qp + dqp;

            if (q < 18) {
                q = 18;
            } else if (q > 48) {
                q = 48;
            }

            active_qp = q;

            continue;
        }

        if (m == M2Y1_RUN_SKIP) {
            if (!have_prev) {
                return -3;
            }

            if (off >= n) {
                return -4;
            }

            u32 run = (u32)src[off++] + 1;

            if (bi + run > block_count) {
                return -5;
            }

            for (u32 i = 0; i < run; i++) {
                m2y1_copy_block(dst, prev, plane_w, bx, by);

                bx++;

                if (bx >= bxcount) {
                    bx = 0;
                    by++;
                }

                bi++;
            }

            continue;
        }

        if (m == M2Y1_GMVCOPY) {
            if (!have_prev) {
                return -81;
            }

            m2y1_mvcopyp_block(
                dst,
                prev,
                plane_w,
                plane_h,
                bx,
                by,
                g_mx,
                g_my
            );

        } else if (m == M2Y1_SKIP) {
            if (!have_prev) {
                return -6;
            }

            m2y1_copy_block(dst, prev, plane_w, bx, by);

        } else if (m == M2Y1_RAW) {
            if (off + 64 > n) {
                return -7;
            }

            m2y1_raw_block(dst, src + off, plane_w, bx, by);
            off += 64;

        } else if (m == M2Y1_DELTA) {
            if (!have_prev) {
                return -8;
            }

            if (off >= n) {
                return -9;
            }

            int delta = (int)(int8_t)src[off++];
            m2y1_delta_block(dst, prev, plane_w, bx, by, delta);

                } else if (m == M2Y1_MVCOPYP) {
            if (!have_prev) return -60;
            if (off + 1 > n) return -61;
            int mx = 0, my = 0;
            m2y1_unpack_mv4(src[off++], &mx, &my);
            m2y1_mvcopy_block(dst, prev, plane_w, plane_h, bx, by, mx, my);
        } else if (m == M2Y1_MVCOPY) {
            if (!have_prev) {
                return -14;
            }

            /*
                MVCOPY payload:
                    signed mx byte
                    signed my byte
            */
            if (off + 2 > n) {
                return -15;
            }

            int mx = (int)(int8_t)src[off++];
            int my = (int)(int8_t)src[off++];

            m2y1_mvcopy_block(
                dst,
                prev,
                plane_w,
                plane_h,
                bx,
                by,
                mx,
                my
            );

        } else if (m == M2Y1_QRESZ) {
            if (!have_prev) {
                return -30;
            }

            /*
                QRESZ payload after mode:
                    signed global delta byte
                    little-endian 16-bit sparse residual mask
                    N signed residual bytes
            */
            if (off + 1 > n) {
                return -31;
            }

            int global_delta = (int)(int8_t)src[off++];

            int8_t residuals[16];

            size_t off2 = off;
            int rr = m2y1_read_sparse16_le(src, n, &off2, residuals);

            if (rr < 0) {
                return -32;
            }

            off = off2;

            m2y1_qresz_block(
                dst,
                prev,
                plane_w,
                plane_h,
                bx,
                by,
                0,
                0,
                global_delta,
                residuals
            );

        } else if (m == M2Y1_MVQRESZ) {
            if (!have_prev) {
                return -33;
            }

            /*
                MVQRESZ payload after mode:
                    signed mx byte
                    signed my byte
                    signed global delta byte
                    little-endian 16-bit sparse residual mask
                    N signed residual bytes
            */
            if (off + 3 > n) {
                return -34;
            }

            int mx = (int)(int8_t)src[off++];
            int my = (int)(int8_t)src[off++];
            int global_delta = (int)(int8_t)src[off++];

            int8_t residuals[16];

            size_t off2 = off;
            int rr = m2y1_read_sparse16_le(src, n, &off2, residuals);

            if (rr < 0) {
                return -35;
            }

            off = off2;

            m2y1_qresz_block(
                dst,
                prev,
                plane_w,
                plane_h,
                bx,
                by,
                mx,
                my,
                global_delta,
                residuals
            );

        } else if (m == M2Y1_TRANSFORMZ) {
            if (!have_prev) {
                return -36;
            }

            /*
                TRANSFORMZ payload after mode:
                    reserved byte
                    little-endian 16-bit sparse coefficient mask
                    N signed coefficient bytes
            */
            if (off + 1 > n) {
                return -37;
            }

            off++; /* reserved */

            int8_t coeffs[MIVF_T_MAX_NSLOT];

            size_t off2 = off;
            int rr = mivf_t_read_sparse(src, n, &off2, coeffs, g_m2y1_nkeep * 4);

            if (rr < 0) {
                return -38;
            }

            off = off2;

            m2y1_transform_block(
                dst,
                prev,
                plane_w,
                plane_h,
                bx,
                by,
                0,
                0,
                active_qp,
                coeffs
            );

                } else if (m == M2Y1_MVTRANSFORMZP) {
            if (!have_prev) return -62;
            if (off + 2 > n) return -63;
            int mx = 0, my = 0;
            m2y1_unpack_mv4(src[off++], &mx, &my);
            off++; /* skip reserved byte */
            int8_t coeffs[MIVF_T_MAX_NSLOT];
            size_t off2 = off;
            int rr = mivf_t_read_sparse(src, n, &off2, coeffs, g_m2y1_nkeep * 4);
            if (rr < 0) return -64;
            off = off2;
            m2y1_transform_block(dst, prev, plane_w, plane_h, bx, by, mx, my, active_qp, coeffs);
        } else if (m == M2Y1_MVTRANSFORMZ) {
            if (!have_prev) {
                return -39;
            }

            /*
                MVTRANSFORMZ payload after mode:
                    signed mx byte
                    signed my byte
                    reserved byte
                    little-endian 16-bit sparse coefficient mask
                    N signed coefficient bytes
            */
            if (off + 3 > n) {
                return -40;
            }

            int mx = (int)(int8_t)src[off++];
            int my = (int)(int8_t)src[off++];

            off++; /* reserved */

            int8_t coeffs[MIVF_T_MAX_NSLOT];

            size_t off2 = off;
            int rr = mivf_t_read_sparse(src, n, &off2, coeffs, g_m2y1_nkeep * 4);

            if (rr < 0) {
                return -41;
            }

            off = off2;

            m2y1_transform_block(
                dst,
                prev,
                plane_w,
                plane_h,
                bx,
                by,
                mx,
                my,
                active_qp,
                coeffs
            );

        } else if (m == M2Y1_TRANSFORM) {
            if (!have_prev) {
                return -18;
            }

            /*
                TRANSFORM payload after mode:
                    reserved byte
                    16 signed coefficients
            */
            if (off + 1 + (size_t)(g_m2y1_nkeep * 4) > n) {
                return -19;
            }

            off++; /* reserved */

            int8_t coeffs[MIVF_T_MAX_NSLOT];

            for (int i = 0; i < g_m2y1_nkeep * 4; i++) {
                coeffs[i] = (int8_t)src[off++];
            }

            m2y1_transform_block(
                dst,
                prev,
                plane_w,
                plane_h,
                bx,
                by,
                0,
                0,
                active_qp,
                coeffs
            );

        } else if (m == M2Y1_MVTRANSFORM) {
            if (!have_prev) {
                return -20;
            }

            /*
                MVTRANSFORM payload after mode:
                    signed mx byte
                    signed my byte
                    reserved byte
                    16 signed coefficients
            */
            if (off + 3 + (size_t)(g_m2y1_nkeep * 4) > n) {
                return -21;
            }

            int mx = (int)(int8_t)src[off++];
            int my = (int)(int8_t)src[off++];

            off++; /* reserved */

            int8_t coeffs[MIVF_T_MAX_NSLOT];

            for (int i = 0; i < g_m2y1_nkeep * 4; i++) {
                coeffs[i] = (int8_t)src[off++];
            }

            m2y1_transform_block(
                dst,
                prev,
                plane_w,
                plane_h,
                bx,
                by,
                mx,
                my,
                active_qp,
                coeffs
            );

        } else if (m == M2Y1_MVQRES) {
            if (!have_prev) {
                return -16;
            }

            if (off + 19 > n) {
                return -17;
            }

            int mx = (int)(int8_t)src[off++];
            int my = (int)(int8_t)src[off++];
            int global_delta = (int)(int8_t)src[off++];

            int8_t residuals[16];

            for (int i = 0; i < 16; i++) {
                residuals[i] = (int8_t)src[off++];
            }

            m2y1_mvqres_block(
                dst,
                prev,
                plane_w,
                plane_h,
                bx,
                by,
                mx,
                my,
                global_delta,
                residuals
            );

        } else if (m == M2Y1_QRES) {
            if (!have_prev) {
                return -12;
            }

            if (off + 17 > n) {
                return -13;
            }

            int global_delta = (int)(int8_t)src[off++];

            int8_t residuals[16];

            for (int i = 0; i < 16; i++) {
                residuals[i] = (int8_t)src[off++];
            }

            m2y1_qres_block(
                dst,
                prev,
                plane_w,
                bx,
                by,
                global_delta,
                residuals
            );

        } else if (m == M2Y1_SOLID) {
            if (off >= n) {
                return -10;
            }

            u8 value = src[off++];
            m2y1_solid_block(dst, plane_w, bx, by, value);

        } else {
            return -11;
        }

        bx++;

        if (bx >= bxcount) {
            bx = 0;
            by++;
        }

        bi++;
    }

    return 0;
}

static int dec_m2y1(
    const u8 *p,
    size_t n,
    M2Y0Frame *out,
    const M2Y0Frame *prev,
    bool have_prev
) {
    if (n < 28 || memcmp(p, "M2Y1", 4)) {
        return -1;
    }

    u16 w = le16(p + 4);
    u16 h = le16(p + 6);

    if (w != out->w || h != out->h) {
        return -2;
    }

    /* Keep-count for this frame's transform tokens (0 == legacy 4). */
    g_m2y1_nkeep = mivf_t_resolve(p[13]);

    u32 y_payload  = le32(p + 16);
    u32 cb_payload = le32(p + 20);
    u32 cr_payload = le32(p + 24);

    size_t need = 28u + (size_t)y_payload + (size_t)cb_payload + (size_t)cr_payload;

    if (n < need) {
        return -3;
    }

    const u8 *q = p + 28;

    const u8 *yp = q;
    q += y_payload;

    const u8 *cbp = q;
    q += cb_payload;

    const u8 *crp = q;

    int r;

    r = dec_m2y1_plane(
        yp,
        y_payload,
        out->y,
        prev ? prev->y : NULL,
        have_prev,
        out->w,
        out->h
    );

    if (r) {
        return -100 + r;
    }

    r = dec_m2y1_plane(
        cbp,
        cb_payload,
        out->cb,
        prev ? prev->cb : NULL,
        have_prev,
        out->w / 2,
        out->h / 2
    );

    if (r) {
        return -200 + r;
    }

    r = dec_m2y1_plane(
        crp,
        cr_payload,
        out->cr,
        prev ? prev->cr : NULL,
        have_prev,
        out->w / 2,
        out->h / 2
    );

    if (r) {
        return -300 + r;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* M2Y2: order-1 range-coded M2Y1 payload (Phase 1b entropy backend).         */
/*                                                                           */
/* M2Y2 is byte-for-byte the same M2Y1 token stream, but each video packet's  */
/* concatenated [Y][Cb][Cr] plane payload is compressed with the shared       */
/* order-1 range coder (mivf_rc.h), model reset per packet (random-access     */
/* safe). We decompress back to the exact M2Y1 bytes and run the unchanged    */
/* dec_m2y1_plane, so decoded pixels are byte-identical to M2Y1.              */
/*                                                                           */
/* Body: "M2Y2"|w u16|h u16|frame u32|kf u8|0|0 u16|y_raw u32|cb_raw u32|     */
/*       cr_raw u32|comp u32| [comp bytes range-coded]                        */
/* ------------------------------------------------------------------------- */
static MivfRcO1 *g_m2y2_model = NULL;
static u8       *g_m2y2_raw = NULL;
static size_t    g_m2y2_raw_cap = 0;

static int dec_m2y2(
    const u8 *p,
    size_t n,
    M2Y0Frame *out,
    const M2Y0Frame *prev,
    bool have_prev
) {
    if (n < 32 || memcmp(p, "M2Y2", 4)) {
        return -1;
    }

    u16 w = le16(p + 4);
    u16 h = le16(p + 6);

    if (w != out->w || h != out->h) {
        return -2;
    }

    /* Keep-count for this frame's transform tokens (0 == legacy 4). */
    g_m2y1_nkeep = mivf_t_resolve(p[13]);

    u32 y_raw  = le32(p + 16);
    u32 cb_raw = le32(p + 20);
    u32 cr_raw = le32(p + 24);
    u32 comp   = le32(p + 28);

    if ((size_t)32u + (size_t)comp > n) {
        return -3;
    }

    size_t raw_total = (size_t)y_raw + (size_t)cb_raw + (size_t)cr_raw;

    if (raw_total == 0) {
        return -4;
    }

    if (comp == 0) {
        return -7;
    }

    if (raw_total > (size_t)(8u * 1024u * 1024u)) {
        /* Implausible plane size: treat as corrupt and fall back to prev
           frame rather than attempting a huge allocation / decode. */
        return -8;
    }

    if (!g_m2y2_model) {
        g_m2y2_model = (MivfRcO1 *)calloc(1, sizeof(MivfRcO1));
        if (!g_m2y2_model) {
            return -5;
        }
    }

    if (g_m2y2_raw_cap < raw_total) {
        u8 *nb = (u8 *)realloc(g_m2y2_raw, raw_total);
        if (!nb) {
            return -6;
        }
        g_m2y2_raw = nb;
        g_m2y2_raw_cap = raw_total;
    }

    mivf_rc_o1_decompress(g_m2y2_model, p + 32, (size_t)comp, g_m2y2_raw, raw_total);

    int r;

    r = dec_m2y1_plane(
        g_m2y2_raw,
        (size_t)y_raw,
        out->y,
        prev ? prev->y : NULL,
        have_prev,
        out->w,
        out->h
    );
    if (r) {
        return -100 + r;
    }

    r = dec_m2y1_plane(
        g_m2y2_raw + y_raw,
        (size_t)cb_raw,
        out->cb,
        prev ? prev->cb : NULL,
        have_prev,
        out->w / 2,
        out->h / 2
    );
    if (r) {
        return -200 + r;
    }

    r = dec_m2y1_plane(
        g_m2y2_raw + y_raw + cb_raw,
        (size_t)cr_raw,
        out->cr,
        prev ? prev->cr : NULL,
        have_prev,
        out->w / 2,
        out->h / 2
    );
    if (r) {
        return -300 + r;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Display                                                                    */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* HFIX52B_Y2R_UI_MASTER                                           */
/*                                                                           */
/* Hardware YUV420 -> linear RGB565 diagnostic.                               */
/*                                                                           */
/* This does NOT replace the HFIX51B direct renderer permanently. It tests:   */
/*     decoded Y/Cb/Cr -> Y2R RGB565 linear buffer -> legacy rotated blit     */
/*                                                                           */
/* If Y2R fails, playback falls back to HFIX51B direct CPU YUV->VRAM.         */
/* ------------------------------------------------------------------------- */
static u8 *g_hfix52a_y2r_rgb565 = NULL;
static size_t g_hfix52a_y2r_rgb565_cap = 0;
static bool g_hfix52a_y2r_ready = false;

static bool hfix52a_y2r_init_once(void) {
    if (g_hfix52a_y2r_ready) {
        return true;
    }

    Result rc = y2rInit();

    if (R_FAILED(rc)) {
        printf("Y2R init failed: 0x%08lx\n", (unsigned long)rc);
        return false;
    }

    Y2RU_ConversionParams params;
    memset(&params, 0, sizeof(params));

    params.input_format = INPUT_YUV420_INDIV_8;
    params.output_format = OUTPUT_RGB_16_565;
    params.rotation = ROTATION_NONE;
    params.block_alignment = BLOCK_LINE;
    params.input_line_width = TOP_W;
    params.input_lines = TOP_H;

    /*
        Current software path uses TV-range style conversion:
            c = Y - 16
            298/409/516 coefficients
        So start with BT.601 scaling.
    */
    params.standard_coefficient = COEFFICIENT_ITU_R_BT_601_SCALING;
    params.alpha = 0xFF;

    rc = Y2RU_SetConversionParams(&params);

    if (R_FAILED(rc)) {
        printf("Y2R params failed: 0x%08lx\n", (unsigned long)rc);
        y2rExit();
        return false;
    }

    /* HFIX52C_SPATIAL_DITHER: reduce RGB565 banding */
    (void)Y2RU_SetSpacialDithering(true);
    /* Keep temporal dithering off to avoid shimmer on flat anime colors. */
    (void)Y2RU_SetTemporalDithering(false);

    g_hfix52a_y2r_ready = true;
    return true;
}

static void hfix52a_y2r_shutdown(void) {
    if (g_hfix52a_y2r_rgb565) {
        linearFree(g_hfix52a_y2r_rgb565);
        g_hfix52a_y2r_rgb565 = NULL;
        g_hfix52a_y2r_rgb565_cap = 0;
    }

    if (g_hfix52a_y2r_ready) {
        y2rExit();
        g_hfix52a_y2r_ready = false;
    }
}

static bool hfix52a_y2r_ensure_buffer(int w, int h) {
    size_t need = (size_t)w * (size_t)h * 2u;

    if (g_hfix52a_y2r_rgb565 && g_hfix52a_y2r_rgb565_cap >= need) {
        return true;
    }

    if (g_hfix52a_y2r_rgb565) {
        linearFree(g_hfix52a_y2r_rgb565);
        g_hfix52a_y2r_rgb565 = NULL;
        g_hfix52a_y2r_rgb565_cap = 0;
    }

    g_hfix52a_y2r_rgb565 = (u8*)linearAlloc(need);

    if (!g_hfix52a_y2r_rgb565) {
        printf("Y2R RGB565 buffer OOM\n");
        return false;
    }

    memset(g_hfix52a_y2r_rgb565, 0, need);
    g_hfix52a_y2r_rgb565_cap = need;
    return true;
}

static bool m2y0_to_rgb565_y2r_linear(const M2Y0Frame *src) {
    if (!src || !src->y || !src->cb || !src->cr) {
        return false;
    }

    int w = (int)src->w;
    int h = (int)src->h;

    if (w != TOP_W || h != TOP_H) {
        return false;
    }

    if (!hfix52a_y2r_init_once()) {
        return false;
    }

    if (!hfix52a_y2r_ensure_buffer(w, h)) {
        return false;
    }

    /*
        Preserve display-only luma deblocking.
        This may copy src->y into temporary display Y, but never mutates
        the closed-loop reference plane.
    */
    const u8 *display_y = src->y;

    if (g_m2y1_deblock_this_frame) {
        u8 *tmp_y = m2y1_get_display_y_copy(
            src->y,
            w,
            h,
            g_m2y1_display_qp
        );

        if (tmp_y) {
            display_y = tmp_y;
        }
    }

    g_m2y1_deblock_this_frame = false;

    u32 y_size = (u32)(w * h);
    u32 c_size = (u32)((w >> 1) * (h >> 1));
    u32 out_size = (u32)(w * h * 2);

    /*
        Critical:
        transfer_unit is s16. Do NOT pass total buffer size.
        Use row stride in bytes/samples.
    */
    s16 y_transfer_unit = (s16)w;        /* 400 */
    s16 c_transfer_unit = (s16)(w >> 1); /* 200 */
    s16 out_transfer_unit = (s16)(w * 2);/* 800 bytes per RGB565 row */

    Result rc;

    rc = Y2RU_SetSendingY(display_y, y_size, y_transfer_unit, 0);
    if (R_FAILED(rc)) {
        printf("Y2R send Y failed: 0x%08lx\n", (unsigned long)rc);
        return false;
    }

    rc = Y2RU_SetSendingU(src->cb, c_size, c_transfer_unit, 0);
    if (R_FAILED(rc)) {
        printf("Y2R send U failed: 0x%08lx\n", (unsigned long)rc);
        return false;
    }

    rc = Y2RU_SetSendingV(src->cr, c_size, c_transfer_unit, 0);
    if (R_FAILED(rc)) {
        printf("Y2R send V failed: 0x%08lx\n", (unsigned long)rc);
        return false;
    }

    rc = Y2RU_SetReceiving(
        g_hfix52a_y2r_rgb565,
        out_size,
        out_transfer_unit,
        0
    );

    if (R_FAILED(rc)) {
        printf("Y2R recv failed: 0x%08lx\n", (unsigned long)rc);
        return false;
    }

    rc = Y2RU_StartConversion();

    if (R_FAILED(rc)) {
        printf("Y2R start failed: 0x%08lx\n", (unsigned long)rc);
        return false;
    }

    /*
        Poll instead of event first. y2r.h warns transfer-end events can fire
        too early depending on transfer_unit.
    */
    bool busy = true;
    int guard = 1000000;

    while (guard-- > 0) {
        rc = Y2RU_IsBusyConversion(&busy);

        if (R_FAILED(rc)) {
            printf("Y2R busy failed: 0x%08lx\n", (unsigned long)rc);
            return false;
        }

        if (!busy) {
            break;
        }

        /*
            Yield briefly; this avoids spinning at 100% while hardware works.
        */
        svcSleepThread(1000);
    }

    if (busy) {
        printf("Y2R timeout\n");
        (void)Y2RU_StopConversion();
        return false;
    }

    return true;
}



static void blit565_scaled(const u8 *src, int w, int h) {
    u16 fw, fh;
    u8 *fb8 = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &fw, &fh);
    u16 *fb = (u16*)fb8;

#if MIVF_SCALE_FULLSCREEN
    memset(fb8, 0, TOP_W * TOP_H * 2);

    for (int y = 0; y < TOP_H; y++) {
        int sy = y * h / TOP_H;

        for (int x = 0; x < TOP_W; x++) {
            int sx = x * w / TOP_W;

            u16 c = rgb565_read(src + ((sy * w + sx) * 2));

            int dst_index = x * TOP_H + (TOP_H - 1 - y);
            fb[dst_index] = c;
        }
    }
#else
    if (w <= 0 || h <= 0) {
        memset(fb8, 0, TOP_W * TOP_H * 2);
    } else if (w == TOP_W && h == TOP_H) {
        const u16 *src16 = (const u16*)src;

        for (int y = 0; y < TOP_H; y++) {
            const u16 *row = src16 + (y * TOP_W);
            u16 *dst_col = fb;

            for (int x = 0; x < TOP_W; x++, dst_col += TOP_H) {
                dst_col[TOP_H - 1 - y] = row[x];
            }
        }
    } else {
        int dst_w, dst_h, x0, y0;
        int src_x0 = 0, src_y0 = 0;
        int mode = (int)g_mivf_settings.aspect_mode;

        if (mode == 1) {
            /* STRETCH: fill the entire top screen, ignoring source aspect. */
            dst_w = TOP_W;
            dst_h = TOP_H;
            x0 = 0;
            y0 = 0;
        } else if (mode == 2) {
            /* NATIVE: 1:1 pixels, centered; crop if larger than the screen. */
            dst_w = (w < TOP_W) ? w : TOP_W;
            dst_h = (h < TOP_H) ? h : TOP_H;
            src_x0 = (w - dst_w) / 2;
            src_y0 = (h - dst_h) / 2;
            x0 = (TOP_W - dst_w) / 2;
            y0 = (TOP_H - dst_h) / 2;
        } else {
            /* FIT (default): preserve aspect, letterbox/pillarbox, centered. */
            dst_w = TOP_W;
            dst_h = (int)(((long long)TOP_W * (long long)h) / (long long)w);

            if (dst_h > TOP_H) {
                dst_h = TOP_H;
                dst_w = (int)(((long long)TOP_H * (long long)w) / (long long)h);
            }

            x0 = (TOP_W - dst_w) / 2;
            y0 = (TOP_H - dst_h) / 2;
        }

        if (dst_w < 1) dst_w = 1;
        if (dst_h < 1) dst_h = 1;
        if (src_x0 < 0) src_x0 = 0;
        if (src_y0 < 0) src_y0 = 0;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;

        if (dst_w != TOP_W || dst_h != TOP_H) {
            memset(fb8, 0, TOP_W * TOP_H * 2);
        }

        for (int y = 0; y < dst_h; y++) {
            int sy = (mode == 2)
                ? (src_y0 + y)
                : (int)(((long long)y * (long long)h) / (long long)dst_h);
            if (sy >= h) sy = h - 1;
            if (sy < 0) sy = 0;

            for (int x = 0; x < dst_w; x++) {
                int sx = (mode == 2)
                    ? (src_x0 + x)
                    : (int)(((long long)x * (long long)w) / (long long)dst_w);
                if (sx >= w) sx = w - 1;
                if (sx < 0) sx = 0;

                u16 c = rgb565_read(src + ((sy * w + sx) * 2));

                int dx = x + x0;
                int dy = y + y0;
                int dst_index = dx * TOP_H + (TOP_H - 1 - dy);
                fb[dst_index] = c;
            }
        }
    }
#endif

    hfix58s_draw_subtitle_overlay_top(fb8);
    hfix51c_present_finish();
}

/* ------------------------------------------------------------------------- */
/* Audio                                                                      */
/* ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
/* HFIX56A_VOLUME_STEREO                                                      */
/*                                                                           */
/* Player-side PCM16 gain, soft limiter, and stereo output/upmix.             */
/*                                                                           */
/* This operates only on decoded PCM16 immediately before NDSP queueing.      */
/* It never mutates compressed audio packets.                                 */
/* ------------------------------------------------------------------------- */
static int  g_hfix56_volume_percent = 100;
static bool g_hfix56_limiter_enabled = false;
static u8   g_hfix56_audio_src_channels = 1;

static u8  *g_hfix56_audio_mix_buf = NULL;
static u32  g_hfix56_audio_mix_cap = 0;

static inline s16 hfix56_clamp_s16_i32(int v) {
    if (v < -32768) {
        return -32768;
    }

    if (v > 32767) {
        return 32767;
    }

    return (s16)v;
}

static bool hfix56_audio_mix_ensure(u32 bytes) {
    if (bytes == 0) {
        return false;
    }

    if (g_hfix56_audio_mix_buf && g_hfix56_audio_mix_cap >= bytes) {
        return true;
    }

    if (g_hfix56_audio_mix_buf) {
        linearFree(g_hfix56_audio_mix_buf);
        g_hfix56_audio_mix_buf = NULL;
        g_hfix56_audio_mix_cap = 0;
    }

    g_hfix56_audio_mix_buf = (u8*)linearAlloc(bytes);

    if (!g_hfix56_audio_mix_buf) {
        printf("HFIX56 audio mix OOM\n");
        return false;
    }

    g_hfix56_audio_mix_cap = bytes;
    return true;
}

static inline int hfix56_apply_gain_one(int sample) {
    int v = (sample * g_hfix56_volume_percent) / 100;

    if (g_hfix56_limiter_enabled) {
        /*
            Cheap soft limiter. Prevents horrible hard-wrap or harsh clipping
            when volume is boosted above 100%.
        */
        const int knee = 28000;

        if (v > knee) {
            v = knee + ((v - knee) >> 2);
        } else if (v < -knee) {
            v = -knee + ((v + knee) >> 2);
        }
    }

    return (int)hfix56_clamp_s16_i32(v);
}

static void hfix56_audio_controls_on_input(u32 down, u32 held) {
    /*
        Runtime controls:
          L + Up    volume +10%
          L + Down  volume -10%
          L + Right toggle forced stereo/upmix
          L + Left  toggle limiter
    */
    if (!(held & KEY_L)) {
        return;
    }

    if (down & KEY_DUP) {
        g_hfix56_volume_percent += 10;

        if (g_hfix56_volume_percent > 300) {
            g_hfix56_volume_percent = 300;
        }

        char hfix58_tmp[64]; snprintf(hfix58_tmp, sizeof(hfix58_tmp), "VOLUME %d%%", g_hfix56_volume_percent); hfix58_alert_set(hfix58_tmp, 1);
    }

    if (down & KEY_DDOWN) {
        g_hfix56_volume_percent -= 10;

        if (g_hfix56_volume_percent < 0) {
            g_hfix56_volume_percent = 0;
        }

        char hfix58_tmp[64]; snprintf(hfix58_tmp, sizeof(hfix58_tmp), "VOLUME %d%%", g_hfix56_volume_percent); hfix58_alert_set(hfix58_tmp, 1);
    }

    if (down & KEY_DRIGHT) {
        g_hfix56_force_stereo = !g_hfix56_force_stereo;
        g_mivf_settings.force_stereo = g_hfix56_force_stereo;
        MIVF_SettingsSave(&g_mivf_settings);
        hfix58_alert_set(g_hfix56_force_stereo ? "STEREO OUTPUT ON" : "STEREO OUTPUT OFF", 1);
    }

    if (down & KEY_DLEFT) {
        g_hfix56_limiter_enabled = !g_hfix56_limiter_enabled;
        hfix58_alert_set(g_hfix56_limiter_enabled ? "LIMITER ON" : "LIMITER OFF", 2);
    }
}


/* ------------------------------------------------------------------------- */
/* HFIX58A_POLISHED_UI_FILE_BROWSER                                           */
/*                                                                           */
/* Professional RGB565 UI drawing, alert/status overlays, and a boot-time     */
/* SD card .mivf file browser.                                                */
/*                                                                           */
/* Design rules:                                                              */
/*   - file scanning happens before playback, never inside the decode loop     */
/*   - bottom UI text is framebuffer-native RGB565, not console debug text     */
/*   - alerts/status draw through dirty UI redraw only                         */
/* ------------------------------------------------------------------------- */

#define HFIX58_MAX_BROWSER_FILES 256
#define HFIX58_BROWSER_VISIBLE_ROWS 10
#define HFIX58_PREVIEW_W 88
#define HFIX58_PREVIEW_H 50

typedef struct {
    char name[256];
    char path[HFIX58_MAX_PATH];
    u8 quick; /* 0 = library, 1 = recent, 2 = favorite */
    u32 file_size_kb; /* populated once at scan time, zero if unknown */
} Hfix58FileEntry;

typedef struct {
    Hfix58FileEntry entries[HFIX58_MAX_BROWSER_FILES];
    int count;
    int selected;
    int scroll;
    char cwd[HFIX58_MAX_PATH];
} Hfix58FileBrowser;

typedef struct {
    bool valid;
    bool has_thumb;
    bool has_resume;
    char path[HFIX58_MAX_PATH];
    char title[64];
    char summary[96];
    char detail[96];
    char extra[96];
    char synopsis1[24];
    char synopsis2[24];
    u16 thumb[HFIX58_PREVIEW_W * HFIX58_PREVIEW_H];
} Hfix58BrowserPreview;

static Hfix58FileBrowser g_hfix58_browser;
static Hfix58BrowserPreview g_hfix58_preview;

/* HFIX60: preview debounce deadline in system ticks (0 = disabled).
   Cursor movement sets a ~200 ms deadline; the preview is only loaded
   once the selection has been stable for that interval. */
static u64 g_hfix58_preview_deadline = 0;

/* HFIX60: show-all toggle — when true the browser scans the SD root
   first so files outside the dedicated media folders are visible.
   Toggled with SELECT in the file browser. */
static bool g_hfix58_show_all_dirs = false;

static char g_hfix58_alert_text[96] = "";
static int  g_hfix58_alert_level = 0;
static u32  g_hfix58_alert_frames = 0;

static inline u16 hfix58_rgb565(int r, int g, int b) {
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;

    return (u16)(((r & 0xF8) << 8) |
                 ((g & 0xFC) << 3) |
                 ((b & 0xF8) >> 3));
}

static inline void hfix58_unpack565(u16 c, int *r, int *g, int *b) {
    int rr = (c >> 11) & 31;
    int gg = (c >> 5) & 63;
    int bb = c & 31;

    *r = (rr << 3) | (rr >> 2);
    *g = (gg << 2) | (gg >> 4);
    *b = (bb << 3) | (bb >> 2);
}

static inline void hfix58_px565(u8 *fb8, int x, int y, u16 c) {
    if (!fb8 || x < 0 || x >= 320 || y < 0 || y >= 240) {
        return;
    }

    ((u16*)fb8)[x * 240 + (239 - y)] = c;
}

static inline void hfix58_blend_px565(u8 *fb8, int x, int y, int r, int g, int b, int a) {
    if (!fb8 || x < 0 || x >= 320 || y < 0 || y >= 240) {
        return;
    }

    if (a <= 0) {
        return;
    }

    if (a >= 255) {
        hfix58_px565(fb8, x, y, hfix58_rgb565(r, g, b));
        return;
    }

    u16 *fb = (u16*)fb8;
    int idx = x * 240 + (239 - y);

    int dr, dg, db;
    hfix58_unpack565(fb[idx], &dr, &dg, &db);

    int nr = (dr * (255 - a) + r * a) / 255;
    int ng = (dg * (255 - a) + g * a) / 255;
    int nb = (db * (255 - a) + b * a) / 255;

    fb[idx] = hfix58_rgb565(nr, ng, nb);
}

static void hfix58_rect565(u8 *fb, int x, int y, int w, int h, int r, int g, int b) {
    u16 c = hfix58_rgb565(r, g, b);

    int x2 = x + w;
    int y2 = y + h;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > 320) x2 = 320;
    if (y2 > 240) y2 = 240;

    for (int xx = x; xx < x2; xx++) {
        u16 *col = ((u16*)fb) + xx * 240;

        for (int yy = y; yy < y2; yy++) {
            col[239 - yy] = c;
        }
    }
}

static void hfix58_blend_rect565(u8 *fb, int x, int y, int w, int h, int r, int g, int b, int a) {
    int x2 = x + w;
    int y2 = y + h;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > 320) x2 = 320;
    if (y2 > 240) y2 = 240;

    for (int xx = x; xx < x2; xx++) {
        for (int yy = y; yy < y2; yy++) {
            hfix58_blend_px565(fb, xx, yy, r, g, b, a);
        }
    }
}

/*
    Compact 5x7 uppercase bitmap font.
    Bits are stored left-to-right in the low five bits.
*/
static const u8 *hfix58_glyph(char c) {
    static const u8 sp[7] = {0,0,0,0,0,0,0};
    static const u8 qn[7] = {14,17,1,2,4,0,4};

    static const u8 d0[7] = {14,17,19,21,25,17,14};
    static const u8 d1[7] = {4,12,4,4,4,4,14};
    static const u8 d2[7] = {14,17,1,2,4,8,31};
    static const u8 d3[7] = {31,2,4,2,1,17,14};
    static const u8 d4[7] = {2,6,10,18,31,2,2};
    static const u8 d5[7] = {31,16,30,1,1,17,14};
    static const u8 d6[7] = {6,8,16,30,17,17,14};
    static const u8 d7[7] = {31,1,2,4,8,8,8};
    static const u8 d8[7] = {14,17,17,14,17,17,14};
    static const u8 d9[7] = {14,17,17,15,1,2,12};

    static const u8 A[7] = {14,17,17,31,17,17,17};
    static const u8 B[7] = {30,17,17,30,17,17,30};
    static const u8 C[7] = {14,17,16,16,16,17,14};
    static const u8 D[7] = {30,17,17,17,17,17,30};
    static const u8 E[7] = {31,16,16,30,16,16,31};
    static const u8 F[7] = {31,16,16,30,16,16,16};
    static const u8 G[7] = {14,17,16,23,17,17,14};
    static const u8 H[7] = {17,17,17,31,17,17,17};
    static const u8 I[7] = {14,4,4,4,4,4,14};
    static const u8 J[7] = {1,1,1,1,17,17,14};
    static const u8 K[7] = {17,18,20,24,20,18,17};
    static const u8 L[7] = {16,16,16,16,16,16,31};
    static const u8 M[7] = {17,27,21,21,17,17,17};
    static const u8 N[7] = {17,25,21,19,17,17,17};
    static const u8 O[7] = {14,17,17,17,17,17,14};
    static const u8 P[7] = {30,17,17,30,16,16,16};
    static const u8 Q[7] = {14,17,17,17,21,18,13};
    static const u8 R[7] = {30,17,17,30,20,18,17};
    static const u8 S[7] = {15,16,16,14,1,1,30};
    static const u8 T[7] = {31,4,4,4,4,4,4};
    static const u8 U[7] = {17,17,17,17,17,17,14};
    static const u8 V[7] = {17,17,17,17,17,10,4};
    static const u8 W[7] = {17,17,17,21,21,21,10};
    static const u8 X[7] = {17,17,10,4,10,17,17};
    static const u8 Y[7] = {17,17,10,4,4,4,4};
    static const u8 Z[7] = {31,1,2,4,8,16,31};

    static const u8 dot[7] = {0,0,0,0,0,12,12};
    static const u8 slash[7] = {1,1,2,4,8,16,16};
    static const u8 dash[7] = {0,0,0,31,0,0,0};
    static const u8 colon[7] = {0,12,12,0,12,12,0};
    static const u8 us[7] = {0,0,0,0,0,0,31};

    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 32);
    }

    if (c == ' ') return sp;
    if (c == '.') return dot;
    if (c == '/') return slash;
    if (c == '-') return dash;
    if (c == ':') return colon;
    if (c == '_') return us;

    if (c >= '0' && c <= '9') {
        const u8 *digits[10] = {d0,d1,d2,d3,d4,d5,d6,d7,d8,d9};
        return digits[c - '0'];
    }

    switch (c) {
        case 'A': return A; case 'B': return B; case 'C': return C; case 'D': return D;
        case 'E': return E; case 'F': return F; case 'G': return G; case 'H': return H;
        case 'I': return I; case 'J': return J; case 'K': return K; case 'L': return L;
        case 'M': return M; case 'N': return N; case 'O': return O; case 'P': return P;
        case 'Q': return Q; case 'R': return R; case 'S': return S; case 'T': return T;
        case 'U': return U; case 'V': return V; case 'W': return W; case 'X': return X;
        case 'Y': return Y; case 'Z': return Z;
        default: return qn;
    }
}

static void hfix58_draw_char(u8 *fb, int x, int y, char c, int scale, int r, int g, int b) {
    const u8 *glyph = hfix58_glyph(c);
    u16 color = hfix58_rgb565(r, g, b);

    if (scale < 1) {
        scale = 1;
    }

    for (int row = 0; row < 7; row++) {
        u8 bits = glyph[row];

        for (int col = 0; col < 5; col++) {
            if (bits & (1 << (4 - col))) {
                for (int yy = 0; yy < scale; yy++) {
                    for (int xx = 0; xx < scale; xx++) {
                        hfix58_px565(fb, x + col * scale + xx, y + row * scale + yy, color);
                    }
                }
            }
        }
    }
}

static void hfix58_draw_text(u8 *fb, int x, int y, const char *text, int scale, int r, int g, int b) {
    if (!text) {
        return;
    }

    int cx = x;

    for (const char *p = text; *p; p++) {
        hfix58_draw_char(fb, cx, y, *p, scale, r, g, b);
        cx += 6 * scale;
    }
}

static void hfix58_draw_text_shadow(u8 *fb, int x, int y, const char *text, int scale, int r, int g, int b) {
    hfix58_draw_text(fb, x + scale, y + scale, text, scale, 0, 0, 0);
    hfix58_draw_text(fb, x, y, text, scale, r, g, b);
}

static inline void hfix58s_top_px565(u8 *fb8, int x, int y, u16 c) {
    if (!fb8 || x < 0 || x >= TOP_W || y < 0 || y >= TOP_H) {
        return;
    }

    ((u16*)fb8)[x * TOP_H + (TOP_H - 1 - y)] = c;
}

static void hfix58s_top_rect565(u8 *fb, int x, int y, int w, int h, int r, int g, int b) {
    u16 c = hfix58_rgb565(r, g, b);
    int x2 = x + w;
    int y2 = y + h;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > TOP_W) x2 = TOP_W;
    if (y2 > TOP_H) y2 = TOP_H;

    for (int xx = x; xx < x2; xx++) {
        u16 *col = ((u16*)fb) + xx * TOP_H;
        for (int yy = y; yy < y2; yy++) {
            col[TOP_H - 1 - yy] = c;
        }
    }
}

static void hfix58s_top_draw_char(u8 *fb, int x, int y, char c, int scale, int r, int g, int b) {
    const u8 *glyph = hfix58_glyph(c);
    u16 color = hfix58_rgb565(r, g, b);

    if (scale < 1) {
        scale = 1;
    }

    for (int row = 0; row < 7; row++) {
        u8 bits = glyph[row];

        for (int col = 0; col < 5; col++) {
            if (bits & (1 << (4 - col))) {
                for (int yy = 0; yy < scale; yy++) {
                    for (int xx = 0; xx < scale; xx++) {
                        hfix58s_top_px565(fb, x + col * scale + xx, y + row * scale + yy, color);
                    }
                }
            }
        }
    }
}

static void hfix58s_top_draw_text_shadow(u8 *fb, int x, int y, const char *text, int scale, int r, int g, int b) {
    int cx = x;

    if (!text) {
        return;
    }

    for (const char *p = text; *p; p++) {
        hfix58s_top_draw_char(fb, cx + scale, y + scale, *p, scale, 0, 0, 0);
        hfix58s_top_draw_char(fb, cx, y, *p, scale, r, g, b);
        cx += 6 * scale;
    }
}

static void hfix58s_split_lines(const char *src, char *l1, size_t n1, char *l2, size_t n2) {
    const char *nl;

    if (!l1 || !l2 || n1 == 0 || n2 == 0) {
        return;
    }

    l1[0] = 0;
    l2[0] = 0;

    if (!src || !*src) {
        return;
    }

    nl = strchr(src, '\n');
    if (!nl) {
        snprintf(l1, n1, "%s", src);
        return;
    }

    {
        size_t first_len = (size_t)(nl - src);
        if (first_len >= n1) {
            first_len = n1 - 1;
        }

        memcpy(l1, src, first_len);
        l1[first_len] = 0;
    }

    snprintf(l2, n2, "%s", nl + 1);
}

static void hfix58s_draw_subtitle_overlay_top(u8 *fb) {
    char line1[96];
    char line2[96];
    int lines;
    int line1_w;
    int line2_w;
    int text_w;
    int box_w;
    int box_h;
    int box_x;
    int box_y;
    int fs;
    int gw;
    int gh;
    int pad;
    int line_gap;

    if (!fb || !g_hfix58s_subtitles_ready || !g_hfix58s_subtitle_current[0]) {
        return;
    }

    /* HFIX60: subtitle text size follows the FONT SCALE setting (1x..3x). */
    fs = (int)g_mivf_settings.font_scale;
    if (fs < 1) fs = 1;
    if (fs > 3) fs = 3;

    gw = 6 * fs;
    gh = 7 * fs;
    pad = 8;
    line_gap = 4 * fs;

    hfix58s_split_lines(
        g_hfix58s_subtitle_current,
        line1,
        sizeof(line1),
        line2,
        sizeof(line2)
    );

    lines = line2[0] ? 2 : 1;
    line1_w = (int)strlen(line1) * gw;
    line2_w = (int)strlen(line2) * gw;
    text_w = line1_w > line2_w ? line1_w : line2_w;

    if (text_w > TOP_W - 16) {
        text_w = TOP_W - 16;
    }

    box_w = text_w + 2 * pad + 4;
    if (box_w < 120) box_w = 120;
    if (box_w > TOP_W - 8) box_w = TOP_W - 8;

    box_h = (lines == 2) ? (gh * 2 + line_gap + 2 * pad) : (gh + 2 * pad);
    box_x = (TOP_W - box_w) / 2;
    switch (g_mivf_settings.subtitle_position % 3u) {
        case 2:
            box_y = 18;
            break;
        case 1:
            box_y = (TOP_H - box_h) / 2;
            break;
        default:
            box_y = TOP_H - box_h - 10;
            break;
    }

    hfix58s_top_rect565(fb, box_x - 2, box_y - 2, box_w + 4, box_h + 4, 0, 0, 0);
    hfix58s_top_rect565(fb, box_x, box_y, box_w, box_h, 2, 6, 14);
    hfix58s_top_rect565(fb, box_x + 2, box_y + 2, box_w - 4, 1, 60, 140, 220);

    hfix58s_top_draw_text_shadow(
        fb,
        box_x + (box_w - line1_w) / 2,
        box_y + pad,
        line1,
        fs,
        235,
        245,
        255
    );

    if (lines == 2) {
        hfix58s_top_draw_text_shadow(
            fb,
            box_x + (box_w - line2_w) / 2,
            box_y + pad + gh + line_gap,
            line2,
            fs,
            235,
            245,
            255
        );
    }
}

static void hfix58_alert_set(const char *msg, int level) {
    if (!msg) {
        return;
    }

    snprintf(g_hfix58_alert_text, sizeof(g_hfix58_alert_text), "%s", msg);
    g_hfix58_alert_level = level;
    g_hfix58_alert_frames = 180;

    /*
        HFIX58A_R5_ALERT_FORCE_REDRAW:
        keep bottom UI visible so alert can render on next present pass.
    */
#ifdef HFIX51C_DIRECT_UI
    g_media_ctl.ui_visible = true;
#endif
}

static void hfix58_alert_clear(void) {
    g_hfix58_alert_text[0] = '\0';
    g_hfix58_alert_level = 0;
    g_hfix58_alert_frames = 0;
}

static void hfix58_draw_alert(u8 *fb) {
    const char *kind = "INFO";

    if (!fb || g_hfix58_alert_text[0] == '\0' || g_hfix58_alert_frames == 0) {
        return;
    }

    int rr = 70;
    int gg = 150;
    int bb = 230;
    int tr = 245;
    int tg = 250;
    int tb = 255;

    if (g_hfix58_alert_level == 1) {
        rr = 70; gg = 210; bb = 110;
        kind = "OK";
        tr = 225; tg = 255; tb = 232;
    } else if (g_hfix58_alert_level == 2) {
        rr = 235; gg = 150; bb = 45;
        kind = "WARN";
        tr = 255; tg = 238; tb = 214;
    } else if (g_hfix58_alert_level == 3) {
        rr = 235; gg = 70; bb = 70;
        kind = "STOP";
        tr = 255; tg = 226; tb = 226;
    }

    hfix58_rect565(fb, 22, 58, 276, 30, 3, 6, 14);
    hfix58_rect565(fb, 24, 60, 272, 26, 14, 20, 34);
    hfix58_rect565(fb, 24, 60, 272, 1, rr / 2, gg / 2, bb / 2);
    hfix58_rect565(fb, 24, 60, 4, 26, rr, gg, bb);
    hfix58_rect565(fb, 32, 64, 38, 14, rr / 2, gg / 2, bb / 2);

    hfix58_draw_text_shadow(fb, 38, 67, kind, 1, tr, tg, tb);
    hfix58_draw_text_shadow(fb, 78, 67, g_hfix58_alert_text, 1, 240, 245, 255);

    if (g_hfix58_alert_frames > 0) {
        g_hfix58_alert_frames--;
        if (g_hfix58_alert_frames == 0) {
            g_hfix58_alert_text[0] = 0;
        }
    }
}

/* HFIX60: favorites store, persisted under the appdata tree (one path per line). */
#define MIVF_FAV_MAX 128
static char g_mivf_favorites[MIVF_FAV_MAX][HFIX58_MAX_PATH];
static int g_mivf_favorites_count = 0;
static bool g_mivf_favorites_loaded = false;

static void hfix60_fav_save(void);

static void hfix60_fav_load(void) {
    FILE *fp;
    char line[HFIX58_MAX_PATH];
    bool used_legacy = false;

    g_mivf_favorites_count = 0;
    g_mivf_favorites_loaded = true;

    MIVF_AppDataEnsureLayout();

    fp = fopen(MIVF_FAVORITES_PATH, "rb");
    if (!fp) {
        fp = fopen(MIVF_FAVORITES_LEGACY_PATH, "rb");
        if (fp) {
            used_legacy = true;
        }
    }

    if (!fp) {
        return;
    }

    while (fgets(line, sizeof(line), fp)) {
        size_t n = strlen(line);

        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r' ||
                         line[n - 1] == ' ' || line[n - 1] == '\t')) {
            line[--n] = 0;
        }

        if (n == 0) {
            continue;
        }

        if (g_mivf_favorites_count >= MIVF_FAV_MAX) {
            break;
        }

        snprintf(g_mivf_favorites[g_mivf_favorites_count], HFIX58_MAX_PATH, "%s", line);
        g_mivf_favorites_count++;
    }

    fclose(fp);

    if (used_legacy) {
        hfix60_fav_save();
    }
}

static void hfix60_fav_save(void) {
    FILE *fp;

    MIVF_AppDataEnsureLayout();
    fp = fopen(MIVF_FAVORITES_PATH, "wb");

    if (!fp) {
        return;
    }

    for (int i = 0; i < g_mivf_favorites_count; i++) {
        fprintf(fp, "%s\n", g_mivf_favorites[i]);
    }

    fclose(fp);
}

static int hfix60_fav_index(const char *path) {
    if (!path) {
        return -1;
    }

    for (int i = 0; i < g_mivf_favorites_count; i++) {
        if (!strcmp(g_mivf_favorites[i], path)) {
            return i;
        }
    }

    return -1;
}

static bool hfix60_fav_is(const char *path) {
    if (!g_mivf_favorites_loaded) {
        hfix60_fav_load();
    }

    return hfix60_fav_index(path) >= 0;
}

static void hfix60_fav_toggle(const char *path) {
    int idx;

    if (!path || !*path) {
        return;
    }

    if (!g_mivf_favorites_loaded) {
        hfix60_fav_load();
    }

    idx = hfix60_fav_index(path);

    if (idx >= 0) {
        for (int i = idx; i < g_mivf_favorites_count - 1; i++) {
            snprintf(g_mivf_favorites[i], HFIX58_MAX_PATH, "%s", g_mivf_favorites[i + 1]);
        }
        g_mivf_favorites_count--;
    } else if (g_mivf_favorites_count < MIVF_FAV_MAX) {
        snprintf(g_mivf_favorites[g_mivf_favorites_count], HFIX58_MAX_PATH, "%s", path);
        g_mivf_favorites_count++;
    }

    hfix60_fav_save();
}

#define MIVF_RECENT_MAX 16
static char g_mivf_recents[MIVF_RECENT_MAX][HFIX58_MAX_PATH];
static int g_mivf_recents_count = 0;
static bool g_mivf_recents_loaded = false;

static void hfix60_recent_save(void);

static void hfix60_recent_load(void) {
    FILE *fp;
    char line[HFIX58_MAX_PATH];
    bool used_legacy = false;

    g_mivf_recents_count = 0;
    g_mivf_recents_loaded = true;

    MIVF_AppDataEnsureLayout();

    fp = fopen(MIVF_RECENTS_PATH, "rb");
    if (!fp) {
        fp = fopen(MIVF_RECENTS_LEGACY_PATH, "rb");
        if (fp) {
            used_legacy = true;
        }
    }

    if (!fp) {
        return;
    }

    while (fgets(line, sizeof(line), fp)) {
        size_t n = strlen(line);

        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r' ||
                         line[n - 1] == ' ' || line[n - 1] == '\t')) {
            line[--n] = 0;
        }

        if (n == 0) {
            continue;
        }

        if (g_mivf_recents_count >= MIVF_RECENT_MAX) {
            break;
        }

        snprintf(g_mivf_recents[g_mivf_recents_count], HFIX58_MAX_PATH, "%s", line);
        g_mivf_recents_count++;
    }

    fclose(fp);

    if (used_legacy) {
        hfix60_recent_save();
    }
}

static void hfix60_recent_save(void) {
    FILE *fp;

    MIVF_AppDataEnsureLayout();
    fp = fopen(MIVF_RECENTS_PATH, "wb");

    if (!fp) {
        return;
    }

    for (int i = 0; i < g_mivf_recents_count; i++) {
        fprintf(fp, "%s\n", g_mivf_recents[i]);
    }

    fclose(fp);
}

static int hfix60_recent_index(const char *path) {
    if (!path) {
        return -1;
    }

    if (!g_mivf_recents_loaded) {
        hfix60_recent_load();
    }

    for (int i = 0; i < g_mivf_recents_count; i++) {
        if (!strcmp(g_mivf_recents[i], path)) {
            return i;
        }
    }

    return -1;
}

static bool hfix60_recent_is(const char *path) {
    return hfix60_recent_index(path) >= 0;
}

static void hfix60_recent_note(const char *path) {
    int idx;

    if (!path || !*path) {
        return;
    }

    if (!g_mivf_recents_loaded) {
        hfix60_recent_load();
    }

    idx = hfix60_recent_index(path);
    if (idx == 0) {
        return;
    }

    if (idx > 0) {
        for (int i = idx; i > 0; i--) {
            snprintf(g_mivf_recents[i], HFIX58_MAX_PATH, "%s", g_mivf_recents[i - 1]);
        }
    } else {
        if (g_mivf_recents_count < MIVF_RECENT_MAX) {
            g_mivf_recents_count++;
        }
        for (int i = g_mivf_recents_count - 1; i > 0; i--) {
            snprintf(g_mivf_recents[i], HFIX58_MAX_PATH, "%s", g_mivf_recents[i - 1]);
        }
    }

    snprintf(g_mivf_recents[0], HFIX58_MAX_PATH, "%s", path);
    hfix60_recent_save();
}

typedef enum {
    HFIX58_MEDIA_UNKNOWN = 0,
    HFIX58_MEDIA_MIVF,
    HFIX58_MEDIA_MOFLEX,
} Hfix58MediaKind;

static Hfix58MediaKind hfix58_media_kind(const char *name) {
    if (!name) {
        return HFIX58_MEDIA_UNKNOWN;
    }

    size_t n = strlen(name);

    if (n >= 5) {
        const char *e = name + n - 5;

        if (tolower((unsigned char)e[0]) == '.' &&
            tolower((unsigned char)e[1]) == 'm' &&
            tolower((unsigned char)e[2]) == 'i' &&
            tolower((unsigned char)e[3]) == 'v' &&
            tolower((unsigned char)e[4]) == 'f') {
            return HFIX58_MEDIA_MIVF;
        }
    }

    if (n >= 7) {
        const char *e = name + n - 7;

        if (tolower((unsigned char)e[0]) == '.' &&
            tolower((unsigned char)e[1]) == 'm' &&
            tolower((unsigned char)e[2]) == 'o' &&
            tolower((unsigned char)e[3]) == 'f' &&
            tolower((unsigned char)e[4]) == 'l' &&
            tolower((unsigned char)e[5]) == 'e' &&
            tolower((unsigned char)e[6]) == 'x') {
            return HFIX58_MEDIA_MOFLEX;
        }
    }

    return HFIX58_MEDIA_UNKNOWN;
}

static bool hfix58_is_supported_media(const char *name) {
    return hfix58_media_kind(name) != HFIX58_MEDIA_UNKNOWN;
}

static int hfix58_file_cmp(const void *a, const void *b) {
    const Hfix58FileEntry *fa = (const Hfix58FileEntry*)a;
    const Hfix58FileEntry *fb = (const Hfix58FileEntry*)b;
    return strcmp(fa->name, fb->name);
}

static bool hfix58_media_file_exists(const char *path) {
    FILE *fp;

    if (!path || !*path) {
        return false;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        return false;
    }

    fclose(fp);
    return true;
}

static int hfix58_browser_find_path(const char *path) {
    if (!path) {
        return -1;
    }

    for (int i = 0; i < g_hfix58_browser.count; i++) {
        if (!strcmp(g_hfix58_browser.entries[i].path, path)) {
            return i;
        }
    }

    return -1;
}

static void hfix58_browser_make_entry(Hfix58FileEntry *entry, const char *path, u8 quick) {
    const char *base = path;

    if (!entry || !path) {
        return;
    }

    memset(entry, 0, sizeof(*entry));

    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }

    snprintf(entry->name, sizeof(entry->name), "%s", base && *base ? base : path);
    snprintf(entry->path, sizeof(entry->path), "%s", path);
    entry->quick = quick;
}

static void hfix58_browser_promote_path(const char *path, u8 quick) {
    Hfix58FileEntry entry;
    int idx;

    if (!path || !*path || !hfix58_media_file_exists(path)) {
        return;
    }

    idx = hfix58_browser_find_path(path);
    if (idx >= 0) {
        entry = g_hfix58_browser.entries[idx];
        if (quick > entry.quick) {
            entry.quick = quick;
        }

        for (int i = idx; i > 0; i--) {
            g_hfix58_browser.entries[i] = g_hfix58_browser.entries[i - 1];
        }
        g_hfix58_browser.entries[0] = entry;
        return;
    }

    if (g_hfix58_browser.count >= HFIX58_MAX_BROWSER_FILES) {
        g_hfix58_browser.count = HFIX58_MAX_BROWSER_FILES - 1;
    }

    hfix58_browser_make_entry(&entry, path, quick);

    for (int i = g_hfix58_browser.count; i > 0; i--) {
        g_hfix58_browser.entries[i] = g_hfix58_browser.entries[i - 1];
    }

    g_hfix58_browser.entries[0] = entry;
    g_hfix58_browser.count++;
}

static void hfix58_browser_promote_quick_access(void) {
    if (!g_mivf_recents_loaded) {
        hfix60_recent_load();
    }

    if (!g_mivf_favorites_loaded) {
        hfix60_fav_load();
    }

    for (int i = g_mivf_favorites_count - 1; i >= 0; i--) {
        hfix58_browser_promote_path(g_mivf_favorites[i], 2);
    }

    for (int i = g_mivf_recents_count - 1; i >= 0; i--) {
        hfix58_browser_promote_path(g_mivf_recents[i], 1);
    }

    g_hfix58_browser.selected = 0;
    g_hfix58_browser.scroll = 0;
}

static void hfix58_preview_clear(void) {
    memset(&g_hfix58_preview, 0, sizeof(g_hfix58_preview));
    g_hfix58_preview_deadline = 0;
}

static const char *hfix58_preview_basename(const char *path) {
    const char *slash;
    const char *backslash;
    const char *base;

    if (!path) {
        return "";
    }

    slash = strrchr(path, '/');
    backslash = strrchr(path, '\\');
    base = slash;

    if (!base || (backslash && backslash > base)) {
        base = backslash;
    }

    return base ? base + 1 : path;
}

static void hfix58_format_duration(char *out, size_t out_sz, u32 sec) {
    u32 h = sec / 3600u;
    u32 m = (sec / 60u) % 60u;
    u32 s = sec % 60u;

    if (!out || out_sz == 0) {
        return;
    }

    if (h > 0) {
        snprintf(out, out_sz, "%u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
    } else {
        snprintf(out, out_sz, "%02u:%02u", (unsigned)m, (unsigned)s);
    }
}

static void hfix58_scale_rgb565(const u16 *src, int sw, int sh, u16 *dst, int dw, int dh) {
    if (!src || !dst || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) {
        return;
    }

    for (int y = 0; y < dh; y++) {
        int sy = (int)(((u64)y * (u64)sh) / (u64)dh);

        if (sy < 0) sy = 0;
        if (sy >= sh) sy = sh - 1;

        for (int x = 0; x < dw; x++) {
            int sx = (int)(((u64)x * (u64)sw) / (u64)dw);

            if (sx < 0) sx = 0;
            if (sx >= sw) sx = sw - 1;

            dst[y * dw + x] = src[sy * sw + sx];
        }
    }
}

static bool hfix58_decode_browser_thumb(FILE *f, const Header *h, const Stream *v) {
    u8 page_hdr[MIVF_PAGE_HEADER_SIZE];
    u8 pkt_hdr[16];
    u64 pos;
    bool decoded = false;
    size_t full_size;
    u8 *frame = NULL;
    u8 *prev = NULL;
    M2Y0Frame m2y0;
    M2Y0Frame m2y0_prev;
    bool have_m2y0_prev = false;

    if (!f || !h || !v || !v->codec[0] || !v->w || !v->h) {
        return false;
    }

    full_size = (size_t)v->w * (size_t)v->h * 2u;

    if (full_size == 0 || full_size > (size_t)(1024u * 1024u * 2u)) {
        return false;
    }

    frame = (u8*)malloc(full_size);
    if (!frame) {
        return false;
    }

    if (!strcmp(v->codec, "M1P0") || !strcmp(v->codec, "M1P1")) {
        prev = (u8*)calloc(1, full_size);
        if (!prev) {
            free(frame);
            return false;
        }
    }

    memset(&m2y0, 0, sizeof(m2y0));
    memset(&m2y0_prev, 0, sizeof(m2y0_prev));

    if (!strcmp(v->codec, "M2Y0") || !strcmp(v->codec, "M2Y1") || !strcmp(v->codec, "M2Y2")) {
        if (!m2y0_frame_alloc(&m2y0, v->w, v->h) ||
            !m2y0_frame_alloc(&m2y0_prev, v->w, v->h)) {
            if (m2y0.base) m2y0_frame_free(&m2y0);
            if (m2y0_prev.base) m2y0_frame_free(&m2y0_prev);
            free(prev);
            free(frame);
            return false;
        }
    }

    if (fseek(f, (long)h->first, SEEK_SET) != 0) {
        if (m2y0.base) m2y0_frame_free(&m2y0);
        if (m2y0_prev.base) m2y0_frame_free(&m2y0_prev);
        free(prev);
        free(frame);
        return false;
    }

    pos = h->first;

    while (fread(page_hdr, 1, MIVF_PAGE_HEADER_SIZE, f) == MIVF_PAGE_HEADER_SIZE) {
        u32 payload = le32(page_hdr + 0x10);
        u16 packets = le16(page_hdr + 0x14);
        u8 *payload_buf;

        if (payload == 0 || payload > (1024u * 1024u * 4u) || packets == 0 || packets > 128) {
            break;
        }

        payload_buf = (u8*)malloc(payload);
        if (!payload_buf) {
            break;
        }

        if (fread(payload_buf, 1, payload, f) != payload) {
            free(payload_buf);
            break;
        }

        size_t off = 0;

        for (u16 i = 0; i < packets; i++) {
            Packet k;
            const u8 *body;

            if (off + 16 > payload) {
                break;
            }

            if (read_packet(payload_buf + off, payload - off, &k)) {
                break;
            }

            body = payload_buf + off + k.hsize;

            if (k.sid != v->id) {
                off += (size_t)k.hsize + (size_t)k.psize;
                continue;
            }

            decoded = false;

            if (!strcmp(v->codec, "RAWV") && k.psize == full_size) {
                memcpy(frame, body, full_size);
                decoded = true;
            } else if (!strcmp(v->codec, "M2Y0") && k.psize >= 28 && body[0] == 'M' && body[1] == '2' && body[2] == 'Y' && body[3] == '0') {
                if (dec_m2y0_raw(body, k.psize, &m2y0) == 0) {
                    m2y0_to_rgb565(&m2y0, frame);
                    decoded = true;
                }
            } else if (!strcmp(v->codec, "M2Y1") && k.psize >= 4 && body[0] == 'M' && body[1] == '2' && body[2] == 'Y' && body[3] == '1') {
                if (dec_m2y1(body, k.psize, &m2y0, &m2y0_prev, have_m2y0_prev) == 0) {
                    m2y0_to_rgb565(&m2y0, frame);
                    m2y0_frame_copy(&m2y0_prev, &m2y0);
                    have_m2y0_prev = true;
                    decoded = true;
                }
            } else if (!strcmp(v->codec, "M2Y2") && k.psize >= 4 && body[0] == 'M' && body[1] == '2' && body[2] == 'Y' && body[3] == '2') {
                if (dec_m2y2(body, k.psize, &m2y0, &m2y0_prev, have_m2y0_prev) == 0) {
                    m2y0_to_rgb565(&m2y0, frame);
                    m2y0_frame_copy(&m2y0_prev, &m2y0);
                    have_m2y0_prev = true;
                    decoded = true;
                }
            } else if (!strcmp(v->codec, "M1P0") && k.psize >= 4 && body[0] == 'M' && body[1] == '1' && body[2] == 'P' && body[3] == '0') {
                if (dec_m1p0(body, k.psize, frame, prev, prev != NULL, v->w, v->h) == 0) {
                    memcpy(prev, frame, full_size);
                    decoded = true;
                }
            } else if (!strcmp(v->codec, "M1P1") && k.psize >= 4 && body[0] == 'M' && body[1] == '1' && body[2] == 'P' && body[3] == '1') {
                if (dec_m1p1(body, k.psize, frame, prev, prev != NULL, v->w, v->h) == 0) {
                    memcpy(prev, frame, full_size);
                    decoded = true;
                }
            }

            if (decoded) {
                hfix58_scale_rgb565((const u16*)frame, v->w, v->h, g_hfix58_preview.thumb, HFIX58_PREVIEW_W, HFIX58_PREVIEW_H);
                g_hfix58_preview.has_thumb = true;
                break;
            }

            off += (size_t)k.hsize + (size_t)k.psize;
        }

        free(payload_buf);

        if (decoded) {
            break;
        }

        pos += MIVF_PAGE_HEADER_SIZE + payload;
        if (fseek(f, (long)pos, SEEK_SET) != 0) {
            break;
        }
    }

    if (m2y0.base) m2y0_frame_free(&m2y0);
    if (m2y0_prev.base) m2y0_frame_free(&m2y0_prev);
    free(prev);
    free(frame);
    return g_hfix58_preview.has_thumb;
}

/* HFIX60: optional ".cover" raw RGB565 poster (exactly preview-sized) overrides
   the auto-decoded first-frame thumbnail. */
static bool hfix60_load_cover(const char *video_path) {
    char path[HFIX58_MAX_PATH];
    FILE *cf;
    size_t need = (size_t)HFIX58_PREVIEW_W * (size_t)HFIX58_PREVIEW_H * 2u;
    size_t got;

    hfix60_make_sidecar_path(path, sizeof(path), video_path, ".cover");
    if (!path[0]) {
        return false;
    }

    cf = fopen(path, "rb");
    if (!cf) {
        return false;
    }

    got = fread(g_hfix58_preview.thumb, 1, need, cf);
    fclose(cf);

    if (got == need) {
        g_hfix58_preview.has_thumb = true;
        return true;
    }

    return false;
}

/* HFIX60: optional ".nfo" synopsis text shown in the preview panel. */
static void hfix60_load_nfo(const char *video_path) {
    char path[HFIX58_MAX_PATH];
    FILE *nf;
    char buf[256];
    char clean[128];
    size_t got;
    int ci = 0;
    bool prev_space = false;

    hfix60_make_sidecar_path(path, sizeof(path), video_path, ".nfo");
    if (!path[0]) {
        return;
    }

    nf = fopen(path, "rb");
    if (!nf) {
        return;
    }

    got = fread(buf, 1, sizeof(buf) - 1, nf);
    fclose(nf);
    buf[got] = 0;

    for (size_t i = 0; buf[i] && ci < (int)sizeof(clean) - 1; i++) {
        char c = buf[i];

        if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
            if (!prev_space && ci > 0) {
                clean[ci++] = ' ';
                prev_space = true;
            }
        } else {
            clean[ci++] = c;
            prev_space = false;
        }
    }

    clean[ci] = 0;

    snprintf(g_hfix58_preview.synopsis1, sizeof(g_hfix58_preview.synopsis1), "%.19s", clean);

    if ((int)strlen(clean) > 19) {
        snprintf(g_hfix58_preview.synopsis2, sizeof(g_hfix58_preview.synopsis2), "%.19s", clean + 19);
    }
}

static bool hfix58_browser_load_preview(const char *path) {
    FILE *f;
    Header h;
    Stream v;
    Stream a;
    char dur[16];
    char srt_path[HFIX58_MAX_PATH];
    bool has_srt = false;
    MivfBookmark bookmark;

    hfix58_preview_clear();

    if (!path || !*path) {
        return false;
    }

    snprintf(g_hfix58_preview.path, sizeof(g_hfix58_preview.path), "%s", path);

    if (hfix58_media_kind(path) == HFIX58_MEDIA_MOFLEX) {
        long long resume_us = moflex_resume_get(path);

        snprintf(g_hfix58_preview.title, sizeof(g_hfix58_preview.title), "%s", hfix58_preview_basename(path));
        snprintf(g_hfix58_preview.summary, sizeof(g_hfix58_preview.summary), "MOFLEX 3D VIDEO");
        snprintf(g_hfix58_preview.detail, sizeof(g_hfix58_preview.detail), "MOBICLIP + ADPCM");
        snprintf(g_hfix58_preview.extra, sizeof(g_hfix58_preview.extra), "CONTINUE %s",
            resume_us > 3000000 ? "YES" : "NO");
        g_hfix58_preview.has_resume = resume_us > 3000000;
        g_hfix58_preview.valid = true;
        return true;
    }

    f = fopen(path, "rb");
    if (!f) {
        return false;
    }

    memset(&h, 0, sizeof(h));
    memset(&v, 0, sizeof(v));
    memset(&a, 0, sizeof(a));

    if (read_header(f, &h)) {
        fclose(f);
        return false;
    }

    for (u32 i = 0; i < h.streams; i++) {
        Stream st;

        if (read_stream(f, &st)) {
            fclose(f);
            return false;
        }

        if (st.type == 1 && !v.type) {
            v = st;
        } else if (st.type == 2 && !a.type) {
            a = st;
        }
    }

    if (!v.type) {
        fclose(f);
        return false;
    }

    snprintf(g_hfix58_preview.title, sizeof(g_hfix58_preview.title), "%s", hfix58_preview_basename(path));
    hfix58_format_duration(dur, sizeof(dur), (u32)(h.duration / 30000ull));

    snprintf(g_hfix58_preview.summary, sizeof(g_hfix58_preview.summary), "%s %ux%u @ %u/%u",
        v.codec,
        v.w,
        v.h,
        v.fpsn,
        v.fpsd ? v.fpsd : 1);

    if (a.type == 2) {
        snprintf(g_hfix58_preview.detail, sizeof(g_hfix58_preview.detail), "AUDIO %s %u/%u",
            a.codec,
            a.w,
            a.h);
    } else {
        snprintf(g_hfix58_preview.detail, sizeof(g_hfix58_preview.detail), "AUDIO NONE");
    }

    if (MIVF_SubtitlesMakeSidecarPath(path, srt_path, sizeof(srt_path))) {
        FILE *sf = fopen(srt_path, "rb");
        if (sf) {
            has_srt = true;
            fclose(sf);
        }
    }

    g_hfix58_preview.has_resume =
        MIVF_BookmarkLoad(path, &bookmark) &&
        bookmark.video_path[0] &&
        !strcmp(bookmark.video_path, path) &&
        bookmark.frame > 0;

    snprintf(g_hfix58_preview.extra, sizeof(g_hfix58_preview.extra), "DUR %s  SUB %s  CONT %s",
        dur,
        has_srt ? "YES" : "NO",
        g_hfix58_preview.has_resume ? "YES" : "NO");

    g_hfix58_preview.valid = true;

    /* HFIX60: load optional synopsis text and poster image sidecars.
       A ".cover" poster (raw RGB565, preview-sized) overrides the auto thumbnail. */
    hfix60_load_nfo(path);

    if (!hfix60_load_cover(path)) {
        hfix58_decode_browser_thumb(f, &h, &v);
    }

    fclose(f);
    return true;
}

static void hfix58_browser_refresh_preview(void) {
    if (g_hfix58_browser.count <= 0 ||
        g_hfix58_browser.selected < 0 ||
        g_hfix58_browser.selected >= g_hfix58_browser.count) {
        hfix58_preview_clear();
        return;
    }

    if (g_hfix58_preview.valid &&
        strcmp(g_hfix58_preview.path, g_hfix58_browser.entries[g_hfix58_browser.selected].path) == 0) {
        return;
    }

    /* Debounce: defer preview load until selection is stable (~200 ms).
       The caller in the browser loop triggers a redraw once the
       deadline expires. */
    if (g_hfix58_preview_deadline != 0 &&
        svcGetSystemTick() < g_hfix58_preview_deadline) {
        return;
    }
    g_hfix58_preview_deadline = 0;

    hfix58_browser_load_preview(g_hfix58_browser.entries[g_hfix58_browser.selected].path);
}

static void hfix58_draw_browser_preview(u8 *fb) {
    int x = 174;
    int y = 58;
    int w = 130;
    int h = 160;

    hfix58_rect565(fb, x, y, w, h, 7, 10, 18);
    hfix58_rect565(fb, x, y, w, 2, g_mivf_theme_r, g_mivf_theme_g, g_mivf_theme_b);
    hfix58_draw_text_shadow(fb, x + 8, y + 6, "PREVIEW", 1, 235, 245, 255);

    hfix58_rect565(fb, x + 8, y + 18, 114, 64, 10, 14, 24);

    if (g_hfix58_preview.valid && g_hfix58_preview.has_thumb) {
        int thumb_x = x + 8;
        int thumb_y = y + 18;

        for (int yy = 0; yy < HFIX58_PREVIEW_H && yy < 64; yy++) {
            for (int xx = 0; xx < HFIX58_PREVIEW_W && xx < 114; xx++) {
                u16 c = g_hfix58_preview.thumb[yy * HFIX58_PREVIEW_W + xx];
                ((u16*)fb)[(thumb_x + xx) * 240 + (239 - (thumb_y + yy))] = c;
            }
        }
    } else if (g_hfix58_preview.valid && hfix58_media_kind(g_hfix58_preview.path) == HFIX58_MEDIA_MOFLEX) {
        hfix58_rect565(fb, x + 8, y + 18, 114, 64, 12, 18, 30);
        hfix58_rect565(fb, x + 22, y + 35, 86, 2, g_mivf_theme_r, g_mivf_theme_g, g_mivf_theme_b);
        hfix58_draw_text_shadow(fb, x + 36, y + 44, "MOFLEX", 1, 225, 235, 255);
    } else {
        hfix58_draw_text_shadow(fb, x + 18, y + 42, "NO THUMB", 1, 200, 210, 220);
    }

    if (g_hfix58_preview.has_resume) {
        hfix58_rect565(fb, x + 78, y + 70, 44, 10, 40, 92, 48);
        hfix58_draw_text_shadow(fb, x + 83, y + 72, "CONT", 1, 220, 255, 220);
    }

    hfix58_rect565(fb, x + 8, y + 84, 114, 1, 34, 48, 72);
    hfix58_draw_text_shadow(fb, x + 8, y + 90, g_hfix58_preview.title[0] ? g_hfix58_preview.title : "NO FILE", 1, 210, 225, 245);
    hfix58_draw_text_shadow(fb, x + 8, y + 102, g_hfix58_preview.summary[0] ? g_hfix58_preview.summary : "", 1, 185, 205, 230);
    hfix58_draw_text_shadow(fb, x + 8, y + 114, g_hfix58_preview.detail[0] ? g_hfix58_preview.detail : "", 1, 185, 205, 230);
    hfix58_draw_text_shadow(fb, x + 8, y + 126, g_hfix58_preview.extra[0] ? g_hfix58_preview.extra : "", 1, 170, 190, 215);

    /* HFIX60: optional synopsis from a ".nfo" sidecar. */
    if (g_hfix58_preview.synopsis1[0]) {
        hfix58_rect565(fb, x + 8, y + 137, 114, 1, 40, 60, 90);
        hfix58_draw_text_shadow(fb, x + 8, y + 140, g_hfix58_preview.synopsis1, 1, 200, 215, 235);
        if (g_hfix58_preview.synopsis2[0]) {
            hfix58_draw_text_shadow(fb, x + 8, y + 150, g_hfix58_preview.synopsis2, 1, 200, 215, 235);
        }
    }

    /* HFIX60: compact file-size badge from scan-time entry data.
       Zero draw-time I/O — the size was captured once during
       hfix58_scan_dir via stat(). */
    {
        u32 kb = 0;
        if (g_hfix58_browser.selected >= 0 &&
            g_hfix58_browser.selected < g_hfix58_browser.count) {
            kb = g_hfix58_browser.entries[g_hfix58_browser.selected].file_size_kb;
        }
        if (kb > 0) {
            char sz[24];
            if (kb >= 1024) {
                snprintf(sz, sizeof(sz), "%lu.%lu MB",
                    (unsigned long)(kb / 1024u),
                    (unsigned long)((kb % 1024u) * 10u / 1024u));
            } else {
                snprintf(sz, sizeof(sz), "%lu KB", (unsigned long)kb);
            }
            int sy = g_hfix58_preview.synopsis2[0] ? y + 158 :
                     g_hfix58_preview.synopsis1[0] ? y + 148 : y + 134;
            hfix58_draw_text_shadow(fb, x + 8, sy, sz, 1, 155, 195, 235);
        }
    }
}

/* HFIX60: known system-folder names to skip when show-all is off.
   These are directory names (case-insensitive match), not full paths.
   In MIVF's flat browser model directories already fail the extension
   check, so this is an explicit belt-and-suspenders filter plus a
   foundation for any future folder-browsing UI. */
static bool hfix58_is_system_folder_name(const char *name) {
    static const char *sys[] = {
        "nintendo 3ds", "dcim", "3ds", "luma", "gm9", "cias",
        "private", "boot9strap", "themes", "fbi", "updates",
        NULL
    };
    if (!name || !*name) return false;
    for (int i = 0; sys[i]; i++) {
        if (!strcasecmp(name, sys[i])) return true;
    }
    return false;
}

static bool hfix58_scan_dir(const char *dir) {
    DIR *d = opendir(dir);

    g_hfix58_browser.count = 0;
    g_hfix58_browser.selected = 0;
    g_hfix58_browser.scroll = 0;
    snprintf(g_hfix58_browser.cwd, sizeof(g_hfix58_browser.cwd), "%s", dir);

    if (!d) {
        return false;
    }

    struct dirent *ent;

    while ((ent = readdir(d)) != NULL) {
        if (g_hfix58_browser.count >= HFIX58_MAX_BROWSER_FILES) {
            break;
        }

        /* HFIX60: when show-all is off, skip entries whose names
           match known system folders — belt-and-suspenders on top
           of the extension filter (directories already fail it). */
        if (!g_hfix58_show_all_dirs &&
            hfix58_is_system_folder_name(ent->d_name)) {
            continue;
        }

        if (!hfix58_is_supported_media(ent->d_name)) {
            continue;
        }

        Hfix58FileEntry *out = &g_hfix58_browser.entries[g_hfix58_browser.count++];

        memset(out, 0, sizeof(*out));
        snprintf(out->name, sizeof(out->name), "%s", ent->d_name);
        out->quick = 0;

        if (dir[strlen(dir) - 1] == '/') {
            snprintf(out->path, sizeof(out->path), "%s%s", dir, ent->d_name);
        } else {
            snprintf(out->path, sizeof(out->path), "%s/%s", dir, ent->d_name);
        }

        /* Capture file size once at scan time — cheap stat() call,
           never repeated during draw or selection changes. */
        {
            struct stat st;
            if (stat(out->path, &st) == 0 && st.st_size > 0) {
                out->file_size_kb = (u32)((u64)st.st_size / 1024u);
            }
        }
    }

    closedir(d);

    if (g_hfix58_browser.count > 1) {
        qsort(
            g_hfix58_browser.entries,
            g_hfix58_browser.count,
            sizeof(g_hfix58_browser.entries[0]),
            hfix58_file_cmp
        );
    }

    return g_hfix58_browser.count > 0;
}

/* Auto-advance helper: find the next .mivf file (natural sort) in the same
   folder as cur_path. Writes the full path to out and returns true, or returns
   false when cur_path is the last file or its folder cannot be scanned.
   Note: this reuses g_hfix58_browser as scratch, which is safe because the
   caller either plays the next file or falls back to a fresh browser scan. */
static bool mivf_find_next_in_folder(const char *cur_path, char *out, size_t out_sz) {
    char dir[HFIX58_MAX_PATH];
    const char *slash;
    const char *base;
    size_t dlen;
    int i;

    if (!cur_path || !*cur_path || !out || out_sz == 0) {
        return false;
    }

    slash = strrchr(cur_path, '/');
    if (!slash) {
        return false;
    }

    base = slash + 1;
    dlen = (size_t)(slash - cur_path);
    if (dlen >= sizeof(dir)) {
        dlen = sizeof(dir) - 1;
    }
    memcpy(dir, cur_path, dlen);
    dir[dlen] = 0;

    /* "sdmc:/file.mivf" leaves dir == "sdmc:" — restore the drive root slash. */
    if (dlen == 0 || dir[dlen - 1] == ':') {
        snprintf(dir + dlen, sizeof(dir) - dlen, "/");
    }

    if (!hfix58_scan_dir(dir)) {
        return false;
    }

    for (i = 0; i < g_hfix58_browser.count; i++) {
        if (!strcmp(g_hfix58_browser.entries[i].name, base)) {
            if (i + 1 < g_hfix58_browser.count) {
                snprintf(out, out_sz, "%s", g_hfix58_browser.entries[i + 1].path);
                return true;
            }
            return false; /* current file is the last in the folder */
        }
    }

    return false;
}

static bool hfix58_scan_default_dirs(void) {
    /* When show-all is off (default), dedicated media folders are
       scanned first; the SD root is only a fallback.  When show-all
       is on the root is scanned first so files placed at the top
       level are visible immediately. */
    static const char *hidden_dirs[] = {
        "sdmc:/mivf",
        "sdmc:/3ds/mivf_player_3ds",
        "sdmc:/",
        NULL
    };
    static const char *show_all_dirs[] = {
        "sdmc:/",
        "sdmc:/mivf",
        "sdmc:/3ds/mivf_player_3ds",
        NULL
    };
    const char **dirs = g_hfix58_show_all_dirs ? show_all_dirs : hidden_dirs;

    for (int i = 0; dirs[i]; i++) {
        if (hfix58_scan_dir(dirs[i])) {
            hfix58_browser_promote_quick_access();
            return true;
        }
    }

    return false;
}

static void hfix58_browser_redraw(void);

static void hfix58_restore_browser_after_moflex(void) {
    gfxSet3D(false);
    gfxSetScreenFormat(GFX_TOP, GSP_RGB565_OES);
    gfxSetScreenFormat(GFX_BOTTOM, GSP_RGB565_OES);
    hfix58_preview_clear();
    hfix58_browser_redraw();
}

static MoflexResult play_moflex_selected_media(const char *path) {
    MoflexResult result;

    if (!path || !*path) {
        return MOFLEX_ERROR;
    }

    hfix58_alert_clear();

    /* Release MIVF's per-file audio state before MoFlex takes NDSP channel 0. */
    audio_shutdown();
    moflex_set_audio_enabled(g_ndsp_ready);

    result = moflex_play(path);

    hfix58_restore_browser_after_moflex();

    if (result == MOFLEX_ERROR) {
        hfix58_alert_set("MoFlex playback error", 2);
        hfix58_browser_redraw();
    }

    return result;
}

static void hfix58_draw_browser(u8 *fb) {
    if (!fb) {
        return;
    }

    hfix58_browser_refresh_preview();

    hfix58_rect565(fb, 0, 0, 320, 240, 4, 8, 18);

    hfix58_blend_rect565(fb, 10, 8, 300, 224, 12, 18, 34, 230);
    hfix58_rect565(fb, 10, 8, 300, 2, g_mivf_theme_r, g_mivf_theme_g, g_mivf_theme_b);
    hfix58_rect565(fb, 10, 230, 300, 2, 2, 4, 8);

    hfix58_draw_text_shadow(fb, 20, 18, "LIBRARY", 2, 235, 245, 255);
    hfix58_draw_text_shadow(fb, 118, 24, "MIVF / MOFLEX", 1, 160, 195, 230);

    hfix58_blend_rect565(fb, 18, 46, 150, 22, 6, 10, 22, 220);
    hfix58_draw_text_shadow(fb, 26, 53, g_hfix58_browser.cwd, 1, 160, 200, 255);
    hfix58_rect565(fb, 24, 68, 132, 1, 36, 58, 90);

    hfix58_draw_browser_preview(fb);

    if (g_hfix58_browser.count <= 0) {
        hfix58_blend_rect565(fb, 24, 98, 138, 52, 10, 14, 26, 230);
        hfix58_rect565(fb, 24, 98, 4, 52, g_mivf_theme_r, g_mivf_theme_g, g_mivf_theme_b);
        hfix58_draw_text_shadow(fb, 34, 108, "NO .MIVF FILES FOUND", 1, 250, 186, 86);
        hfix58_draw_text_shadow(fb, 38, 132, "PUT FILES IN SDMC:/MIVF", 1, 216, 226, 236);
    } else {
        int first = g_hfix58_browser.scroll;
        int last = first + HFIX58_BROWSER_VISIBLE_ROWS;

        if (last > g_hfix58_browser.count) {
            last = g_hfix58_browser.count;
        }

        hfix58_draw_text_shadow(fb, 28, 72,
            (first == 0 && g_hfix58_browser.entries[0].quick) ? "QUICK ACCESS" : "FILES",
            1, 140, 175, 210);

        int y = 84;

        for (int i = first; i < last; i++) {
            bool selected = (i == g_hfix58_browser.selected);
            Hfix58FileEntry *entry = &g_hfix58_browser.entries[i];
            Hfix58MediaKind kind = hfix58_media_kind(entry->name);
            const char *badge = kind == HFIX58_MEDIA_MOFLEX ? "MOF" : "MIV";
            int br = 34;
            int bg = 48;
            int bb = 72;

            if (entry->quick == 2) {
                badge = "FAV";
                br = 130; bg = 102; bb = 34;
            } else if (entry->quick == 1 || hfix60_recent_is(entry->path)) {
                badge = "REC";
                br = 42; bg = 86; bb = 126;
            }

            if (selected) {
                hfix58_blend_rect565(fb, 22, y - 4, 140, 16, 42, 100, 185, 230);
                hfix58_rect565(fb, 22, y - 4, 4, 16, g_mivf_theme_r, g_mivf_theme_g, g_mivf_theme_b);
            }

            char line[48];
            snprintf(line, sizeof(line), "%s", entry->name);

            /*
                Clip long names visually by truncating the text buffer.
            */
            line[16] = '\0';

            hfix58_blend_rect565(fb, 29, y - 3, 24, 12,
                selected ? br + 20 : br,
                selected ? bg + 20 : bg,
                selected ? bb + 18 : bb,
                selected ? 242 : 228);
            hfix58_rect565(fb, 30, y - 2, 22, 1,
                selected ? 255 : 220,
                selected ? 255 : 235,
                selected ? 255 : 248);
            hfix58_rect565(fb, 30, y + 7, 22, 1,
                br > 10 ? br - 10 : br,
                bg > 10 ? bg - 10 : bg,
                bb > 12 ? bb - 12 : bb);
            hfix58_draw_text_shadow(fb, 33, y, badge, 1, 235, 245, 255);

            hfix58_draw_text_shadow(
                fb,
                selected ? 60 : 56,
                y,
                line,
                1,
                selected ? 255 : 205,
                selected ? 255 : 220,
                selected ? 255 : 240
            );

            if (selected && g_hfix58_preview.has_resume) {
                hfix58_rect565(fb, 132, y - 2, 28, 10, 40, 92, 48);
                hfix58_draw_text_shadow(fb, 136, y, "GO", 1, 220, 255, 220);
            }

            y += 14;
        }

        /*
            Scroll bar.
        */
        if (g_hfix58_browser.count > HFIX58_BROWSER_VISIBLE_ROWS) {
            int track_y = 84;
            int track_h = HFIX58_BROWSER_VISIBLE_ROWS * 14;
            int knob_h = (track_h * HFIX58_BROWSER_VISIBLE_ROWS) / g_hfix58_browser.count;

            if (knob_h < 12) knob_h = 12;

            int max_scroll = g_hfix58_browser.count - HFIX58_BROWSER_VISIBLE_ROWS;
            int knob_y = track_y;

            if (max_scroll > 0) {
                knob_y += ((track_h - knob_h) * g_hfix58_browser.scroll) / max_scroll;
            }

            hfix58_rect565(fb, 148, track_y, 4, track_h, 30, 35, 48);
            hfix58_rect565(fb, 148, knob_y, 4, knob_h, 80, 170, 255);
        }
    }

    hfix58_blend_rect565(fb, 18, 206, 284, 18, 6, 10, 22, 210);
    hfix58_rect565(fb, 22, 205, 276, 1, 40, 62, 94);
    {
        char footer[56];
        snprintf(footer, sizeof(footer), "A OPEN  Y FAV  %s  B BACK  START EXIT",
            g_hfix58_show_all_dirs ? "SEL:HIDE SYS" : "SEL:SHOW SYS");
        hfix58_draw_text_shadow(fb, 24, 212, footer, 1, 222, 236, 252);
    }

    hfix58_draw_alert(fb);
}

static void hfix58_browser_redraw(void) {
    u16 fw = 0;
    u16 fh = 0;
    u8 *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &fw, &fh);

    hfix58_draw_browser(fb);
    gfxFlushBuffers();
    gfxSwapBuffers();
}

static bool hfix58_file_browser_select(char *out_path, size_t out_sz) {
    if (!out_path || out_sz == 0) {
        return false;
    }

    hfix58_alert_clear();

    if (!hfix58_scan_default_dirs()) {
        /*
            Still show a no-files screen so the user gets feedback.
        */
        g_hfix58_browser.count = 0;
        snprintf(g_hfix58_browser.cwd, sizeof(g_hfix58_browser.cwd), "sdmc:/mivf");
    }

    hfix58_browser_redraw();
    hfix58_browser_redraw();

    while (aptMainLoop()) {
        hidScanInput();

        u32 down = hidKeysDown();

        if (down & KEY_START) {
            return false;
        }

        if (down & KEY_B) {
            return false;
        }

        /* HFIX60: SELECT toggles show-all mode — rescans with the
           alternate directory order so the user can see files at
           the SD root (or hide them again). */
        if (down & KEY_SELECT) {
            g_hfix58_show_all_dirs = !g_hfix58_show_all_dirs;
            hfix58_scan_default_dirs();
            hfix58_preview_clear();
            hfix58_browser_redraw();
        }

        if (g_hfix58_browser.count > 0) {
            if (down & KEY_DUP) {
                g_hfix58_browser.selected--;

                if (g_hfix58_browser.selected < 0) {
                    g_hfix58_browser.selected = g_hfix58_browser.count - 1;
                }

                if (g_hfix58_browser.selected < g_hfix58_browser.scroll) {
                    g_hfix58_browser.scroll = g_hfix58_browser.selected;
                }

                if (g_hfix58_browser.selected >= g_hfix58_browser.scroll + HFIX58_BROWSER_VISIBLE_ROWS) {
                    g_hfix58_browser.scroll = g_hfix58_browser.selected - HFIX58_BROWSER_VISIBLE_ROWS + 1;
                }

                /* Defer preview load until selection is stable. */
                g_hfix58_preview_deadline = svcGetSystemTick() +
                    (u64)SYSCLOCK_ARM11 / 5ULL;

                hfix58_browser_redraw();
            }

            if (down & KEY_DDOWN) {
                g_hfix58_browser.selected++;

                if (g_hfix58_browser.selected >= g_hfix58_browser.count) {
                    g_hfix58_browser.selected = 0;
                }

                if (g_hfix58_browser.selected < g_hfix58_browser.scroll) {
                    g_hfix58_browser.scroll = g_hfix58_browser.selected;
                }

                if (g_hfix58_browser.selected >= g_hfix58_browser.scroll + HFIX58_BROWSER_VISIBLE_ROWS) {
                    g_hfix58_browser.scroll = g_hfix58_browser.selected - HFIX58_BROWSER_VISIBLE_ROWS + 1;
                }

                /* Defer preview load until selection is stable. */
                g_hfix58_preview_deadline = svcGetSystemTick() +
                    (u64)SYSCLOCK_ARM11 / 5ULL;

                hfix58_browser_redraw();
            }

            /* HFIX60: toggle favorite on the selected entry. */
            if (down & KEY_Y) {
                if (g_mivf_settings.remember_favorites) {
                    hfix60_fav_toggle(g_hfix58_browser.entries[g_hfix58_browser.selected].path);
                    hfix58_browser_promote_quick_access();
                    /* Defer preview reload after list reorder. */
                    g_hfix58_preview_deadline = svcGetSystemTick() +
                        (u64)SYSCLOCK_ARM11 / 5ULL;
                    hfix58_browser_redraw();
                }
            }

            if (down & KEY_A) {
                const char *selected_path = g_hfix58_browser.entries[g_hfix58_browser.selected].path;

                snprintf(
                    out_path,
                    out_sz,
                    "%s",
                    selected_path
                );

                if (out_path != g_hfix58_selected_media) {
                    snprintf(g_hfix58_selected_media, sizeof(g_hfix58_selected_media), "%s", selected_path);
                }
                g_hfix58_has_selected_media = true;
                hfix60_recent_note(selected_path);
                return true;
            }
        }

        /* Load preview once selection has been stable long enough. */
        if (g_hfix58_preview_deadline != 0 &&
            svcGetSystemTick() >= g_hfix58_preview_deadline) {
            g_hfix58_preview_deadline = 0;
            hfix58_browser_refresh_preview();
            hfix58_browser_redraw();
        }

        gspWaitForVBlank();
    }

    return false;
}



/* ------------------------------------------------------------------------- */
/* HFIX58B_CUSTOM_GLASS_UI                                                    */
/*                                                                           */
/* Native retained UI skin and vector transport icon engine.                  */
/* No external textures. No NintendoWare runtime. RGB565 framebuffer only.    */
/* ------------------------------------------------------------------------- */

typedef struct {
    int x, y, w, h;
    u16 bg_color;
    u16 border_color;
    bool hovered;
    bool pressed;
} MivfButtonSkin;

typedef struct {
    int panel_x, panel_y, panel_w, panel_h;
    MivfButtonSkin rewind;
    MivfButtonSkin play_pause;
    MivfButtonSkin forward;
    MivfButtonSkin volume_indicator;
    int selected_index; /* 0=RW, 1=Play/Pause, 2=FF */
    bool initialized;
} MivfTransportSkin;

static MivfTransportSkin g_mivf_ui_skin;


/* ------------------------------------------------------------------------- */
/* HFIX58B_R3_ACCESSOR_DEFS                                                   */
/* ------------------------------------------------------------------------- */
static int hfix58b_get_selected_index(void) {
    return g_mivf_ui_skin.selected_index;
}

static bool hfix58b_get_play_pressed(void) {
    return g_mivf_ui_skin.play_pause.pressed;
}

static void hfix58b_ui_init_once(void) {
    if (g_mivf_ui_skin.initialized) {
        return;
    }

    g_mivf_ui_skin.panel_x = 10;
    g_mivf_ui_skin.panel_y = 96;
    g_mivf_ui_skin.panel_w = 300;
    g_mivf_ui_skin.panel_h = 112;

    g_mivf_ui_skin.rewind.x = 28;
    g_mivf_ui_skin.rewind.y = 116;
    g_mivf_ui_skin.rewind.w = 70;
    g_mivf_ui_skin.rewind.h = 54;

    g_mivf_ui_skin.play_pause.x = 125;
    g_mivf_ui_skin.play_pause.y = 110;
    g_mivf_ui_skin.play_pause.w = 70;
    g_mivf_ui_skin.play_pause.h = 66;

    g_mivf_ui_skin.forward.x = 222;
    g_mivf_ui_skin.forward.y = 116;
    g_mivf_ui_skin.forward.w = 70;
    g_mivf_ui_skin.forward.h = 54;

    g_mivf_ui_skin.volume_indicator.x = 214;
    g_mivf_ui_skin.volume_indicator.y = 56;
    g_mivf_ui_skin.volume_indicator.w = 88;
    g_mivf_ui_skin.volume_indicator.h = 34;

    g_mivf_ui_skin.selected_index = 1;
    g_mivf_ui_skin.initialized = true;
}

static void hfix58b_sync_hover_state(void) {
    g_mivf_ui_skin.rewind.hovered = false;
    g_mivf_ui_skin.play_pause.hovered = false;
    g_mivf_ui_skin.forward.hovered = false;

    if (g_mivf_ui_skin.selected_index == 0) {
        g_mivf_ui_skin.rewind.hovered = true;
    } else if (g_mivf_ui_skin.selected_index == 2) {
        g_mivf_ui_skin.forward.hovered = true;
    } else {
        g_mivf_ui_skin.play_pause.hovered = true;
    }

    g_mivf_ui_skin.rewind.pressed = (g_media_ctl.dummy_seek_state == -1);
    g_mivf_ui_skin.forward.pressed = (g_media_ctl.dummy_seek_state == 1);
}

static void hfix58b_transport_handle_input(u32 down, u32 held) {
    hfix58b_ui_init_once();

    if (down & KEY_DLEFT) {
        g_mivf_ui_skin.selected_index--;
        if (g_mivf_ui_skin.selected_index < 0) {
            g_mivf_ui_skin.selected_index = 0;
        }
    }

    if (down & KEY_DRIGHT) {
        g_mivf_ui_skin.selected_index++;
        if (g_mivf_ui_skin.selected_index > 2) {
            g_mivf_ui_skin.selected_index = 2;
        }
    }

    /*
        Existing KEY_A / KEY_LEFT / KEY_RIGHT engine behavior stays centralized
        in the playback loop. This routine only updates retained UI state.
    */
    g_mivf_ui_skin.play_pause.pressed =
        ((held & KEY_A) && g_mivf_ui_skin.selected_index == 1);

    hfix58b_sync_hover_state();
}

static void hfix58b_draw_shadow_strip(u8 *fb, int x, int y, int w) {
    hfix58_blend_rect565(fb, x, y + 0, w, 2, 0, 0, 0, 75);
    hfix58_blend_rect565(fb, x, y + 2, w, 2, 0, 0, 0, 45);
    hfix58_blend_rect565(fb, x, y + 4, w, 2, 0, 0, 0, 20);
}

static void hfix58b_draw_roundedish_panel(u8 *fb, int x, int y, int w, int h) {
    /*
        HFIX58E:
        Pre-baked glass colors. The bottom background is always RGB(3,6,14),
        so alpha blending huge panel rectangles is wasted CPU work.
    */
    hfix58_rect565(fb, x + 8, y,     w - 16, h,     10, 14, 24);
    hfix58_rect565(fb, x,     y + 8, w,      h - 16, 10, 14, 24);

    hfix58_rect565(fb, x + 4, y + 4, w - 8,  h - 8,  16, 22, 36);

    /*
        Top glass highlight and console-blue accent are small solid bands.
    */
    hfix58_rect565(fb, x + 10, y + 6, w - 20, 3, 34, 54, 86);
    hfix58_rect565(fb, x + 12, y + 2, w - 24, 2, 0, 140, 255);

    /*
        Cheap shadow approximation: solid strips instead of alpha blend.
    */
    hfix58_rect565(fb, x + 10, y + h + 2, w - 20, 2, 1, 2, 6);
    hfix58_rect565(fb, x + 10, y + h + 4, w - 20, 2, 2, 3, 8);
}

static void hfix58b_draw_button(u8 *fb, MivfButtonSkin *b) {
    if (!b) {
        return;
    }

    /*
        HFIX58E:
        Use baked solid colors for buttons. This avoids large per-pixel
        RGB565 unpack/blend/repack loops while the UI is visible.
    */
    bool hot = b->hovered || b->pressed;

    int br = b->pressed ? 30 : (hot ? 34 : 18);
    int bg = b->pressed ? 72 : (hot ? 66 : 26);
    int bb = b->pressed ? 122 : (hot ? 116 : 42);

    /*
        Shadow / rounded-ish silhouette.
    */
    hfix58_rect565(fb, b->x + 5, b->y,     b->w - 10, b->h,      4, 6, 12);
    hfix58_rect565(fb, b->x,     b->y + 5, b->w,      b->h - 10, 4, 6, 12);

    /*
        Solid button face.
    */
    hfix58_rect565(fb, b->x + 4, b->y + 4, b->w - 8, b->h - 8, br, bg, bb);

    if (hot) {
        hfix58_rect565(fb, b->x + 7, b->y + 5, b->w - 14, 2, 120, 200, 255);
        hfix58_rect565(fb, b->x + 7, b->y + b->h - 7, b->w - 14, 2, 0, 70, 150);
        hfix58_rect565(fb, b->x + 4, b->y + 8, 2, b->h - 16, 70, 160, 255);
        hfix58_rect565(fb, b->x + b->w - 6, b->y + 8, 2, b->h - 16, 70, 160, 255);
    } else {
        hfix58_rect565(fb, b->x + 8, b->y + 6, b->w - 16, 1, 90, 120, 160);
    }
}

static void hfix58b_draw_vector_play(u8 *fb, int x, int y, int size, u16 color) {
    /*
        HFIX58K: Genuine right-pointing vector triangle.
        Wide flat base on the left, sharp apex pointing right.
    */
    for (int dx = 0; dx < size; dx++) {
        int h = (size - 1 - dx) / 2;
        for (int yy = -h; yy <= h; yy++) {
            hfix58_px565(fb, x + dx, y + yy, color);
        }
    }
}

static void hfix58b_draw_vector_pause(u8 *fb, int x, int y, int size, u16 color) {
    int bar_w = size / 4;
    int gap = size / 5;
    int h = size;

    hfix58_rect565(fb, x, y - h / 2, bar_w, h, 245, 245, 245);
    hfix58_rect565(fb, x + bar_w + gap, y - h / 2, bar_w, h, 245, 245, 245);

    (void)color;
}

static void hfix58b_draw_left_tri(u8 *fb, int tip_x, int cy, int half_h, u16 color) {
    for (int dx = 0; dx < half_h; dx++) {
        int x = tip_x + dx;

        for (int y = cy - dx; y <= cy + dx; y++) {
            hfix58_px565(fb, x, y, color);
        }
    }
}

static void hfix58b_draw_right_tri(u8 *fb, int tip_x, int cy, int half_h, u16 color) {
    for (int dx = 0; dx < half_h; dx++) {
        int x = tip_x - dx;

        for (int y = cy - dx; y <= cy + dx; y++) {
            hfix58_px565(fb, x, y, color);
        }
    }
}

static void hfix58b_draw_vector_rewind(u8 *fb, int x, int y, int size, u16 color) {
    hfix58b_draw_left_tri(fb, x + size / 3, y, size / 2, color);
    hfix58b_draw_left_tri(fb, x + size,     y, size / 2, color);
}

static void hfix58b_draw_vector_forward(u8 *fb, int x, int y, int size, u16 color) {
    hfix58b_draw_right_tri(fb, x + size,     y, size / 2, color);
    hfix58b_draw_right_tri(fb, x + size / 3, y, size / 2, color);
}

static void hfix58b_draw_timeline(u8 *fb) {
    int x = 30;
    int y = 194;
    int w = 260;
    int h = 5;

    /*
        Without a stable public timestamp field in this branch, draw a clean
        inactive track. Future seek/index work can wire real progress here.
    */
    hfix58_blend_rect565(fb, x, y, w, h, 22, 30, 45, 230);
    hfix58_rect565(fb, x, y, w / 3, h, 0, 140, 255);
    hfix58_rect565(fb, x + w / 3 - 2, y - 3, 5, 11, 230, 245, 255);
}

static void hfix58b_draw_bottom_glass_ui(u8 *fb) {
    if (!fb) {
        return;
    }

    hfix58b_ui_init_once();
    hfix58b_sync_hover_state();

    /*
        Full bottom background: professional dark, not console debug.
    */
    hfix58_rect565(fb, 0, 0, 320, 240, 3, 6, 14);

    /*
        Header/status panel.
    */
    hfix58_rect565(fb, 10, 10, 300, 42, 11, 15, 26);
    hfix58_rect565(fb, 18, 12, 284, 2, 0, 140, 255);

    if (g_media_ctl.state == STATE_PLAYING) {
        hfix58_draw_text_shadow(fb, 22, 22, "MIVF PLAYER   PLAYING", 1, 220, 245, 255);
    } else {
        hfix58_draw_text_shadow(fb, 22, 22, "MIVF PLAYER   PAUSED", 1, 255, 210, 120);
    }

    /*
        Optional alert/toast from HFIX58A.
    */
    hfix58_draw_alert(fb);

    /*
        Main transport panel.
    */
    hfix58b_draw_roundedish_panel(
        fb,
        g_mivf_ui_skin.panel_x,
        g_mivf_ui_skin.panel_y,
        g_mivf_ui_skin.panel_w,
        g_mivf_ui_skin.panel_h
    );

    /*
        Side audio/status area.
    */
    hfix58_blend_rect565(fb, 214, 100, 88, 56, 8, 12, 22, 180);
    hfix58_rect565(fb, 218, 102, 80, 2, 70, 120, 210);


    /*
        Volume meter, integrated into the right status module.
    */
    int vol = g_hfix56_volume_percent;
    if (vol < 0) vol = 0;
    if (vol > 300) vol = 300;

    int meter_x = 222;
    int meter_y = 118;
    int meter_w = 72;
    int meter_h = 8;
    int fill = (vol * meter_w) / 300;

    hfix58_rect565(fb, meter_x, meter_y, meter_w, meter_h, 19, 27, 40);
    hfix58_rect565(fb, meter_x, meter_y, fill, meter_h, 70, 210, 130);
    hfix58_rect565(fb, meter_x + (100 * meter_w) / 300, meter_y - 2, 1, meter_h + 4, 235, 210, 90);
    hfix58_rect565(fb, meter_x + (200 * meter_w) / 300, meter_y - 2, 1, meter_h + 4, 235, 150, 70);

    char vol_txt[32];
    snprintf(vol_txt, sizeof(vol_txt), "VOL %d%%", vol);
    hfix58_draw_text_shadow(fb, 222, 102, vol_txt, 1, 230, 240, 250);

    if (g_hfix56_limiter_enabled) {
        hfix58_draw_text_shadow(fb, 222, 132, "LIM ON", 1, 120, 230, 150);
    } else {
        hfix58_draw_text_shadow(fb, 222, 132, "LIM OFF", 1, 160, 165, 175);
    }

    if (g_hfix56_force_stereo) {
        hfix58_draw_text_shadow(fb, 222, 144, "STEREO", 1, 130, 190, 255);
    } else {
        hfix58_draw_text_shadow(fb, 222, 144, "MONO", 1, 160, 165, 175);
    }


    /*
        Buttons.
    */
    hfix58b_draw_button(fb, &g_mivf_ui_skin.rewind);
    hfix58b_draw_button(fb, &g_mivf_ui_skin.play_pause);
    hfix58b_draw_button(fb, &g_mivf_ui_skin.forward);

    u16 white = hfix58_rgb565(245, 245, 245);

    hfix58b_draw_vector_rewind(
        fb,
        g_mivf_ui_skin.rewind.x + 16,
        g_mivf_ui_skin.rewind.y + g_mivf_ui_skin.rewind.h / 2,
        32,
        white
    );

    if (g_media_ctl.state == STATE_PLAYING) {
        hfix58b_draw_vector_pause(
            fb,
            g_mivf_ui_skin.play_pause.x + 24,
            g_mivf_ui_skin.play_pause.y + g_mivf_ui_skin.play_pause.h / 2,
            30,
            white
        );
    } else {
        hfix58b_draw_vector_play(
            fb,
            g_mivf_ui_skin.play_pause.x + 24,
            g_mivf_ui_skin.play_pause.y + g_mivf_ui_skin.play_pause.h / 2,
            34,
            white
        );
    }

    hfix58b_draw_vector_forward(
        fb,
        g_mivf_ui_skin.forward.x + 16,
        g_mivf_ui_skin.forward.y + g_mivf_ui_skin.forward.h / 2,
        32,
        white
    );

    hfix58b_draw_timeline(fb);

    /*
        Footer hints.
    */
    hfix58_draw_text_shadow(fb, 22, 216, "D-PAD SELECT   A PRESS   L+D-PAD AUDIO", 1, 170, 190, 215);
}



/* ------------------------------------------------------------------------- */
/* HFIX58D_FLUENT_UI_ENGINE                                                   */
/* ------------------------------------------------------------------------- */
typedef enum {
    UI_STATE_HIDDEN,
    UI_STATE_SLIDING_UP,
    UI_STATE_VISIBLE,
    UI_STATE_SLIDING_DOWN
} MivfUiVisibilityState;

typedef struct {
    int panel_target_y;
    int panel_current_y;
    MivfUiVisibilityState visibility_state;

    u32 idle_frame_counter;
    u32 last_input_mask;

    int hover_box_target_x;
    int hover_box_current_x;
    int hover_box_w;
    int hover_box_h;

    int marquee_scroll_offset;
    u32 marquee_delay_ticks;
    bool marquee_reverse_dir;
    u32 marquee_frame_counter;
    u32 force_clear_frames;
    bool is_touch_scrubbing;
    u32 scrub_target_frame;
    u32 wake_settle_frames;
    bool initialized;
} MivfAnimationEngine;

static MivfAnimationEngine g_mivf_anim;

static int hfix58d_iabs(int v) {
    return v < 0 ? -v : v;
}

static const char *hfix58d_basename(const char *path) {
    const char *last = path;
    if (!path) {
        return "";
    }

    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            last = p + 1;
        }
    }

    return last;
}

static void hfix58d_anim_init_once(void) {
    if (g_mivf_anim.initialized) {
        return;
    }

    g_mivf_anim.panel_target_y = 96;
    g_mivf_anim.panel_current_y = 96;
    g_mivf_anim.visibility_state = UI_STATE_VISIBLE;
    g_mivf_anim.idle_frame_counter = 0;
    g_mivf_anim.last_input_mask = 0;
    g_mivf_anim.hover_box_target_x = 125;
    g_mivf_anim.hover_box_current_x = 125;
    g_mivf_anim.hover_box_w = 78;
    g_mivf_anim.hover_box_h = 72;
    g_mivf_anim.marquee_scroll_offset = 0;
    g_mivf_anim.marquee_delay_ticks = 30;
    g_mivf_anim.marquee_reverse_dir = false;
    g_mivf_anim.marquee_frame_counter = 0;
    g_mivf_anim.force_clear_frames = 2;
    g_mivf_anim.wake_settle_frames = 0;
    g_mivf_anim.initialized = true;
}

static void hfix58d_set_hover_target_from_selection(void) {
    int sel = hfix58b_get_selected_index();

    if (sel == 0) {
        g_mivf_anim.hover_box_target_x = 28 - 4;
        g_mivf_anim.hover_box_w = 78;
    } else if (sel == 2) {
        g_mivf_anim.hover_box_target_x = 222 - 4;
        g_mivf_anim.hover_box_w = 78;
    } else {
        g_mivf_anim.hover_box_target_x = 125 - 4;
        g_mivf_anim.hover_box_w = 78;
    }

    g_mivf_anim.hover_box_h = 74;
}

static void hfix58d_notify_input(u32 down, u32 held) {
    hfix58d_anim_init_once();

    u32 relevant = down & (
        KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT |
        KEY_A | KEY_B | KEY_X | KEY_Y | KEY_L | KEY_R |
        KEY_TOUCH
    );

    if (relevant || (held & KEY_TOUCH)) {
        g_mivf_anim.idle_frame_counter = 0;
        g_mivf_anim.last_input_mask = relevant;

        if (g_mivf_anim.visibility_state == UI_STATE_HIDDEN ||
            g_mivf_anim.visibility_state == UI_STATE_SLIDING_DOWN) {
            g_mivf_anim.visibility_state = UI_STATE_SLIDING_UP;
            g_mivf_anim.panel_target_y = 96;
        }

        g_mivf_anim.force_clear_frames = 2;
        /* HFIX58J_WAKE_INPUT */
        g_mivf_anim.wake_settle_frames = 30;
        /* HFIX58H_WAKE_SETTLE_INPUT */
        g_mivf_anim.wake_settle_frames = 30;
    }

    hfix58d_set_hover_target_from_selection();
}

static void hfix58d_anim_tick(void) {
    hfix58d_anim_init_once();

    /*
        HFIX58J_WAKE_SETTLE:
        Prevent immediate hide re-trigger after waking.
    */
    if (g_mivf_anim.wake_settle_frames > 0) {
        g_mivf_anim.wake_settle_frames--;
        g_mivf_anim.idle_frame_counter = 0;
    }

    if (g_mivf_anim.visibility_state == UI_STATE_VISIBLE) {
        if (g_mivf_anim.idle_frame_counter < 1000000) {
            g_mivf_anim.idle_frame_counter++;
        }

        if (g_mivf_anim.idle_frame_counter >= 240) {
            g_mivf_anim.visibility_state = UI_STATE_SLIDING_DOWN;
            g_mivf_anim.panel_target_y = 240;
            g_mivf_anim.force_clear_frames = 2;
        }
    }

    /*
        HFIX58J_KINEMATIC_EASING:
        integer-only deceleration. No alpha fade, no float.
    */
    if (g_mivf_anim.visibility_state == UI_STATE_SLIDING_UP ||
        g_mivf_anim.visibility_state == UI_STATE_SLIDING_DOWN) {
        int dy = g_mivf_anim.panel_target_y - g_mivf_anim.panel_current_y;

        if (dy >= -2 && dy <= 2) {
            g_mivf_anim.panel_current_y = g_mivf_anim.panel_target_y;

            if (g_mivf_anim.visibility_state == UI_STATE_SLIDING_UP) {
                g_mivf_anim.visibility_state = UI_STATE_VISIBLE;
                g_mivf_anim.idle_frame_counter = 0;
                g_mivf_anim.wake_settle_frames = 30;
            } else {
                g_mivf_anim.visibility_state = UI_STATE_HIDDEN;
                g_mivf_anim.idle_frame_counter = 0;
            }

            g_mivf_anim.force_clear_frames = 2;
        } else {
            int step = dy / 4;

            if (step == 0) {
                step = dy > 0 ? 1 : -1;
            }

            g_mivf_anim.panel_current_y += step;
        }
    }

    hfix58d_set_hover_target_from_selection();

    int dx = g_mivf_anim.hover_box_target_x - g_mivf_anim.hover_box_current_x;
    if (hfix58d_iabs(dx) < 2) {
        g_mivf_anim.hover_box_current_x = g_mivf_anim.hover_box_target_x;
    } else {
        g_mivf_anim.hover_box_current_x += dx / 2;
    }

    if (g_mivf_anim.force_clear_frames > 0) {
        g_mivf_anim.force_clear_frames--;
    }

    if (g_mivf_anim.visibility_state == UI_STATE_VISIBLE ||
        g_mivf_anim.visibility_state == UI_STATE_SLIDING_UP) {
        g_mivf_anim.marquee_frame_counter++;
    }

    hfix58f_tick_seek_ui_tail();

}

static bool hfix58d_anim_needs_redraw(void) {
    /* HFIX58F_SEEK_REDRAW_PRIORITY */
    if (hfix58f_seek_active()) {
        return true;
    }

    if (g_hfix58_alert_frames > 0) {
        return true;
    }

    hfix58d_anim_init_once();

    if (g_mivf_anim.visibility_state == UI_STATE_SLIDING_UP ||
        g_mivf_anim.visibility_state == UI_STATE_SLIDING_DOWN) {
        return true;
    }

    if (g_mivf_anim.force_clear_frames > 0) {
        return true;
    }

    if (g_mivf_anim.hover_box_current_x != g_mivf_anim.hover_box_target_x) {
        return true;
    }

    /*
        HFIX58E:
        Marquee redraw is throttled aggressively. It only wakes the UI if the
        filename is long enough to scroll, and only every 8 frames.
    */
    if (g_mivf_anim.visibility_state == UI_STATE_VISIBLE) {
        const char *base = hfix58d_basename(MIVF_PATH);
        int title_len = 17 + 10 + (int)strlen(base); /* "MIVF PLAYER   " + state + filename */
        int text_w = title_len * 6;

        if (text_w > 276 && (g_mivf_anim.marquee_frame_counter % 8) == 0) {
            return true;
        }
    }

    return false;
}

static void hfix58d_draw_char_clipped(
    u8 *fb,
    int x,
    int y,
    char c,
    int scale,
    int r,
    int g,
    int b,
    int clip_x,
    int clip_w
) {
    const u8 *glyph = hfix58_glyph(c);
    u16 color = hfix58_rgb565(r, g, b);
    int clip_r = clip_x + clip_w;

    if (scale < 1) {
        scale = 1;
    }

    for (int row = 0; row < 7; row++) {
        u8 bits = glyph[row];

        for (int col = 0; col < 5; col++) {
            if (bits & (1 << (4 - col))) {
                for (int yy = 0; yy < scale; yy++) {
                    for (int xx = 0; xx < scale; xx++) {
                        int px = x + col * scale + xx;
                        if (px >= clip_x && px < clip_r) {
                            hfix58_px565(fb, px, y + row * scale + yy, color);
                        }
                    }
                }
            }
        }
    }
}

static void hfix58d_draw_text_shadow_clipped(
    u8 *fb,
    int x,
    int y,
    const char *text,
    int scale,
    int r,
    int g,
    int b,
    int clip_x,
    int clip_w
) {
    if (!text) {
        return;
    }

    int cx = x;
    for (const char *p = text; *p; p++) {
        hfix58d_draw_char_clipped(fb, cx + scale, y + scale, *p, scale, 0, 0, 0, clip_x, clip_w);
        hfix58d_draw_char_clipped(fb, cx, y, *p, scale, r, g, b, clip_x, clip_w);
        cx += 6 * scale;
    }
}

static void hfix58d_draw_header_marquee(u8 *fb) {
    char title[192];
    const char *base = hfix58d_basename(MIVF_PATH);

    snprintf(title, sizeof(title), "MIVF PLAYER   %s   %s",
        g_media_ctl.state == STATE_PLAYING ? "PLAYING" : "PAUSED",
        base);

    int clip_x = 22;
    int clip_w = 276;
    int text_w = (int)strlen(title) * 6;
    int offset = 0;

    if (text_w > clip_w) {
        int max_off = text_w - clip_w + 18;

        if (g_mivf_anim.marquee_delay_ticks > 0) {
            g_mivf_anim.marquee_delay_ticks--;
        } else {
            if (!g_mivf_anim.marquee_reverse_dir) {
                g_mivf_anim.marquee_scroll_offset++;
                if (g_mivf_anim.marquee_scroll_offset >= max_off) {
                    g_mivf_anim.marquee_scroll_offset = max_off;
                    g_mivf_anim.marquee_reverse_dir = true;
                    g_mivf_anim.marquee_delay_ticks = 30;
                }
            } else {
                g_mivf_anim.marquee_scroll_offset--;
                if (g_mivf_anim.marquee_scroll_offset <= 0) {
                    g_mivf_anim.marquee_scroll_offset = 0;
                    g_mivf_anim.marquee_reverse_dir = false;
                    g_mivf_anim.marquee_delay_ticks = 30;
                }
            }
        }

        offset = g_mivf_anim.marquee_scroll_offset;
    } else {
        g_mivf_anim.marquee_scroll_offset = 0;
        g_mivf_anim.marquee_reverse_dir = false;
        g_mivf_anim.marquee_delay_ticks = 30;
    }

    hfix58d_draw_text_shadow_clipped(
        fb,
        clip_x - offset,
        22,
        title,
        1,
        g_media_ctl.state == STATE_PLAYING ? 220 : 255,
        g_media_ctl.state == STATE_PLAYING ? 245 : 210,
        g_media_ctl.state == STATE_PLAYING ? 255 : 120,
        clip_x,
        clip_w
    );
}

static void hfix58d_draw_fluent_panel(u8 *fb, int panel_y) {
    int panel_x = 10;
    int panel_w = 300;
    int panel_h = 112;

    if (panel_y >= 240) {
        return;
    }

    hfix58b_draw_roundedish_panel(fb, panel_x, panel_y, panel_w, panel_h);

    int off = panel_y - 96;

    MivfButtonSkin rew = g_mivf_ui_skin.rewind;
    MivfButtonSkin play = g_mivf_ui_skin.play_pause;
    MivfButtonSkin fwd = g_mivf_ui_skin.forward;

    rew.y += off;
    play.y += off;
    fwd.y += off;

    int hover_y = play.y - 4;
    int hover_x = g_mivf_anim.hover_box_current_x;

    /*
        Traveling hover halo.
    */
    hfix58_rect565(fb, hover_x, hover_y, g_mivf_anim.hover_box_w, g_mivf_anim.hover_box_h,
        3, 38, 82);
    hfix58_rect565(fb, hover_x + 6, hover_y + 3, g_mivf_anim.hover_box_w - 12, 2,
        120, 210, 255);

    hfix58b_draw_button(fb, &rew);
    hfix58b_draw_button(fb, &play);
    hfix58b_draw_button(fb, &fwd);

    u16 white = hfix58_rgb565(245, 245, 245);

    hfix58b_draw_vector_rewind(fb, rew.x + 16, rew.y + rew.h / 2, 32, white);

    if (g_media_ctl.state == STATE_PLAYING) {
        hfix58b_draw_vector_pause(fb, play.x + 25, play.y + play.h / 2, 30, white);
    } else {
        hfix58b_draw_vector_play(fb, play.x + 18, play.y + play.h / 2, 34, white);
    }

    hfix58b_draw_vector_forward(fb, fwd.x + 16, fwd.y + fwd.h / 2, 32, white);

    /*
        Timeline follows the panel.
    */
    int tx = 30;
    int ty = panel_y + 98;
    int tw = 260;
    int th = 5;

    /* HFIX58K: Removed legacy fake timeline placeholder block */

    hfix58f_draw_timeline(fb, panel_y);

    int footer_y = panel_y + 120;
    if (footer_y < 232) {
        hfix58_rect565(fb, 22, footer_y - 4, 276, 1, 40, 62, 94);
        hfix58_draw_text_shadow(fb, 22, footer_y, "D-PAD SELECT   A PRESS   L+D-PAD AUDIO", 1, 202, 222, 244);
    }
}

static void hfix58d_draw_bottom_fluent_ui(u8 *fb) {
    if (!fb) {
        return;
    }

    hfix58d_anim_init_once();
    hfix58b_ui_init_once();
    hfix58b_sync_hover_state();

    /*
        Mandatory hard clear for double-buffered animation.
    */
    hfix58_rect565(fb, 0, 0, 320, 240, 3, 6, 14);

    /*
        Header/status panel.
    */
    hfix58_rect565(fb, 10, 10, 300, 42, 11, 15, 26);
    hfix58_rect565(fb, 18, 12, 284, 2, 0, 140, 255);
    hfix58d_draw_header_marquee(fb);
    hfix58j_draw_system_overlay(fb, 0);

    /*
        Optional HFIX58A alert/toast.
    */
    hfix58_draw_alert(fb);

    if (g_hfix59r3_settings_visible) {
        hfix59r3_draw_settings_overlay(fb);
    }

    /*
        Volume/status module stays above transport panel.
    */

    if (!g_hfix59r3_settings_visible) {
        int vol = g_hfix56_volume_percent;
        if (vol < 0) vol = 0;
        if (vol > 300) vol = 300;

        u32 cur_frame = hfix58f_current_frame();
        u32 total_frame = hfix58f_total_frames();
        u32 cur_sec = hfix59r2_frame_to_sec(cur_frame);
        u32 total_sec = g_hfix59r2_duration_ticks
            ? (u32)(g_hfix59r2_duration_ticks / 30000ull)
            : hfix59r2_frame_to_sec(total_frame);
        char cur_t[16];
        char total_t[16];
        char time_t[40];
        char speed_t[16];
        u32 pct = mivf_speed_pct();

        hfix59r2_format_time(cur_t, sizeof(cur_t), cur_sec);
        hfix59r2_format_time(total_t, sizeof(total_t), total_sec);
        snprintf(time_t, sizeof(time_t), "%s / %s", cur_t, total_t);
        snprintf(speed_t, sizeof(speed_t), "%lu.%02lux",
            (unsigned long)(pct / 100u),
            (unsigned long)(pct % 100u));

        hfix58_rect565(fb, 18, 56, 188, 34, 7, 11, 20);
        hfix58_rect565(fb, 22, 58, 180, 2, g_mivf_theme_r, g_mivf_theme_g, g_mivf_theme_b);
        hfix58_draw_text_shadow(fb, 26, 63, time_t, 1, 232, 242, 252);

        hfix58_rect565(fb, 26, 76, 42, 10, 20, 30, 48);
        hfix58_draw_text_shadow(fb, 31, 78, speed_t, 1, 190, 220, 255);

        hfix58_rect565(fb, 74, 76, 42, 10, 20, 30, 48);
        hfix58_draw_text_shadow(fb, 79, 78, hfix60_aspect_name(g_mivf_settings.aspect_mode), 1, 206, 228, 255);

        hfix58_rect565(fb, 122, 76, 38, 10, 20, 30, 48);
        hfix58_draw_text_shadow(fb, 127, 78,
            (g_mivf_settings.show_subtitle_tracks && g_hfix58s_subtitles_ready) ? "SUB" : "NONE",
            1,
            (g_mivf_settings.show_subtitle_tracks && g_hfix58s_subtitles_ready) ? 190 : 150,
            (g_mivf_settings.show_subtitle_tracks && g_hfix58s_subtitles_ready) ? 235 : 165,
            (g_mivf_settings.show_subtitle_tracks && g_hfix58s_subtitles_ready) ? 210 : 175);

        hfix58_rect565(fb, 166, 76, 36, 10, 20, 30, 48);
        hfix58_draw_text_shadow(fb, 171, 78, g_mivf_settings.resume_enabled ? "RES" : "OFF", 1, 206, 228, 255);

        int meter_x = 222;
        int meter_y = 74;
        int meter_w = 72;
        int meter_h = 8;
        int fill = (vol * meter_w) / 300;

        hfix58_rect565(fb, 214, 56, 88, 34, 7, 11, 20);
        hfix58_rect565(fb, 218, 58, 80, 2, 70, 120, 210);

        char vol_txt[32];
        snprintf(vol_txt, sizeof(vol_txt), "VOL %d%%", vol);
        hfix58_draw_text_shadow(fb, 222, 62, vol_txt, 1, 238, 246, 255);

        hfix58_rect565(fb, meter_x, meter_y, meter_w, meter_h, 19, 27, 40);
        hfix58_rect565(fb, meter_x, meter_y, fill, meter_h, 70, 210, 130);
        hfix58_rect565(fb, meter_x + (100 * meter_w) / 300, meter_y - 2, 1, meter_h + 4, 235, 210, 90);
        hfix58_rect565(fb, meter_x + (200 * meter_w) / 300, meter_y - 2, 1, meter_h + 4, 235, 150, 70);

        hfix58_draw_text_shadow(fb, 222, 84, "LIM", 1,
            g_hfix56_limiter_enabled ? 120 : 160,
            g_hfix56_limiter_enabled ? 230 : 165,
            g_hfix56_limiter_enabled ? 150 : 175);

        hfix58_draw_text_shadow(fb, 254, 84, g_hfix56_force_stereo ? "ST" : "MO", 1,
            g_hfix56_force_stereo ? 130 : 160,
            g_hfix56_force_stereo ? 190 : 165,
            g_hfix56_force_stereo ? 255 : 175);
    }


    /*
        Sliding transport panel.
        HFIX60: hidden while the settings overlay is open so the
        play/back/forward controls don't sit behind/around the menu.
    */
    if (!g_hfix59r3_settings_visible) {
        hfix58d_draw_fluent_panel(fb, g_mivf_anim.panel_current_y);
    }

    /* Subtitles are rendered on the top movie screen. */
}


static bool audio_parse_stream(const Stream *s) {
    if (!s) {
        return false;
    }

    /*
        HFIX27A:
        Accept both legacy PC16 and compressed IA4M audio.
        IA4M packets are decoded to PCM16 immediately before audio_queue().
    */
    if (memcmp(s->codec, "PC16", 4) != 0 &&
        memcmp(s->codec, "IA4M", 4) != 0) {
        return false;
    }

    audio.sid = s->id;
    audio.rate = s->w ? s->w : 16000;
    audio.channels = s->h ? (u8)s->h : 1;

    if (audio.channels != 1 && audio.channels != 2) {
        audio.channels = 1;
    }

    /*
        HFIX56A:
        Preserve source channel count, then optionally force NDSP output to
        stereo. Mono sources are upmixed immediately before audio_queue_raw_ndsp.
    */
    g_hfix56_audio_src_channels = audio.channels;

    if (g_hfix56_force_stereo) {
        audio.channels = 2;
    }

    audio.samples_per_frame = s->fpsn ? s->fpsn : (audio.rate / 30);

    if (audio.samples_per_frame == 0) {
        audio.samples_per_frame = audio.rate / 30;
    }

    if (audio.samples_per_frame == 0) {
        audio.samples_per_frame = 1;
    }

    /*
        This is decoded PCM16 packet size, even for IA4M.
        audio_queue() receives PCM16 bytes.
    */
    audio.bytes_per_packet = audio.samples_per_frame * audio.channels * 2;

    if (audio.bytes_per_packet > AUDIO_MAX_PACKET) {
        audio.bytes_per_packet = AUDIO_MAX_PACKET;
    }

    return true;
}

static void audio_shutdown(void);

static bool audio_init_from_stream(const Stream *s) {
    if (!s) {
        return false;
    }

    audio_shutdown();

    if (!audio_parse_stream(s)) {
        return false;
    }

    printf("audio stream present\n");
    printf("audio configured: rate/channels/samples_per_frame=%lu/%u/%lu\n",
        (unsigned long)audio.rate,
        (unsigned int)audio.channels,
        (unsigned long)audio.samples_per_frame);

#if MIVF_DISABLE_AUDIO
    printf("audio disabled: NDSP unavailable, video-only mode\n");
    audio.ready = false;
    audio.ndsp_ready = false;
    return false;
#else
    if (!g_ndsp_ready) {
        printf("audio disabled: NDSP unavailable, video-only mode\n");
        audio.ready = false;
        audio.ndsp_ready = false;
        return true;
    }

    printf("audio_init: codec=%c%c%c%c rate=%lu ch=%u samples/frame=%lu\n",
        s->codec[0], s->codec[1], s->codec[2], s->codec[3],
        (unsigned long)audio.rate,
        (unsigned int)audio.channels,
        (unsigned long)audio.samples_per_frame);

    for (int i = 0; i < AUDIO_BUFS; i++) {
        audio.buf[i] = (u8*)linearAlloc(AUDIO_MAX_PACKET);

        if (!audio.buf[i]) {
            printf("audio_init: linearAlloc fail\n");
            audio_shutdown();
            return false;
        }

        memset(&audio.wb[i], 0, sizeof(ndspWaveBuf));
        audio.wb[i].data_pcm16 = (s16*)audio.buf[i];
        audio.wb[i].nsamples = audio.samples_per_frame;
        audio.wb[i].looping = false;
    }

    /*
        First-playback setup must not depend on audio.ready: this function is
        the code that makes audio ready. The seek path always reconfigured
        NDSP, which is why seeking back to frame 0 could "fix" bad startup
        audio.
    */
    if (!audio_configure_ndsp_channel()) {
        audio_shutdown();
        return false;
    }

    audio.ready = true;
    audio.next = 0;
    audio.ndsp_ready = true;

    printf("audio_init: success\n");

    return true;
#endif
}

static void audio_shutdown(void) {
    printf("audio_shutdown: start ndsp_ready=%s\n", g_ndsp_ready ? "yes" : "no");

    if (audio_can_use_ndsp()) {
        ndspChnReset(0);
    }

    for (int i = 0; i < AUDIO_BUFS; i++) {
        if (audio.buf[i]) {
            linearFree(audio.buf[i]);
            audio.buf[i] = NULL;
        }
    }

    memset(&audio, 0, sizeof(audio));
    audio.ndsp_ready = false;
    printf("audio_shutdown: complete\n");
}


/* ------------------------------------------------------------------------- */
/* HFIX24A IA4M IMA ADPCM software decoder                                   */
/* ------------------------------------------------------------------------- */
static const int ia4m_index_table[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };
static const int ia4m_step_table[89] = {
        7,     8,     9,    10,    11,    12,    13,    14, 16,    17,    19,    21,    23,    25,    28,    31,
       34,    37,    41,    45,    50,    55,    60,    66, 73,    80,    88,    97,   107,   118,   130,   143,
      157,   173,   190,   209,   230,   253,   279,   307, 337,   371,   408,   449,   494,   544,   598,   658,
      724,   796,   876,   963,  1060,  1166,  1282,  1411, 1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,
     3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484, 7132,  7845,  8630,  9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};
static inline int ia4m_clamp_index(int v) { return (v < 0) ? 0 : (v > 88) ? 88 : v; }
static inline s16 ia4m_clamp_s16(int v) { return (v < -32768) ? -32768 : (v > 32767) ? 32767 : (s16)v; }

static s16 ia4m_decode_nibble(int nibble, int *predictor, int *index) {
    int step = ia4m_step_table[*index];
    int diff = step >> 3;
    if (nibble & 1) diff += step >> 2;
    if (nibble & 2) diff += step >> 1;
    if (nibble & 4) diff += step;
    if (nibble & 8) *predictor -= diff; else *predictor += diff;
    *predictor = ia4m_clamp_s16(*predictor);
    *index = ia4m_clamp_index(*index + ia4m_index_table[nibble & 15]);
    return (s16)(*predictor);
}

static int decode_ia4m_packet(const u8 *p, size_t n, s16 *out, int out_cap) {
    if (n < 20 || memcmp(p, "IA4M", 4)) return -1;
    u16 nsamples = le16(p + 8);
    if (p[10] != 1) return -2;
    if (nsamples == 0 || nsamples > (u16)out_cap) return -3;
    int predictor = (int)(int16_t)le16(p + 12);
    int index = ia4m_clamp_index((int)p[14]);
    u32 adpcm_bytes = le32(p + 16);
    if (20u + adpcm_bytes > n) return -4;
    const u8 *q = p + 20;
    int sample = 0;
    out[sample++] = (s16)predictor;
    for (u32 i = 0; i < adpcm_bytes && sample < nsamples; i++) {
        u8 b = q[i];
        out[sample++] = ia4m_decode_nibble(b & 15, &predictor, &index);
        if (sample < nsamples) {
            out[sample++] = ia4m_decode_nibble((b >> 4) & 15, &predictor, &index);
        }
    }
    return sample;
}

static void audio_queue(const u8 *data, u32 size);

static bool hfix58_queue_audio_packet(const Stream *a, const u8 *body, u32 psize, u32 frame_no) {
    (void)frame_no;

    if (!a || !audio.ready || !body || psize == 0) {
        return false;
    }

    if (memcmp(a->codec, "IA4M", 4) == 0) {
        static s16 ia4m_pcm[4096];
        int ns = decode_ia4m_packet(body, psize, ia4m_pcm, 4096);

        if (ns <= 0) {
            return false;
        }

        audio_queue((const u8*)ia4m_pcm, (u32)(ns * 2));
        return true;
    }

    audio_queue(body, psize);
    return true;
}

static void audio_queue_raw_ndsp(const u8 *data, u32 size) {
    if (!audio_can_use_ndsp() || !data || size == 0) {
        return;
    }

    enum { AUDIO_FREE_WAIT_VBLANKS = 4 };
    int start = audio.next;

    for (int wait = 0; wait <= AUDIO_FREE_WAIT_VBLANKS; wait++) {
        for (int tries = 0; tries < AUDIO_BUFS; tries++) {
            int i = (start + tries) % AUDIO_BUFS;

            if (!audio.buf[i]) {
                continue;
            }

            if (audio.wb[i].status != NDSP_WBUF_FREE &&
                audio.wb[i].status != NDSP_WBUF_DONE) {
                continue;
            }

            /*
                HFIX9:
                Use the actual packet size to determine the NDSP sample count.

                This matters for rates like 16000 Hz at 30 FPS:
                    16000 / 30 = 533.333...

                The muxer may emit a pattern of 533/534-sample packets.
                Older code forced every packet to 533 samples and truncated
                534-sample packets, causing tiny audio/video wobble.
            */
            u32 bytes_per_sample_frame = audio.channels * 2;

            if (bytes_per_sample_frame == 0) {
                bytes_per_sample_frame = 2;
            }

            u32 max_bytes = AUDIO_MAX_PACKET;
            u32 n = size;

            if (n > max_bytes) {
                n = max_bytes;
            }

            /*
                Keep sample alignment.
            */
            n -= (n % bytes_per_sample_frame);

            u32 nsamples = n / bytes_per_sample_frame;

            if (nsamples == 0) {
                g_audio_drop++;
                return;
            }

            memset(&audio.wb[i], 0, sizeof(audio.wb[i]));
            memcpy(audio.buf[i], data, n);

            audio.wb[i].data_pcm16 = (s16*)audio.buf[i];
            audio.wb[i].nsamples = nsamples;
            audio.wb[i].looping = false;

            DSP_FlushDataCache(audio.buf[i], n);
            ndspChnWaveBufAdd(0, &audio.wb[i]);

            audio.next = (i + 1) % AUDIO_BUFS;

            g_audio_submit++;
            g_last_audio_bytes = n;
            g_last_audio_samples = nsamples;

            return;
        }

        gspWaitForVBlank();
    }

    /*
        No free audio buffer after a bounded wait.
    */
    g_audio_drop++;
}

/*
    HFIX56A wrapper:
    Input is decoded PCM16, using g_hfix56_audio_src_channels as source layout.
    Output is audio.channels, which may be forced to stereo.
*/
static void audio_queue(const u8 *data, u32 size) {
    if (!data || size == 0) {
        return;
    }

    int src_ch = g_hfix56_audio_src_channels ? g_hfix56_audio_src_channels : 1;
    int out_ch = audio.channels ? audio.channels : src_ch;

    if (src_ch != 1 && src_ch != 2) {
        src_ch = 1;
    }

    if (out_ch != 1 && out_ch != 2) {
        out_ch = src_ch;
    }

    u32 in_frame_bytes = (u32)(src_ch * 2);

    if (in_frame_bytes == 0) {
        return;
    }

    u32 sample_frames = size / in_frame_bytes;

    if (sample_frames == 0) {
        return;
    }

    /*
        Keep output inside AUDIO_MAX_PACKET because the NDSP wave buffers were
        allocated at AUDIO_MAX_PACKET.
    */
    u32 max_frames = AUDIO_MAX_PACKET / (u32)(out_ch * 2);

    if (sample_frames > max_frames) {
        sample_frames = max_frames;
    }

    u32 out_bytes = sample_frames * (u32)(out_ch * 2);

    /*
        Fast path if no processing needed.
    */
    if (g_hfix56_volume_percent == 100 &&
        !g_hfix56_limiter_enabled &&
        src_ch == out_ch) {
        audio_queue_raw_ndsp(data, out_bytes);
        return;
    }

    if (!hfix56_audio_mix_ensure(out_bytes)) {
        audio_queue_raw_ndsp(data, out_bytes);
        return;
    }

    const s16 *in = (const s16*)data;
    s16 *out = (s16*)g_hfix56_audio_mix_buf;

    if (src_ch == 1 && out_ch == 2) {
        /*
            Mono -> stereo upmix.
        */
        for (u32 i = 0; i < sample_frames; i++) {
            int v = hfix56_apply_gain_one(in[i]);
            out[i * 2 + 0] = (s16)v;
            out[i * 2 + 1] = (s16)v;
        }
    } else if (src_ch == 2 && out_ch == 2) {
        /*
            True stereo path.
        */
        for (u32 i = 0; i < sample_frames; i++) {
            int l = hfix56_apply_gain_one(in[i * 2 + 0]);
            int r = hfix56_apply_gain_one(in[i * 2 + 1]);
            out[i * 2 + 0] = (s16)l;
            out[i * 2 + 1] = (s16)r;
        }
    } else if (src_ch == 2 && out_ch == 1) {
        /*
            Stereo -> mono downmix if forced stereo is off and stream/output
            somehow request mono.
        */
        for (u32 i = 0; i < sample_frames; i++) {
            int mixed = ((int)in[i * 2 + 0] + (int)in[i * 2 + 1]) >> 1;
            out[i] = (s16)hfix56_apply_gain_one(mixed);
        }
    } else {
        /*
            Mono -> mono.
        */
        for (u32 i = 0; i < sample_frames; i++) {
            out[i] = (s16)hfix56_apply_gain_one(in[i]);
        }
    }

    audio_queue_raw_ndsp(g_hfix56_audio_mix_buf, out_bytes);
}



/* ------------------------------------------------------------------------- */
/* Frame pacing                                                               */
/* ------------------------------------------------------------------------- */

static void pace(void) {
    /*
        Kept as a no-op compatibility stub.
    */
}

static void cap_frame_budget(u64 frame_start_tick, const Stream *v) {
    u32 fpsn = v->fpsn ? v->fpsn : 30;
    u32 fpsd = v->fpsd ? v->fpsd : 1;

    u64 frame_ticks = ((u64)SYSCLOCK_ARM11 * fpsd) / fpsn;

    if (frame_ticks == 0) {
        frame_ticks = ((u64)SYSCLOCK_ARM11 / 30);
    }

    u64 target = frame_start_tick + frame_ticks;

    /*
        VBlank-aware pacing:
        - Reduces tiny fast/slow wobble from pure busy-yield timing.
        - Avoids pegging ARM11 while waiting.
        - At 30 FPS on a 60 Hz display, this naturally settles near every
          second VBlank when decode/blit time is stable.
    */
    while (svcGetSystemTick() < target) {
        gspWaitForVBlank();
    }
}

static void hfix59r3_present_video_frame(
    const Stream *v,
    M2Y0Frame *m2y0,
    bool *m2y0_have_prev,
    u8 **frame,
    u8 **prev,
    size_t fsz,
    bool *have_prev,
    bool hfix51b_direct_present_pending,
    bool *hfix51c_last_direct_yuv,
    u32 *shown,
    u64 *next_frame_tick,
    u64 frame_ticks_abs,
    u32 fpsn_abs,
    u32 fpsd_abs
) {
    if (!v || !m2y0 || !frame || !prev || !shown || !next_frame_tick) {
        return;
    }

    while (svcGetSystemTick() < *next_frame_tick) {
        gspWaitForVBlank();
    }

    if (hfix51b_direct_present_pending &&
        m2y0->w == TOP_W &&
        m2y0->h == TOP_H &&
        m2y0_to_rgb565_y2r_linear(m2y0)) {
        blit565_scaled(g_hfix52a_y2r_rgb565, v->w, v->h);
    } else if (hfix51b_direct_present_pending &&
               m2y0->w == TOP_W &&
               m2y0->h == TOP_H) {
        m2y0_to_top_rgb565_direct(m2y0);
    } else {
        if (hfix51c_last_direct_yuv) {
            *hfix51c_last_direct_yuv = false;
        }
        blit565_scaled(*frame, v->w, v->h);
    }

    u8 *tmp = *prev;
    *prev = *frame;
    *frame = tmp;

    if (have_prev) {
        *have_prev = true;
    }

    (*shown)++;
    hfix58s_subtitles_set_frame_time(*shown, fpsn_abs, fpsd_abs);

    u64 now_tick = svcGetSystemTick();

    if (now_tick > *next_frame_tick + frame_ticks_abs * 2) {
        *next_frame_tick = now_tick + frame_ticks_abs;
    } else {
        *next_frame_tick += frame_ticks_abs;
    }
}

/* ------------------------------------------------------------------------- */
/* Phase 5A streaming playback                                                */
/* ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
/* HFIX58F keyframe seek index and execution                                  */
/* ------------------------------------------------------------------------- */

#define HFIX58F_MAX_SEEK_POINTS 4096
#define HFIX58F_SEEK_STEP_FRAMES 150
#define HFIX58F_SYNC_INDEX_FAST_LIMIT_BYTES (256ull * 1024ull * 1024ull)

typedef struct {
    u32 frame;
    u64 file_offset;
} Hfix58FSeekPoint;

typedef struct {
    Hfix58FSeekPoint points[HFIX58F_MAX_SEEK_POINTS];
    u32 count;
    u32 total_frames;
    bool ready;
} Hfix58FSeekIndex;

static Hfix58FSeekIndex g_hfix58f_seek;
static u64 g_hfix58f_media_end_offset = 0;



/* ------------------------------------------------------------------------- */
/* HFIX58J_TOUCH_SCRUB_HELPERS                                                */
/* ------------------------------------------------------------------------- */

/* HFIX58J-R2 RESTORED SEEK GLOBALS */
static u32 g_hfix58f_seek_target = 0;
static bool g_hfix58f_seek_pending = false;
static bool g_hfix58f_seek_preview_pending = false;
static bool g_hfix58f_seek_ui_active = false;
static u32 g_hfix58f_seek_ui_frames = 0;
static bool g_hfix58f_seek_catchup_active = false;
static u32 g_hfix58f_seek_catchup_target = 0;
static bool g_hfix58f_seek_preview_decode_pending = false;

static u32 hfix58j_clamp_seek_target(u32 target_frame) {
    u32 total = hfix58f_total_frames();

    if (total > 30 && target_frame > total - 30) {
        target_frame = total - 30;
    } else if (total <= 30) {
        target_frame = 0;
    }

    return target_frame;
}

static void hfix58j_request_absolute_seek(u32 target_frame) {
    target_frame = hfix58j_clamp_seek_target(target_frame);

    if (!g_hfix58f_seek.ready || g_hfix58f_seek.count == 0) {
        hfix58_alert_set("SEEK INDEX MISSING", 2);
        return;
    }

    g_hfix58f_seek_target = target_frame;
    g_hfix58f_seek_pending = true;
    g_hfix58f_seek_preview_pending = false;
    g_hfix58f_seek_ui_active = true;
    g_hfix58f_seek_ui_frames = 18;
}

static void hfix58j_request_preview_seek(u32 target_frame) {
    target_frame = hfix58j_clamp_seek_target(target_frame);

    if (!g_hfix58f_seek.ready || g_hfix58f_seek.count == 0) {
        return;
    }

    g_hfix58f_seek_target = target_frame;
    g_hfix58f_seek_pending = true;
    g_hfix58f_seek_preview_pending = true;
    g_hfix58f_seek_ui_active = true;
    g_hfix58f_seek_ui_frames = 8;
}

/* HFIX60: load chapter markers from a ".chapters" sidecar.
   Each line is "SECONDS Label", "H:MM:SS Label", or "SECONDS|Label". */
static void hfix60_chapters_load(const char *video_path, u32 fpsn, u32 fpsd) {
    char path[HFIX58_MAX_PATH];
    FILE *fp;
    char line[128];

    g_mivf_chapters_count = 0;

    if (!g_mivf_settings.show_chapters || !video_path || !*video_path) {
        return;
    }

    if (fpsn == 0) fpsn = 30;
    if (fpsd == 0) fpsd = 1;

    hfix60_make_sidecar_path(path, sizeof(path), video_path, ".chapters");
    if (!path[0]) {
        return;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        return;
    }

    while (fgets(line, sizeof(line), fp)) {
        char *p = line;
        char *label;
        u32 secs = 0;
        int hh = 0, mm = 0, ss = 0;
        MivfChapter *c;
        size_t ln;

        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (*p == 0 || *p == '#' || *p == '\r' || *p == '\n') {
            continue;
        }

        if (sscanf(p, "%d:%d:%d", &hh, &mm, &ss) == 3) {
            secs = (u32)(hh * 3600 + mm * 60 + ss);
        } else if (sscanf(p, "%d:%d", &mm, &ss) == 2) {
            secs = (u32)(mm * 60 + ss);
        } else {
            secs = (u32)strtoul(p, NULL, 10);
        }

        label = strchr(p, '|');
        if (label) {
            label++;
        } else {
            label = p;
            while (*label && *label != ' ' && *label != '\t') {
                label++;
            }
            while (*label == ' ' || *label == '\t') {
                label++;
            }
        }

        if (g_mivf_chapters_count >= MIVF_CHAP_MAX) {
            break;
        }

        c = &g_mivf_chapters[g_mivf_chapters_count];
        c->frame = (u32)(((u64)secs * (u64)fpsn) / (u64)fpsd);
        snprintf(c->label, sizeof(c->label), "%s", label ? label : "");

        ln = strlen(c->label);
        while (ln > 0 && (c->label[ln - 1] == '\n' || c->label[ln - 1] == '\r' || c->label[ln - 1] == ' ')) {
            c->label[--ln] = 0;
        }

        if (c->label[0] == 0) {
            snprintf(c->label, sizeof(c->label), "CH %d", g_mivf_chapters_count + 1);
        }

        g_mivf_chapters_count++;
    }

    fclose(fp);
}

/* HFIX60: jump to the previous (dir<0) or next (dir>0) chapter. */
static void hfix60_chapter_jump(int dir, u32 cur_frame) {
    int target = -1;

    if (g_mivf_chapters_count <= 0) {
        hfix58_alert_set("NO CHAPTERS", 2);
        return;
    }

    if (dir > 0) {
        for (int i = 0; i < g_mivf_chapters_count; i++) {
            if (g_mivf_chapters[i].frame > cur_frame + 2) {
                target = i;
                break;
            }
        }
        if (target < 0) {
            target = g_mivf_chapters_count - 1;
        }
    } else {
        for (int i = g_mivf_chapters_count - 1; i >= 0; i--) {
            if (g_mivf_chapters[i].frame + 30 < cur_frame) {
                target = i;
                break;
            }
        }
        if (target < 0) {
            target = 0;
        }
    }

    hfix58j_request_absolute_seek(g_mivf_chapters[target].frame);

    {
        char msg[64];
        snprintf(msg, sizeof(msg), "CH %d: %s", target + 1, g_mivf_chapters[target].label);
        hfix58_alert_set(msg, 1);
    }
}

static void hfix58j_touch_scrub_update(u32 down, u32 held, u32 up) {
    touchPosition touch;
    static u32 last_live_seek_frame = 0xFFFFFFFFu;
    static u32 live_seek_cooldown = 0;
    bool seek_busy =
        g_hfix58f_seek_pending ||
        g_hfix58f_seek_catchup_active ||
        g_hfix58f_seek_preview_decode_pending;

    if (live_seek_cooldown > 0) {
        live_seek_cooldown--;
    }

    if ((held & KEY_TOUCH) != 0) {
        int timeline_x = 30;
        int timeline_w = 260;
        int timeline_y = g_mivf_anim.panel_current_y + 103;
        int timeline_h = 9;

        hidTouchRead(&touch);

        if (touch.py >= timeline_y - 10 && touch.py <= timeline_y + timeline_h + 10) {
            u32 total = hfix58f_total_frames();
            int x = (int)touch.px;

            if (x < timeline_x) x = timeline_x;
            if (x > timeline_x + timeline_w) x = timeline_x + timeline_w;

            g_mivf_anim.is_touch_scrubbing = true;
            g_mivf_anim.scrub_target_frame =
                (u32)(((u64)(x - timeline_x) * (u64)total) / (u64)timeline_w);

            if (g_mivf_anim.scrub_target_frame >= total && total > 0) {
                g_mivf_anim.scrub_target_frame = total - 1u;
            }

            g_mivf_anim.force_clear_frames = 2;
            g_mivf_anim.idle_frame_counter = 0;
            g_mivf_anim.wake_settle_frames = 30;

            if (g_mivf_anim.visibility_state == UI_STATE_HIDDEN ||
                g_mivf_anim.visibility_state == UI_STATE_SLIDING_DOWN) {
                g_mivf_anim.visibility_state = UI_STATE_SLIDING_UP;
                g_mivf_anim.panel_target_y = 96;
            }

            {
                u32 target = g_mivf_anim.scrub_target_frame;
                u32 fps = g_hfix59r2_video_fps_num ? g_hfix59r2_video_fps_num : 30;
                u32 threshold = fps;
                u32 delta;
                bool first_touch = (down & KEY_TOUCH) != 0 || last_live_seek_frame == 0xFFFFFFFFu;

                if (threshold < 15u) {
                    threshold = 15u;
                }

                delta = target > last_live_seek_frame
                    ? target - last_live_seek_frame
                    : last_live_seek_frame - target;

                if (!seek_busy && (first_touch || (live_seek_cooldown == 0 && delta >= threshold))) {
                    hfix58j_request_preview_seek(target);
                    last_live_seek_frame = target;
                    live_seek_cooldown = 12;
                }
            }
        }
    }

    if ((up & KEY_TOUCH) != 0 && g_mivf_anim.is_touch_scrubbing) {
        g_mivf_anim.is_touch_scrubbing = false;

        if (last_live_seek_frame != g_mivf_anim.scrub_target_frame) {
            hfix58j_request_absolute_seek(g_mivf_anim.scrub_target_frame);
        }

        last_live_seek_frame = 0xFFFFFFFFu;
        live_seek_cooldown = 0;
        g_mivf_anim.force_clear_frames = 2;
    }

    (void)down;
}






/* HFIX58F_R2_SEEK_UI_TAIL_DEF */
static void hfix58f_tick_seek_ui_tail(void) {
    if (g_hfix58f_seek_ui_frames > 0) {
        g_hfix58f_seek_ui_frames--;

        if (g_hfix58f_seek_ui_frames == 0) {
            g_hfix58f_seek_ui_active = false;
        }
    }
}

static u32 hfix58f_current_frame(void) {
    if (g_hfix58f_seek_pending) {
        return g_hfix58f_seek_target;
    }

    if (g_hfix58f_seek_catchup_active) {
        return g_hfix58f_seek_catchup_target;
    }

    return g_media_ctl.current_frame_idx;
}

static u32 hfix59r2_frame_to_sec(u32 frame) {
    u32 fpsn = g_hfix59r2_video_fps_num ? g_hfix59r2_video_fps_num : 30;
    u32 fpsd = g_hfix59r2_video_fps_den ? g_hfix59r2_video_fps_den : 1;

    return (u32)(((u64)frame * (u64)fpsd) / (u64)fpsn);
}

static void hfix59r2_format_time(char *out, size_t out_sz, u32 sec) {
    u32 h = sec / 3600u;
    u32 m = (sec / 60u) % 60u;
    u32 ss = sec % 60u;

    if (!out || out_sz == 0) {
        return;
    }

    if (h > 0) {
        snprintf(out, out_sz, "%lu:%02lu:%02lu",
            (unsigned long)h,
            (unsigned long)m,
            (unsigned long)ss);
    } else {
        snprintf(out, out_sz, "%02lu:%02lu",
            (unsigned long)m,
            (unsigned long)ss);
    }
}

static u32 hfix58f_total_frames(void) {
    if (g_hfix59r2_duration_ticks != 0 && g_hfix59r2_video_fps_num != 0) {
        u64 fpsn = (u64)g_hfix59r2_video_fps_num;
        u64 fpsd = (u64)(g_hfix59r2_video_fps_den ? g_hfix59r2_video_fps_den : 1);
        u64 den = 30000ull * fpsd;
        u64 frames = (g_hfix59r2_duration_ticks * fpsn + den - 1) / den;

        if (frames > 0xffffffffull) {
            return 0xffffffffu;
        }

        if (frames > 0) {
            return (u32)frames;
        }
    }

    if (g_hfix58f_seek.total_frames) {
        return g_hfix58f_seek.total_frames;
    }

    if (g_media_ctl.total_frames) {
        return g_media_ctl.total_frames;
    }

    return 1;
}

static void hfix58f_seed_total_frames_from_duration(const Stream *v) {
    u32 fpsn = v && v->fpsn ? v->fpsn : (g_hfix59r2_video_fps_num ? g_hfix59r2_video_fps_num : 30u);
    u32 fpsd = v && v->fpsd ? v->fpsd : (g_hfix59r2_video_fps_den ? g_hfix59r2_video_fps_den : 1u);

    if (g_hfix59r2_duration_ticks != 0 && fpsn != 0) {
        u64 den = 30000ull * (u64)(fpsd ? fpsd : 1u);
        u64 frames = (g_hfix59r2_duration_ticks * (u64)fpsn + den - 1u) / den;

        if (frames > 0xffffffffull) {
            frames = 0xffffffffull;
        }

        g_hfix58f_seek.total_frames = (u32)frames;
        g_media_ctl.total_frames = (u32)frames;
    }
}

static bool hfix58f_seek_active(void) {
    return g_hfix58f_seek_ui_active ||
        g_hfix58f_seek_pending ||
        g_hfix58f_seek_catchup_active ||
        g_hfix58f_seek_preview_decode_pending;
}

static bool hfix58f_body_is_m2y_delta_codec(const u8 *body, u32 psize) {
    return body &&
        psize >= 13 &&
        body[0] == 'M' &&
        body[1] == '2' &&
        body[2] == 'Y' &&
        (body[3] == '1' || body[3] == '2');
}

static bool hfix58f_body_is_m2y_keyframe(const u8 *body, u32 psize) {
    if (!hfix58f_body_is_m2y_delta_codec(body, psize)) {
        return false;
    }

    return body[12] == 1;
}

static bool hfix58f_packet_body_is_sync_video(const u8 *body, u32 psize, const Stream *v, const Packet *k) {
    if (!body || psize < 4 || !v || !k) {
        return false;
    }

    /*
        Best-known safe sync points:
          - M2Y0 raw YUV420 packet
          - M1P0 all-intra packet if present
          - packet flag bit 0 if encoder marks key packets
          - RAWV-sized video packet
    */
    if (body[0] == 'M' && body[1] == '2' && body[2] == 'Y' && body[3] == '0') {
        return true;
    }

    if (hfix58f_body_is_m2y_delta_codec(body, psize)) {
        return hfix58f_body_is_m2y_keyframe(body, psize);
    }

    if (body[0] == 'M' && body[1] == '1' && body[2] == 'P' && body[3] == '0') {
        return true;
    }

    if ((k->flags & 1) != 0 &&
        body[0] == 'M' &&
        (body[1] == '2' || body[1] == '1')) {
        return true;
    }

    if (!strcmp(v->codec, "RAWV")) {
        u32 raw_size = (u32)v->w * (u32)v->h * 2u;
        if (raw_size != 0 && psize == raw_size) {
            return true;
        }
    }

    return false;
}


/* ------------------------------------------------------------------------- */
/* HFIX58I_IDX_CACHE_HELPERS                                                   */
/* Persistent .idx seek table cache.                                           */
/* ------------------------------------------------------------------------- */

#define HFIX58I_IDX_MAGIC   0x31494458u  /* "XDI1" little-endian-ish */
#define HFIX58I_IDX_VERSION 1u

static bool hfix58i_make_idx_path(char *out, size_t out_sz, const char *mivf_path) {
    if (!out || out_sz == 0 || !mivf_path) {
        return false;
    }

    snprintf(out, out_sz, "%s", mivf_path);

    char *dot = strrchr(out, '.');

    if (dot && dot > out) {
        snprintf(dot, out_sz - (size_t)(dot - out), ".idx");
    } else {
        size_t len = strlen(out);
        if (len + 4 >= out_sz) {
            return false;
        }
        strcat(out, ".idx");
    }

    return true;
}

static bool hfix58i_write_u32(FILE *f, u32 v) {
    return fwrite(&v, 1, sizeof(v), f) == sizeof(v);
}

static bool hfix58i_write_u64(FILE *f, u64 v) {
    return fwrite(&v, 1, sizeof(v), f) == sizeof(v);
}

static bool hfix58i_read_u32(FILE *f, u32 *v) {
    return v && fread(v, 1, sizeof(*v), f) == sizeof(*v);
}

static bool hfix58i_read_u64(FILE *f, u64 *v) {
    return v && fread(v, 1, sizeof(*v), f) == sizeof(*v);
}

static bool hfix58i_try_load_seek_cache(const char *cache_path, u64 file_size, u32 first_offset) {
    if (!cache_path) {
        return false;
    }

    FILE *cf = fopen(cache_path, "rb");
    if (!cf) {
        return false;
    }

    u32 magic = 0;
    u32 version = 0;
    u64 cached_file_size = 0;
    u32 cached_first = 0;
    u32 total_frames = 0;
    u32 count = 0;

    bool ok =
        hfix58i_read_u32(cf, &magic) &&
        hfix58i_read_u32(cf, &version) &&
        hfix58i_read_u64(cf, &cached_file_size) &&
        hfix58i_read_u32(cf, &cached_first) &&
        hfix58i_read_u32(cf, &total_frames) &&
        hfix58i_read_u32(cf, &count);

    if (!ok ||
        magic != HFIX58I_IDX_MAGIC ||
        version != HFIX58I_IDX_VERSION ||
        cached_file_size != file_size ||
        cached_first != first_offset ||
        count == 0 ||
        count > HFIX58F_MAX_SEEK_POINTS) {
        fclose(cf);
        return false;
    }

    size_t want = (size_t)count * sizeof(g_hfix58f_seek.points[0]);

    if (fread(g_hfix58f_seek.points, 1, want, cf) != want) {
        fclose(cf);
        return false;
    }

    fclose(cf);

    g_hfix58f_seek.count = count;
    g_hfix58f_seek.total_frames = total_frames;
    g_hfix58f_seek.ready = true;
    g_media_ctl.total_frames = total_frames;

    hfix58_alert_set("SEEK CACHE HIT", 1);
    return true;
}

static void hfix58i_save_seek_cache(const char *cache_path, u64 file_size, u32 first_offset) {
    if (!cache_path || !g_hfix58f_seek.ready || g_hfix58f_seek.count == 0) {
        return;
    }

    FILE *cf = fopen(cache_path, "wb");

    if (!cf) {
        return;
    }

    bool ok = true;

    ok = ok && hfix58i_write_u32(cf, HFIX58I_IDX_MAGIC);
    ok = ok && hfix58i_write_u32(cf, HFIX58I_IDX_VERSION);
    ok = ok && hfix58i_write_u64(cf, file_size);
    ok = ok && hfix58i_write_u32(cf, first_offset);
    ok = ok && hfix58i_write_u32(cf, g_hfix58f_seek.total_frames);
    ok = ok && hfix58i_write_u32(cf, g_hfix58f_seek.count);

    if (ok) {
        size_t bytes = (size_t)g_hfix58f_seek.count * sizeof(g_hfix58f_seek.points[0]);
        ok = fwrite(g_hfix58f_seek.points, 1, bytes, cf) == bytes;
    }

    fclose(cf);

    if (ok) {
        hfix58_alert_set("SEEK CACHE SAVED", 1);
    }
}


/* ------------------------------------------------------------------------- */
/* HFIX58J_IDX_CACHE_HELPERS                                                   */
/* ------------------------------------------------------------------------- */

#define HFIX58J_IDX_MAGIC   0x314A4458u
#define HFIX58J_IDX_VERSION 2u

/* Embedded seek-index footer: [index payload][MIDX footer trailer]. */
#define MIVF_EMBED_IDX_FOOTER_MAGIC   0x5844494Du /* "MIDX" little-endian */
#define MIVF_EMBED_IDX_FOOTER_VERSION 1u
#define MIVF_EMBED_IDX_FOOTER_SIZE    32u

static bool hfix58j_make_idx_path(char *out, size_t out_sz, const char *mivf_path) {
    if (!out || out_sz == 0 || !mivf_path) {
        return false;
    }

    snprintf(out, out_sz, "%s", mivf_path);

    char *dot = strrchr(out, '.');
    if (dot && dot > out) {
        snprintf(dot, out_sz - (size_t)(dot - out), ".idx");
    } else {
        size_t len = strlen(out);
        if (len + 4 >= out_sz) {
            return false;
        }
        strcat(out, ".idx");
    }

    return true;
}

static bool hfix58j_read_u32(FILE *f, u32 *v) {
    return v && fread(v, 1, sizeof(*v), f) == sizeof(*v);
}

static bool hfix58j_read_u64(FILE *f, u64 *v) {
    return v && fread(v, 1, sizeof(*v), f) == sizeof(*v);
}

static bool hfix58j_write_u32(FILE *f, u32 v) {
    return fwrite(&v, 1, sizeof(v), f) == sizeof(v);
}

static bool hfix58j_write_u64(FILE *f, u64 v) {
    return fwrite(&v, 1, sizeof(v), f) == sizeof(v);
}

static bool hfix58j_try_load_seek_cache(const char *cache_path, u64 file_size, u32 first_offset) {
    FILE *cf = fopen(cache_path, "rb");
    if (!cf) {
        return false;
    }

    u32 magic = 0;
    u32 version = 0;
    u64 cached_file_size = 0;
    u32 cached_first = 0;
    u32 total_frames = 0;
    u32 count = 0;

    bool ok =
        hfix58j_read_u32(cf, &magic) &&
        hfix58j_read_u32(cf, &version) &&
        hfix58j_read_u64(cf, &cached_file_size) &&
        hfix58j_read_u32(cf, &cached_first) &&
        hfix58j_read_u32(cf, &total_frames) &&
        hfix58j_read_u32(cf, &count);

    if (!ok ||
        magic != HFIX58J_IDX_MAGIC ||
        version != HFIX58J_IDX_VERSION ||
        cached_file_size != file_size ||
        cached_first != first_offset ||
        count == 0 ||
        count > HFIX58F_MAX_SEEK_POINTS) {
        fclose(cf);
        return false;
    }

    size_t bytes = (size_t)count * sizeof(g_hfix58f_seek.points[0]);

    if (fread(g_hfix58f_seek.points, 1, bytes, cf) != bytes) {
        fclose(cf);
        return false;
    }

    fclose(cf);

    g_hfix58f_seek.count = count;
    g_hfix58f_seek.total_frames = total_frames;
    g_hfix58f_seek.ready = true;
    g_media_ctl.total_frames = total_frames;

    hfix58_alert_set("SEEK CACHE HIT", 1);
    return true;
}

static void hfix58j_save_seek_cache(const char *cache_path, u64 file_size, u32 first_offset) {
    if (!cache_path || !g_hfix58f_seek.ready || g_hfix58f_seek.count == 0) {
        return;
    }

    FILE *cf = fopen(cache_path, "wb");
    if (!cf) {
        return;
    }

    bool ok = true;

    ok = ok && hfix58j_write_u32(cf, HFIX58J_IDX_MAGIC);
    ok = ok && hfix58j_write_u32(cf, HFIX58J_IDX_VERSION);
    ok = ok && hfix58j_write_u64(cf, file_size);
    ok = ok && hfix58j_write_u32(cf, first_offset);
    ok = ok && hfix58j_write_u32(cf, g_hfix58f_seek.total_frames);
    ok = ok && hfix58j_write_u32(cf, g_hfix58f_seek.count);

    if (ok) {
        size_t bytes = (size_t)g_hfix58f_seek.count * sizeof(g_hfix58f_seek.points[0]);
        ok = fwrite(g_hfix58f_seek.points, 1, bytes, cf) == bytes;
    }

    fclose(cf);

    if (ok) {
        hfix58_alert_set("SEEK CACHE SAVED", 1);
    }
}

static bool hfix58f_try_load_embedded_index(FILE *f, u32 first_offset) {
    if (!f) {
        return false;
    }

    long saved_pos = ftell(f);
    bool loaded = false;

    if (fseek(f, 0, SEEK_END) != 0) {
        goto out;
    }

    long end_pos_l = ftell(f);
    if (end_pos_l <= 0) {
        goto out;
    }

    u64 file_size = (u64)end_pos_l;
    if (file_size < MIVF_EMBED_IDX_FOOTER_SIZE) {
        goto out;
    }

    if (fseek(f, (long)(file_size - MIVF_EMBED_IDX_FOOTER_SIZE), SEEK_SET) != 0) {
        goto out;
    }

    u8 footer[MIVF_EMBED_IDX_FOOTER_SIZE];
    if (fread(footer, 1, sizeof(footer), f) != sizeof(footer)) {
        goto out;
    }

    u32 footer_magic = le32(footer + 0);
    u32 footer_version = le32(footer + 4);
    u64 index_offset = le64(footer + 8);
    u32 index_size = le32(footer + 16);
    u32 idx_magic = le32(footer + 20);
    u32 idx_version = le32(footer + 24);

    if (footer_magic != MIVF_EMBED_IDX_FOOTER_MAGIC ||
        footer_version != MIVF_EMBED_IDX_FOOTER_VERSION ||
        idx_magic != HFIX58J_IDX_MAGIC ||
        idx_version != HFIX58J_IDX_VERSION) {
        goto out;
    }

    if (index_size < 28u ||
        index_offset >= file_size ||
        index_offset + (u64)index_size > file_size - (u64)MIVF_EMBED_IDX_FOOTER_SIZE) {
        goto out;
    }

    if (fseek(f, (long)index_offset, SEEK_SET) != 0) {
        goto out;
    }

    u32 magic = 0;
    u32 version = 0;
    u64 cached_file_size = 0;
    u32 cached_first = 0;
    u32 total_frames = 0;
    u32 count = 0;

    bool ok =
        hfix58j_read_u32(f, &magic) &&
        hfix58j_read_u32(f, &version) &&
        hfix58j_read_u64(f, &cached_file_size) &&
        hfix58j_read_u32(f, &cached_first) &&
        hfix58j_read_u32(f, &total_frames) &&
        hfix58j_read_u32(f, &count);

    if (!ok ||
        magic != HFIX58J_IDX_MAGIC ||
        version != HFIX58J_IDX_VERSION ||
        cached_file_size != file_size ||
        cached_first != first_offset ||
        count == 0 ||
        count > HFIX58F_MAX_SEEK_POINTS) {
        goto out;
    }

    size_t bytes = (size_t)count * sizeof(g_hfix58f_seek.points[0]);
    if ((u64)index_size < 28ull + (u64)bytes) {
        goto out;
    }

    if (fread(g_hfix58f_seek.points, 1, bytes, f) != bytes) {
        goto out;
    }

    g_hfix58f_seek.count = count;
    g_hfix58f_seek.total_frames = total_frames;
    g_hfix58f_seek.ready = true;
    g_media_ctl.total_frames = total_frames;
    g_hfix58f_media_end_offset = index_offset;

    loaded = true;

out:
    if (saved_pos >= 0) {
        fseek(f, saved_pos, SEEK_SET);
    }
    return loaded;
}

static bool hfix58f_build_seek_index(FILE *f, u32 first_offset, const Stream *v) {
    memset(&g_hfix58f_seek, 0, sizeof(g_hfix58f_seek));

    if (!f || !v) {
        return false;
    }

    long saved = ftell(f);

    if (fseek(f, 0, SEEK_END) != 0) {
        return false;
    }

    long end_pos_l = ftell(f);

    if (end_pos_l <= 0) {
        if (saved >= 0) {
            fseek(f, saved, SEEK_SET);
        }
        return false;
    }

    u64 file_size = (u64)end_pos_l;
    char cache_path[512];

    /* Prefer embedded footer index when present. */
    if (hfix58f_try_load_embedded_index(f, first_offset)) {
        if (saved >= 0) {
            fseek(f, saved, SEEK_SET);
        }
        return true;
    }

    if (hfix58j_make_idx_path(cache_path, sizeof(cache_path), MIVF_PATH)) {
        /*
            HFIX58J_IDX_CACHE_LOAD:
            Avoid thousands of tiny SD reads when cache exists.
        */
        if (hfix58j_try_load_seek_cache(cache_path, file_size, first_offset)) {
            if (saved >= 0) {
                fseek(f, saved, SEEK_SET);
            }
            return true;
        }
    } else {
        cache_path[0] = 0;
    }

    if (file_size > HFIX58F_SYNC_INDEX_FAST_LIMIT_BYTES) {
        hfix58f_seed_total_frames_from_duration(v);

        if (saved >= 0) {
            fseek(f, saved, SEEK_SET);
        }

        /*
            Movie-sized uncached files must start playback immediately. A full
            first-run seek-index scan can walk gigabytes and looks like a hang.
            Existing .idx files are still used, and small files still build one.
        */
        return false;
    }

    /*
        HFIX58J_IDX_CACHE_MISS_SCAN:
        First-run fallback. Metadata-only scan; payloads are skipped.
    */
    u64 end_pos = file_size;
    u64 pos = (u64)first_offset;
    u8 page_hdr[MIVF_PAGE_HEADER_SIZE];
    u8 pkt_hdr[16];
    u32 highest_frame = 0;

    while (pos + MIVF_PAGE_HEADER_SIZE < end_pos &&
           g_hfix58f_seek.count < HFIX58F_MAX_SEEK_POINTS) {
        if (fseek(f, (long)pos, SEEK_SET) != 0) {
            break;
        }

        if (fread(page_hdr, 1, MIVF_PAGE_HEADER_SIZE, f) != MIVF_PAGE_HEADER_SIZE) {
            break;
        }

        u32 payload = le32(page_hdr + 0x10);
        u16 packets = le16(page_hdr + 0x14);

        if (payload == 0 || payload > (512 * 1024) || packets == 0 || packets > 128) {
            break;
        }

        u64 page_payload_start = pos + MIVF_PAGE_HEADER_SIZE;
        u64 page_end = page_payload_start + payload;

        if (page_end > end_pos) {
            break;
        }

        u64 pkt_pos = page_payload_start;
        bool page_has_sync = false;
        u32 sync_frame = 0;

        for (u16 i = 0; i < packets && pkt_pos + 16 <= page_end; i++) {
            if (fseek(f, (long)pkt_pos, SEEK_SET) != 0) {
                break;
            }

            if (fread(pkt_hdr, 1, sizeof(pkt_hdr), f) != sizeof(pkt_hdr)) {
                break;
            }

            Packet k;
            memset(&k, 0, sizeof(k));

            /*
                Packet has no .stream field in this branch.
                Header byte 0 is the stream id.
            */
            u8 pkt_stream = pkt_hdr[0];

            k.flags = pkt_hdr[1];
            k.hsize = le16(pkt_hdr + 2);
            k.psize = le32(pkt_hdr + 8);
            k.frame = le32(pkt_hdr + 12);

            if (k.hsize < 16 || k.psize > payload || pkt_pos + k.hsize + k.psize > page_end) {
                break;
            }

            if (pkt_stream != v->id) {
                pkt_pos += (u64)k.hsize + (u64)k.psize;
                continue;
            }

            if (k.frame > highest_frame) {
                highest_frame = k.frame;
            }

            bool sync = false;
            u8 body_head[16];
            memset(body_head, 0, sizeof(body_head));

            if (k.psize >= 4 && pkt_pos + k.hsize + 4 <= page_end) {
                u32 probe = k.psize < (u32)sizeof(body_head) ? k.psize : (u32)sizeof(body_head);

                if (fseek(f, (long)(pkt_pos + k.hsize), SEEK_SET) == 0) {
                    (void)fread(body_head, 1, probe, f);
                }
            }

            if (body_head[0] == 'M' && body_head[1] == '2' && body_head[2] == 'Y' && body_head[3] == '0') {
                sync = true;
            }

            if (hfix58f_body_is_m2y_delta_codec(body_head, k.psize)) {
                sync = hfix58f_body_is_m2y_keyframe(body_head, k.psize);
            }

            if (body_head[0] == 'M' && body_head[1] == '1' && body_head[2] == 'P' && body_head[3] == '0') {
                sync = true;
            }

            if (!hfix58f_body_is_m2y_delta_codec(body_head, k.psize) &&
                (k.flags & 1) != 0 &&
                body_head[0] == 'M' &&
                (body_head[1] == '2' || body_head[1] == '1')) {
                sync = true;
            }

            if (!strcmp(v->codec, "RAWV")) {
                u32 raw_size = (u32)v->w * (u32)v->h * 2u;
                if (raw_size != 0 && k.psize == raw_size) {
                    sync = true;
                }
            }

            if (sync) {
                page_has_sync = true;
                sync_frame = k.frame;
                break;
            }

            pkt_pos += (u64)k.hsize + (u64)k.psize;
        }

        if (page_has_sync) {
            Hfix58FSeekPoint *sp = &g_hfix58f_seek.points[g_hfix58f_seek.count++];
            sp->frame = sync_frame;
            sp->file_offset = pos;
        }

        pos = page_end;
    }

    g_hfix58f_seek.total_frames = highest_frame + 1;
    g_media_ctl.total_frames = g_hfix58f_seek.total_frames;
    g_hfix58f_seek.ready = g_hfix58f_seek.count > 0;

    if (saved >= 0) {
        fseek(f, saved, SEEK_SET);
    }

    if (!g_hfix58f_seek.ready) {
        hfix58_alert_set("SEEK INDEX MISSING", 2);
        return false;
    }

    if (cache_path[0]) {
        /*
            HFIX58J_IDX_CACHE_SAVE:
            First run creates cache; later boots hit cache.
        */
        hfix58j_save_seek_cache(cache_path, file_size, first_offset);
    }

    char msg[64];
    snprintf(msg, sizeof(msg), "SEEK POINTS %lu", (unsigned long)g_hfix58f_seek.count);
    hfix58_alert_set(msg, 1);
    return true;
}

static const Hfix58FSeekPoint *hfix58f_find_seekpoint(u32 target_frame) {
    if (!g_hfix58f_seek.ready || g_hfix58f_seek.count == 0) {
        return NULL;
    }

    const Hfix58FSeekPoint *best = &g_hfix58f_seek.points[0];

    for (u32 i = 0; i < g_hfix58f_seek.count; i++) {
        const Hfix58FSeekPoint *sp = &g_hfix58f_seek.points[i];

        if (sp->frame <= target_frame) {
            best = sp;
        } else {
            break;
        }
    }

    return best;
}

static void hfix58f_request_relative_seek(int delta_frames) {
    u32 cur = hfix58f_current_frame();
    u32 total = hfix58f_total_frames();
    u32 target = cur;

    if (delta_frames < 0) {
        u32 step = (u32)(-delta_frames);
        target = cur < step ? 0 : cur - step;
    } else {
        target = cur + (u32)delta_frames;

        if (total > 30 && target > total - 30) {
            target = total - 30;
        } else if (total <= 30) {
            target = 0;
        }
    }

    if (!g_hfix58f_seek.ready || g_hfix58f_seek.count == 0) {
        hfix58_alert_set("SEEK INDEX MISSING", 2);
        g_hfix58f_seek_ui_active = true;
        g_hfix58f_seek_ui_frames = 12;
        return;
    }

    g_hfix58f_seek_target = target;
    g_hfix58f_seek_pending = true;
    g_hfix58f_seek_preview_pending = false;
    g_hfix58f_seek_ui_active = true;
    g_hfix58f_seek_ui_frames = 18;
}

static void hfix58f_audio_flush_for_seek(void) {
    if (!audio_can_use_ndsp()) {
        return;
    }

    audio_configure_ndsp_channel();

    for (int i = 0; i < AUDIO_BUFS; i++) {
        memset(&audio.wb[i], 0, sizeof(ndspWaveBuf));
        audio.wb[i].data_pcm16 = (s16*)audio.buf[i];
        audio.wb[i].nsamples = audio.samples_per_frame;
    }

    g_audio_submit = 0;
    g_audio_drop = 0;
    g_last_audio_bytes = 0;
    g_last_audio_samples = 0;
}

static void hfix58f_reset_m2y_frame(M2Y0Frame *f) {
    if (!f || !f->base) {
        return;
    }

    if (f->y && f->y_size) {
        memset(f->y, 0, f->y_size);
    }

    if (f->cb && f->c_size) {
        memset(f->cb, 128, f->c_size);
    }

    if (f->cr && f->c_size) {
        memset(f->cr, 128, f->c_size);
    }
}

static bool hfix58f_execute_pending_seek(
    MivfStream *stream,
    FILE *f,
    const Stream *v,
    M2Y0Frame *m2y0,
    M2Y0Frame *m2y0_prev,
    bool *m2y0_have_prev,
    u8 *frame,
    u8 *prev,
    size_t fsz,
    bool *have_prev,
    bool *hfix51c_last_direct_yuv,
    u32 *shown,
    u64 *next_frame_tick,
    u64 frame_ticks_abs
) {
    /* HFIX58J_ZERO_WAIT_SEEK: do not block; reader refills asynchronously. */
    if (!g_hfix58f_seek_pending) {
        return false;
    }

    g_hfix58f_seek_pending = false;
    bool preview_seek = g_hfix58f_seek_preview_pending;
    g_hfix58f_seek_preview_pending = false;

    const Hfix58FSeekPoint *sp = hfix58f_find_seekpoint(g_hfix58f_seek_target);

    if (!sp) {
        hfix58_alert_set("SEEK FAILED", 2);
        return false;
    }

    hfix58f_audio_flush_for_seek();

    /*
        HFIX61: fast in-place reseek — keeps the async reader thread and the
        2 MB ring buffer alive and just repositions the file. This removes the
        thread teardown/recreate + ring realloc that made seeks stall.
        Falls back to a full close/reopen if the in-place path ever fails.
    */
    if (!mivf_stream_reseek(stream, (long)sp->file_offset)) {
        mivf_stream_close(stream);

        if (fseek(f, (long)sp->file_offset, SEEK_SET) != 0) {
            hfix58_alert_set("FSEEK FAILED", 2);
            return false;
        }

        if (!mivf_stream_open(stream, f)) {
            hfix58_alert_set("STREAM REOPEN FAIL", 2);
            return false;
        }

        mivf_stream_set_media_end_offset(stream, g_hfix58f_media_end_offset);
    }

    /* HFIX58I_ZERO_WAIT_SEEK: do not block after seek; reader fills asynchronously. */

    if (hfix51c_last_direct_yuv &&
        *hfix51c_last_direct_yuv &&
        m2y0_have_prev &&
        *m2y0_have_prev &&
        m2y0 &&
        m2y0->w == TOP_W &&
        m2y0->h == TOP_H) {
        m2y0_to_top_rgb565_direct(m2y0);
    } else if (have_prev && *have_prev && prev && v) {
        blit565_scaled(prev, (int)v->w, (int)v->h);
    }

    (void)fsz;

    if (have_prev) {
        *have_prev = false;
    }

    hfix58f_reset_m2y_frame(m2y0);
    hfix58f_reset_m2y_frame(m2y0_prev);

    if (m2y0_have_prev) {
        *m2y0_have_prev = false;
    }

    if (hfix51c_last_direct_yuv) {
        *hfix51c_last_direct_yuv = false;
    }

    g_m2y1_deblock_this_frame = false;

    if (shown) {
        *shown = sp->frame;
    }

    g_hfix58f_seek_catchup_target = g_hfix58f_seek_target;
    g_hfix58f_seek_catchup_active = !preview_seek && g_hfix58f_seek_target > sp->frame;
    g_hfix58f_seek_preview_decode_pending = preview_seek;
    g_media_ctl.current_frame_idx = preview_seek ? sp->frame : g_hfix58f_seek_target;

    if (next_frame_tick) {
        *next_frame_tick = svcGetSystemTick();
    }

    if (!preview_seek) {
        char msg[64];
        snprintf(msg, sizeof(msg), "SEEK %lu", (unsigned long)hfix59r2_frame_to_sec(g_hfix58f_seek_target));
        hfix58_alert_set(msg, 1);
    }

    g_hfix58f_seek_ui_active = true;
    g_hfix58f_seek_ui_frames = preview_seek ? 8 : 18;
    return true;
}

static void hfix58f_draw_mmss(u8 *fb, int x, int y, u32 seconds) {
    char t[16];

    hfix59r2_format_time(t, sizeof(t), seconds);
    hfix58_draw_text_shadow(fb, x, y, t, 1, 220, 235, 250);
}

static void hfix58f_draw_timeline(u8 *fb, int panel_y) {
    int x = 30;
    int y = panel_y + 103;
    int w = 260;
    int h = 9;

    if (y >= 240) {
        return;
    }

    u32 cur = hfix58f_current_frame();
    /* HFIX58J_SCRUB_TIMELINE_FRAME */
    if (g_mivf_anim.is_touch_scrubbing) {
        cur = g_mivf_anim.scrub_target_frame;
    }
    u32 total = hfix58f_total_frames();

    if (total == 0) {
        total = 1;
    }

    if (cur > total) {
        cur = total;
    }

    u32 cur_secs = hfix59r2_frame_to_sec(cur);
    u32 total_secs = g_hfix59r2_duration_ticks
        ? (u32)(g_hfix59r2_duration_ticks / 30000ull)
        : hfix59r2_frame_to_sec(total);

    int fill_w = (int)(((u64)cur * (u64)w) / (u64)total);

    if (fill_w < 0) fill_w = 0;
    if (fill_w > w) fill_w = w;

    int text_y = panel_y + 88;

    if (text_y >= 0 && text_y < 240) {
        hfix58f_draw_mmss(fb, 30, text_y, cur_secs);
        hfix58f_draw_mmss(fb, 254, text_y, total_secs);
    }

    /* Track: background + subtle top edge for depth. */
    hfix58_rect565(fb, x, y, w, h, 20, 28, 42);
    hfix58_rect565(fb, x, y, w, 1, 32, 42, 60);

    /* Progress fill + subtle bottom shade edge. */
    hfix58_rect565(fb, x, y, fill_w, h, 0, 140, 255);
    if (fill_w > 0) {
        hfix58_rect565(fb, x, y + h - 1, fill_w, 1, 0, 100, 190);
    }

    /* HFIX60: chapter tick markers along the timeline. */
    if (g_mivf_chapters_count > 0 && total > 0) {
        for (int ci = 0; ci < g_mivf_chapters_count; ci++) {
            u32 cf = g_mivf_chapters[ci].frame;
            int mx;

            if (cf > total) {
                cf = total;
            }

            mx = x + (int)(((u64)cf * (u64)w) / (u64)total);

            if (mx < x) mx = x;
            if (mx > x + w - 1) mx = x + w - 1;

            /* Subtle dim glow behind the tick for readability. */
            hfix58_blend_rect565(fb, mx - 1, y - 3, 3, h + 6,
                34, 38, 52, 180);
            /* Bright chapter tick. */
            hfix58_rect565(fb, mx, y - 3, 1, h + 6, 252, 228, 110);
        }
    }

    /* A/B scene-loop markers: green for A, red for B. */
    if (total > 0) {
        if (g_mivf_ab_a != MIVF_AB_UNSET) {
            u32 af = g_mivf_ab_a > total ? total : g_mivf_ab_a;
            int ax = x + (int)(((u64)af * (u64)w) / (u64)total);
            if (ax < x) ax = x;
            if (ax > x + w - 1) ax = x + w - 1;
            hfix58_rect565(fb, ax, y - 3, 2, h + 6, 90, 230, 120);
        }
        if (g_mivf_ab_b != MIVF_AB_UNSET) {
            u32 bf = g_mivf_ab_b > total ? total : g_mivf_ab_b;
            int bx = x + (int)(((u64)bf * (u64)w) / (u64)total);
            if (bx < x) bx = x;
            if (bx > x + w - 1) bx = x + w - 1;
            hfix58_rect565(fb, bx, y - 3, 2, h + 6, 240, 90, 90);
        }
    }

    int knob_x = x + fill_w - 3;

    if (knob_x < x) {
        knob_x = x;
    }

    if (knob_x > x + w - 7) {
        knob_x = x + w - 7;
    }

    hfix58_rect565(fb, knob_x, y - 3, 7, 15, 230, 245, 255);
        if (g_mivf_anim.is_touch_scrubbing) {
            char scrub_time[16];
            int bubble_w = 48;
            int bubble_h = 16;
            int bubble_x = knob_x - 20;
            int bubble_y = y - 21;

            if (bubble_x < 8) {
                bubble_x = 8;
            }

            if (bubble_x + bubble_w > 312) {
                bubble_x = 312 - bubble_w;
            }

            if (bubble_y < 8) {
                bubble_y = y + 13;
            }

            hfix59r2_format_time(scrub_time, sizeof(scrub_time), cur_secs);
            hfix58_rect565(fb, bubble_x, bubble_y, bubble_w, bubble_h, 12, 20, 32);
            hfix58_rect565(fb, bubble_x + 2, bubble_y + 2, bubble_w - 4, 1, 0, 140, 255);
            hfix58_draw_text_shadow(fb, bubble_x + 6, bubble_y + 4, scrub_time, 1, 240, 248, 255);
        }
}


static void wait_stream_prebuffer(MivfStream *stream) {
    if (!stream) {
        return;
    }

    /*
        PC16 stereo pages are much larger than the original IA4M pages. Starting
        with only a few chunks buffered lets the first cold file read fight the
        decoder and NDSP queue, then a seek back works because the host cache is
        warm. Build a real startup cushion, but cap the wait so bad media never
        hangs the app.
    */
    u32 target = stream->ring.size / 2;
    const u32 min_target = 512 * 1024;
    const u32 max_target = 1024 * 1024;

    if (target < min_target) {
        target = min_target;
    }

    if (target > max_target) {
        target = max_target;
    }

    int spins = 0;
    const int max_spins = 120;

    while (aptMainLoop()) {
        u32 fill = 0;
        bool eof = false;
        bool err = false;

        RecursiveLock_Lock(&stream->ring.lock);
        fill = stream->ring.fill;
        eof = stream->ring.eof;
        err = stream->ring.error;
        RecursiveLock_Unlock(&stream->ring.lock);

        if (fill >= target || eof || err) {
            break;
        }

        spins++;

        if (spins >= max_spins) {
            break;
        }

        gspWaitForVBlank();
    }
}

static void print_ring_telemetry(MivfStream *stream, u32 shown) {
    u32 fill = 0;
    u32 size = 0;
    bool eof = false;
    bool err = false;

    RecursiveLock_Lock(&stream->ring.lock);

    fill = stream->ring.fill;
    size = stream->ring.size;
    eof = stream->ring.eof;
    err = stream->ring.error;

    RecursiveLock_Unlock(&stream->ring.lock);

    /* HFIX58D: scrubbed full bottom-console printf statement. */
}

static int play(void) {
    FILE *f = fopen(MIVF_PATH, "rb");

    if (!f) {
        printf("open fail: %s\n", MIVF_PATH);
        return -1;
    }

    setvbuf(f, (char*)file_iobuf, _IOFBF, sizeof(file_iobuf));

    Header h;

    if (read_header(f, &h)) {
        printf("bad header\n");
        fclose(f);
        return -2;
    }

    Stream v;
    Stream a;

    memset(&v, 0, sizeof(v));
    memset(&a, 0, sizeof(a));

    bool hv = false;
    bool ha = false;

    for (u32 i = 0; i < h.streams; i++) {
        Stream st;

        if (read_stream(f, &st)) {
            printf("stream read err\n");
            fclose(f);
            return -3;
        }

        if (st.type == 1 && !hv) {
            v = st;
            hv = true;
        } else if (st.type == 2 && !ha) {
            a = st;
            ha = true;
        }
    }

    if (!hv) {
        printf("no video stream\n");
        fclose(f);
        return -4;
    }

    printf("%ux%u %s fps=%u/%u RGB565fb\n",
        v.w,
        v.h,
        v.codec,
        v.fpsn,
        v.fpsd ? v.fpsd : 1);

    if (ha) {
        audio_init_from_stream(&a);
    } else {
        printf("no audio stream\n");
    }

    g_hfix58f_media_end_offset = 0;

    g_hfix59r2_video_fps_num = v.fpsn ? v.fpsn : 30;
    g_hfix59r2_video_fps_den = v.fpsd ? v.fpsd : 1;

    hfix58s_subtitles_load_for_video(MIVF_PATH);
    hfix60_chapters_load(MIVF_PATH, v.fpsn, v.fpsd);

    /* HFIX58F_BUILD_SEEK_INDEX: scan keyframe/sync page offsets before streaming. */
    hfix58f_build_seek_index(f, h.first, &v);

    fseek(f, (long)h.first, SEEK_SET);

    MivfStream stream;

    if (!mivf_stream_open(&stream, f)) {
        printf("stream open fail\n");
        audio_shutdown();
        fclose(f);
        return -5;
    }

    mivf_stream_set_media_end_offset(&stream, g_hfix58f_media_end_offset);

    size_t fsz = (size_t)v.w * (size_t)v.h * 2u;


    M2Y0Frame m2y0;
    memset(&m2y0, 0, sizeof(m2y0));

    M2Y0Frame m2y0_prev;
    memset(&m2y0_prev, 0, sizeof(m2y0_prev));

    bool m2y0_ready = false;
    bool m2y0_have_prev = false;

    u8 *frame = (u8*)malloc(fsz);
    u8 *prev  = (u8*)malloc(fsz);

    if (!frame || !prev) {
        printf("OOM frame\n");

        free(frame);
        free(prev);

        /*
            Strict order:
            close stream / join I/O thread before fclose().
        */
        mivf_stream_close(&stream);
        audio_shutdown();
        fclose(f);

        return -6;
    }

    if (!strcmp(v.codec, "M2Y0") || !strcmp(v.codec, "M2Y1") || !strcmp(v.codec, "M2Y2")) {
        if (!m2y0_frame_alloc(&m2y0, v.w, v.h) ||
            !m2y0_frame_alloc(&m2y0_prev, v.w, v.h)) {
            printf("OOM M2Y0/M2Y1 frame\n");
            m2y0_frame_free(&m2y0);
            m2y0_frame_free(&m2y0_prev);
            free(frame);
            free(prev);
            mivf_stream_close(&stream);
            audio_shutdown();
            fclose(f);
            return -7;
        }

        m2y0_ready = true;

        if (!strcmp(v.codec, "M2Y0")) {
            printf("M2Y0 YUV420 chassis ready %ux%u\n", v.w, v.h);
        } else {
            printf("%s compressed YUV420 chassis ready %ux%u\n", v.codec, v.w, v.h);
        }
    }

    memset(frame, 0, fsz);
    memset(prev, 0, fsz);

    bool have_prev = false;
    u32 shown = 0;

    u32 fpsn_abs = v.fpsn ? v.fpsn : 30;
    u32 fpsd_abs = v.fpsd ? v.fpsd : 1;
    u64 base_frame_ticks = ((u64)SYSCLOCK_ARM11 * fpsd_abs) / fpsn_abs;
    if (base_frame_ticks == 0) base_frame_ticks = ((u64)SYSCLOCK_ARM11 / 30);
    u64 frame_ticks_abs = base_frame_ticks * 100u / mivf_speed_pct();
    if (frame_ticks_abs == 0) frame_ticks_abs = 1;
    u64 next_frame_tick = svcGetSystemTick() + frame_ticks_abs;

    /*
        HFIX51C:
        Persistent presentation-history flag. If the last successful video
        presentation used the direct YUV path, pause mode redraws from m2y0
        rather than stale RGB565 frame memory.
    */
    bool hfix51c_last_direct_yuv = false;

    g_hfix58f_seek_catchup_active = false;
    g_hfix58f_seek_catchup_target = 0;
    g_hfix58f_seek_preview_decode_pending = false;

    if (g_mivf_settings.resume_enabled) {
        MivfBookmark bookmark;
        if (MIVF_BookmarkLoad(MIVF_PATH, &bookmark) &&
            bookmark.video_path[0] &&
            !strcmp(bookmark.video_path, MIVF_PATH) &&
            bookmark.frame > 0) {
            hfix58j_request_absolute_seek(bookmark.frame);
            (void)hfix58f_execute_pending_seek(
                &stream,
                f,
                &v,
                &m2y0,
                &m2y0_prev,
                &m2y0_have_prev,
                frame,
                prev,
                fsz,
                &have_prev,
                &hfix51c_last_direct_yuv,
                &shown,
                &next_frame_tick,
                frame_ticks_abs);
        }
    }

    wait_stream_prebuffer(&stream);

    /* Reset per-playback feature state. */
    g_mivf_ab_a = MIVF_AB_UNSET;
    g_mivf_ab_b = MIVF_AB_UNSET;
    g_mivf_ab_state = 0;
    g_mivf_play_reached_eof = false;
    g_mivf_sleep_fired = false;
    if (g_mivf_settings.sleep_timer_min > 0) {
        g_mivf_sleep_deadline_tick = svcGetSystemTick() +
            (u64)SYSCLOCK_ARM11 * 60ull * (u64)g_mivf_settings.sleep_timer_min;
    } else {
        g_mivf_sleep_deadline_tick = 0;
    }

    while (aptMainLoop()) {
        u64 frame_start_tick = svcGetSystemTick();
        (void)frame_start_tick;

        u64 page_wait_us = 0;
        u64 parse_us = 0;
        u64 blit_us = 0;
        u32 diag_ring_kb = 0;
        u32 diag_page_no = 0;
        u32 diag_page_payload = 0;
        u16 diag_page_packets = 0;
        u32 diag_video_pkts = 0;
        u32 diag_audio_pkts = 0;
        /* HFIX51B: Direct present flag instantiation */
        bool hfix51b_direct_present_pending = false;

        hidScanInput();

        u32 h_keys_down = hidKeysDown();
        u32 h_keys_held = hidKeysHeld();
        u32 h_keys_up = hidKeysUp();

        bool hfix59r3_activity = (h_keys_down | h_keys_held | h_keys_up) != 0;
        bool hfix59r3_opened_settings = false;

        /* Sleep timer: pause playback once the configured deadline passes. */
        if (g_mivf_sleep_deadline_tick != 0 &&
            !g_mivf_sleep_fired &&
            g_media_ctl.state == STATE_PLAYING &&
            svcGetSystemTick() >= g_mivf_sleep_deadline_tick) {
            g_mivf_sleep_fired = true;
            g_mivf_sleep_deadline_tick = 0;
            g_media_ctl.state = STATE_PAUSED;
            if (audio_can_use_ndsp()) {
                ndspChnSetPaused(0, true);
            }
            hfix58_alert_set("SLEEP TIMER - PRESS KEY", 3);
        }

        /* While the sleep timer is firing, the next key press resumes (START exits). */
        if (g_mivf_sleep_fired && h_keys_down != 0) {
            if (!(h_keys_down & KEY_START)) {
                g_mivf_sleep_fired = false;
                g_media_ctl.state = STATE_PLAYING;
                if (audio_can_use_ndsp()) {
                    ndspChnSetPaused(0, false);
                }
                hfix58_alert_set("RESUMED", 1);
                h_keys_down = 0; /* consume the wake key */
            }
        }

        /* Keep frame pacing in sync with the current playback speed (menu/X). */
        frame_ticks_abs = base_frame_ticks * 100u / mivf_speed_pct();
        if (frame_ticks_abs == 0) frame_ticks_abs = 1;

        if (h_keys_down & KEY_START) {
            break;
        }

        if (h_keys_down & KEY_SELECT) {
            hfix59r3_set_settings_open(!g_hfix59r3_settings_visible);
            hfix59r3_activity = true;
            hfix59r3_opened_settings = true;
        }

        if (g_hfix59r3_settings_visible) {
            u32 settings_keys = h_keys_down;
            if (hfix59r3_opened_settings) {
                settings_keys &= ~KEY_SELECT;
            }

            hfix59r3_handle_settings_menu(settings_keys);
            hfix59r3_tick_idle(hfix59r3_activity);
        } else {
            /* HFIX60: R modifier — brightness (up/down) and chapter nav (left/right). */
            if (h_keys_held & KEY_R) {
                int brightness_step = 0;

                if (h_keys_down & KEY_DUP) {
                    brightness_step = 1;
                } else if (h_keys_down & KEY_DDOWN) {
                    brightness_step = -1;
                }

                if (brightness_step != 0) {
                    int b = (int)g_mivf_settings.active_brightness + brightness_step;
                    char m[24];

                    if (b < 1) b = 1;
                    if (b > 5) b = 5;

                    g_mivf_settings.active_brightness = (u32)b;
                    g_mivf_brightness_active = (u32)b;
                    hfix59r3_apply_screen_brightness(false);

                    snprintf(m, sizeof(m), "BRIGHTNESS %d", b);
                    hfix58_alert_set(m, 1);
                    MIVF_SettingsSave(&g_mivf_settings);
                }

                if (g_mivf_chapters_count > 0) {
                    if (h_keys_down & KEY_DLEFT) {
                        hfix60_chapter_jump(-1, shown);
                    } else if (h_keys_down & KEY_DRIGHT) {
                        hfix60_chapter_jump(1, shown);
                    }
                }

                /* Reserve direction keys so seek/transport do not also fire. */
                h_keys_down &= ~(KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT |
                                 KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT);
            }

            /* HFIX60: Y cycles the subtitle track (0..3) and reloads the sidecar. */
            if (h_keys_down & KEY_Y) {
                char m[28];

                g_mivf_settings.subtitle_track_index =
                    (g_mivf_settings.subtitle_track_index + 1u) % 4u;
                hfix58s_subtitles_load_for_video(MIVF_PATH);

                if (g_hfix58s_subtitles_ready) {
                    snprintf(m, sizeof(m), "SUBS TRACK %lu",
                        (unsigned long)g_mivf_settings.subtitle_track_index);
                } else {
                    snprintf(m, sizeof(m), "SUBS %lu (NONE)",
                        (unsigned long)g_mivf_settings.subtitle_track_index);
                }

                hfix58_alert_set(m, 1);
                MIVF_SettingsSave(&g_mivf_settings);
            }

            /* X cycles playback speed (0.5x .. 2.0x). Audio rate follows to stay in sync. */
            if (h_keys_down & KEY_X) {
                char m[24];
                u32 pct;

                g_mivf_settings.playback_speed_idx =
                    (g_mivf_settings.playback_speed_idx + 1u) % (u32)MIVF_SPEED_COUNT;
                pct = mivf_speed_pct();

                frame_ticks_abs = base_frame_ticks * 100u / pct;
                if (frame_ticks_abs == 0) frame_ticks_abs = 1;
                next_frame_tick = svcGetSystemTick() + frame_ticks_abs;

                if (audio_can_use_ndsp()) {
                    ndspChnSetRate(0, (float)audio.rate * (float)pct / 100.0f);
                }

                snprintf(m, sizeof(m), "SPEED %lu.%02lux",
                    (unsigned long)(pct / 100u), (unsigned long)(pct % 100u));
                hfix58_alert_set(m, 1);
                MIVF_SettingsSave(&g_mivf_settings);
            }

            /* B cycles the A/B scene looper: set A -> set B (loop on) -> clear. */
            if (h_keys_down & KEY_B) {
                char m[24];

                if (g_mivf_ab_state == 0) {
                    g_mivf_ab_a = shown;
                    g_mivf_ab_state = 1;
                    snprintf(m, sizeof(m), "LOOP A @ %lu", (unsigned long)shown);
                } else if (g_mivf_ab_state == 1) {
                    if (shown > g_mivf_ab_a + 1u) {
                        g_mivf_ab_b = shown;
                        g_mivf_ab_state = 2;
                        snprintf(m, sizeof(m), "LOOP ON %lu-%lu",
                            (unsigned long)g_mivf_ab_a, (unsigned long)g_mivf_ab_b);
                    } else {
                        g_mivf_ab_a = shown;
                        snprintf(m, sizeof(m), "LOOP A @ %lu", (unsigned long)shown);
                    }
                } else {
                    g_mivf_ab_a = MIVF_AB_UNSET;
                    g_mivf_ab_b = MIVF_AB_UNSET;
                    g_mivf_ab_state = 0;
                    snprintf(m, sizeof(m), "LOOP OFF");
                }

                hfix58_alert_set(m, 1);
            }

            hfix58b_transport_handle_input(h_keys_down, h_keys_held);
            hfix58d_notify_input(h_keys_down, h_keys_held);
            hfix58j_touch_scrub_update(h_keys_down, h_keys_held, h_keys_up);
            /* HFIX57A_INPUT_REPAIR */

            hfix56_audio_controls_on_input(h_keys_down, h_keys_held);

            /*
                HFIX58A_R5_CONSUME_L_DPAD:
                when L is held, D-pad is reserved for audio controls.
            */
            if (h_keys_held & KEY_L) {
                h_keys_down &= ~(KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT);
            }


            /*
                HFIX57A input repair:
                When L is held, D-pad is reserved for audio controls.
                This prevents transport LEFT/RIGHT dummy highlights from firing.
            */
            if (h_keys_held & KEY_L) {
                h_keys_down &= ~(KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT);
            }


            /*
                HFIX57A inject touch transport keys:
                touch controls synthesize KEY_A/KEY_LEFT/KEY_RIGHT so existing
                pause/audio/transport logic remains centralized.
            */
            u32 hfix57_touch_keys = hfix57_touch_transport_to_keys(h_keys_down, h_keys_held);
            h_keys_down |= hfix57_touch_keys;

            if (h_keys_down & KEY_START) {
                break;
            }

            if (h_keys_down & KEY_A) {
                g_media_ctl.dummy_seek_state = 0;

                if (g_media_ctl.state == STATE_PLAYING) {
                    g_media_ctl.state = STATE_PAUSED;
                    hfix58_alert_set("PAUSED", 2);

                    if (audio_can_use_ndsp()) {
                        ndspChnSetPaused(0, true);
                    }
                } else {
                    g_media_ctl.state = STATE_PLAYING;
                    hfix58_alert_set("PLAYING", 1);

                    if (audio_can_use_ndsp()) {
                        ndspChnSetPaused(0, false);
                    }
                }
            }

            /* HFIX58F_REQUEST_SEEK_KEYS */
            if (h_keys_down & KEY_LEFT) {
                g_media_ctl.dummy_seek_state = -1;
                hfix58f_request_relative_seek(-HFIX58F_SEEK_STEP_FRAMES);
            } else if (h_keys_down & KEY_RIGHT) {
                g_media_ctl.dummy_seek_state = 1;
                hfix58f_request_relative_seek(HFIX58F_SEEK_STEP_FRAMES);
            }

            hfix59r3_tick_idle(hfix59r3_activity);
        }

        g_media_ctl.current_frame_idx = shown;

        /* Execute seeks before the pause redraw gate so paused scrubbing shows the target frame. */
        if (hfix58f_execute_pending_seek(
                &stream,
                f,
                &v,
                &m2y0,
                &m2y0_prev,
                &m2y0_have_prev,
                frame,
                prev,
                fsz,
                &have_prev,
                &hfix51c_last_direct_yuv,
                &shown,
                &next_frame_tick,
                frame_ticks_abs)) {
            continue;
        }

        /*
            HFIX51C pause gate:
            Do not consume stream pages while paused. Redraw the last known
            presentation source safely.
        */
        if ((g_media_ctl.state == STATE_PAUSED || g_mivf_anim.is_touch_scrubbing) &&
            !g_hfix58f_seek_catchup_active &&
            !g_hfix58f_seek_preview_decode_pending) {
            if (hfix51c_last_direct_yuv &&
                m2y0_have_prev &&
                m2y0.w == TOP_W &&
                m2y0.h == TOP_H) {
                m2y0_to_top_rgb565_direct(&m2y0);
            } else {
                hfix51c_last_direct_yuv = false;
                blit565_scaled(frame, v.w, v.h);
            }

            gspWaitForVBlank();
            continue;
        }

        MivfPageView page;

        u64 page_t0 = svcGetSystemTick();

        if (!mivf_stream_next_page(&stream, &page)) {
            /* HFIX58D: scrubbed full bottom-console printf statement. */
            g_mivf_play_reached_eof = true;
            break;
        }

        u64 page_t1 = svcGetSystemTick();
        page_wait_us = ticks_to_us(page_t1 - page_t0);

        MivfPage pg = page.pg;
        u8 *cur_page = page.data;

        diag_page_no = pg.no;
        diag_page_payload = pg.payload;
        diag_page_packets = pg.packets;

        size_t off = 0;
        bool got_video = false;
        bool audio_prequeued = false;

        u64 parse_t0 = svcGetSystemTick();

        /*
            Queue audio before video decode/presentation. The muxer writes video
            first, then audio, but waiting to submit audio until after frame
            pacing makes NDSP vulnerable to underruns whenever video decode is
            uneven. A fast prepass gives the audio queue one frame of cushion.
        */
        if (audio.ready && !g_hfix58f_seek_catchup_active && !g_mivf_anim.is_touch_scrubbing) {
            size_t audio_off = 0;

            for (u16 ai = 0; ai < pg.packets; ai++) {
                Packet ak;

                if (read_packet(cur_page + audio_off, pg.payload - audio_off, &ak)) {
                    break;
                }

                if ((u64)audio_off + (u64)ak.hsize + (u64)ak.psize > (u64)pg.payload) {
                    break;
                }

                if (ak.sid == audio.sid) {
                    const u8 *abody = cur_page + audio_off + ak.hsize;

                    diag_audio_pkts++;
                    if (hfix58_queue_audio_packet(&a, abody, ak.psize, ak.frame)) {
                        audio_prequeued = true;
                    }
                }

                audio_off += ak.hsize + ak.psize;
            }
        }

        for (u16 i = 0; i < pg.packets; i++) {
            Packet k;

            if (read_packet(cur_page + off, pg.payload - off, &k)) {
                /* HFIX58D: scrubbed full bottom-console printf statement. */
                break;
            }

            if ((u64)off + (u64)k.hsize + (u64)k.psize > (u64)pg.payload) {
                break;
            }

            const u8 *body = cur_page + off + k.hsize;

            if (k.sid == v.id) {
                bool display_video_packet = true;

                diag_video_pkts++;
                if (!strcmp(v.codec, "RAWV")) {
                    if (k.psize == fsz) {
                        memcpy(frame, body, fsz);
                    }
                } else if (k.psize >= 4 &&
                           body[0] == 'M' &&
                           body[1] == '2' &&
                           body[2] == 'Y' &&
                           body[3] == '1') {
                    /*
                        HFIX21:
                        Decode compressed M2Y1 YUV420 packet, then convert
                        into the existing RGB565 frame buffer.
                    */
                    if (!m2y0_ready) {
                        /* HFIX58D: scrubbed full bottom-console printf statement. */
                    } else {
                        int r = dec_m2y1(body, k.psize, &m2y0, &m2y0_prev, m2y0_have_prev);

                        if (r) {
                            /* HFIX58D: scrubbed full bottom-console printf statement. */

                            if (have_prev) {
                                memcpy(frame, prev, fsz);
                            } else {
                                memset(frame, 0, fsz);
                            }
                        } else {
                            m2y0_frame_copy(&m2y0_prev, &m2y0);
                            m2y0_have_prev = true;
                            g_m2y1_deblock_this_frame = true;
                            hfix51b_direct_present_pending = true;
                            hfix51c_last_direct_yuv = true;
                        }
                    }
                } else if (k.psize >= 4 &&
                           body[0] == 'M' &&
                           body[1] == '2' &&
                           body[2] == 'Y' &&
                           body[3] == '2') {
                    /*
                        M2Y2: range-coded M2Y1 payload. Decode is identical to
                        M2Y1 after the entropy backend, including deblock.
                    */
                    if (!m2y0_ready) {
                        /* not ready */
                    } else {
                        int r = dec_m2y2(body, k.psize, &m2y0, &m2y0_prev, m2y0_have_prev);

                        if (r) {
                            if (have_prev) {
                                memcpy(frame, prev, fsz);
                            } else {
                                memset(frame, 0, fsz);
                            }
                        } else {
                            m2y0_frame_copy(&m2y0_prev, &m2y0);
                            m2y0_have_prev = true;
                            g_m2y1_deblock_this_frame = true;
                            hfix51b_direct_present_pending = true;
                            hfix51c_last_direct_yuv = true;
                        }
                    }
                } else if (k.psize >= 4 &&
                           body[0] == 'M' &&
                           body[1] == '2' &&
                           body[2] == 'Y' &&
                           body[3] == '0') {
                    /*
                        HFIX20:
                        Decode raw M2Y0 YUV420 packet, then immediately
                        convert into the existing RGB565 frame buffer.
                    */
                    if (!m2y0_ready) {
                        /* HFIX58D: scrubbed full bottom-console printf statement. */
                    } else {
                        int r = dec_m2y0_raw(body, k.psize, &m2y0);

                        if (r) {
                            /* HFIX58D: scrubbed full bottom-console printf statement. */

                            if (have_prev) {
                                memcpy(frame, prev, fsz);
                            } else {
                                memset(frame, 0, fsz);
                            }
                        } else {
                            m2y0_frame_copy(&m2y0_prev, &m2y0);
                            m2y0_have_prev = true;
                            hfix51b_direct_present_pending = true;
                            hfix51c_last_direct_yuv = true;
                        }
                    }
                } else if (k.psize >= 4 &&
                           body[0] == 'M' &&
                           body[1] == '1' &&
                           body[2] == 'P') {
                    /*
                        HFIX14 defensive decode:
                        Start from a sane baseline before applying block
                        updates. If the decoder leaves any block untouched,
                        it will show the previous frame instead of stale malloc
                        data or stale alternate-buffer data.
                    */
                    if (have_prev) {
                        memcpy(frame, prev, fsz);
                    } else {
                        memset(frame, 0, fsz);
                    }

                    int r = 0;

                    /*
                        HFIX16:
                        M1P1 is M1P0 plus byte-aligned RLE tokens.
                        M1P0 remains fully supported.
                    */
                    if (body[3] == '1') {
                        r = dec_m1p1(body, k.psize, frame, prev, have_prev, v.w, v.h);
                    } else {
                        r = dec_m1p0(body, k.psize, frame, prev, have_prev, v.w, v.h);
                    }

                    if (r) {
                        /* HFIX58D: scrubbed full bottom-console printf statement. */

                        /*
                            Never present a partially decoded corrupted frame.
                            Keep playback cadence by presenting the previous
                            known-good frame or black on the first frame.
                        */
                        if (have_prev) {
                            memcpy(frame, prev, fsz);
                        } else {
                            memset(frame, 0, fsz);
                        }
                    }
                }

                if (g_hfix58f_seek_catchup_active) {
                    if (k.frame < g_hfix58f_seek_catchup_target) {
                        shown = k.frame + 1u;
                        g_media_ctl.current_frame_idx = g_hfix58f_seek_catchup_target;
                        display_video_packet = false;
                    } else {
                        g_hfix58f_seek_catchup_active = false;
                        shown = k.frame;
                        next_frame_tick = svcGetSystemTick();
                    }
                }

                if (display_video_packet) {
                    got_video = true;
                }
            } else if (!g_hfix58f_seek_catchup_active &&
                       !g_mivf_anim.is_touch_scrubbing &&
                       !audio_prequeued &&
                       audio.ready &&
                       k.sid == audio.sid) {
                diag_audio_pkts++;
                hfix58_queue_audio_packet(&a, body, k.psize, k.frame);
            }

            off += k.hsize + k.psize;
        }

        u64 parse_t1 = svcGetSystemTick();
        parse_us = ticks_to_us(parse_t1 - parse_t0);

        mivf_stream_release_page(&stream, &page);

        if (got_video) {
            u64 blit_t0 = svcGetSystemTick();

            hfix59r3_present_video_frame(
                &v,
                &m2y0,
                &m2y0_have_prev,
                &frame,
                &prev,
                fsz,
                &have_prev,
                hfix51b_direct_present_pending,
                &hfix51c_last_direct_yuv,
                &shown,
                &next_frame_tick,
                frame_ticks_abs,
                fpsn_abs,
                fpsd_abs);

            g_hfix58f_seek_preview_decode_pending = false;
            blit_us = ticks_to_us(svcGetSystemTick() - blit_t0);

            if (g_mivf_ab_state == 2 &&
                g_mivf_ab_b != MIVF_AB_UNSET &&
                g_mivf_ab_a != MIVF_AB_UNSET &&
                shown >= g_mivf_ab_b) {
                hfix58j_request_absolute_seek(g_mivf_ab_a);
            }
        }

        if (g_mivf_diag && got_video) {
            diag_ring_kb = stream.ring.fill >> 10;
            u64 total_us = ticks_to_us(svcGetSystemTick() - frame_start_tick);

            fprintf(g_mivf_diag,
                "%lu,%lu,%lu,%u,%lu,%llu,%llu,%llu,%llu,%lu,%lu,%lu,%lu,%lu,%lu\n",
                (unsigned long)shown,
                (unsigned long)diag_page_no,
                (unsigned long)diag_page_payload,
                (unsigned int)diag_page_packets,
                (unsigned long)diag_ring_kb,
                (unsigned long long)page_wait_us,
                (unsigned long long)parse_us,
                (unsigned long long)blit_us,
                (unsigned long long)total_us,
                (unsigned long)diag_video_pkts,
                (unsigned long)diag_audio_pkts,
                (unsigned long)g_last_audio_bytes,
                (unsigned long)g_last_audio_samples,
                (unsigned long)g_audio_submit,
                (unsigned long)g_audio_drop);

            if ((shown & 31u) == 0u) {
                fflush(g_mivf_diag);
            }
        }

    }

    g_media_ctl.state = STATE_PLAYING;
    g_media_ctl.current_frame_idx = 0;
    g_media_ctl.dummy_seek_state = 0;

    m2y0_frame_free(&m2y0);
    free(frame);
    free(prev);

    /*
        Strict cleanup order:
        1. Stop/join background stream reader.
        2. Shut down audio.
        3. Close FILE* after reader thread is gone.
    */
    mivf_stream_close(&stream);
    hfix52a_y2r_shutdown();
    audio_shutdown();
    hfix58s_subtitles_unload();

    if (g_mivf_settings.resume_enabled && !g_mivf_play_reached_eof) {
        MIVF_BookmarkSave(MIVF_PATH, shown);
    } else {
        MIVF_BookmarkClear(MIVF_PATH);
    }

    fclose(f);

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Main                                                                       */
/* ------------------------------------------------------------------------- */

int main(void) {
    gfxInitDefault();
    ptmuInit();
    aptInit();
    gspLcdInit();
    gfxSetScreenFormat(GFX_TOP, GSP_RGB565_OES);
    gfxSetScreenFormat(GFX_BOTTOM, GSP_RGB565_OES);

    /* HFIX58D: bottom console disabled; RGB565 fluent UI owns bottom framebuffer. */
    /* consoleInit(GFX_BOTTOM, NULL); */
    mivf_log_open();
    mivf_diag_open();

    app_audio_system_init();

    MIVF_SettingsInit(&g_mivf_settings);
    MIVF_SettingsLoad(&g_mivf_settings);
    MIVF_SettingsClamp(&g_mivf_settings);
    g_mivf_settings_loaded = true;
    g_mivf_brightness_active = 5u;
    aptHook(&g_mivf_apt_hook, hfix59r3_apt_hook, NULL);
    hfix59r3_apply_runtime_settings();
    GSPLCD_PowerOnAllBacklights();
    hfix59r3_apply_screen_brightness(false);

    /* HFIX58D: scrubbed full bottom-console printf statement. */
    /* HFIX58D: scrubbed full bottom-console printf statement. */

    while (aptMainLoop()) {
        /*
            HFIX58A_R5_BROWSER_BEFORE_PLAY:
            play() is no-argument on this branch and reads MIVF_PATH.
            MIVF_PATH is redirected to g_hfix58_selected_media above.
        */
        if (!g_hfix58_has_selected_media) {
            if (!hfix58_file_browser_select(g_hfix58_selected_media, sizeof(g_hfix58_selected_media))) {
                break;
            }

            g_hfix58_has_selected_media = true;
        }

        if (hfix58_media_kind(MIVF_PATH) == HFIX58_MEDIA_MOFLEX) {
            MoflexResult result = play_moflex_selected_media(MIVF_PATH);

            if (result == MOFLEX_QUIT_EXIT) {
                break;
            }

            g_hfix58_has_selected_media = false;
            continue;
        }

        int r = play();

        if (r == 1) {
            break;
        }

        /*
            Auto-advance: when a file ends on its own and the playlist option is
            enabled, jump straight into the next file in the same folder.
        */
        if (g_mivf_play_reached_eof && g_mivf_settings.auto_advance) {
            char next_path[HFIX58_MAX_PATH];

            if (mivf_find_next_in_folder(MIVF_PATH, next_path, sizeof(next_path))) {
                snprintf(g_hfix58_selected_media, sizeof(g_hfix58_selected_media), "%s", next_path);
                g_hfix58_has_selected_media = true;
                continue;
            }
        }

        /*
            Otherwise return to the file browser so the user can choose what to
            watch next. Exiting the app happens from the browser (START / B).
        */
        g_hfix58_has_selected_media = false;
    }

    MIVF_SettingsSave(&g_mivf_settings);
    audio_shutdown();
    app_audio_system_shutdown();
    gspLcdExit();
    aptExit();
    ptmuExit();
    mivf_diag_close();
    mivf_log_close();
    gfxExit();
    return 0;
}
