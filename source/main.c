#include <3ds.h>
#include <sys/stat.h>

/* ------------------------------------------------------------------------- */
/* HFIX58J_RETAIL_UX_AND_IO                                                   */
/* Version: MIVF Phase 5G Retail UX HFIX58J                                   */
/* ------------------------------------------------------------------------- */

/* HFIX58J-R1 FORWARD PROTOTYPES */
static void hfix58_rect565(u8 *fb, int x, int y, int w, int h, int r, int g, int b);
static void hfix58_rect565_top(u8 *fb, int x, int y, int w, int h, int r, int g, int b);
static void hfix58_blend_rect565(u8 *fb, int x, int y, int w, int h, int r, int g, int b, int a);
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
#include "mivf_customization.h"
#include "hfix56_gain.h"
#include "mivf_series_parse.h"
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

/*
    HFIX72: fflush() on a FILE* backed by the SD card is a real synchronous
    stall on 3DS hardware -- measured at ~20ms for a single call (via the
    avsync diagnostics: two svcGetSystemTick() calls with only one printf
    between them, ~5.3M ticks apart at 268MHz). tee_printf used to fflush on
    every single call, codebase-wide. During timeline scrubbing in particular,
    hfix58j_request_preview_seek fires repeatedly as the user drags, and each
    one printfs (idx: find, audio_delay_ring, etc.) -- turning one scrub
    gesture into a burst of ~20ms stalls.

    That matters for A/V sync specifically: hfix59r3_present_video_frame's
    "late" catchup branch silently jumps next_frame_tick forward past any
    stall of 2+ frame durations, permanently absorbing it -- but NDSP audio
    is hardware-clocked and does not jump forward the same way. So a printf
    burst leaves video's presentation clock permanently ahead of audio's
    real playback position, which matches the reported symptom exactly:
    sync is fine at the start, and drifts behind after scrubbing/seeking
    around. This mirrors the exact tradeoff hfix59r3_set_settings_open
    already documents for settings saves ("saving on every value change +
    held-key repeat causes SD I/O stalls") -- same fix here: flush
    periodically instead of on every call. Still durable enough for
    post-crash log review (loses at most this many trailing lines, plus
    mivf_log_close() always flushes on clean shutdown).
*/
#define TEE_PRINTF_FLUSH_EVERY 16

static int tee_printf(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    va_end(ap);

    if (g_mivf_log) {
        static u32 tee_printf_calls_since_flush = 0;

        va_start(ap, fmt);
        vfprintf(g_mivf_log, fmt, ap);
        va_end(ap);

        tee_printf_calls_since_flush++;
        if (tee_printf_calls_since_flush >= TEE_PRINTF_FLUSH_EVERY) {
            fflush(g_mivf_log);
            tee_printf_calls_since_flush = 0;
        }
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
static bool g_hfix62_help_visible = false;
static int g_hfix62_help_scroll = 0;

static MivfSettings g_mivf_settings;
static bool g_mivf_settings_loaded = false;
/* MIVF_PHASE6_PERSIST_VOLUME_V1: declared with the settings globals so the
   runtime-settings synchronizer can initialize it before playback starts. */
static int g_hfix56_volume_percent = 100;
/* P.A1: independent left/right channel attenuation, 0..100, attenuation
   only. Declared alongside g_hfix56_volume_percent for the same reason --
   synced from settings by hfix59r3_sync_runtime_settings() before
   playback starts. */
static int g_hfix56_left_gain_percent = 100;
static int g_hfix56_right_gain_percent = 100;
static int g_hfix59r3_settings_index = 0;
static u32 g_mivf_idle_frames = 0;
static bool g_mivf_brightness_dimmed = false;
static u32 g_mivf_brightness_active = 5;
static aptHookCookie g_mivf_apt_hook;

/* HFIX60: built-in theme accent (selected by g_mivf_settings.theme_index). */
static u8 g_mivf_theme_r = 70;
static u8 g_mivf_theme_g = 120;
static u8 g_mivf_theme_b = 210;
typedef enum {
    MIVF_CVD_STANDARD = 0,
    MIVF_CVD_PROTAN,
    MIVF_CVD_DEUTAN,
    MIVF_CVD_TRITAN,
    MIVF_CVD_MONOCHROME,
    MIVF_CVD_HIGH_CONTRAST,
    MIVF_CVD_COUNT
} MivfColorVisionMode;

typedef struct {
    u8 accent_r, accent_g, accent_b;
    u8 light_r, light_g, light_b;
    u8 soft_r, soft_g, soft_b;
    u8 dark_r, dark_g, dark_b;
    u8 on_r, on_g, on_b;
    u8 info_r, info_g, info_b;
    u8 success_r, success_g, success_b;
    u8 warning_r, warning_g, warning_b;
    u8 danger_r, danger_g, danger_b;
    u8 chapter_r, chapter_g, chapter_b;
    u8 loop_a_r, loop_a_g, loop_a_b;
    u8 loop_b_r, loop_b_g, loop_b_b;
    bool strong_focus;
    bool monochrome;
    bool reduce_transparency;
} MivfThemePalette;
static MivfThemePalette g_mivf_theme_palette;
static u32 g_mivf_theme_generation = 1;

typedef struct {
    bool active;
    u32 original_mode;
    u32 preview_mode;
} MivfCvdPicker;
static MivfCvdPicker g_mivf_cvd_picker;

typedef struct {
    bool active;
    u32 original_style;
    u32 preview_style;
    /* MIVF_TRANSPORT_SHOWCASE_DEMO_V1 */
    bool demo_active;
    u64 demo_start_tick;
    u32 demo_last_scene;
    /* MIVF_AUTOTEST_LOG_V1: last values emitted to the diagnostic log. */
    u32 demo_last_control;
    u32 demo_last_log_second;
    u32 saved_color_mode, saved_theme_index;
    bool saved_custom;
    u8 saved_r, saved_g, saved_b;
} MivfTransportPicker;
static MivfTransportPicker g_mivf_transport_picker;

typedef struct {
    bool active;
    u8 original_r, original_g, original_b;
    bool original_custom;
    u32 original_index;
    int hue, sat, val;
} MivfThemePicker;
static MivfThemePicker g_mivf_theme_picker;

/* HFIX60: chapter markers loaded from a ".chapters" sidecar next to the video. */
#define MIVF_CHAP_MAX 64
typedef struct {
    u32 frame;
    char label[40];
} MivfChapter;
static MivfChapter g_mivf_chapters[MIVF_CHAP_MAX];
static int g_mivf_chapters_count = 0;

/* hfix84: chapter thumbnails, loaded from a ".chapthumbs" sidecar (see
   tools/mivf_build_chapter_thumbs.py) -- generated once on a PC from the
   original source video at each chapter's timestamp, not decoded on-device.
   Deliberately no player-side decode-and-downscale path: this codebase has
   already hit real, hard-to-diagnose-without-hardware issues from clever
   framebuffer/decode tricks, and a multi-chapter on-device decode would
   also risk a multi-minute Scene Selection load on a long movie (catchup
   decode from the nearest seekpoint per chapter). Pre-rendering on a PC
   matches how real DVD authoring generates scene-selection thumbnails in
   the first place -- at author time, not at playback time. */
#define MIVF_CHAPTHUMB_W 96
#define MIVF_CHAPTHUMB_H 54
static u16 g_mivf_chapthumbs[MIVF_CHAP_MAX * MIVF_CHAPTHUMB_W * MIVF_CHAPTHUMB_H];
static int g_mivf_chapthumbs_count = 0;

/* HFIX66: DVD-style MIVF Menu Packs.
   A ".menu.ini" sidecar next to a movie switches its browser-selection entry
   point from immediate playback to a simple DVD-menu-style screen (Play /
   Resume / Chapters / Back). This is intentionally NOT a DVD VM: no ISO/
   VIDEO_TS/IFO parsing happens on the 3DS, no button-VM commands, no
   subpicture overlays -- just a small hand-rolled INI describing button
   rects/labels/actions, rendered with the existing RGB565 draw helpers.
   Chapters reuses the existing ".chapters" sidecar and hfix60_chapters_load
   verbatim (called once here for labels only, and again later inside play()
   with the real per-file fps for the actual seek target). */
#define MIVF_MENU_MAX_BUTTONS 8
#define MIVF_MENU_LABEL_MAX 32
#define MIVF_MENU_ID_MAX 16
#define MIVF_MENU_TITLE_MAX 64
/* hfix84: 6 per page (2x3 thumbnail grid) now, not a text-row count --
   still the single knob for both pagination math and the on-screen layout
   (grid when thumbnails are loaded, plain list otherwise), same as before. */
#define MIVF_MENU_CHAPTERS_VISIBLE_ROWS 6

typedef enum {
    MIVF_MENU_ACTION_NONE = 0,
    MIVF_MENU_ACTION_PLAY,
    MIVF_MENU_ACTION_RESUME,
    MIVF_MENU_ACTION_CHAPTERS,
    MIVF_MENU_ACTION_BACK
} MivfMenuAction;

typedef struct {
    char id[MIVF_MENU_ID_MAX];
    char label[MIVF_MENU_LABEL_MAX];
    int x, y, w, h;
    MivfMenuAction action;
    bool enabled;
} MivfMenuButton;

typedef struct {
    bool valid;
    char title[MIVF_MENU_TITLE_MAX];
    bool has_background;
    int button_count;
    int selected;
    MivfMenuButton buttons[MIVF_MENU_MAX_BUTTONS];
    char movie_path[HFIX58_MAX_PATH]; /* for the last-selection memory (hfix81) */
    bool has_resume_progress;         /* hfix79: valid bookmark_frame/total_frames below */
    u32 resume_bookmark_frame;
    u32 resume_total_frames;
    bool has_screensaver_image;       /* hfix85: custom bounce image loaded into g_mivf_screensaver_img */
} MivfMenu;

typedef enum {
    MIVF_MENU_RESULT_PLAY = 0,
    MIVF_MENU_RESULT_BACK
} MivfMenuResult;

/* One-shot launch directive consumed by play() and reset to DEFAULT right
   after use, so auto-advance/normal browser selection is never affected by
   a previous menu-driven launch. */
typedef enum {
    MIVF_LAUNCH_DEFAULT = 0,
    MIVF_LAUNCH_START_OVER,
    MIVF_LAUNCH_RESUME,
    MIVF_LAUNCH_CHAPTER
} MivfLaunchMode;

static MivfLaunchMode g_mivf_launch_mode = MIVF_LAUNCH_DEFAULT;
static int g_mivf_launch_chapter_index = -1;

/* hfix81: remember the last-highlighted root button / chapter row per movie,
   in-session only (resets on app restart, like most DVD players' menu
   cursor memory) -- not written to settings.ini, since this is transient UI
   state, not a user preference. Keyed by movie_path so switching files (or
   coming back to one after browsing others) doesn't show a stale cursor
   position from an unrelated title. */
static char g_mivf_menu_last_path[HFIX58_MAX_PATH] = {0};
static int  g_mivf_menu_last_button = -1;
static bool g_mivf_menu_last_in_chapters = false;
static int  g_mivf_menu_last_chapter = 0;

/* Raw RGB565 top-screen-sized (400x240) still background, distinct from the
   browser's small preview ".cover" thumbnail (88x50) -- different extension,
   so the two never collide on the same base filename. */
#define MIVF_MENU_BG_W TOP_W
#define MIVF_MENU_BG_H TOP_H
static u16 g_mivf_menu_bg[MIVF_MENU_BG_W * MIVF_MENU_BG_H];

/* hfix85: idle-menu "bouncing logo" screensaver, classic DVD-player style --
   corner-hunting included for free, since that's just what the physics do.
   Bounces an original MIVF mark by default; if a ".screensaver.cover"
   sidecar exists next to the movie (same raw-RGB565, no-header convention
   as ".menu_bg.cover"), that image bounces instead. This is deliberately
   how a user could bounce their own 3DS logo image (or anything else) --
   this codebase doesn't ship a reproduction of Nintendo's trademarked logo
   itself, but the mechanism to bounce *any* image the user supplies is
   exactly this. */
#define MIVF_SCREENSAVER_W 96
#define MIVF_SCREENSAVER_H 54
static u16 g_mivf_screensaver_img[MIVF_SCREENSAVER_W * MIVF_SCREENSAVER_H];

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

/*
    Lifecycle-safe bookmark checkpoint request. aptHook fires from a
    context this codebase has not verified is safe for filesystem I/O
    (see bookmark_hardening_20260718_071900 evidence dir), so ONSUSPEND/
    ONSLEEP/ONEXIT only set this flag -- they never call
    MIVF_BookmarkSave directly. play()'s own loop, a context already
    proven safe for bookmark I/O (the existing post-loop save has always
    run from there), notices the flag and performs the actual save.

    Audited (audit_correction_20260718_081500) and CORRECTED from an
    earlier, wrong claim here: this flag's consumer sits inside the
    while(aptMainLoop()) loop body, and that same aptMainLoop() call
    does not return -- so the consumer does not run -- until the app is
    either resumed or told to exit. This flag therefore does NOT protect
    the "suspended and the console never comes back" case (battery dies
    while suspended, or force-quit from the HOME task-switcher without
    resuming): a request set at ONSUSPEND/ONSLEEP is provably only
    consumed after a successful wake, and a request set at ONEXIT is
    provably never consumed at all (see that case's own comment). The
    real, narrower value: a fresh checkpoint on every successful resume
    (protects against a later failure after that point), and the
    separate periodic-checkpoint mechanism below, which is the only
    thing that can actually bound loss during a suspend that never ends.
*/
static bool g_mivf_bookmark_checkpoint_requested = false;

/*
    Player Touch Lock. Session-only by design (never persisted to
    settings.ini) -- reset at every play() entry alongside the other
    per-session state above, so it never surprises a user after a new
    title, a library return, or a relaunch. Gated into the narrowest
    existing touch-dispatch point, hfix57_touch_transport_to_keys's
    existing modal-state early-return list -- does not touch touch
    calibration, seeking, the audio clock, NDSP, decoding, scheduling,
    frame presentation, subtitle timing, or bookmark identity.

    Lock/unlock is the same physical-button gesture (a toggle): hold
    KEY_L and KEY_R together for MIVF_TOUCH_LOCK_HOLD_FRAMES. Chosen
    because holding both simultaneously is otherwise inert during
    playback -- each is individually a D-pad modifier (KEY_R:
    brightness/chapter-nav, KEY_L: audio controls), and neither does
    anything on its own without a D-pad press alongside it (main.c's
    existing HFIX60/HFIX58A_R5/HFIX57A input-repair blocks) -- confirmed
    by source read, not assumed, before picking this gesture.
*/
#define MIVF_TOUCH_LOCK_HOLD_FRAMES 90u /* ~1.5s at 60fps */
static bool g_mivf_touch_locked = false;
static u32 g_mivf_touch_lock_hold_frames = 0;
static bool g_mivf_touch_lock_gesture_fired = false;
/* hfix_touch_lock_update() itself is defined later (after
   hfix58_alert_set's forward declaration, which it calls) -- see
   the definition near hfix58_alert_set for why. */
static void hfix_touch_lock_update(u32 keys_held);

/*
    Periodic in-loop checkpoint: bounds how much progress a genuine
    crash or hang can lose, independent of any lifecycle event (a
    suspend/sleep/exit hook never fires for a crash or a hang -- there
    is no notification to hang a flag off). Deliberately a separate
    mechanism from g_mivf_bookmark_checkpoint_requested above rather
    than folded into it: one is time-driven and runs only while
    actively playing, the other is event-driven and can fire while
    paused too. Kept as two clearly separate triggers per the
    established discipline for this subsystem (see
    bookmark_hardening_20260718_071900 and
    lifecycle_checkpoint_20260718_075200 evidence dirs) rather than
    merged just because they end in the same save call.
*/
#define MIVF_PERIODIC_CHECKPOINT_INTERVAL_SEC 20u
static u64 g_mivf_next_periodic_checkpoint_tick = 0;

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
static u32 g_audio_wait_events = 0; /* audio_queue_raw_ndsp had to gspWaitForVBlank at least once */
static u32 g_audio_submit_diag_count = 0; /* HFIX86: how many submitted-buffer content lines logged */

/* Integer sqrt of a u64, for the HFIX86 rms diagnostic -- avoids pulling in
   <math.h>/float sqrt for a single bounded-count use. */
static u32 hfix_isqrt64(u64 v) {
    u64 x = v, r = 0, b = 1ull << 62;
    while (b > x) b >>= 2;
    while (b) {
        if (x >= r + b) { x -= r + b; r = (r >> 1) + b; }
        else { r >>= 1; }
        b >>= 2;
    }
    return (u32)r;
}
static u32 g_last_audio_bytes = 0;
static u32 g_last_audio_samples = 0;
/* Receiver UI stereo meters. Values are measured from the exact post-mix
   PCM16 packets submitted to NDSP, so the display follows audible program
   material rather than a decorative timer. Range is 0..32767. */
static u32 g_audio_meter_left = 0;
static u32 g_audio_meter_right = 0;

/* ------------------------------------------------------------------------- */
/* HFIX88_GENERATION_SAFE_AUDIO_CLOCK                                         */
/*                                                                           */
/* Foundation only: this does not alter pacing. It makes the NDSP cursor safe */
/* enough to become a future master clock by rejecting stale modulo-map hits, */
/* reset-generation collisions, and seq/sample reads that straddle a buffer. */
/* ------------------------------------------------------------------------- */
#define AUDIO_SEQ_MAP 256   /* power of two; > maximum simultaneously in flight */

typedef struct {
    bool valid;
    u16 sequence_id;       /* exact ID, not merely sequence_id % AUDIO_SEQ_MAP */
    u32 generation;        /* incremented whenever channel 0 is reset */
    u32 media_frame;       /* MIVF frame carried by this wavebuf */
    u32 nsamples;          /* PCM sample frames in this wavebuf */
    u64 media_sample_start;/* nominal media sample timestamp of media_frame */
} AudioClockMapEntry;

typedef struct {
    u16 sequence_id;
    u32 generation;
    u32 media_frame;
    u32 nsamples;
    u32 sample_pos;
    u64 media_sample;
} AudioClockSnapshot;

static AudioClockMapEntry g_audio_clock_map[AUDIO_SEQ_MAP];
static u32  g_audio_clock_generation = 1;
static u32  g_audio_pending_submit_frame = 0; /* packet being submitted now */
static u16  g_audio_last_submitted_seq = 0;
static bool g_audio_have_submitted_seq = false;

static u64 audio_clock_frame_to_sample(u32 frame) {
    u32 fpsn = g_hfix59r2_video_fps_num ? g_hfix59r2_video_fps_num : 0u;
    u32 fpsd = g_hfix59r2_video_fps_den ? g_hfix59r2_video_fps_den : 1u;

    if (audio.rate > 0 && fpsn > 0) {
        return ((u64)frame * (u64)audio.rate * (u64)fpsd) / (u64)fpsn;
    }
    return (u64)frame * (u64)audio.samples_per_frame;
}

static void audio_clock_new_generation(const char *reason) {
    g_audio_clock_generation++;
    if (g_audio_clock_generation == 0) {
        g_audio_clock_generation = 1;
    }
    memset(g_audio_clock_map, 0, sizeof(g_audio_clock_map));
    g_audio_have_submitted_seq = false;
    g_audio_last_submitted_seq = 0;
    /* MIVF_RECEIVER_AUDIO_REACTIVE_METERS_V1: a generation reset means the audio
       channel was just (re)configured, seeked, or shut down. Drop the Receiver
       L/R meters to zero so they never linger at a stale pre-reset level across
       the gap. Touches only the two UI meter accumulators -- no PCM, NDSP,
       wave-buffer, clock, or timing state. */
    g_audio_meter_left = 0;
    g_audio_meter_right = 0;
    printf("audio_clock: generation=%lu reset=%s\n",
        (unsigned long)g_audio_clock_generation,
        reason ? reason : "unknown");
}
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
    audio_clock_new_generation("channel_configure");
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

    /* HFIX86: configure the GLOBAL DSP output here, at audio-system init,
       before anything can play a channel. Previously the output mode and
       master volume were only ever set lazily inside play()'s
       audio_configure_ndsp_channel -- which was fine as long as nothing
       touched the DSP before the first movie. The DVD-menu sound effects
       (HFIX80, channel 1) broke that assumption: the menu runs before any
       play(), so menu SFX became the first thing to submit to the DSP, with
       the global output still unconfigured. Kicking a channel before global
       output setup can leave NDSP's output path wedged (observed as total
       silence on *both* channels -- menu SFX and, afterwards, movie audio --
       even though per-channel state looked healthy). Setting global output
       once, up front, is both the fix and the correct place for it.
       audio_configure_ndsp_channel still sets the mode per-movie (mono files
       want NDSP_OUTPUT_MONO); this just guarantees a sane global output
       exists from the very first audio of the session. */
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspSetMasterVol(1.0f);

    printf("app audio: ndspInit ok (output=stereo mastervol=1.0)\n");
    return true;
#endif
}

static void app_audio_system_shutdown(void) {
    if (g_ndsp_ready) {
        printf("lifecycle: calling ndspExit\n");
        ndspExit();
        g_ndsp_ready = false;
        printf("lifecycle: ndspExit returned\n");
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

/* MIVF_TRANSPORT_TOUCH_ALIGNMENT_R2: verified non-overlapping style-aware touch regions. */
/* MIVF_TRANSPORT_STYLE_TOUCH_LAYOUTS_V1
   Geometry mirrors the ten active C2.7 renderers. Values are deliberately
   generous but non-overlapping. Style IDs remain settings-compatible 0..9. */
typedef struct { int x,y,w,h; } MivfTouchRect;
typedef struct { MivfTouchRect main[3]; MivfTouchRect timeline; } MivfTouchLayout;
/* MIVF_DEVICE_UI_COLLECTION_PHASE1_V1
   Six original gaming-hardware experiences added to the ten MIVF originals.
   These evoke broad hardware eras without platform logos or copied dashboards. */
#define MIVF_DEVICE_UI_STYLE_COUNT 16u
static const MivfTouchLayout g_mivf_touch_layouts[MIVF_DEVICE_UI_STYLE_COUNT] = {
    {{{34,107,64,60},{123,97,74,78},{222,107,64,60}}, {26,176,268,28}},
    {{{38,82,64,60},{122,70,76,82},{218,82,64,60}}, {39,183,242,30}},
    {{{34,138,64,55},{126,132,68,62},{222,138,64,55}}, {50,210,220,30}},
    {{{24,100,78,70},{106,98,108,74},{217,100,78,70}}, {16,184,288,30}},
    {{{7,62,96,84},{107,58,106,90},{217,62,96,84}}, {30,168,260,38}},
    {{{103,148,60,48},{173,148,60,48},{243,148,60,48}}, {22,38,276,70}},
    {{{31,97,64,60},{120,87,80,80},{225,97,64,60}}, {20,194,280,30}},
    {{{40,108,62,64},{125,102,70,74},{218,108,62,64}}, {18,190,284,34}},
    {{{9,139,90,57},{115,139,90,57},{221,139,90,57}}, {10,106,300,33}},
    {{{10,160,90,52},{115,158,90,56},{220,160,90,52}}, {10,116,300,42}},
    {{{18,147,68,60},{218,145,78,64},{94,147,68,60}}, {34,60,252,54}},  /* Cartridge Controller */
    {{{24,147,72,66},{216,145,78,68},{110,147,72,66}}, {38,45,244,68}}, /* Portable Mono */
    {{{15,151,82,64},{119,143,82,72},{223,151,82,64}}, {28,62,264,54}}, /* Dual Screen Touch */
    {{{27,103,70,74},{125,91,70,94},{223,103,70,74}}, {34,193,252,31}}, /* Industrial Green */
    {{{9,151,94,58},{113,151,94,58},{217,151,94,58}}, {14,53,292,52}},  /* Flat Tile */
    {{{24,143,72,68},{124,133,72,82},{224,143,72,68}}, {30,69,260,48}}  /* Blue Wave */
};
static const MivfTouchLayout *hfix57_current_touch_layout(void) {
    u32 style=g_mivf_settings.transport_style;
    if(style>=MIVF_DEVICE_UI_STYLE_COUNT)style=0u;
    return &g_mivf_touch_layouts[style];
}
/* C.6: the touch hitbox must move by exactly the same delta as the visual
   (mivf_c25_premiere_controls) so they never silently diverge -- Premiere
   (style 0) only, matching mivf_customization_active_for_premiere()'s own
   scope. Returns the base rect unchanged (dx=dy=0) for every other style,
   or when no override is authored, so this is a no-op everywhere else. */
static MivfTouchRect hfix57_premiere_touch_rect(MivfTouchRect base, MivfCtrlId ctrl) {
    int dx = 0, dy = 0;
    if (g_mivf_settings.transport_style == 0 &&
        mivf_customization_active_for_premiere() &&
        mivf_customization_resolve_position(ctrl, &dx, &dy)) {
        base.x += dx;
        base.y += dy;
    }
    return base;
}

static Hfix57TouchButton hfix57_hit_transport(int px,int py) {
    const MivfTouchLayout*l=hfix57_current_touch_layout();
    MivfTouchRect rw = hfix57_premiere_touch_rect(l->main[0], MIVF_CTRL_REWIND);
    MivfTouchRect pp = hfix57_premiere_touch_rect(l->main[1], MIVF_CTRL_PLAY_PAUSE);
    MivfTouchRect ff = hfix57_premiere_touch_rect(l->main[2], MIVF_CTRL_FAST_FORWARD);
    if(hfix57_point_in_rect(px,py,rw.x,rw.y,rw.w,rw.h))return HFIX57_TOUCH_REWIND;
    if(hfix57_point_in_rect(px,py,pp.x,pp.y,pp.w,pp.h))return HFIX57_TOUCH_PLAY;
    if(hfix57_point_in_rect(px,py,ff.x,ff.y,ff.w,ff.h))return HFIX57_TOUCH_FORWARD;
    return HFIX57_TOUCH_NONE;
}

/*
    Convert touch presses into the same logical input flags the keyboard path
    already understands. This keeps pause/audio gating centralized in the
    existing KEY_A handler.
*/
static u32 hfix57_touch_transport_to_keys(u32 keys_down, u32 keys_held) {
    if (g_hfix59r3_settings_visible || g_hfix62_help_visible ||
        g_mivf_theme_picker.active || g_mivf_cvd_picker.active ||
        g_mivf_transport_picker.active || g_mivf_touch_locked) {
        g_hfix57_touch_button = HFIX57_TOUCH_NONE;
        g_media_ctl.dummy_seek_state = 0;
        return 0;
    }
    if (!(keys_held & KEY_TOUCH)) {
        if (g_hfix57_touch_button != HFIX57_TOUCH_NONE) {
            g_hfix57_touch_button = HFIX57_TOUCH_NONE;
            g_media_ctl.dummy_seek_state = 0;
        }

        return 0;
    }

    touchPosition touch;
    hidTouchRead(&touch);

    const MivfTouchLayout *layout = hfix57_current_touch_layout();
    Hfix57TouchButton hit;
    if (hfix57_point_in_rect((int)touch.px,(int)touch.py,
            layout->timeline.x,layout->timeline.y,
            layout->timeline.w,layout->timeline.h)) {
        hit = HFIX57_TOUCH_NONE;
    } else {
        hit = hfix57_hit_transport((int)touch.px,(int)touch.py);
    }
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

/* Called once per rendered frame during playback, regardless of touch-lock
   state or dashboard visibility -- physical buttons must always be able to
   toggle the lock, including while the dashboard is dimmed/collapsed. */
static void hfix_touch_lock_update(u32 keys_held) {
    if ((keys_held & KEY_L) && (keys_held & KEY_R)) {
        if (g_mivf_touch_lock_hold_frames < 0xFFFFFFFFu) {
            g_mivf_touch_lock_hold_frames++;
        }
        if (!g_mivf_touch_lock_gesture_fired && g_mivf_touch_lock_hold_frames >= MIVF_TOUCH_LOCK_HOLD_FRAMES) {
            g_mivf_touch_locked = !g_mivf_touch_locked;
            g_mivf_touch_lock_gesture_fired = true;
            hfix58_alert_set(g_mivf_touch_locked ? "TOUCH LOCKED" : "TOUCH UNLOCKED", 1);
        }
    } else {
        g_mivf_touch_lock_hold_frames = 0;
        g_mivf_touch_lock_gesture_fired = false;
    }
}


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

/* Visible-but-unobtrusive touch-lock indicator: icon + text, never
   color-alone, so it stays legible for colorblind users and doesn't
   rely on being noticed as a color change. Placed top-left, clear of
   the battery/clock cluster (top-right) and all transport hitboxes. */
static void hfix_draw_touch_lock_indicator(u8 *fb) {
    if (!fb || !g_mivf_touch_locked) return;

    int x = 6, y = 6;

    hfix58_blend_rect565(fb, x, y, 62, 15, 3, 6, 14, 190);

    /* Small padlock glyph: shackle (upside-down U) over a body. */
    hfix58_rect565(fb, x + 5, y + 3, 2, 4, 210, 220, 235);
    hfix58_rect565(fb, x + 11, y + 3, 2, 4, 210, 220, 235);
    hfix58_rect565(fb, x + 5, y + 3, 8, 2, 210, 220, 235);
    hfix58_rect565(fb, x + 4, y + 6, 10, 6, 230, 200, 60);

    hfix58_draw_text_shadow(fb, x + 18, y + 4, "LOCKED", 1, 235, 240, 250);
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

/* Global accent palette. Presets and custom colors share one render path. */
static u8 mivf_theme_mix(u8 a, u8 b, int amount) {
    return (u8)(((int)a * (255 - amount) + (int)b * amount + 127) / 255);
}
static int mivf_theme_luma(u8 r, u8 g, u8 b) {
    return ((int)r * 54 + (int)g * 183 + (int)b * 19) >> 8;
}

static void mivf_theme_set_role(u8 *rr, u8 *gg, u8 *bb, int r, int g, int b) {
    *rr = (u8)r; *gg = (u8)g; *bb = (u8)b;
}

static const char *mivf_cvd_name(u32 mode) {
    static const char *names[MIVF_CVD_COUNT] = {
        "STANDARD", "PROTAN", "DEUTAN", "TRITAN", "MONO", "HIGH CONTRAST"
    };
    return names[mode < MIVF_CVD_COUNT ? mode : 0u];
}

static void mivf_theme_set_rgb(u8 base_r, u8 base_g, u8 base_b) {
    u8 r = base_r, g = base_g, b = base_b;
    u32 mode = g_mivf_settings.color_vision_mode;
    int luma;

    if (mode >= MIVF_CVD_COUNT) mode = MIVF_CVD_STANDARD;
    memset(&g_mivf_theme_palette, 0, sizeof(g_mivf_theme_palette));

    /* Accessibility profiles change functional UI roles, never artwork/video. */
    switch (mode) {
        case MIVF_CVD_PROTAN: r=40; g=170; b=245; break;
        case MIVF_CVD_DEUTAN: r=65; g=135; b=245; break;
        case MIVF_CVD_TRITAN: r=225; g=80; b=205; break;
        case MIVF_CVD_MONOCHROME: {
            int y=mivf_theme_luma(base_r,base_g,base_b); if(y<150)y=205;
            r=g=b=(u8)y; break;
        }
        case MIVF_CVD_HIGH_CONTRAST: r=255; g=224; b=70; break;
        default: break;
    }

    g_mivf_theme_r=r; g_mivf_theme_g=g; g_mivf_theme_b=b;
    g_mivf_theme_palette.accent_r=r; g_mivf_theme_palette.accent_g=g; g_mivf_theme_palette.accent_b=b;
    g_mivf_theme_palette.light_r=mivf_theme_mix(r,255,96);
    g_mivf_theme_palette.light_g=mivf_theme_mix(g,255,96);
    g_mivf_theme_palette.light_b=mivf_theme_mix(b,255,96);
    g_mivf_theme_palette.soft_r=mivf_theme_mix(r,18,174);
    g_mivf_theme_palette.soft_g=mivf_theme_mix(g,28,174);
    g_mivf_theme_palette.soft_b=mivf_theme_mix(b,44,174);
    g_mivf_theme_palette.dark_r=mivf_theme_mix(r,0,154);
    g_mivf_theme_palette.dark_g=mivf_theme_mix(g,0,154);
    g_mivf_theme_palette.dark_b=mivf_theme_mix(b,0,154);
    luma=mivf_theme_luma(r,g,b);
    if(luma>160){g_mivf_theme_palette.on_r=8;g_mivf_theme_palette.on_g=12;g_mivf_theme_palette.on_b=18;}
    else {g_mivf_theme_palette.on_r=255;g_mivf_theme_palette.on_g=255;g_mivf_theme_palette.on_b=255;}

    /* Semantic roles always include text/shape labels; colors are reinforcement. */
    mivf_theme_set_role(&g_mivf_theme_palette.info_r,&g_mivf_theme_palette.info_g,&g_mivf_theme_palette.info_b,70,150,230);
    mivf_theme_set_role(&g_mivf_theme_palette.success_r,&g_mivf_theme_palette.success_g,&g_mivf_theme_palette.success_b,70,210,110);
    mivf_theme_set_role(&g_mivf_theme_palette.warning_r,&g_mivf_theme_palette.warning_g,&g_mivf_theme_palette.warning_b,235,150,45);
    mivf_theme_set_role(&g_mivf_theme_palette.danger_r,&g_mivf_theme_palette.danger_g,&g_mivf_theme_palette.danger_b,235,70,70);
    mivf_theme_set_role(&g_mivf_theme_palette.chapter_r,&g_mivf_theme_palette.chapter_g,&g_mivf_theme_palette.chapter_b,252,228,110);
    mivf_theme_set_role(&g_mivf_theme_palette.loop_a_r,&g_mivf_theme_palette.loop_a_g,&g_mivf_theme_palette.loop_a_b,90,230,120);
    mivf_theme_set_role(&g_mivf_theme_palette.loop_b_r,&g_mivf_theme_palette.loop_b_g,&g_mivf_theme_palette.loop_b_b,240,90,90);

    if(mode==MIVF_CVD_PROTAN || mode==MIVF_CVD_DEUTAN){
        mivf_theme_set_role(&g_mivf_theme_palette.success_r,&g_mivf_theme_palette.success_g,&g_mivf_theme_palette.success_b,70,190,245);
        mivf_theme_set_role(&g_mivf_theme_palette.warning_r,&g_mivf_theme_palette.warning_g,&g_mivf_theme_palette.warning_b,255,205,65);
        mivf_theme_set_role(&g_mivf_theme_palette.danger_r,&g_mivf_theme_palette.danger_g,&g_mivf_theme_palette.danger_b,225,85,195);
        mivf_theme_set_role(&g_mivf_theme_palette.loop_a_r,&g_mivf_theme_palette.loop_a_g,&g_mivf_theme_palette.loop_a_b,60,185,245);
        mivf_theme_set_role(&g_mivf_theme_palette.loop_b_r,&g_mivf_theme_palette.loop_b_g,&g_mivf_theme_palette.loop_b_b,255,205,65);
    } else if(mode==MIVF_CVD_TRITAN){
        mivf_theme_set_role(&g_mivf_theme_palette.success_r,&g_mivf_theme_palette.success_g,&g_mivf_theme_palette.success_b,235,245,250);
        mivf_theme_set_role(&g_mivf_theme_palette.warning_r,&g_mivf_theme_palette.warning_g,&g_mivf_theme_palette.warning_b,245,105,65);
        mivf_theme_set_role(&g_mivf_theme_palette.danger_r,&g_mivf_theme_palette.danger_g,&g_mivf_theme_palette.danger_b,210,65,170);
        mivf_theme_set_role(&g_mivf_theme_palette.loop_a_r,&g_mivf_theme_palette.loop_a_g,&g_mivf_theme_palette.loop_a_b,230,80,205);
        mivf_theme_set_role(&g_mivf_theme_palette.loop_b_r,&g_mivf_theme_palette.loop_b_g,&g_mivf_theme_palette.loop_b_b,105,230,225);
    } else if(mode==MIVF_CVD_MONOCHROME){
        mivf_theme_set_role(&g_mivf_theme_palette.info_r,&g_mivf_theme_palette.info_g,&g_mivf_theme_palette.info_b,190,190,190);
        mivf_theme_set_role(&g_mivf_theme_palette.success_r,&g_mivf_theme_palette.success_g,&g_mivf_theme_palette.success_b,230,230,230);
        mivf_theme_set_role(&g_mivf_theme_palette.warning_r,&g_mivf_theme_palette.warning_g,&g_mivf_theme_palette.warning_b,170,170,170);
        mivf_theme_set_role(&g_mivf_theme_palette.danger_r,&g_mivf_theme_palette.danger_g,&g_mivf_theme_palette.danger_b,255,255,255);
        mivf_theme_set_role(&g_mivf_theme_palette.chapter_r,&g_mivf_theme_palette.chapter_g,&g_mivf_theme_palette.chapter_b,175,175,175);
        mivf_theme_set_role(&g_mivf_theme_palette.loop_a_r,&g_mivf_theme_palette.loop_a_g,&g_mivf_theme_palette.loop_a_b,245,245,245);
        mivf_theme_set_role(&g_mivf_theme_palette.loop_b_r,&g_mivf_theme_palette.loop_b_g,&g_mivf_theme_palette.loop_b_b,135,135,135);
        g_mivf_theme_palette.monochrome=true;
    } else if(mode==MIVF_CVD_HIGH_CONTRAST){
        mivf_theme_set_role(&g_mivf_theme_palette.info_r,&g_mivf_theme_palette.info_g,&g_mivf_theme_palette.info_b,120,220,255);
        mivf_theme_set_role(&g_mivf_theme_palette.success_r,&g_mivf_theme_palette.success_g,&g_mivf_theme_palette.success_b,120,240,255);
        mivf_theme_set_role(&g_mivf_theme_palette.warning_r,&g_mivf_theme_palette.warning_g,&g_mivf_theme_palette.warning_b,255,232,70);
        mivf_theme_set_role(&g_mivf_theme_palette.danger_r,&g_mivf_theme_palette.danger_g,&g_mivf_theme_palette.danger_b,255,80,105);
        mivf_theme_set_role(&g_mivf_theme_palette.chapter_r,&g_mivf_theme_palette.chapter_g,&g_mivf_theme_palette.chapter_b,255,255,255);
        mivf_theme_set_role(&g_mivf_theme_palette.loop_a_r,&g_mivf_theme_palette.loop_a_g,&g_mivf_theme_palette.loop_a_b,255,255,255);
        mivf_theme_set_role(&g_mivf_theme_palette.loop_b_r,&g_mivf_theme_palette.loop_b_g,&g_mivf_theme_palette.loop_b_b,255,224,70);
        g_mivf_theme_palette.strong_focus=true;
        g_mivf_theme_palette.reduce_transparency=true;
    }

    g_mivf_theme_generation++; if(!g_mivf_theme_generation) g_mivf_theme_generation=1;
}
static void hfix60_apply_theme(u32 idx) {
    if (g_mivf_settings.theme_custom) {
        mivf_theme_set_rgb((u8)g_mivf_settings.theme_r,(u8)g_mivf_settings.theme_g,(u8)g_mivf_settings.theme_b);
        return;
    }
    switch (idx & 3u) {
        case 0: mivf_theme_set_rgb(70,120,210); break;
        case 1: mivf_theme_set_rgb(0,170,95); break;
        case 2: mivf_theme_set_rgb(150,80,220); break;
        case 3: mivf_theme_set_rgb(235,140,40); break;
        default: mivf_theme_set_rgb(70,120,210); break;
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
    /* HFIX_BOTTOMDIM: playback autodim no longer scales hardware backlight
       brightness (that hit GSPLCD_SCREEN_BOTH, dimming the top video along
       with the bottom UI). Waking/restoring still sets real hardware
       brightness for both screens as before; entering the dimmed state only
       flips g_mivf_brightness_dimmed, which hfix51c_draw_bottom_ui uses to
       draw a bottom-screen-only dark overlay -- the top screen's backlight
       and video output are never touched. */
    if (!dimmed) {
        u32 brightness = g_mivf_brightness_active;

        if (brightness < 1u) brightness = 1u;
        if (brightness > 5u) brightness = 5u;

        GSPLCD_SetBrightness(GSPLCD_SCREEN_BOTH, brightness);
    }

    g_mivf_brightness_dimmed = dimmed;
}

static void hfix59r3_sync_runtime_settings(void) {
    g_hfix56_force_stereo = g_mivf_settings.force_stereo;
    g_hfix56_volume_percent = (int)g_mivf_settings.volume_percent;
    g_hfix56_left_gain_percent = (int)g_mivf_settings.left_gain_percent;
    g_hfix56_right_gain_percent = (int)g_mivf_settings.right_gain_percent;

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

/* Watch-state checkpoint/finish bodies are defined later (after
   hfix58f_total_frames' real definition, which they call) -- forward
   declared here so the checkpoint/teardown call sites below can reach
   them regardless of definition order. */
static void hfix_watchstate_checkpoint(u32 shown_frame);
static void hfix_watchstate_finish(u32 shown_frame, bool reached_eof);

/*
    The one guarded save shared by every checkpoint trigger (lifecycle
    request, periodic timer) -- NOT used by the end-of-playback save,
    which has its own additional "clear the bookmark if we reached EOF"
    behavior that doesn't belong in a mid-playback checkpoint.
*/
static void mivf_bookmark_checkpoint_if_eligible(u32 shown_frame) {
    if (g_mivf_settings.resume_enabled && !g_mivf_play_reached_eof) {
        MIVF_BookmarkSave(MIVF_PATH, shown_frame);
    }
    /* Watch-state tracks regardless of resume_enabled -- "have I seen
       this" is independent of whether resume is turned on. Skipped only
       once EOF is reached, since hfix_watchstate_finish (teardown) is
       the one authoritative place that records a completion. */
    if (!g_mivf_play_reached_eof) {
        hfix_watchstate_checkpoint(shown_frame);
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
            /*
                Audited (audit_correction_20260718_081500): this flag is
                consumed only AFTER a successful wake/restore, because
                its consumer sits inside the while(aptMainLoop()) body,
                and that same aptMainLoop() call does not return until
                either resumed or told to exit. So this does NOT protect
                the "suspended and the console never comes back" case
                (battery dies while suspended, or force-quit from the
                HOME task-switcher without resuming) -- for that,
                periodic checkpointing below is the only real protection,
                bounded by whatever position was current at the last
                periodic tick before the suspend. What this flag DOES
                guarantee: a fresh checkpoint fires the moment the user
                successfully resumes, protecting against a LATER failure
                (e.g. a crash sometime after a good resume). No file I/O
                here regardless -- see the flag's declaration comment.
            */
            if (g_media_ctl.state == STATE_PLAYING || g_media_ctl.state == STATE_PAUSED) {
                g_mivf_bookmark_checkpoint_requested = true;
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
            if (g_media_ctl.state == STATE_PLAYING || g_media_ctl.state == STATE_PAUSED) {
                g_mivf_bookmark_checkpoint_requested = true;
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
            printf("lifecycle: APTHOOK_ONEXIT fired (state=%d)\n", (int)g_media_ctl.state);
            /*
                Audited (audit_correction_20260718_081500): this flag is
                PROVABLY NEVER CONSUMED. The consuming code sits inside
                the while(aptMainLoop()) loop body, which only runs
                again if that same call to aptMainLoop() returns true.
                On exit it returns false instead, so the loop body --
                including this flag's consumer -- never executes again
                for this session. Kept only because it's inert, not
                because it does anything: if the standard libctru
                convention holds (aptMainLoop() returning false is how
                a HOME-menu "Close" is communicated, not verified against
                vendored source in this repo), the pre-existing
                unconditional post-loop save already covers that case on
                its own, with or without this flag.
            */
            if (g_media_ctl.state == STATE_PLAYING || g_media_ctl.state == STATE_PAUSED) {
                g_mivf_bookmark_checkpoint_requested = true;
            }
            GSPLCD_PowerOffAllBacklights();
            break;
        default:
            break;
    }
}

#define HFIX59R3_SETTINGS_COUNT 33
#define HFIX59R3_SETTINGS_VISIBLE 13

static const char *hfix59r3_settings_group(int idx) {
    if (idx <= 3) return "PLAY";
    if (idx <= 8) return "DISPLAY";
    if (idx == 9) return "AUDIO";
    if (idx <= 14) return "SUBS";
    if (idx <= 19) return "ADV";
    if (idx == 20) return "HELP";
    if (idx == 23) return "ACCESS";
    if (idx == 24) return "APPEAR";
    /* P.A1: idx 25-27, LEFT GAIN/RIGHT GAIN/BALANCE RESET -- appended at
       the end rather than inserted next to idx 9 (STEREO OUT) to avoid
       renumbering the 15 existing case labels below idx 25 in
       hfix59r3_handle_settings_menu(). Still grouped under "AUDIO" for an
       accurate section label, even though not textually adjacent to idx 9. */
    if (idx >= 25 && idx <= 27) return "AUDIO";
    if (idx >= 28 && idx <= 32) return "SAVER";
    return "SUBS"; /* HFIX68: idx 21, CHAPTER MARKERS -- grouped with CHAPTERS */
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
        "DEBUG",
        "CONTROLS",
        "CHAPTER MARKERS",
        "A/V SYNC",
        "COLOR VISION",
        "TRANSPORT STYLE",
        "LEFT GAIN",
        "RIGHT GAIN",
        "BALANCE RESET",
        "SAVER IDLE",
        "SAVER SPEED",
        "SAVER FADE",
        "SAVER MOTION",
        "SAVER RESET"
    };

    if (idx < 0 || idx >= HFIX59R3_SETTINGS_COUNT) {
        return "";
    }

    return labels[idx];
}

static const char *mivf_transport_style_name(u32 style);

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
        case 7: snprintf(out, out_sz, "%s", g_mivf_settings.theme_custom ? "CUSTOM" : hfix60_theme_name(g_mivf_settings.theme_index)); break;
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
        case 20: snprintf(out, out_sz, "A: VIEW"); break;
        case 21: snprintf(out, out_sz, "%s", g_mivf_settings.chapter_markers_enabled ? "ON" : "OFF"); break;
        case 22: snprintf(out, out_sz, "%+dms", g_mivf_settings.audio_offset_ms); break;
        case 23: snprintf(out, out_sz, "%s", mivf_cvd_name(g_mivf_settings.color_vision_mode)); break;
        case 24: snprintf(out, out_sz, "%s", mivf_transport_style_name(g_mivf_settings.transport_style)); break;
        case 25: snprintf(out, out_sz, "%lu%%", (unsigned long)g_mivf_settings.left_gain_percent); break;
        case 26: snprintf(out, out_sz, "%lu%%", (unsigned long)g_mivf_settings.right_gain_percent); break;
        case 27: snprintf(out, out_sz, "A: RESET"); break;
        case 28: snprintf(out, out_sz, "%lus", (unsigned long)(g_mivf_settings.screensaver_idle_frames / 60u)); break;
        case 29: snprintf(out, out_sz, "%lu", (unsigned long)g_mivf_settings.screensaver_speed); break;
        case 30: snprintf(out, out_sz, "%.2fs", (double)g_mivf_settings.screensaver_fade_frames / 60.0); break;
        case 31: snprintf(out, out_sz, "%s", g_mivf_settings.screensaver_reduce_motion ? "STATIC" : "NORMAL"); break;
        case 32: snprintf(out, out_sz, "A: RESET"); break;
        default: break;
    }
}

/* HFIX62 forward decl: the Settings menu's CONTROLS row hands off to Help,
   defined further down this file (see hfix62_set_help_open). */
static void hfix62_set_help_open(bool open);

/* hfix71 forward decl: the Settings menu's AUDIO SYNC row reconfigures the
   manual sync delay ring, defined further down this file. */
static void audio_delay_ring_reconfigure(u32 offset_ms);

/* hfix77 forward decl: the Settings menu's A/V SYNC row routes its value to
   the audio-hold / video-delay mechanisms, defined further down this file. */
static void av_sync_offset_apply(int offset_ms);

/* MIVF_TRANSPORT_PHASE_C2_1_AESTHETIC_OVERHAUL_V1
   Full style families, not icon-size variants.  These helpers are draw-only:
   no playback state, decode, seek, audio, NDSP, or touch action logic changes. */
static void mivf_transport_picker_open(void);
static bool mivf_transport_picker_input(u32 down);
static void mivf_transport_picker_draw(u8 *fb);

static void mivf_theme_hsv_to_rgb(int h,int s,int v,u8 *rr,u8 *gg,u8 *bb) {
    int region, rem, p, q, t; h%=360; if(h<0)h+=360;
    if(s<=0){*rr=*gg=*bb=(u8)v;return;} region=h/60; rem=((h%60)*255)/60;
    p=(v*(255-s))/255; q=(v*(255-(s*rem)/255))/255; t=(v*(255-(s*(255-rem))/255))/255;
    switch(region){case 0:*rr=v;*gg=t;*bb=p;break;case 1:*rr=q;*gg=v;*bb=p;break;case 2:*rr=p;*gg=v;*bb=t;break;case 3:*rr=p;*gg=q;*bb=v;break;case 4:*rr=t;*gg=p;*bb=v;break;default:*rr=v;*gg=p;*bb=q;break;}
}
static void mivf_theme_rgb_to_hsv(u8 r,u8 g,u8 b,int *hh,int *ss,int *vv) {
    int mx=r, mn=r, d, h=0; if(g>mx)mx=g;if(b>mx)mx=b;if(g<mn)mn=g;if(b<mn)mn=b;d=mx-mn;
    if(d){if(mx==r)h=60*(g-b)/d;else if(mx==g)h=120+60*(b-r)/d;else h=240+60*(r-g)/d;if(h<0)h+=360;}
    *hh=h;*ss=mx?d*255/mx:0;*vv=mx;
}
static void mivf_theme_picker_preview(void){u8 r,g,b;mivf_theme_hsv_to_rgb(g_mivf_theme_picker.hue,g_mivf_theme_picker.sat,g_mivf_theme_picker.val,&r,&g,&b);mivf_theme_set_rgb(r,g,b);}
static void mivf_theme_picker_open(void){
    memset(&g_mivf_theme_picker,0,sizeof(g_mivf_theme_picker));g_mivf_theme_picker.active=true;
    g_mivf_theme_picker.original_r=g_mivf_theme_r;g_mivf_theme_picker.original_g=g_mivf_theme_g;g_mivf_theme_picker.original_b=g_mivf_theme_b;
    g_mivf_theme_picker.original_custom=g_mivf_settings.theme_custom;g_mivf_theme_picker.original_index=g_mivf_settings.theme_index;
    mivf_theme_rgb_to_hsv(g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b,&g_mivf_theme_picker.hue,&g_mivf_theme_picker.sat,&g_mivf_theme_picker.val);
}
static void mivf_theme_picker_cancel(void){g_mivf_theme_picker.active=false;g_mivf_settings.theme_custom=g_mivf_theme_picker.original_custom;g_mivf_settings.theme_index=g_mivf_theme_picker.original_index;mivf_theme_set_rgb(g_mivf_theme_picker.original_r,g_mivf_theme_picker.original_g,g_mivf_theme_picker.original_b);}
static void mivf_theme_picker_apply(void){g_mivf_theme_picker.active=false;g_mivf_settings.theme_custom=true;g_mivf_settings.theme_r=g_mivf_theme_r;g_mivf_settings.theme_g=g_mivf_theme_g;g_mivf_settings.theme_b=g_mivf_theme_b;MIVF_SettingsSave(&g_mivf_settings);hfix58_alert_set("CUSTOM THEME SAVED",1);}
static bool mivf_theme_picker_input(u32 down,u32 held){
    bool changed=false;
    if(down&(KEY_B|KEY_START)){mivf_theme_picker_cancel();return true;} if(down&KEY_A){mivf_theme_picker_apply();return true;}
    if(down&KEY_X){g_mivf_theme_picker.hue=219;g_mivf_theme_picker.sat=170;g_mivf_theme_picker.val=210;changed=true;}
    if(down&KEY_Y){static const u8 p[4][3]={{70,120,210},{0,170,95},{150,80,220},{235,140,40}};u32 i=(g_mivf_settings.theme_index+1u)%4u;g_mivf_settings.theme_index=i;mivf_theme_rgb_to_hsv(p[i][0],p[i][1],p[i][2],&g_mivf_theme_picker.hue,&g_mivf_theme_picker.sat,&g_mivf_theme_picker.val);changed=true;}
    if(down&KEY_L){g_mivf_theme_picker.hue=(g_mivf_theme_picker.hue+354)%360;changed=true;} if(down&KEY_R){g_mivf_theme_picker.hue=(g_mivf_theme_picker.hue+6)%360;changed=true;}
    if(down&KEY_DLEFT){if(g_mivf_theme_picker.sat>0)g_mivf_theme_picker.sat--;changed=true;} if(down&KEY_DRIGHT){if(g_mivf_theme_picker.sat<255)g_mivf_theme_picker.sat++;changed=true;}
    if(down&KEY_DUP){if(g_mivf_theme_picker.val<255)g_mivf_theme_picker.val++;changed=true;} if(down&KEY_DDOWN){if(g_mivf_theme_picker.val>0)g_mivf_theme_picker.val--;changed=true;}
    if(held&KEY_TOUCH){touchPosition t;hidTouchRead(&t);if(t.px>=18&&t.px<242&&t.py>=42&&t.py<158){g_mivf_theme_picker.sat=((int)t.px-18)*255/223;g_mivf_theme_picker.val=255-((int)t.py-42)*255/115;changed=true;}else if(t.px>=252&&t.px<286&&t.py>=42&&t.py<158){g_mivf_theme_picker.hue=((int)t.py-42)*359/115;changed=true;}}
    if(changed) mivf_theme_picker_preview();
    return true;
}
static void mivf_theme_picker_draw(u8 *fb){
    char rgb[48]; if(!fb||!g_mivf_theme_picker.active)return;
    hfix58_rect565(fb,0,0,320,240,2,5,11);hfix58_rect565(fb,8,7,304,203,10,17,29);
    hfix58_draw_text_shadow(fb,18,14,"THEME COLOR",1,242,248,255);hfix58_draw_text_shadow(fb,216,14,"LIVE PREVIEW",1,g_mivf_theme_palette.light_r,g_mivf_theme_palette.light_g,g_mivf_theme_palette.light_b);
    for(int yy=0;yy<116;yy+=4)for(int xx=0;xx<224;xx+=4){u8 r,g,b;mivf_theme_hsv_to_rgb(g_mivf_theme_picker.hue,xx*255/223,255-yy*255/115,&r,&g,&b);hfix58_rect565(fb,18+xx,42+yy,4,4,r,g,b);}
    for(int yy=0;yy<116;yy+=2){u8 r,g,b;mivf_theme_hsv_to_rgb(yy*359/115,255,255,&r,&g,&b);hfix58_rect565(fb,252,42+yy,34,2,r,g,b);}
    {int cx=18+g_mivf_theme_picker.sat*223/255,cy=42+(255-g_mivf_theme_picker.val)*115/255;hfix58_rect565(fb,cx-3,cy-3,7,1,255,255,255);hfix58_rect565(fb,cx-3,cy+3,7,1,255,255,255);hfix58_rect565(fb,cx-3,cy-3,1,7,255,255,255);hfix58_rect565(fb,cx+3,cy-3,1,7,255,255,255);}
    hfix58_rect565(fb,294,42+g_mivf_theme_picker.hue*115/359,10,2,255,255,255);
    hfix58_rect565(fb,18,169,44,23,g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);snprintf(rgb,sizeof(rgb),"RGB %03u %03u %03u",g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);hfix58_draw_text_shadow(fb,72,176,rgb,1,220,234,248);
    hfix58_rect565(fb,0,216,320,24,3,7,14);hfix58_draw_text_shadow(fb,10,222,"A APPLY  B CANCEL  X DEFAULT  Y PRESET",1,190,212,234);
}

static void mivf_cvd_picker_open(void) {
    g_mivf_cvd_picker.active=true;
    g_mivf_cvd_picker.original_mode=g_mivf_settings.color_vision_mode;
    g_mivf_cvd_picker.preview_mode=g_mivf_settings.color_vision_mode;
}
static void mivf_cvd_picker_preview(u32 mode) {
    g_mivf_cvd_picker.preview_mode=mode%MIVF_CVD_COUNT;
    g_mivf_settings.color_vision_mode=g_mivf_cvd_picker.preview_mode;
    hfix60_apply_theme(g_mivf_settings.theme_index);
}
static void mivf_cvd_picker_cancel(void) {
    g_mivf_cvd_picker.active=false;
    g_mivf_settings.color_vision_mode=g_mivf_cvd_picker.original_mode;
    hfix60_apply_theme(g_mivf_settings.theme_index);
}
static void mivf_cvd_picker_apply(void) {
    g_mivf_cvd_picker.active=false;
    g_mivf_settings.color_vision_mode=g_mivf_cvd_picker.preview_mode;
    hfix60_apply_theme(g_mivf_settings.theme_index);
    MIVF_SettingsSave(&g_mivf_settings);
    hfix58_alert_set("COLOR VISION SAVED",1);
}
static bool mivf_cvd_picker_input(u32 down) {
    if(down&(KEY_B|KEY_START)){mivf_cvd_picker_cancel();return true;}
    if(down&KEY_A){mivf_cvd_picker_apply();return true;}
    if(down&KEY_X){mivf_cvd_picker_preview(MIVF_CVD_STANDARD);return true;}
    if(down&(KEY_DLEFT|KEY_L)) mivf_cvd_picker_preview((g_mivf_cvd_picker.preview_mode+MIVF_CVD_COUNT-1u)%MIVF_CVD_COUNT);
    if(down&(KEY_DRIGHT|KEY_R|KEY_Y)) mivf_cvd_picker_preview((g_mivf_cvd_picker.preview_mode+1u)%MIVF_CVD_COUNT);
    return true;
}
static void mivf_cvd_picker_draw(u8 *fb) {
    const char *mode=mivf_cvd_name(g_mivf_cvd_picker.preview_mode);
    int focus_h=g_mivf_theme_palette.strong_focus?18:14;
    if(!fb||!g_mivf_cvd_picker.active)return;
    hfix58_rect565(fb,0,0,320,240,1,3,8);
    hfix58_rect565(fb,8,7,304,203,8,14,25);
    hfix58_rect565(fb,8,7,304,18,g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);
    hfix58_draw_text_shadow(fb,18,12,"COLOR VISION PREVIEW",1,255,255,255);
    hfix58_draw_text_shadow(fb,18,34,mode,1,g_mivf_theme_palette.light_r,g_mivf_theme_palette.light_g,g_mivf_theme_palette.light_b);

    hfix58_draw_text_shadow(fb,18,55,"> SELECTED ITEM",1,255,255,255);
    hfix58_blend_rect565(fb,14,49,286,focus_h,g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b,g_mivf_theme_palette.reduce_transparency?130:70);
    hfix58_rect565(fb,14,49,g_mivf_theme_palette.strong_focus?6:4,focus_h,g_mivf_theme_palette.light_r,g_mivf_theme_palette.light_g,g_mivf_theme_palette.light_b);
    hfix58_draw_text_shadow(fb,18,75,"  UNSELECTED ITEM",1,188,204,224);

    hfix58_rect565(fb,18,94,62,18,g_mivf_theme_palette.success_r,g_mivf_theme_palette.success_g,g_mivf_theme_palette.success_b);
    hfix58_draw_text_shadow(fb,26,100,"OK SAVED",1,255,255,255);
    hfix58_rect565(fb,86,94,62,18,g_mivf_theme_palette.warning_r,g_mivf_theme_palette.warning_g,g_mivf_theme_palette.warning_b);
    hfix58_draw_text_shadow(fb,94,100,"! WARN",1,255,255,255);
    hfix58_rect565(fb,154,94,62,18,g_mivf_theme_palette.danger_r,g_mivf_theme_palette.danger_g,g_mivf_theme_palette.danger_b);
    hfix58_draw_text_shadow(fb,170,100,"X STOP",1,255,255,255);
    hfix58_rect565(fb,222,94,62,18,g_mivf_theme_palette.info_r,g_mivf_theme_palette.info_g,g_mivf_theme_palette.info_b);
    hfix58_draw_text_shadow(fb,238,100,"i INFO",1,255,255,255);

    hfix58_rect565(fb,18,132,266,9,20,28,42);
    hfix58_rect565(fb,18,132,164,9,g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);
    hfix58_rect565(fb,90,128,1,17,g_mivf_theme_palette.chapter_r,g_mivf_theme_palette.chapter_g,g_mivf_theme_palette.chapter_b);
    hfix58_rect565(fb,136,126,3,20,g_mivf_theme_palette.loop_a_r,g_mivf_theme_palette.loop_a_g,g_mivf_theme_palette.loop_a_b);
    hfix58_draw_text_shadow(fb,132,148,"A",1,255,255,255);
    hfix58_rect565(fb,178,126,1,20,g_mivf_theme_palette.loop_b_r,g_mivf_theme_palette.loop_b_g,g_mivf_theme_palette.loop_b_b);
    hfix58_rect565(fb,181,126,1,20,g_mivf_theme_palette.loop_b_r,g_mivf_theme_palette.loop_b_g,g_mivf_theme_palette.loop_b_b);
    hfix58_draw_text_shadow(fb,176,148,"B",1,255,255,255);
    hfix58_draw_text_shadow(fb,18,172,"COLOR + SHAPE + LABEL",1,220,232,246);
    hfix58_draw_text_shadow(fb,18,188,"LEFT/RIGHT PREVIEW",1,180,202,226);
    hfix58_rect565(fb,0,216,320,24,3,7,14);
    hfix58_draw_text_shadow(fb,12,222,"A APPLY  B CANCEL  X STANDARD",1,205,225,245);
}

static bool hfix59r3_handle_settings_menu(u32 down, u32 held) {
    char value[32];

    value[0] = 0;

    if (g_mivf_theme_picker.active) return mivf_theme_picker_input(down, held);
    if (g_mivf_cvd_picker.active) return mivf_cvd_picker_input(down);
    if (g_mivf_transport_picker.active) return mivf_transport_picker_input(down);
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
            if (down & KEY_A) { mivf_theme_picker_open(); snprintf(value,sizeof(value),"THEME PICKER"); }
            else { if (down & KEY_DLEFT) g_mivf_settings.theme_index=(g_mivf_settings.theme_index+3u)%4u; else g_mivf_settings.theme_index=(g_mivf_settings.theme_index+1u)%4u; g_mivf_settings.theme_custom=false; snprintf(value,sizeof(value),"THEME %s",hfix60_theme_name(g_mivf_settings.theme_index)); }
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
        case 20:
            /* Hand off from Settings to the Help overlay directly (no
               resume-tracking side effects here -- playback is already
               paused from opening Settings; hfix62_set_help_open(false)
               performs the actual resume check when Help closes). */
            g_hfix59r3_settings_visible = false;
            hfix62_set_help_open(true);
            break;
        case 21:
            g_mivf_settings.chapter_markers_enabled = !g_mivf_settings.chapter_markers_enabled;
            snprintf(value, sizeof(value), "CHAPTER MARKERS %s", g_mivf_settings.chapter_markers_enabled ? "ON" : "OFF");
            break;
        case 22: {
            /* Manual A/V sync calibration, tunable live by ear. Positive holds
               audio later (hfix71 delay ring); negative delays video instead
               (hfix77), which is what pulls late audio back into sync. Range
               -600..+3000; same sign convention as encoder --audio-offset-ms. */
            int step = (down & KEY_DLEFT) ? -100 : 100;
            int next = g_mivf_settings.audio_offset_ms + step;
            if (next < -600) next = -600;
            if (next > 3000) next = 3000;
            g_mivf_settings.audio_offset_ms = next;
            av_sync_offset_apply(g_mivf_settings.audio_offset_ms);
            snprintf(value, sizeof(value), "A/V SYNC %+dms", g_mivf_settings.audio_offset_ms);
            break;
        }
        case 23:
            if (down & KEY_A) {
                mivf_cvd_picker_open();
                snprintf(value,sizeof(value),"COLOR VISION PREVIEW");
            } else {
                u32 mode=g_mivf_settings.color_vision_mode;
                if(down&KEY_DLEFT) mode=(mode+MIVF_CVD_COUNT-1u)%MIVF_CVD_COUNT;
                else mode=(mode+1u)%MIVF_CVD_COUNT;
                g_mivf_settings.color_vision_mode=mode;
                hfix60_apply_theme(g_mivf_settings.theme_index);
                snprintf(value,sizeof(value),"COLOR VISION %s",mivf_cvd_name(mode));
            }
            break;
        case 24:
            if (down & KEY_A) {
                mivf_transport_picker_open();
                snprintf(value,sizeof(value),"TRANSPORT PREVIEW");
            } else {
                u32 style=g_mivf_settings.transport_style;
                if(down&KEY_DLEFT) style=(style+MIVF_DEVICE_UI_STYLE_COUNT-1u)%MIVF_DEVICE_UI_STYLE_COUNT;
                else style=(style+1u)%MIVF_DEVICE_UI_STYLE_COUNT;
                g_mivf_settings.transport_style=style;
                g_mivf_theme_generation++;
                snprintf(value,sizeof(value),"TRANSPORT %s",mivf_transport_style_name(style));
            }
            break;
        case 25: {
            /* P.A1: left-channel attenuation, 0..100, attenuation only. */
            int step = (down & KEY_DLEFT) ? -5 : 5;
            int next = (int)g_mivf_settings.left_gain_percent + step;
            if (next < 0) next = 0;
            if (next > 100) next = 100;
            g_mivf_settings.left_gain_percent = (u32)next;
            if (audio.channels == 1) {
                /* value[] is 32 bytes -- keep this short enough to never truncate
                   (worst case "100% (MONO: NO EFFECT)" is 23 bytes + nul). */
                snprintf(value, sizeof(value), "%d%% (MONO: NO EFFECT)", next);
            } else {
                snprintf(value, sizeof(value), "L GAIN %d%%", next);
            }
            break;
        }
        case 26: {
            /* P.A1: right-channel attenuation, 0..100, attenuation only. */
            int step = (down & KEY_DLEFT) ? -5 : 5;
            int next = (int)g_mivf_settings.right_gain_percent + step;
            if (next < 0) next = 0;
            if (next > 100) next = 100;
            g_mivf_settings.right_gain_percent = (u32)next;
            if (audio.channels == 1) {
                snprintf(value, sizeof(value), "%d%% (MONO: NO EFFECT)", next);
            } else {
                snprintf(value, sizeof(value), "R GAIN %d%%", next);
            }
            break;
        }
        case 27:
            /* P.A1: one discoverable, always-reachable way back from an
               accidental 0%/0% (or any other) balance -- deliberately only
               responds to KEY_A, so stray D-pad presses on this row are a
               harmless no-op rather than an accidental reset. */
            if (down & KEY_A) {
                g_mivf_settings.left_gain_percent = 100u;
                g_mivf_settings.right_gain_percent = 100u;
                snprintf(value, sizeof(value), "BALANCE RESET (100%%/100%%)");
            }
            break;
        case 28: {
            /* Idle delay before the screensaver activates, in frames
               (300..7200 = 5s..120s at 60fps); step matches the existing
               DIM TIME row's 5-second granularity for consistency. */
            int step = (down & KEY_DLEFT) ? -300 : 300;
            int next = (int)g_mivf_settings.screensaver_idle_frames + step;
            if (next < 300) next = 300;
            if (next > 7200) next = 7200;
            g_mivf_settings.screensaver_idle_frames = (u32)next;
            snprintf(value, sizeof(value), "SAVER IDLE %lus", (unsigned long)(g_mivf_settings.screensaver_idle_frames / 60u));
            break;
        }
        case 29: {
            int step = (down & KEY_DLEFT) ? -1 : 1;
            int next = (int)g_mivf_settings.screensaver_speed + step;
            if (next < 1) next = 1;
            if (next > 5) next = 5;
            g_mivf_settings.screensaver_speed = (u32)next;
            snprintf(value, sizeof(value), "SAVER SPEED %lu", (unsigned long)g_mivf_settings.screensaver_speed);
            break;
        }
        case 30: {
            int step = (down & KEY_DLEFT) ? -5 : 5;
            int next = (int)g_mivf_settings.screensaver_fade_frames + step;
            if (next < 0) next = 0;
            if (next > 60) next = 60;
            g_mivf_settings.screensaver_fade_frames = (u32)next;
            snprintf(value, sizeof(value), "SAVER FADE %.2fs", (double)g_mivf_settings.screensaver_fade_frames / 60.0);
            break;
        }
        case 31:
            g_mivf_settings.screensaver_reduce_motion = !g_mivf_settings.screensaver_reduce_motion;
            snprintf(value, sizeof(value), "SAVER MOTION %s", g_mivf_settings.screensaver_reduce_motion ? "STATIC" : "NORMAL");
            break;
        case 32:
            /* Same discoverable, accident-resistant reset pattern as
               BALANCE RESET (case 27) -- KEY_A only, restores the exact
               defaults MIVF_SettingsInit already uses. */
            if (down & KEY_A) {
                g_mivf_settings.screensaver_idle_frames = 1200u;
                g_mivf_settings.screensaver_speed = 2u;
                g_mivf_settings.screensaver_fade_frames = 0u;
                g_mivf_settings.screensaver_reduce_motion = false;
                snprintf(value, sizeof(value), "SCREENSAVER RESET");
            }
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

    if (!g_hfix59r3_settings_visible) { return; }
    if (g_mivf_theme_picker.active) { mivf_theme_picker_draw(fb); return; }
    if (g_mivf_cvd_picker.active) { mivf_cvd_picker_draw(fb); return; }
    if (g_mivf_transport_picker.active) { mivf_transport_picker_draw(fb); return; }

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
            hfix58_rect565(fb, 22, y - 2, g_mivf_theme_palette.strong_focus ? 7 : 4, 12, g_mivf_theme_r, g_mivf_theme_g, g_mivf_theme_b);
            hfix58_draw_text_shadow(fb, 27, y, ">", 1, 255, 255, 255);
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

/* ------------------------------------------------------------------------- */
/* HFIX62: in-app controls/keybinds help screen.                             */
/*                                                                           */
/* Reachable two ways: pressing X in the file browser (X is otherwise unused */
/* there), or selecting the CONTROLS row at the end of the Settings menu     */
/* during playback (X is already bound to playback speed there, so it isn't */
/* reused as a direct hotkey in that context). Read-only, scrollable list;  */
/* content here must stay in sync with the actual key handling below if      */
/* those bindings ever change.                                              */
/* ------------------------------------------------------------------------- */
typedef struct {
    const char *label;
    const char *desc;
    bool header;
} Hfix62HelpRow;

static const Hfix62HelpRow g_hfix62_help_rows[] = {
    { "BROWSER", NULL, true },
    { "D-PAD UP/DOWN", "Move selection", false },
    { "A", "Open file", false },
    { "Y", "Toggle favorite", false },
    { "HOLD Y", "Mark watched / unwatched", false },
    { "L / R", "Cycle sort: name / added / played / series", false },
    { "HOLD SELECT", "Cycle filter: all / unwatched / in progress / watched", false },
    { "SELECT", "Toggle show-all folders", false },
    { "HOLD X", "Search library by name", false },
    { "X", "Open this help screen", false },
    { "B / START", "Exit app", false },

    { "PLAYBACK", NULL, true },
    { "A", "Play / pause", false },
    { "LEFT / RIGHT", "Seek -/+ 5 seconds", false },
    { "TOUCH + DRAG", "Scrub timeline", false },
    { "X", "Cycle playback speed", false },
    { "Y", "Cycle subtitle track", false },
    { "B", "A/B loop: set A, set B, clear", false },
    { "HOLD L+R", "Lock / unlock touch screen", false },
    { "SELECT", "Open settings", false },
    { "START", "Stop, return to browser", false },

    { "HOLD L +", NULL, true },
    { "UP / DOWN", "Volume +/- 10%", false },
    { "RIGHT", "Toggle forced stereo", false },
    { "LEFT", "Toggle audio limiter", false },

    { "HOLD R +", NULL, true },
    { "UP / DOWN", "Screen brightness", false },
    { "LEFT / RIGHT", "Previous / next chapter", false },

    { "SETTINGS MENU", NULL, true },
    { "D-PAD UP/DOWN", "Move selection", false },
    { "A / LEFT / RIGHT", "Change value", false },
    { "B / SELECT", "Close (saves)", false },

    { "THIS SCREEN", NULL, true },
    { "D-PAD UP/DOWN", "Scroll", false },
    { "B / START / X", "Close", false },
};

#define HFIX62_HELP_ROW_COUNT ((int)(sizeof(g_hfix62_help_rows) / sizeof(g_hfix62_help_rows[0])))
#define HFIX62_HELP_VISIBLE 13

/* Mirrors hfix59r3_set_settings_open's pause/resume bookkeeping, sharing
   g_hfix59r3_resume_after_settings so a Settings -> Help transition (see the
   CONTROLS row handling in hfix59r3_handle_settings_menu) doesn't trigger a
   second, incorrect pause/resume capture -- the guard below only fires when
   neither menu was already open. */
static void hfix62_set_help_open(bool open) {
    if (open) {
        if (!g_hfix62_help_visible && !g_hfix59r3_settings_visible &&
            g_media_ctl.state == STATE_PLAYING) {
            g_hfix59r3_resume_after_settings = true;
            g_media_ctl.state = STATE_PAUSED;
            if (audio_can_use_ndsp()) {
                ndspChnSetPaused(0, true);
            }
        }

        g_hfix62_help_visible = true;
        g_hfix62_help_scroll = 0;
        g_media_ctl.ui_visible = true;
    } else {
        g_hfix62_help_visible = false;

        if (g_hfix59r3_resume_after_settings) {
            g_media_ctl.state = STATE_PLAYING;
            if (audio_can_use_ndsp()) {
                ndspChnSetPaused(0, false);
            }
        }
    }
}

static bool hfix62_handle_help_menu(u32 down) {
    if (!g_hfix62_help_visible) {
        return false;
    }

    if ((down & KEY_B) || (down & KEY_START) || (down & KEY_X)) {
        hfix62_set_help_open(false);
        return true;
    }

    if (down & KEY_DUP) {
        if (g_hfix62_help_scroll > 0) {
            g_hfix62_help_scroll--;
        }
        return true;
    }

    if (down & KEY_DDOWN) {
        int max_scroll = HFIX62_HELP_ROW_COUNT - HFIX62_HELP_VISIBLE;
        if (max_scroll < 0) {
            max_scroll = 0;
        }
        if (g_hfix62_help_scroll < max_scroll) {
            g_hfix62_help_scroll++;
        }
        return true;
    }

    return true;
}

static void hfix62_draw_help_overlay(u8 *fb) {
    if (!g_hfix62_help_visible) {
        return;
    }

    int max_first = HFIX62_HELP_ROW_COUNT - HFIX62_HELP_VISIBLE;
    if (max_first < 0) {
        max_first = 0;
    }

    int first = g_hfix62_help_scroll;
    if (first > max_first) {
        first = max_first;
    }
    if (first < 0) {
        first = 0;
    }

    hfix58_rect565(fb, 14, 6, 292, 228, 8, 13, 22);
    hfix58_rect565(fb, 16, 8, 288, 224, 18, 28, 44);
    hfix58_rect565(fb, 16, 8, 288, 14, g_mivf_theme_r, g_mivf_theme_g, g_mivf_theme_b);
    hfix58_rect565(fb, 16, 23, 288, 1, 40, 95, 135);

    hfix58_draw_text_shadow(fb, 24, 10, "CONTROLS", 1, 245, 255, 255);
    hfix58_draw_text_shadow(fb, 150, 10, "UP/DOWN SCROLL  B CLOSE", 1, 235, 245, 255);

    for (int row = 0; row < HFIX62_HELP_VISIBLE; row++) {
        int i = first + row;
        int y = 30 + row * 14;

        if (i >= HFIX62_HELP_ROW_COUNT) {
            break;
        }

        const Hfix62HelpRow *r = &g_hfix62_help_rows[i];

        if (r->header) {
            hfix58_rect565(fb, 22, y - 2, 266, 12, 26, 50, 78);
            hfix58_draw_text_shadow(fb, 26, y, r->label, 1, g_mivf_theme_r, g_mivf_theme_g, g_mivf_theme_b);
        } else {
            hfix58_draw_text_shadow(fb, 30, y, r->label, 1, 200, 216, 236);
            hfix58_draw_text_shadow(fb, 150, y, r->desc, 1, 170, 190, 214);
        }
    }

    if (HFIX62_HELP_ROW_COUNT > HFIX62_HELP_VISIBLE) {
        int track_h = 178;
        int knob_h = (track_h * HFIX62_HELP_VISIBLE) / HFIX62_HELP_ROW_COUNT;
        int knob_y = 30;
        if (knob_h < 18) {
            knob_h = 18;
        }
        if (max_first > 0) {
            knob_y += ((track_h - knob_h) * first) / max_first;
        }
        hfix58_rect565(fb, 292, 30, 4, track_h, 30, 36, 52);
        hfix58_rect565(fb, 292, knob_y, 4, knob_h, g_mivf_theme_r, g_mivf_theme_g, g_mivf_theme_b);
    }

    hfix58_rect565(fb, 22, 212, 266, 1, 44, 66, 96);
    hfix58_draw_text_shadow(fb, 24, 216, "D-PAD UP/DOWN SCROLL", 1, 208, 228, 246);
    hfix58_draw_text_shadow(fb, 24, 226, "B, START, OR X CLOSES", 1, 208, 228, 246);
}

static void hfix58d_draw_bottom_fluent_ui(u8 *fb);
static void hfix58s_draw_subtitle_overlay_top(u8 *fb);

/* HFIX58F_R2_FORWARD_DECLS */
static void hfix58f_tick_seek_ui_tail(void);
static void wait_stream_prebuffer(MivfStream *stream);

static void hfix58d_anim_tick(void);
static bool hfix58d_anim_needs_redraw(void);
static void hfix58d_notify_input(u32 down, u32 held);
/* MIVF_TRANSPORT_C27_EARLY_DECLS_V1 */
static u32 g_mivf_c27_anim_tick = 0;
static u32 mivf_c21_style_id(void);
static bool mivf_c27_style_animated(u32 style);

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
/* MIVF_PHASE6_AUTODIM_LEVEL_V1
   Apply the configured DIM LEVEL to the bottom framebuffer only. Level 1 is
   exactly the historical 50% dim, preserving the old/default appearance;
   levels 2..5 retain progressively more light (about 60, 70, 80, 90%).
   Q8 factors avoid per-pixel division on Old 3DS. */
static void hfix59r3_dim_bottom_framebuffer(u8 *fb, u16 fw, u16 fh) {
    static const u16 keep_q8[5] = { 128u, 154u, 179u, 205u, 230u };
    u32 level = g_mivf_settings.autodim_brightness;
    u16 factor;
    u16 *px = (u16 *)fb;
    size_t count = (size_t)fw * (size_t)fh;

    if (!fb) return;
    if (level < 1u) level = 1u;
    if (level > 5u) level = 5u;
    factor = keep_q8[level - 1u];

    for (size_t i = 0; i < count; i++) {
        u16 c = px[i];
        u16 r = (u16)((((u32)((c >> 11) & 0x1Fu)) * factor) >> 8);
        u16 g = (u16)((((u32)((c >> 5) & 0x3Fu)) * factor) >> 8);
        u16 b = (u16)((((u32)(c & 0x1Fu)) * factor) >> 8);
        px[i] = (u16)((r << 11) | (g << 5) | b);
    }
}

static void hfix51c_draw_bottom_ui(void) {
    u16 fw = 0;
    u16 fh = 0;
    u8 *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &fw, &fh);

    hfix58d_draw_bottom_fluent_ui(fb);

    if (g_mivf_brightness_dimmed) {
        hfix59r3_dim_bottom_framebuffer(fb, fw, fh);
    }
}

static void hfix51c_draw_bottom_ui_throttled(void) {
    static u32 last_frame_idx = 0xFFFFFFFFu;
    static u32 hfix58f_last_rendered_sec = 0xFFFFFFFFu;
    static MediaPlaybackState last_state = STATE_STOPPED;
    static int last_dummy_seek_state = 999;
    static bool last_visible = false;
    static bool last_settings_visible = false;
    static int last_settings_index = -1;
    static bool last_help_visible = false;
    static int last_help_scroll = -1;
    static int force_redraw_frames = 2;
    static bool last_dimmed = false;
    static u32 last_theme_generation = 0;

    /* HFIX58D_THROTTLER_TICK */
    hfix58d_anim_tick();
    g_mivf_c27_anim_tick++;

    /* MIVF_RECEIVER_AUDIO_REACTIVE_METERS_V1: presentation-only meter release.
       The Receiver meters are driven by the exact post-mix PCM in
       audio_queue_raw_ndsp, which only runs while packets flow. When playback
       is paused, stopped, seeking, or scanning (FF/RW) no packets arrive, so
       settle both meters toward zero here each frame instead of letting them
       freeze at their last level. Same 7/8 release ratio as the packet path,
       snapping sub-32 residue to zero so they fully reach silence. This mutates
       only the two UI meter accumulators -- never PCM, NDSP, the audio clock,
       seeking, decoding, or any timing/presentation state. */
    if (g_media_ctl.state != STATE_PLAYING) {
        g_audio_meter_left  = (g_audio_meter_left  * 7u) / 8u;
        g_audio_meter_right = (g_audio_meter_right * 7u) / 8u;
        if (g_audio_meter_left  < 32u) g_audio_meter_left  = 0;
        if (g_audio_meter_right < 32u) g_audio_meter_right = 0;
    }

    if (mivf_c27_style_animated(mivf_c21_style_id()) &&
        !g_hfix59r3_settings_visible && !g_hfix62_help_visible &&
        ((g_mivf_c27_anim_tick % 3u) == 0u)) {
        force_redraw_frames = 1;
    }
    if (hfix58d_anim_needs_redraw()) {
        force_redraw_frames = 2;
    }
    static int last_hfix58b_selected_index = -1;
    static bool last_hfix58b_play_pressed = false;
    static Hfix57TouchButton last_hfix57_touch_button = HFIX57_TOUCH_NONE;

    if (!g_media_ctl.ui_visible) {
        last_visible = false;
        last_settings_visible = false;
        last_help_visible = false;
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

    if (g_hfix62_help_visible) {
        force_redraw_frames = 2;
    }

    if (g_hfix62_help_visible != last_help_visible ||
        g_hfix62_help_scroll != last_help_scroll) {
        force_redraw_frames = 2;
        last_help_visible = g_hfix62_help_visible;
        last_help_scroll = g_hfix62_help_scroll;
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

    if (last_theme_generation != g_mivf_theme_generation) { force_redraw_frames=2; last_theme_generation=g_mivf_theme_generation; }
    /* HFIX_BOTTOMDIM: force a redraw as soon as the autodim state flips
       so the bottom-screen dim overlay (or its removal on wake) shows up
       promptly instead of waiting for an unrelated state change. */
    if (g_mivf_brightness_dimmed != last_dimmed) {
        force_redraw_frames = 2;
        last_dimmed = g_mivf_brightness_dimmed;
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

/* Overflow-safe sum of three u32 values into a size_t. On the real 3DS
   target, size_t is 32 bits, so a naive "(size_t)a + (size_t)b + (size_t)c"
   can silently wrap past a small value before any bounds check runs --
   this checks each addition against the type's max before performing it,
   so a corrupted/huge individual value can never hide behind a small
   wrapped total. Returns false (leaving *out_sum untouched) if the true
   sum would not fit in a size_t. */
static bool mivf_u32_sum3(u32 a, u32 b, u32 c, size_t *out_sum) {
    size_t sum = (size_t)a;

    if ((size_t)b > (size_t)-1 - sum) {
        return false;
    }
    sum += (size_t)b;

    if ((size_t)c > (size_t)-1 - sum) {
        return false;
    }
    sum += (size_t)c;

    *out_sum = sum;
    return true;
}

/* Validates the three M2Y1 plane payload sizes (y/cb/cr) before they are
   used as buffer lengths, per the packet layout "[header_bytes header]
   [y_payload bytes][cb_payload bytes][cr_payload bytes]":
     1. each individually must fit within the packet body remaining after
        the fixed header prefix -- catches a single corrupted huge field
        even before any sum is formed;
     2. their sum must not overflow (see mivf_u32_sum3);
     3. header_bytes + the sum must not exceed n.
   All three are explicit/independent checks (not merely implied by
   evaluation order), so this stays correct even if reused or reordered.
   For a valid, non-corrupted packet these checks are exactly as
   permissive as the single "header_bytes + y + cb + cr <= n" check this
   replaces -- decode behavior for valid files is unchanged. */
static bool mivf_m2y1_plane_sizes_ok(size_t header_bytes, u32 y, u32 cb, u32 cr, size_t n) {
    if (header_bytes > n) {
        return false;
    }

    size_t remaining = n - header_bytes;

    if ((size_t)y > remaining || (size_t)cb > remaining || (size_t)cr > remaining) {
        return false;
    }

    size_t sum;

    if (!mivf_u32_sum3(y, cb, cr, &sum)) {
        return false;
    }

    if (sum > remaining) {
        return false;
    }

    if (header_bytes + sum > n) {
        return false;
    }

    return true;
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

    if (!mivf_m2y1_plane_sizes_ok(28u, y_payload, cb_payload, cr_payload, n)) {
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

    /* Unlike M2Y1's plane sizes, y_raw/cb_raw/cr_raw describe *decompressed*
       sizes, which are legitimately allowed to exceed the compressed
       packet's own size (comp) -- that's the point of compression -- so
       there is no "remaining packet body" bound to check here, only that
       the sum itself does not silently overflow before the implausible-
       size cap below gets a chance to reject it (that overflow previously
       let a corrupted/huge individual value hide behind a small wrapped
       total and under-allocate g_m2y2_raw). */
    size_t raw_total;

    if (!mivf_u32_sum3(y_raw, cb_raw, cr_raw, &raw_total)) {
        return -9;
    }

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

        g_mivf_settings.volume_percent = (u32)g_hfix56_volume_percent;
        MIVF_SettingsSave(&g_mivf_settings);
        char hfix58_tmp[64]; snprintf(hfix58_tmp, sizeof(hfix58_tmp), "VOLUME %d%%", g_hfix56_volume_percent); hfix58_alert_set(hfix58_tmp, 1);
    }

    if (down & KEY_DDOWN) {
        g_hfix56_volume_percent -= 10;

        if (g_hfix56_volume_percent < 0) {
            g_hfix56_volume_percent = 0;
        }

        g_mivf_settings.volume_percent = (u32)g_hfix56_volume_percent;
        MIVF_SettingsSave(&g_mivf_settings);
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
#define HFIX58_BROWSER_VISIBLE_ROWS 7
#define HFIX58_PREVIEW_W 88
#define HFIX58_PREVIEW_H 50
/* MIVF_PHASE8_1_VISUAL_POLISH_V1
   High-quality browser preview layer. Legacy .cover files remain
   88x50 RGB565, but the browser now caches and draws a 176x100
   version to avoid repeated nearest-neighbor 2X enlargement. */
#define HFIX58_PREVIEW2_W 176
#define HFIX58_PREVIEW2_H 100
#define MIVF8_PREVIEW_CACHE_SLOTS 4

typedef struct {
    char name[256];
    char path[HFIX58_MAX_PATH];
    u8 quick; /* 0 = library, 1 = recent, 2 = favorite */
    u8 watch_status; /* MivfWatchStatus, cached once per scan -- see
                         hfix_watchstate_effective(); avoids a per-frame
                         file read during redraw. */
    u32 added_unix; /* stable "date added", cached once per scan -- see
                        hfix_added_dates_get_or_set(). */
    u32 last_played_unix; /* 0 = never played; cached once per scan from
                              MivfWatchState.last_played_unix. */
    bool series_ok;      /* true only if hfix_series_parse recognized a
                             real "SxxEyy" pattern in the filename --
                             never silently assigned when uncertain. */
    char series_name[64];
    u32 series_season;
    u32 series_episode;
    bool series_duplicate; /* true if another entry shares the identical
                               series+season+episode (set after sorting). */
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
    u32 file_size_kb; /* populated lazily when this path is previewed, zero if unknown */
    /* MIVF_PHASE8_PREVIEW_PROGRESS_V1 */
    u32 bookmark_frame;
    u32 total_frames;
} Hfix58BrowserPreview;

static Hfix58FileBrowser g_hfix58_browser;
static Hfix58BrowserPreview g_hfix58_preview;
typedef struct {
    bool valid;
    char path[HFIX58_MAX_PATH];
    u32 stamp;
    u16 pixels[HFIX58_PREVIEW2_W * HFIX58_PREVIEW2_H];
} Mivf8PreviewCacheEntry;
static Mivf8PreviewCacheEntry g_mivf8_preview_cache[MIVF8_PREVIEW_CACHE_SLOTS];
static u16 g_mivf8_preview_hi[HFIX58_PREVIEW2_W * HFIX58_PREVIEW2_H];
static bool g_mivf8_preview_hi_valid = false;
static u32 g_mivf8_preview_cache_tick = 1;
static char g_mivf8_preview_hi_path[HFIX58_MAX_PATH];

/* HFIX60: preview debounce deadline in system ticks (0 = disabled).
   Cursor movement sets a ~200 ms deadline; the preview is only loaded
   once the selection has been stable for that interval. */
static u64 g_hfix58_preview_deadline = 0;

/* HFIX60: show-all toggle — when true the browser scans the SD root
   first so files outside the dedicated media folders are visible.
   Toggled with SELECT in the file browser. */
static bool g_hfix58_show_all_dirs = false;

/* HFIX60: lightweight performance counters for debug overlay.
   Updated every frame; max/last values shown when debug overlay
   is enabled in Settings.  No file I/O, negligible overhead. */
static u64 g_perf_decode_us_max = 0;
static u64 g_perf_blit_us_max = 0;
static u64 g_perf_page_us_max = 0;
static u64 g_perf_audio_gap_ms_max = 0;
static u64 g_perf_audio_gap_tick = 0;
static u32 g_perf_late_count = 0;

static void hfix58_perf_diag_reset(void) {
    g_perf_decode_us_max = 0;
    g_perf_blit_us_max = 0;
    g_perf_page_us_max = 0;
    g_perf_audio_gap_ms_max = 0;
    g_perf_audio_gap_tick = 0;
    g_perf_late_count = 0;
}

static char g_hfix58_alert_text[96] = "";
static int  g_hfix58_alert_level = 0;
static u32  g_hfix58_alert_frames = 0;

/* Set when play() fails; consumed (and shown as a real alert) the next
   time hfix58_file_browser_select() is entered, right after its own
   unconditional hfix58_alert_clear() -- see both sites. */
static char g_mivf_pending_error_alert[96] = "";

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

/* HFIX_SHOWCASE_SCREEN_FILL_BUG_V2: hfix58_rect565() above hardcodes an
   x2>320 clamp that's correct for every existing (bottom-screen or
   320-wide-layout) call site, but silently clips any fill requested wider
   than 320 -- including the Showcase title card / no-project message's
   own 400px-wide top-screen fill, which is why the v1 fix (widening the
   call-site arguments to 400) still left the rightmost 80px of the real
   top screen un-cleared on actual hardware/emulator. Rather than change
   the shared 320-clamped primitive (used pervasively elsewhere and
   correct there), this is a dedicated variant clamped to the top
   screen's real 400px width, used only where a full top-screen fill is
   actually intended. */
static void hfix58_rect565_top(u8 *fb, int x, int y, int w, int h, int r, int g, int b) {
    u16 c = hfix58_rgb565(r, g, b);

    int x2 = x + w;
    int y2 = y + h;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > 400) x2 = 400;
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
    /* HFIX70: apostrophe -- also used for straight double-quote (") so
       quoted chapter/menu titles like "Life Is a Highway" don't fall
       through to the qn/"?" glyph below. */
    static const u8 apos[7] = {12,12,8,0,0,0,0};

    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 32);
    }

    if (c == ' ') return sp;
    if (c == '.') return dot;
    if (c == '/') return slash;
    if (c == '-') return dash;
    if (c == ':') return colon;
    if (c == '_') return us;
    if (c == '\'' || c == '"') return apos;

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

/* HFIX71: top-screen translucent blend, mirroring hfix58_blend_px565/
   hfix58_blend_rect565 exactly but addressed for the 400x240 top framebuffer
   (TOP_W/TOP_H) instead of the 320x240 bottom one. Needed now that menu
   buttons/panels render on the top screen. */
static inline void hfix58s_top_blend_px565(u8 *fb8, int x, int y, int r, int g, int b, int a) {
    if (!fb8 || x < 0 || x >= TOP_W || y < 0 || y >= TOP_H) {
        return;
    }

    if (a <= 0) {
        return;
    }

    if (a >= 255) {
        hfix58s_top_px565(fb8, x, y, hfix58_rgb565(r, g, b));
        return;
    }

    u16 *fb = (u16*)fb8;
    int idx = x * TOP_H + (TOP_H - 1 - y);

    int dr, dg, db;
    hfix58_unpack565(fb[idx], &dr, &dg, &db);

    int nr = (dr * (255 - a) + r * a) / 255;
    int ng = (dg * (255 - a) + g * a) / 255;
    int nb = (db * (255 - a) + b * a) / 255;

    fb[idx] = hfix58_rgb565(nr, ng, nb);
}

static void hfix58s_top_blend_rect565(u8 *fb, int x, int y, int w, int h, int r, int g, int b, int a) {
    int x2 = x + w;
    int y2 = y + h;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > TOP_W) x2 = TOP_W;
    if (y2 > TOP_H) y2 = TOP_H;

    for (int xx = x; xx < x2; xx++) {
        for (int yy = y; yy < y2; yy++) {
            hfix58s_top_blend_px565(fb, xx, yy, r, g, b, a);
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

    int rr = g_mivf_theme_palette.info_r;
    int gg = g_mivf_theme_palette.info_g;
    int bb = g_mivf_theme_palette.info_b;
    int tr = 245;
    int tg = 250;
    int tb = 255;

    if (g_hfix58_alert_level == 1) {
        rr = g_mivf_theme_palette.success_r; gg = g_mivf_theme_palette.success_g; bb = g_mivf_theme_palette.success_b;
        kind = "OK";
        tr = 225; tg = 255; tb = 232;
    } else if (g_hfix58_alert_level == 2) {
        rr = g_mivf_theme_palette.warning_r; gg = g_mivf_theme_palette.warning_g; bb = g_mivf_theme_palette.warning_b;
        kind = "WARN";
        tr = 255; tg = 238; tb = 214;
    } else if (g_hfix58_alert_level == 3) {
        rr = g_mivf_theme_palette.danger_r; gg = g_mivf_theme_palette.danger_g; bb = g_mivf_theme_palette.danger_b;
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

/* ------------------------------------------------------------------------- */
/* Continue Watching index                                                   */
/*                                                                           */
/* An MRU-style flat list, deliberately mirroring g_mivf_recents' own        */
/* load/save/index/note pattern exactly rather than inventing a new one --   */
/* the only real difference is WHEN entries are added/removed (watch-state   */
/* transitions, not "file was opened"), and that entries are pruned on       */
/* completion instead of only capped by MRU eviction.                       */
/* ------------------------------------------------------------------------- */
#define MIVF_CONTINUE_MAX 16
#define MIVF_RECENTLY_ADDED_SHOW_COUNT 5
static char g_mivf_continue_watching[MIVF_CONTINUE_MAX][HFIX58_MAX_PATH];
static int g_mivf_continue_watching_count = 0;
static bool g_mivf_continue_watching_loaded = false;

static void hfix_continue_save(void) {
    FILE *fp;

    MIVF_AppDataEnsureLayout();
    fp = fopen(MIVF_CONTINUE_WATCHING_PATH, "wb");
    if (!fp) {
        return;
    }
    for (int i = 0; i < g_mivf_continue_watching_count; i++) {
        fprintf(fp, "%s\n", g_mivf_continue_watching[i]);
    }
    fclose(fp);
}

static void hfix_continue_load(void) {
    FILE *fp;
    char line[HFIX58_MAX_PATH];

    g_mivf_continue_watching_count = 0;
    g_mivf_continue_watching_loaded = true;

    MIVF_AppDataEnsureLayout();

    fp = fopen(MIVF_CONTINUE_WATCHING_PATH, "rb");
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
        if (g_mivf_continue_watching_count >= MIVF_CONTINUE_MAX) {
            break;
        }
        snprintf(g_mivf_continue_watching[g_mivf_continue_watching_count], HFIX58_MAX_PATH, "%s", line);
        g_mivf_continue_watching_count++;
    }

    fclose(fp);
}

static int hfix_continue_index(const char *path) {
    if (!path) {
        return -1;
    }
    if (!g_mivf_continue_watching_loaded) {
        hfix_continue_load();
    }
    for (int i = 0; i < g_mivf_continue_watching_count; i++) {
        if (!strcmp(g_mivf_continue_watching[i], path)) {
            return i;
        }
    }
    return -1;
}

/* Add (or move to front, never duplicating -- "no duplicate titles") a
   title that now has real in-progress watch-state. */
static void hfix_continue_note(const char *path) {
    int idx;

    if (!path || !*path) {
        return;
    }
    if (!g_mivf_continue_watching_loaded) {
        hfix_continue_load();
    }

    idx = hfix_continue_index(path);
    if (idx == 0) {
        return;
    }

    if (idx > 0) {
        for (int i = idx; i > 0; i--) {
            snprintf(g_mivf_continue_watching[i], HFIX58_MAX_PATH, "%s", g_mivf_continue_watching[i - 1]);
        }
    } else {
        if (g_mivf_continue_watching_count < MIVF_CONTINUE_MAX) {
            g_mivf_continue_watching_count++;
        }
        for (int i = g_mivf_continue_watching_count - 1; i > 0; i--) {
            snprintf(g_mivf_continue_watching[i], HFIX58_MAX_PATH, "%s", g_mivf_continue_watching[i - 1]);
        }
    }

    snprintf(g_mivf_continue_watching[0], HFIX58_MAX_PATH, "%s", path);
    hfix_continue_save();
}

/* Automatic removal on completion (or a manual unwatched reset) -- the
   row must never keep showing a title that's no longer actually "in
   progress". */
static void hfix_continue_remove(const char *path) {
    int idx = hfix_continue_index(path);

    if (idx < 0) {
        return;
    }

    for (int i = idx; i < g_mivf_continue_watching_count - 1; i++) {
        snprintf(g_mivf_continue_watching[i], HFIX58_MAX_PATH, "%s", g_mivf_continue_watching[i + 1]);
    }
    g_mivf_continue_watching_count--;
    hfix_continue_save();
}

/* ------------------------------------------------------------------------- */
/* Recently Added -- stable "date added" index                               */
/*                                                                           */
/* "Added" is defined as: the first time this scanner ever saw the file.     */
/* Stored once, never updated by a later rescan (a rescan must not treat an  */
/* already-known file as newly added). For a file's FIRST-ever encounter,    */
/* the initial stamp prefers the file's real filesystem mtime (a meaningful  */
/* proxy for "when it was copied onto the card") over "now" -- this matters  */
/* specifically for the very first scan after this feature ships: without   */
/* it, every pre-existing file in the library would otherwise all receive   */
/* the exact same "now" timestamp and appear equally "new" forever. Capped  */
/* at HFIX58_MAX_BROWSER_FILES entries, matching the library's own cap --   */
/* bounded, not unlimited growth.                                          */
/* ------------------------------------------------------------------------- */
static char g_mivf_added_dates_path[HFIX58_MAX_BROWSER_FILES][HFIX58_MAX_PATH];
static u32 g_mivf_added_dates_ts[HFIX58_MAX_BROWSER_FILES];
static int g_mivf_added_dates_count = 0;
static bool g_mivf_added_dates_loaded = false;

static void hfix_added_dates_load(void) {
    FILE *fp;
    char line[HFIX58_MAX_PATH + 32];

    g_mivf_added_dates_count = 0;
    g_mivf_added_dates_loaded = true;

    MIVF_AppDataEnsureLayout();

    fp = fopen(MIVF_ADDED_DATES_PATH, "rb");
    if (!fp) {
        return;
    }

    while (fgets(line, sizeof(line), fp)) {
        char *sep = strrchr(line, '|');
        size_t n;

        if (!sep) {
            continue;
        }
        *sep = 0;
        n = strlen(line);
        if (n == 0 || n >= HFIX58_MAX_PATH) {
            continue;
        }
        if (g_mivf_added_dates_count >= HFIX58_MAX_BROWSER_FILES) {
            break;
        }

        snprintf(g_mivf_added_dates_path[g_mivf_added_dates_count], HFIX58_MAX_PATH, "%s", line);
        g_mivf_added_dates_ts[g_mivf_added_dates_count] = (u32)strtoul(sep + 1, NULL, 10);
        g_mivf_added_dates_count++;
    }

    fclose(fp);
}

static void hfix_added_dates_save(void) {
    FILE *fp;

    MIVF_AppDataEnsureLayout();
    fp = fopen(MIVF_ADDED_DATES_PATH, "wb");
    if (!fp) {
        return;
    }
    for (int i = 0; i < g_mivf_added_dates_count; i++) {
        fprintf(fp, "%s|%lu\n", g_mivf_added_dates_path[i], (unsigned long)g_mivf_added_dates_ts[i]);
    }
    fclose(fp);
}

/* Returns the stable added-date for `path`, creating and persisting one
   (mtime-preferred, see block comment above) if this is genuinely the
   first time it's been seen. A full index (HFIX58_MAX_BROWSER_FILES
   already-tracked files) silently declines to add more rather than
   growing unbounded -- a real, documented bound, not a crash risk. */
static u32 hfix_added_dates_get_or_set(const char *path) {
    if (!path || !*path) {
        return 0;
    }

    if (!g_mivf_added_dates_loaded) {
        hfix_added_dates_load();
    }

    for (int i = 0; i < g_mivf_added_dates_count; i++) {
        if (!strcmp(g_mivf_added_dates_path[i], path)) {
            return g_mivf_added_dates_ts[i];
        }
    }

    if (g_mivf_added_dates_count >= HFIX58_MAX_BROWSER_FILES) {
        return (u32)time(NULL);
    }

    {
        struct stat st;
        u32 stamp = (stat(path, &st) == 0) ? (u32)st.st_mtime : (u32)time(NULL);

        snprintf(g_mivf_added_dates_path[g_mivf_added_dates_count], HFIX58_MAX_PATH, "%s", path);
        g_mivf_added_dates_ts[g_mivf_added_dates_count] = stamp;
        g_mivf_added_dates_count++;
        hfix_added_dates_save();
        return stamp;
    }
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

/* MIVF_PHASE6_NATURAL_SORT_V1
   Compare ASCII names case-insensitively while treating each run of decimal
   digits as an integer. No integer conversion is used, so arbitrarily long
   digit runs cannot overflow. A bytewise strcmp fallback makes ordering fully
   deterministic when the natural comparison considers two names equivalent. */
static int hfix58_natural_name_cmp(const char *a, const char *b) {
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;

    while (*pa && *pb) {
        if (isdigit(*pa) && isdigit(*pb)) {
            const unsigned char *za = pa;
            const unsigned char *zb = pb;
            const unsigned char *ea;
            const unsigned char *eb;
            size_t lena, lenb;

            while (*za == '0') za++;
            while (*zb == '0') zb++;
            ea = za;
            eb = zb;
            while (isdigit(*ea)) ea++;
            while (isdigit(*eb)) eb++;
            lena = (size_t)(ea - za);
            lenb = (size_t)(eb - zb);

            /* A run containing only zeroes has numeric length zero. */
            if (lena != lenb) return lena < lenb ? -1 : 1;
            if (lena > 0) {
                int digit_cmp = memcmp(za, zb, lena);
                if (digit_cmp != 0) return digit_cmp < 0 ? -1 : 1;
            }

            /* Equal numeric values: consume the complete original runs. */
            while (isdigit(*pa)) pa++;
            while (isdigit(*pb)) pb++;
            continue;
        }

        {
            int ca = tolower(*pa);
            int cb = tolower(*pb);
            if (ca != cb) return ca < cb ? -1 : 1;
        }
        pa++;
        pb++;
    }

    if (*pa != *pb) return *pa ? 1 : -1;
    return 0;
}

static int hfix58_file_cmp(const void *a, const void *b) {
    const Hfix58FileEntry *fa = (const Hfix58FileEntry*)a;
    const Hfix58FileEntry *fb = (const Hfix58FileEntry*)b;
    int natural = hfix58_natural_name_cmp(fa->name, fb->name);
    return natural != 0 ? natural : strcmp(fa->name, fb->name);
}

/* ------------------------------------------------------------------------- */
/* Library Sort / Filter / Search                                            */
/*                                                                           */
/* Available sort/filter keys are deliberately limited to what real,        */
/* trustworthy per-file data exists right now: title (the pre-existing      */
/* natural-sort default), date added (Phase 3), last played and watched     */
/* state (Phase 1). Year/genre/series/season/episode are explicitly NOT     */
/* offered here -- movie_info.py's genre/release_year fields are desktop-   */
/* only and never exported to the player, and series/season/episode        */
/* structure does not exist yet (a later phase). Offering a sort/filter key */
/* for data the player cannot actually read would silently do nothing or    */
/* guess, which this project's own standing rule forbids.                   */
/* ------------------------------------------------------------------------- */
#define MIVF_SEARCH_QUERY_MAX 64
static char g_hfix58_search_query[MIVF_SEARCH_QUERY_MAX] = {0};

/* qsort's comparator signature has no room for extra context (no
   qsort_r on this libc) -- set immediately before the one qsort call
   that needs it, exactly like existing code elsewhere in this file
   already does for similar constraints. */
static u32 g_hfix58_active_sort_mode = 0;

/* Series / Season / Episode -- filename-pattern inference only              */
/*                                                                           */
/* hfix58_scan_dir is flat and non-recursive (see its own definition) --    */
/* it never sees files nested under Show/Season subfolders, so genuine      */
/* folder-preserving series/season structure is not achievable without      */
/* adding recursive directory traversal, a materially larger architecture   */
/* change deliberately out of scope for this phase (see this phase's        */
/* locked-in design decision: additive only, never a scanner rewrite).      */
/* The actual "SxxEyy" parser lives in mivf_series_parse.h -- pure, no      */
/* <3ds.h> dependency, host-testable (tools/test_series_parse.c), same      */
/* precedent as mivf_bookmark_io.h. */
static bool hfix_series_parse(const char *name, bool *ok, char *series, size_t series_sz, u32 *season, u32 *episode) {
    *ok = mivf_series_parse(name, series, series_sz, season, episode);
    return *ok;
}

static int hfix58_file_cmp_by_series(const void *a, const void *b) {
    const Hfix58FileEntry *fa = (const Hfix58FileEntry*)a;
    const Hfix58FileEntry *fb = (const Hfix58FileEntry*)b;

    /* Recognized series episodes sort before unrecognized files
       (grouped, not interleaved), each group internally deterministic. */
    if (fa->series_ok != fb->series_ok) {
        return fa->series_ok ? -1 : 1;
    }
    if (fa->series_ok) {
        int name_cmp = strcasecmp(fa->series_name, fb->series_name);
        if (name_cmp != 0) {
            return name_cmp;
        }
        if (fa->series_season != fb->series_season) {
            return fa->series_season < fb->series_season ? -1 : 1;
        }
        if (fa->series_episode != fb->series_episode) {
            return fa->series_episode < fb->series_episode ? -1 : 1;
        }
    }
    return hfix58_file_cmp(a, b);
}

/* Duplicate detection: two entries claiming the identical series+season+
   episode. Only meaningful once entries are already series-sorted (so
   duplicates are adjacent) -- called right after that sort. */
static void hfix58_library_mark_series_duplicates(void) {
    for (int i = 0; i < g_hfix58_browser.count; i++) {
        g_hfix58_browser.entries[i].series_duplicate = false;
    }
    for (int i = 1; i < g_hfix58_browser.count; i++) {
        Hfix58FileEntry *prev = &g_hfix58_browser.entries[i - 1];
        Hfix58FileEntry *cur = &g_hfix58_browser.entries[i];
        if (prev->series_ok && cur->series_ok &&
            prev->series_season == cur->series_season &&
            prev->series_episode == cur->series_episode &&
            !strcasecmp(prev->series_name, cur->series_name)) {
            prev->series_duplicate = true;
            cur->series_duplicate = true;
        }
    }
}

static int hfix58_file_cmp_by_mode(const void *a, const void *b) {
    const Hfix58FileEntry *fa = (const Hfix58FileEntry*)a;
    const Hfix58FileEntry *fb = (const Hfix58FileEntry*)b;

    if (g_hfix58_active_sort_mode == 1) {
        /* Date Added, newest first; stable tiebreak on name. */
        if (fa->added_unix != fb->added_unix) {
            return fa->added_unix > fb->added_unix ? -1 : 1;
        }
    } else if (g_hfix58_active_sort_mode == 2) {
        /* Last Played, most-recent first; never-played (0) sorts last;
           stable tiebreak on name. */
        if (fa->last_played_unix != fb->last_played_unix) {
            return fa->last_played_unix > fb->last_played_unix ? -1 : 1;
        }
    }
    return hfix58_file_cmp(a, b);
}

static void hfix58_library_apply_sort_mode(void) {
    if (g_mivf_settings.library_sort_mode == 0 || g_hfix58_browser.count < 2) {
        return; /* Name mode: already sorted above, nothing further to do. */
    }
    if (g_mivf_settings.library_sort_mode == 3) {
        qsort(g_hfix58_browser.entries, g_hfix58_browser.count,
              sizeof(g_hfix58_browser.entries[0]), hfix58_file_cmp_by_series);
        hfix58_library_mark_series_duplicates();
        return;
    }
    g_hfix58_active_sort_mode = g_mivf_settings.library_sort_mode;
    qsort(g_hfix58_browser.entries, g_hfix58_browser.count,
          sizeof(g_hfix58_browser.entries[0]), hfix58_file_cmp_by_mode);
}

/* Shared compaction primitive both filter and search use: keep only
   entries `keep` returns true for, preserving relative order, adjusting
   count in place. Never reallocates -- operates within the existing
   fixed-size entries[] array. */
static void hfix58_library_compact(bool (*keep)(const Hfix58FileEntry *)) {
    int write = 0;

    for (int read = 0; read < g_hfix58_browser.count; read++) {
        if (keep(&g_hfix58_browser.entries[read])) {
            if (write != read) {
                g_hfix58_browser.entries[write] = g_hfix58_browser.entries[read];
            }
            write++;
        }
    }
    g_hfix58_browser.count = write;
    if (g_hfix58_browser.selected >= g_hfix58_browser.count) {
        g_hfix58_browser.selected = g_hfix58_browser.count > 0 ? g_hfix58_browser.count - 1 : 0;
    }
    if (g_hfix58_browser.scroll >= g_hfix58_browser.count) {
        g_hfix58_browser.scroll = 0;
    }
}

static bool hfix58_filter_keep_unwatched(const Hfix58FileEntry *e) { return e->watch_status == MIVF_WATCH_UNWATCHED; }
static bool hfix58_filter_keep_in_progress(const Hfix58FileEntry *e) { return e->watch_status == MIVF_WATCH_IN_PROGRESS; }
static bool hfix58_filter_keep_watched(const Hfix58FileEntry *e) { return e->watch_status == MIVF_WATCH_WATCHED; }

static void hfix58_library_apply_filter_mode(void) {
    switch (g_mivf_settings.library_filter_mode) {
        case 1: hfix58_library_compact(hfix58_filter_keep_unwatched); break;
        case 2: hfix58_library_compact(hfix58_filter_keep_in_progress); break;
        case 3: hfix58_library_compact(hfix58_filter_keep_watched); break;
        default: break; /* 0 = All, no filtering */
    }
}

/* Case-tolerant, safe with an empty/missing name (never crashes on a
   NULL/empty needle -- an empty search query is handled by the caller
   never invoking this compaction at all, see below). */
static bool hfix58_name_contains_ci(const char *haystack, const char *needle) {
    size_t hlen, nlen, i;

    if (!haystack || !needle || !*needle) {
        return true;
    }
    hlen = strlen(haystack);
    nlen = strlen(needle);
    if (nlen > hlen) {
        return false;
    }
    for (i = 0; i + nlen <= hlen; i++) {
        size_t j;
        for (j = 0; j < nlen; j++) {
            char a = haystack[i + j], b = needle[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
        }
        if (j == nlen) {
            return true;
        }
    }
    return false;
}

static bool hfix58_filter_keep_search_match(const Hfix58FileEntry *e) {
    return hfix58_name_contains_ci(e->name, g_hfix58_search_query);
}

static void hfix58_library_apply_search_query(void) {
    if (!g_hfix58_search_query[0]) {
        return; /* empty query: no filtering, deterministic no-op */
    }
    hfix58_library_compact(hfix58_filter_keep_search_match);
}

/* ------------------------------------------------------------------------- */
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

    if (!g_mivf_continue_watching_loaded) {
        hfix_continue_load();
    }

    for (int i = g_mivf_favorites_count - 1; i >= 0; i--) {
        hfix58_browser_promote_path(g_mivf_favorites[i], 2);
    }

    for (int i = g_mivf_recents_count - 1; i >= 0; i--) {
        hfix58_browser_promote_path(g_mivf_recents[i], 1);
    }

    /* Recently Added: the N most-recent-by-added_unix entries currently
       in the scanned list (already populated by hfix58_scan_dir), a
       simple bounded top-N selection over at most HFIX58_MAX_BROWSER_FILES
       entries -- cheap, no sort needed. Selection is done as a read-only
       pass into a temporary path list FIRST, then promoted in a separate
       pass -- hfix58_browser_promote_path mutates/reorders
       g_hfix58_browser.entries as it goes, which would otherwise
       invalidate an in-progress index-based selection over the same
       array. Promoted before Continue Watching so a title that's both
       stays tagged CONTINUE (the higher quick tier wins the tag), not
       ADDED. */
    {
        int used[HFIX58_MAX_BROWSER_FILES];
        char picked[MIVF_RECENTLY_ADDED_SHOW_COUNT][HFIX58_MAX_PATH];
        int picked_count = 0;

        memset(used, 0, sizeof(used));
        for (int pick = 0; pick < MIVF_RECENTLY_ADDED_SHOW_COUNT && pick < g_hfix58_browser.count; pick++) {
            int best = -1;
            for (int i = 0; i < g_hfix58_browser.count; i++) {
                if (used[i]) {
                    continue;
                }
                if (best < 0 || g_hfix58_browser.entries[i].added_unix > g_hfix58_browser.entries[best].added_unix) {
                    best = i;
                }
            }
            if (best < 0 || g_hfix58_browser.entries[best].added_unix == 0) {
                break;
            }
            used[best] = 1;
            snprintf(picked[picked_count], HFIX58_MAX_PATH, "%s", g_hfix58_browser.entries[best].path);
            picked_count++;
        }

        for (int i = 0; i < picked_count; i++) {
            hfix58_browser_promote_path(picked[i], 3);
        }
    }

    /* Promoted last (and with the highest quick tier) so Continue
       Watching titles surface as the most prominent section at the top
       of the same flat list -- no new carousel/shelf UI, per this
       phase's own locked-in design decision. */
    for (int i = g_mivf_continue_watching_count - 1; i >= 0; i--) {
        hfix58_browser_promote_path(g_mivf_continue_watching[i], 4);
    }

    g_hfix58_browser.selected = 0;
    g_hfix58_browser.scroll = 0;
}

static void hfix58_preview_clear(void) {
    memset(&g_hfix58_preview, 0, sizeof(g_hfix58_preview));
    g_hfix58_preview_deadline = 0;
    g_mivf8_preview_hi_valid = false;
    g_mivf8_preview_hi_path[0] = 0;
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


/* MIVF_PHASE8_1_BROWSER_PREVIEW_CACHE_V1
   176x100 browser preview cache. This is intentionally RAM-only in the
   player: it removes repeated SD reads and repeated 2X scaling during one
   browsing session without adding filesystem churn on hardware. Users who
   want exact high-quality artwork can provide <movie>.preview.cover as a
   raw 176x100 RGB565 sidecar. */
static bool mivf8_preview_hi_matches(const char *path) {
    return g_mivf8_preview_hi_valid && path &&
        !strcmp(g_mivf8_preview_hi_path, path);
}

static void mivf8_preview_cache_store(const char *path, const u16 *pixels) {
    int slot = 0;
    u32 oldest;
    if (!path || !*path || !pixels) return;
    oldest = g_mivf8_preview_cache[0].stamp;
    for (int i = 0; i < MIVF8_PREVIEW_CACHE_SLOTS; i++) {
        if (g_mivf8_preview_cache[i].valid &&
            !strcmp(g_mivf8_preview_cache[i].path, path)) {
            slot = i;
            goto found;
        }
        if (!g_mivf8_preview_cache[i].valid) {
            slot = i;
            goto found;
        }
        if (g_mivf8_preview_cache[i].stamp < oldest) {
            oldest = g_mivf8_preview_cache[i].stamp;
            slot = i;
        }
    }
found:
    memcpy(g_mivf8_preview_cache[slot].pixels, pixels,
        sizeof(g_mivf8_preview_cache[slot].pixels));
    snprintf(g_mivf8_preview_cache[slot].path,
        sizeof(g_mivf8_preview_cache[slot].path), "%s", path);
    g_mivf8_preview_cache[slot].stamp = g_mivf8_preview_cache_tick++;
    if (g_mivf8_preview_cache_tick == 0) g_mivf8_preview_cache_tick = 1;
    g_mivf8_preview_cache[slot].valid = true;
}

static bool mivf8_preview_cache_lookup(const char *path) {
    if (!path || !*path) return false;
    for (int i = 0; i < MIVF8_PREVIEW_CACHE_SLOTS; i++) {
        if (g_mivf8_preview_cache[i].valid &&
            !strcmp(g_mivf8_preview_cache[i].path, path)) {
            memcpy(g_mivf8_preview_hi, g_mivf8_preview_cache[i].pixels,
                sizeof(g_mivf8_preview_hi));
            snprintf(g_mivf8_preview_hi_path,
                sizeof(g_mivf8_preview_hi_path), "%s", path);
            g_mivf8_preview_hi_valid = true;
            g_mivf8_preview_cache[i].stamp = g_mivf8_preview_cache_tick++;
            if (g_mivf8_preview_cache_tick == 0) g_mivf8_preview_cache_tick = 1;
            return true;
        }
    }
    return false;
}

static u16 mivf8_rgb565_avg4(u16 a, u16 b, u16 c, u16 d) {
    int ar,ag,ab,br,bg,bb,cr,cg,cb,dr,dg,db;
    hfix58_unpack565(a,&ar,&ag,&ab);
    hfix58_unpack565(b,&br,&bg,&bb);
    hfix58_unpack565(c,&cr,&cg,&cb);
    hfix58_unpack565(d,&dr,&dg,&db);
    return hfix58_rgb565((ar+br+cr+dr+2)>>2,
                         (ag+bg+cg+dg+2)>>2,
                         (ab+bb+cb+db+2)>>2);
}

static void mivf8_preview_make_hi_from_small(const char *path) {
    /* The legacy image is exactly half-size. Generate a softened 2X copy once
       instead of stamping each source pixel as four identical screen pixels.
       Odd pixels are blended with neighbors, so diagonal edges and subtitles
       in cover art look less blocky while preserving deterministic RGB565. */
    if (!g_hfix58_preview.has_thumb) return;
    for (int sy = 0; sy < HFIX58_PREVIEW_H; sy++) {
        for (int sx = 0; sx < HFIX58_PREVIEW_W; sx++) {
            u16 c00 = g_hfix58_preview.thumb[sy * HFIX58_PREVIEW_W + sx];
            u16 c10 = g_hfix58_preview.thumb[sy * HFIX58_PREVIEW_W +
                (sx + 1 < HFIX58_PREVIEW_W ? sx + 1 : sx)];
            u16 c01 = g_hfix58_preview.thumb[(sy + 1 < HFIX58_PREVIEW_H ? sy + 1 : sy) *
                HFIX58_PREVIEW_W + sx];
            u16 c11 = g_hfix58_preview.thumb[(sy + 1 < HFIX58_PREVIEW_H ? sy + 1 : sy) *
                HFIX58_PREVIEW_W + (sx + 1 < HFIX58_PREVIEW_W ? sx + 1 : sx)];
            int dx = sx * 2;
            int dy = sy * 2;
            g_mivf8_preview_hi[dy * HFIX58_PREVIEW2_W + dx] = c00;
            g_mivf8_preview_hi[dy * HFIX58_PREVIEW2_W + dx + 1] =
                mivf8_rgb565_avg4(c00, c00, c10, c10);
            g_mivf8_preview_hi[(dy + 1) * HFIX58_PREVIEW2_W + dx] =
                mivf8_rgb565_avg4(c00, c00, c01, c01);
            g_mivf8_preview_hi[(dy + 1) * HFIX58_PREVIEW2_W + dx + 1] =
                mivf8_rgb565_avg4(c00, c10, c01, c11);
        }
    }
    if (path && *path) {
        snprintf(g_mivf8_preview_hi_path, sizeof(g_mivf8_preview_hi_path),
            "%s", path);
        g_mivf8_preview_hi_valid = true;
        mivf8_preview_cache_store(path, g_mivf8_preview_hi);
    }
}

static bool hfix60_load_preview_cover(const char *video_path) {
    char path[HFIX58_MAX_PATH];
    FILE *cf;
    size_t need = (size_t)HFIX58_PREVIEW2_W * (size_t)HFIX58_PREVIEW2_H * 2u;
    size_t got;
    hfix60_make_sidecar_path(path, sizeof(path), video_path, ".preview.cover");
    if (!path[0]) return false;
    cf = fopen(path, "rb");
    if (!cf) return false;
    got = fread(g_mivf8_preview_hi, 1, need, cf);
    fclose(cf);
    if (got == need) {
        snprintf(g_mivf8_preview_hi_path, sizeof(g_mivf8_preview_hi_path),
            "%s", video_path);
        g_mivf8_preview_hi_valid = true;
        mivf8_preview_cache_store(video_path, g_mivf8_preview_hi);
        /* Keep the legacy has_thumb path true so old code/status treats this
           selection as having artwork. Derive a small representative thumbnail
           from the high-res image for any fallback caller. */
        for (int y = 0; y < HFIX58_PREVIEW_H; y++) {
            for (int x = 0; x < HFIX58_PREVIEW_W; x++) {
                g_hfix58_preview.thumb[y * HFIX58_PREVIEW_W + x] =
                    g_mivf8_preview_hi[(y * 2) * HFIX58_PREVIEW2_W + x * 2];
            }
        }
        g_hfix58_preview.has_thumb = true;
        return true;
    }
    return false;
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
    if (g_hfix58_preview.has_thumb) {
        mivf8_preview_make_hi_from_small(g_hfix58_preview.path);
    }
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
        mivf8_preview_make_hi_from_small(video_path);
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
    mivf8_preview_cache_lookup(path);

    /* HFIX_BROWSERPERF: compute file size lazily, only for the single
       previewed entry, once selection has settled (see the debounce in
       hfix58_browser_refresh_preview) — instead of stat()-ing every
       matched media file during the full directory scan. */
    {
        struct stat st;
        if (stat(path, &st) == 0 && st.st_size > 0) {
            g_hfix58_preview.file_size_kb = (u32)((u64)st.st_size / 1024u);
        }
    }

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

    snprintf(g_hfix58_preview.summary, sizeof(g_hfix58_preview.summary), "%s  %uX%u  FPS %u/%u",
        v.codec,
        v.w,
        v.h,
        v.fpsn,
        v.fpsd ? v.fpsd : 1);

    if (a.type == 2) {
        snprintf(g_hfix58_preview.detail, sizeof(g_hfix58_preview.detail), "%s  %u HZ  %u CH",
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
    if (g_hfix58_preview.has_resume) {
        u64 den = 30000ull * (u64)(v.fpsd ? v.fpsd : 1u);
        u64 frames = den ? (h.duration * (u64)(v.fpsn ? v.fpsn : 30u) + den - 1u) / den : 0;
        if (frames > 0xffffffffull) frames = 0xffffffffull;
        g_hfix58_preview.bookmark_frame = bookmark.frame;
        g_hfix58_preview.total_frames = (u32)frames;
    }

    snprintf(g_hfix58_preview.extra, sizeof(g_hfix58_preview.extra), "%s  SUB %s  CONT %s",
        dur,
        has_srt ? "YES" : "NO",
        g_hfix58_preview.has_resume ? "YES" : "NO");

    g_hfix58_preview.valid = true;

    /* HFIX60: load optional synopsis text and poster image sidecars.
       A ".cover" poster (raw RGB565, preview-sized) overrides the auto thumbnail. */
    hfix60_load_nfo(path);

    if (!mivf8_preview_hi_matches(path)) {
        if (!hfix60_load_preview_cover(path)) {
            if (!hfix60_load_cover(path)) {
                hfix58_decode_browser_thumb(f, &h, &v);
            }
        }
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

/* MIVF_PHASE8_GLOBAL_UI_V1
   Shared, performance-aware library primitives. They use the existing RGB565
   renderer and perform no file I/O. Browser pages redraw only on input or when
   the existing 200 ms preview debounce expires. */
static int mivf8_clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void mivf8_ellipsis(char *dst, size_t dst_sz, const char *src, int max_chars) {
    size_t n;
    if (!dst || dst_sz == 0) return;
    dst[0] = 0;
    if (!src) return;
    if (max_chars < 4) max_chars = 4;
    n = strlen(src);
    if ((int)n <= max_chars) {
        snprintf(dst, dst_sz, "%s", src);
        return;
    }
    if ((size_t)max_chars >= dst_sz) max_chars = (int)dst_sz - 1;
    if (max_chars < 4) return;
    memcpy(dst, src, (size_t)max_chars - 3u);
    dst[max_chars - 3] = '.';
    dst[max_chars - 2] = '.';
    dst[max_chars - 1] = '.';
    dst[max_chars] = 0;
}

static void mivf8_bottom_panel(u8 *fb, int x, int y, int w, int h) {
    hfix58_blend_rect565(fb,x+2,y+3,w,h,0,0,0,90);
    hfix58_blend_rect565(fb,x,y,w,h,9,16,29,232);
    hfix58_rect565(fb,x,y,w,1,70,88,114);
    hfix58_rect565(fb,x,y+h-1,w,1,18,27,42);
}

static void mivf8_top_panel(u8 *fb, int x, int y, int w, int h, int alpha) {
    hfix58s_top_blend_rect565(fb,x+2,y+3,w,h,0,0,0,80);
    hfix58s_top_blend_rect565(fb,x,y,w,h,5,10,20,alpha);
    hfix58s_top_rect565(fb,x,y,w,1,70,88,114);
    hfix58s_top_rect565(fb,x,y+h-1,w,1,18,27,42);
}

static void mivf8_bottom_footer(u8 *fb, const char *left, const char *right) {
    int rw=right?(int)strlen(right)*6:0;
    hfix58_rect565(fb,0,216,320,24,3,7,14);
    hfix58_rect565(fb,0,216,320,1,34,48,68);
    if(left) hfix58_draw_text_shadow(fb,12,226,left,1,174,194,216);
    if(right) hfix58_draw_text_shadow(fb,308-rw,226,right,1,174,194,216);
}

static void mivf8_bottom_focus_row(u8 *fb, int x, int y, int w,
                                   const char *title, const char *status,
                                   bool selected) {
    char shown[48];
    int sw=status?(int)strlen(status)*6:0;
    int max_chars=(w-44-sw)/6;
    mivf8_ellipsis(shown,sizeof(shown),title,max_chars);
    if(selected) {
        hfix58_blend_rect565(fb,x+2,y+2,w,20,0,0,0,80);
        hfix58_blend_rect565(fb,x,y,w,20,
            g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b,60);
        hfix58_rect565(fb,x,y,g_mivf_theme_palette.strong_focus?6:3,20,g_mivf_theme_palette.light_r,g_mivf_theme_palette.light_g,g_mivf_theme_palette.light_b);
        hfix58_draw_text_shadow(fb,x+8,y+6,">",1,255,255,255);
    }
    hfix58_draw_text_shadow(fb,x+24,y+6,shown,1,
        selected?255:202,selected?249:216,selected?236:232);
    if(status && *status)
        hfix58_draw_text_shadow(fb,x+w-8-sw,y+6,status,1,
            selected?202:132,selected?220:154,selected?238:182);
}

static void mivf8_top_progress(u8 *fb, int x, int y, int w,
                               u32 value, u32 maximum) {
    int fill=0;
    if(maximum>0) fill=(int)(((u64)value*(u64)w)/(u64)maximum);
    fill=mivf8_clampi(fill,0,w);
    hfix58s_top_rect565(fb,x,y,w,5,18,27,42);
    if(fill>0) hfix58s_top_rect565(fb,x,y,fill,5,
        g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);
    if(fill>2) hfix58s_top_rect565(fb,x+fill-2,y-2,3,9,g_mivf_theme_palette.light_r,g_mivf_theme_palette.light_g,g_mivf_theme_palette.light_b);
}

static void hfix58_draw_browser_preview(u8 *fb) {
    /* MIVF_PHASE8_LIBRARY_TOP_V1 */
    const Hfix58FileEntry *entry=NULL;
    bool ready=false;
    char title[64];
    char path[64];
    char items[32];
    char progress[40];
    int title_x=216;

    if(!fb) return;
    if(g_hfix58_browser.count>0 && g_hfix58_browser.selected>=0 &&
       g_hfix58_browser.selected<g_hfix58_browser.count)
        entry=&g_hfix58_browser.entries[g_hfix58_browser.selected];
    ready=entry && g_hfix58_preview.valid &&
        !strcmp(g_hfix58_preview.path,entry->path);

    hfix58s_top_rect565(fb,0,0,TOP_W,TOP_H,2,5,11);
    for(int y=0;y<TOP_H;y+=16)
        hfix58s_top_rect565(fb,0,y,TOP_W,1,4,9,18);
    mivf8_top_panel(fb,12,8,TOP_W-24,36,144);
    hfix58s_top_draw_text_shadow(fb,24,16,"MIVF LIBRARY",1,242,248,255);
    mivf8_ellipsis(path,sizeof(path),g_hfix58_browser.cwd,38);
    hfix58s_top_draw_text_shadow(fb,24,30,path,1,126,150,180);
    snprintf(items,sizeof(items),"%d ITEMS",g_hfix58_browser.count);
    hfix58s_top_draw_text_shadow(fb,TOP_W-24-(int)strlen(items)*6,16,items,1,150,174,200);
    hfix58s_top_rect565(fb,20,43,88,2,
        g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);

    mivf8_top_panel(fb,18,56,364,166,122);
    hfix58s_top_rect565(fb,30,70,176,100,8,13,23);

    if(ready && g_hfix58_preview.has_thumb) {
        if(!mivf8_preview_hi_matches(g_hfix58_preview.path)) {
            mivf8_preview_make_hi_from_small(g_hfix58_preview.path);
        }
        if(mivf8_preview_hi_matches(g_hfix58_preview.path)) {
            for(int yy=0;yy<HFIX58_PREVIEW2_H;yy++) {
                for(int xx=0;xx<HFIX58_PREVIEW2_W;xx++) {
                    hfix58s_top_px565(fb,30+xx,70+yy,
                        g_mivf8_preview_hi[yy*HFIX58_PREVIEW2_W+xx]);
                }
            }
        }
    } else {
        hfix58s_top_blend_rect565(fb,30,70,176,100,
            g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b,28);
        for(int i=0;i<5;i++)
            hfix58s_top_rect565(fb,48+i*12,92+i*7,104-i*24,1,
                g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);
        hfix58s_top_draw_text_shadow(fb,82,128,
            entry?(ready?"MIVF MEDIA":"LOADING PREVIEW"):"NO MEDIA",1,
            190,208,228);
    }

    if(ready) mivf8_ellipsis(title,sizeof(title),g_hfix58_preview.title,25);
    else if(entry) mivf8_ellipsis(title,sizeof(title),entry->name,25);
    else snprintf(title,sizeof(title),"NO PLAYABLE MEDIA");
    hfix58s_top_draw_text_shadow(fb,title_x,72,title,1,240,247,255);

    if(ready) {
        hfix58s_top_draw_text_shadow(fb,title_x,91,g_hfix58_preview.summary,1,158,184,210);
        hfix58s_top_draw_text_shadow(fb,title_x,107,g_hfix58_preview.detail,1,142,166,194);
        hfix58s_top_draw_text_shadow(fb,title_x,127,g_hfix58_preview.extra,1,132,156,184);

        if(g_hfix58_preview.has_resume && g_hfix58_preview.total_frames>0) {
            u32 pct=(u32)(((u64)g_hfix58_preview.bookmark_frame*100ull)/
                (u64)g_hfix58_preview.total_frames);
            if(pct>100u) pct=100u;
            snprintf(progress,sizeof(progress),"CONTINUE WATCHING   %lu PCT",
                (unsigned long)pct);
            hfix58s_top_draw_text_shadow(fb,title_x,150,progress,1,126,205,162);
            mivf8_top_progress(fb,title_x,168,148,
                g_hfix58_preview.bookmark_frame,g_hfix58_preview.total_frames);
        } else {
            hfix58s_top_draw_text_shadow(fb,title_x,150,"READY TO PLAY",1,154,178,204);
        }

        if(g_hfix58_preview.synopsis1[0])
            hfix58s_top_draw_text_shadow(fb,32,187,g_hfix58_preview.synopsis1,1,188,207,226);
        if(g_hfix58_preview.synopsis2[0])
            hfix58s_top_draw_text_shadow(fb,32,202,g_hfix58_preview.synopsis2,1,164,184,206);
    } else if(entry) {
        hfix58s_top_draw_text_shadow(fb,title_x,96,"READING COVER AND METADATA",1,142,166,194);
    } else {
        hfix58s_top_draw_text_shadow(fb,title_x,96,"COPY MIVF OR MOFLEX FILES",1,142,166,194);
        hfix58s_top_draw_text_shadow(fb,title_x,112,"TO THE SD CARD",1,142,166,194);
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

/* ------------------------------------------------------------------------- */
/* Watch-state model (unwatched/in-progress/watched)                         */
/*                                                                           */
/* A genuinely separate persisted store from bookmarks -- a natural EOF      */
/* always clears the bookmark (see the play() teardown), so a finished       */
/* title has NO bookmark, identical to a never-started one. Trustworthy      */
/* completion threshold below 100% to tolerate trailing credits/black        */
/* frames a user may not sit through, matching common industry convention.  */
/* ------------------------------------------------------------------------- */
#define MIVF_WATCH_COMPLETE_THRESHOLD_PCT 92u

static u32 hfix_watchstate_compute_status(u32 last_frame, u32 total_frames, bool reached_eof) {
    if (reached_eof) {
        return MIVF_WATCH_WATCHED;
    }
    if (total_frames > 0) {
        u64 pct = ((u64)last_frame * 100ull) / (u64)total_frames;
        if (pct >= (u64)MIVF_WATCH_COMPLETE_THRESHOLD_PCT) {
            return MIVF_WATCH_WATCHED;
        }
    }
    /* total_frames == 0 means unknown/untrustworthy -- never inferred as
       WATCHED from a percentage alone; only a real EOF (handled above)
       or a stored manual mark can produce WATCHED in that case. */
    if (last_frame > 0) {
        return MIVF_WATCH_IN_PROGRESS;
    }
    return MIVF_WATCH_UNWATCHED;
}

/* The single source of truth every reader (browser badges, future
   Continue Watching/Recently-Added rows) must call -- never re-derive
   this fallback logic elsewhere. */
static u8 hfix_watchstate_effective(const char *path) {
    MivfWatchState state;
    MivfBookmark bookmark;

    if (!path || !*path) {
        return MIVF_WATCH_UNWATCHED;
    }

    if (MIVF_WatchStateLoad(path, &state)) {
        return (u8)state.status;
    }

    /* Lazy migration from bookmarks: a bookmark's mere existence proves
       real playback happened (it is cleared on EOF, so its presence can
       only mean "in progress, not finished") -- a stronger signal than
       silently defaulting to unwatched. This path can never produce
       WATCHED; only a real recorded EOF (hfix_watchstate_finish) or an
       explicit manual mark can. */
    if (MIVF_BookmarkLoad(path, &bookmark) && bookmark.video_path[0] &&
        !strcmp(bookmark.video_path, path) && bookmark.frame > 0) {
        return MIVF_WATCH_IN_PROGRESS;
    }

    return MIVF_WATCH_UNWATCHED;
}

/* "Last played" for sort purposes -- deliberately NOT bookmark-derived
   like hfix_watchstate_effective's fallback (MivfBookmark has no
   timestamp field at all), so a title with no real watch-state record
   honestly reports 0 (never played) rather than guessing. */
static u32 hfix_watchstate_last_played(const char *path) {
    MivfWatchState state;

    if (!path || !*path) {
        return 0;
    }
    if (MIVF_WatchStateLoad(path, &state)) {
        return state.last_played_unix;
    }
    return 0;
}

/* Manual reset/correction -- long-press Y on the selected library entry
   (see hfix58_file_browser_select). Resets the tracked position (a
   manual correction isn't tied to a precise playback frame) and sets
   `manual` so future readers can distinguish it from automatic tracking
   if ever needed; a subsequent real playback session's own automatic
   checkpoint/finish calls simply overwrite this with fresh, current
   truth, which is correct -- a manual mark records a point-in-time
   correction, not a permanent override. */
static void hfix_watchstate_toggle_manual(const char *path) {
    MivfWatchState state;
    u32 current;

    if (!path || !*path) {
        return;
    }

    current = hfix_watchstate_effective(path);
    memset(&state, 0, sizeof(state));
    state.status = (current == MIVF_WATCH_WATCHED) ? MIVF_WATCH_UNWATCHED : MIVF_WATCH_WATCHED;
    state.last_played_unix = (u32)time(NULL);
    state.manual = true;
    MIVF_WatchStateSave(path, &state);
    /* Neither WATCHED nor UNWATCHED is "in progress" -- a manual mark
       always removes from Continue Watching, same as automatic
       completion does. */
    hfix_continue_remove(path);
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

        /* Cached once per scan (not per redraw frame) -- see the field's
           own comment on Hfix58FileEntry. */
        out->watch_status = hfix_watchstate_effective(out->path);
        out->added_unix = hfix_added_dates_get_or_set(out->path);
        out->last_played_unix = hfix_watchstate_last_played(out->path);
        hfix_series_parse(out->name, &out->series_ok, out->series_name, sizeof(out->series_name),
                           &out->series_season, &out->series_episode);
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

    /* Sort/Filter/Search: applied after the base natural-name sort above
       (which stays the deterministic tiebreak/fallback order for Name
       mode and for any two entries the active mode ranks as equal). */
    hfix58_library_apply_sort_mode();
    hfix58_library_apply_filter_mode();
    hfix58_library_apply_search_query();

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
    /* MIVF_PHASE8_LIBRARY_LIST_V1 */
    if(!fb) return;
    hfix58_browser_refresh_preview();
    hfix58_rect565(fb,0,0,320,240,2,5,11);
    for(int x=0;x<320;x+=24) hfix58_rect565(fb,x,0,1,216,5,10,19);
    mivf8_bottom_panel(fb,8,7,304,203);

    hfix58_draw_text_shadow(fb,18,17,"LIBRARY",1,240,247,255);
    hfix58_rect565(fb,18,30,44,2,
        g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);
    {
        char pos[32];
        int pw;
        snprintf(pos,sizeof(pos),g_hfix58_browser.count>0?"%d OF %d":"0 ITEMS",
            g_hfix58_browser.count>0?g_hfix58_browser.selected+1:0,
            g_hfix58_browser.count);
        pw=(int)strlen(pos)*6;
        hfix58_draw_text_shadow(fb,302-pw,17,pos,1,138,160,188);
    }
    {
        char cwd[48];
        mivf8_ellipsis(cwd,sizeof(cwd),g_hfix58_browser.cwd,43);
        hfix58_draw_text_shadow(fb,18,39,cwd,1,126,150,180);
    }

    /* Sort/Filter/Search status -- shown only when it deviates from the
       legacy default (Name / All / no query), so the header stays clean
       for anyone who never touches these controls. */
    if (g_mivf_settings.library_sort_mode != 0 || g_mivf_settings.library_filter_mode != 0 || g_hfix58_search_query[0]) {
        static const char *sort_names[4] = { "NAME", "DATE ADDED", "LAST PLAYED", "BY SERIES" };
        static const char *filter_names[4] = { "ALL", "UNWATCHED", "IN PROGRESS", "WATCHED" };
        char line[80];
        if (g_hfix58_search_query[0]) {
            snprintf(line, sizeof(line), "SEARCH \"%.20s\"  SORT %s  FILTER %s",
                g_hfix58_search_query, sort_names[g_mivf_settings.library_sort_mode], filter_names[g_mivf_settings.library_filter_mode]);
        } else {
            snprintf(line, sizeof(line), "SORT %s  FILTER %s",
                sort_names[g_mivf_settings.library_sort_mode], filter_names[g_mivf_settings.library_filter_mode]);
        }
        hfix58_draw_text_shadow(fb,18,49,line,1,110,190,230);
    }

    if(g_hfix58_browser.count<=0) {
        hfix58_rect565(fb,28,78,3,72,
            g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);
        hfix58_draw_text_shadow(fb,42,86,"NO PLAYABLE MEDIA",1,238,244,250);
        hfix58_draw_text_shadow(fb,42,109,"COPY MIVF OR MOFLEX FILES",1,154,178,204);
        hfix58_draw_text_shadow(fb,42,126,"TO SDMC:/MIVF",1,154,178,204);
    } else {
        int first=g_hfix58_browser.scroll;
        int last=first+HFIX58_BROWSER_VISIBLE_ROWS;
        if(last>g_hfix58_browser.count) last=g_hfix58_browser.count;
        int y=58;
        for(int i=first;i<last;i++,y+=22) {
            Hfix58FileEntry *e=&g_hfix58_browser.entries[i];
            Hfix58MediaKind kind=hfix58_media_kind(e->name);
            const char *status=kind==HFIX58_MEDIA_MOFLEX?"MOFLEX":"MIVF";
            char series_tag[16];
            /* Continue Watching: highest-priority tag, checked first --
               this IS the row (see hfix58_browser_promote_quick_access,
               which promotes these titles to the top of this same
               list). */
            if(e->quick==4) status="CONTINUE";
            else if(e->quick==2 || hfix60_fav_is(e->path)) status="FAVORITE";
            else if(i==g_hfix58_browser.selected && g_hfix58_preview.valid &&
                    !strcmp(g_hfix58_preview.path,e->path) && g_hfix58_preview.has_resume)
                status="RESUME";
            else if(e->quick==1 && i<3) status="RECENT";
            else if(e->quick==3) status="ADDED";
            else if(e->watch_status==MIVF_WATCH_WATCHED) status="WATCHED";
            else if(e->watch_status==MIVF_WATCH_IN_PROGRESS) status="IN PROGRESS";
            else if(e->series_duplicate) status="DUPLICATE";
            else if(e->series_ok) {
                snprintf(series_tag,sizeof(series_tag),"S%02luE%02lu",
                    (unsigned long)e->series_season,(unsigned long)e->series_episode);
                status=series_tag;
            }
            mivf8_bottom_focus_row(fb,16,y,286,e->name,status,
                i==g_hfix58_browser.selected);
        }

        if(g_hfix58_browser.count>HFIX58_BROWSER_VISIBLE_ROWS) {
            int ty=58,th=HFIX58_BROWSER_VISIBLE_ROWS*22;
            int kh=(th*HFIX58_BROWSER_VISIBLE_ROWS)/g_hfix58_browser.count;
            int maxs=g_hfix58_browser.count-HFIX58_BROWSER_VISIBLE_ROWS;
            if(kh<12)kh=12;
            int ky=ty+(maxs>0?((th-kh)*g_hfix58_browser.scroll)/maxs:0);
            hfix58_rect565(fb,305,ty,3,th,24,34,49);
            hfix58_rect565(fb,305,ky,3,kh,
                g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);
        }
    }
    mivf8_bottom_footer(fb,"A OPEN  Y FAVORITE","X HELP  B BACK");
    hfix58_draw_alert(fb);
}

static void hfix58_browser_redraw(void) {
    u16 fw=0,fh=0;
    u8 *top=gfxGetFramebuffer(GFX_TOP,GFX_LEFT,&fw,&fh);
    u8 *bot=gfxGetFramebuffer(GFX_BOTTOM,GFX_LEFT,&fw,&fh);
    if(top) hfix58_draw_browser_preview(top);
    if(bot) {
        hfix58_draw_browser(bot);
        if(g_hfix62_help_visible) hfix62_draw_help_overlay(bot);
    }
    gfxFlushBuffers();
    gfxSwapBuffers();
}

#ifdef MIVF_SHOWCASE_FULL
/* Forward declarations (file scope -- a block-scope declaration cannot
   carry a static storage-class specifier in C, so these must live here,
   not inside the function below): the Showcase controller is defined
   later in this file, near the existing Transport Preview demo it
   extends, but this loop is textually earlier and calls into it. */
static void mivf_showcase_tick(void);
static void mivf_showcase_synth_key(u32 *down_bits);
static void mivf_showcase_cancel_check(u32 real_down, u32 real_held);
static bool mivf_showcase_render_overlay_if_needed(void);
static bool mivf_showcase_maybe_scan_demo_dir(void);
static void mivf_showcase_note_loop_browser(void);
#ifdef MIVF_SHOWCASE_CAPTURE
static void mivf_showcase_maybe_capture(void);
#endif
#endif

static bool hfix58_file_browser_select(char *out_path, size_t out_sz) {
    if (!out_path || out_sz == 0) {
        return false;
    }

    hfix58_alert_clear();

    if (g_mivf_pending_error_alert[0]) {
        hfix58_alert_set(g_mivf_pending_error_alert, 2);
        g_mivf_pending_error_alert[0] = 0;
    }

    u64 scan_t0 = svcGetSystemTick();
    bool scan_ok = hfix58_scan_default_dirs();
#ifdef MIVF_SHOWCASE_FULL
    if (mivf_showcase_maybe_scan_demo_dir()) {
        scan_ok = true;
    }
#endif
    printf("lifecycle: browser dir scan took %llu us (ok=%d, entries=%u)\n",
        (unsigned long long)ticks_to_us(svcGetSystemTick() - scan_t0),
        (int)scan_ok, (unsigned int)g_hfix58_browser.count);

    if (!scan_ok) {
        /*
            Still show a no-files screen so the user gets feedback.
        */
        g_hfix58_browser.count = 0;
        snprintf(g_hfix58_browser.cwd, sizeof(g_hfix58_browser.cwd), "sdmc:/mivf");
    }

    hfix58_browser_redraw();
    hfix58_browser_redraw();

    /* HFIX_WATCHSTATE: long-press Y (~1.2s, same hold-timer pattern as
       Touch Lock's L+R gesture) manually toggles watched/unwatched on
       the selected entry -- distinct from short-press Y (toggle
       favorite) above, never fires on a quick tap. Reset on every fresh
       call to this function (a new browser session), not carried over
       from a previous one. */
    u32 watch_toggle_hold_frames = 0;
    bool watch_toggle_fired = false;

    /* Sort/Filter/Search: hold SELECT cycles the filter mode (short tap
       keeps its existing show-all-folders toggle); hold X opens search
       (short tap keeps its existing help-screen open). Same hold-timer
       pattern as watch_toggle_* above. */
    u32 filter_hold_frames = 0;
    bool filter_toggle_fired = false;
    u32 search_hold_frames = 0;
    bool search_toggle_fired = false;

    while (aptMainLoop()) {
        hidScanInput();

        u32 down = hidKeysDown();
        u32 held = hidKeysHeld();
#ifdef MIVF_SHOWCASE_FULL
        mivf_showcase_tick();
        mivf_showcase_note_loop_browser();
        mivf_showcase_synth_key(&down);
        mivf_showcase_cancel_check(down, held);
#ifdef MIVF_SHOWCASE_CAPTURE
        mivf_showcase_maybe_capture();
#endif
        /* Auto-start means this browser loop -- the very first screen
           main() enters -- can be current when the Showcase is still in
           NO_PROJECT or has already reached TITLE (e.g. a very short or
           degenerate run). Same helper the per-movie-menu loop uses further
           below, so the message/card is never skipped just because of
           which screen happens to be active. */
        if (mivf_showcase_render_overlay_if_needed()) {
            continue;
        }
#endif

        /* HFIX62: while the help overlay is open, it owns all input --
           handle it here and skip straight to the frame's vblank wait so
           B/START don't fall through to the exit checks below. */
        if (g_hfix62_help_visible) {
            hfix62_handle_help_menu(down);
            hfix58_browser_redraw();
            gspWaitForVBlank();
            continue;
        }

        if (down & KEY_START) {
            return false;
        }

        if (down & KEY_B) {
            return false;
        }

        /* Sort/Filter/Search: hold X (~1.2s) opens the search keyboard;
           a short tap (released before the threshold) still opens the
           help screen exactly as before. Resolved on key-up, same
           reasoning as the Y long-press fix above -- a single X press
           can only ever do ONE of the two things, never both. */
        if (held & KEY_X) {
            if (search_hold_frames < 0xFFFFFFFFu) {
                search_hold_frames++;
            }
            if (!search_toggle_fired && search_hold_frames >= 72u) {
                search_toggle_fired = true;
                {
                    SwkbdState swkbd;
                    char buf[MIVF_SEARCH_QUERY_MAX];
                    SwkbdButton button;

                    snprintf(buf, sizeof(buf), "%s", g_hfix58_search_query);
                    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, MIVF_SEARCH_QUERY_MAX - 1);
                    swkbdSetHintText(&swkbd, "Search library (empty = show all)");
                    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Clear", false);
                    button = swkbdInputText(&swkbd, buf, sizeof(buf));
                    if (button == SWKBD_BUTTON_CONFIRM) {
                        snprintf(g_hfix58_search_query, sizeof(g_hfix58_search_query), "%s", buf);
                    } else if (button == SWKBD_BUTTON_LEFT) {
                        g_hfix58_search_query[0] = 0;
                    }
                    /* SWKBD_BUTTON_NONE (cancel) leaves the query untouched. */
                    hfix58_scan_dir(g_hfix58_browser.cwd);
                    hfix58_browser_promote_quick_access();
                    hfix58_preview_clear();
                    hfix58_browser_redraw();
                }
            }
        } else {
            if ((hidKeysUp() & KEY_X) && !search_toggle_fired) {
                /* HFIX62: X opens the in-app controls/keybinds help screen. */
                hfix62_set_help_open(true);
                hfix58_browser_redraw();
                gspWaitForVBlank();
                search_hold_frames = 0;
                search_toggle_fired = false;
                continue;
            }
            search_hold_frames = 0;
            search_toggle_fired = false;
        }

        /* Hold SELECT (~1.2s) cycles the library filter mode (All ->
           Unwatched -> In Progress -> Watched -> All); a short tap still
           toggles show-all-folders exactly as before. Same key-up
           resolution as above. */
        if (held & KEY_SELECT) {
            if (filter_hold_frames < 0xFFFFFFFFu) {
                filter_hold_frames++;
            }
            if (!filter_toggle_fired && filter_hold_frames >= 72u) {
                filter_toggle_fired = true;
                g_mivf_settings.library_filter_mode = (g_mivf_settings.library_filter_mode + 1u) % 4u;
                MIVF_SettingsSave(&g_mivf_settings);
                hfix58_scan_dir(g_hfix58_browser.cwd);
                hfix58_browser_promote_quick_access();
                hfix58_preview_clear();
                {
                    static const char *names[4] = { "SHOWING ALL", "UNWATCHED ONLY", "IN PROGRESS ONLY", "WATCHED ONLY" };
                    hfix58_alert_set(names[g_mivf_settings.library_filter_mode], 1);
                }
                hfix58_browser_redraw();
            }
        } else {
            if ((hidKeysUp() & KEY_SELECT) && !filter_toggle_fired) {
                /* HFIX60: SELECT toggles show-all mode — rescans with the
                   alternate directory order so the user can see files at
                   the SD root (or hide them again). */
                g_hfix58_show_all_dirs = !g_hfix58_show_all_dirs;
                hfix58_scan_default_dirs();
                hfix58_preview_clear();
                hfix58_browser_redraw();
            }
            filter_hold_frames = 0;
            filter_toggle_fired = false;
        }

        /* Sort/Filter/Search: L/R cycle the sort mode (Name -> Date
           Added -> Last Played -> By Series), unused by anything else in
           the browser. Immediate on key-down, no long-press needed. */
        if (down & (KEY_L | KEY_R)) {
            u32 n = g_mivf_settings.library_sort_mode;
            n = (down & KEY_L) ? (n + 3u) % 4u : (n + 1u) % 4u;
            g_mivf_settings.library_sort_mode = n;
            MIVF_SettingsSave(&g_mivf_settings);
            hfix58_scan_dir(g_hfix58_browser.cwd);
            hfix58_browser_promote_quick_access();
            hfix58_preview_clear();
            {
                static const char *names[4] = { "SORT: NAME", "SORT: DATE ADDED", "SORT: LAST PLAYED", "SORT: BY SERIES" };
                hfix58_alert_set(names[n], 1);
            }
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

            /* HFIX_WATCHSTATE: long-press Y (~1.2s) manually toggles
               watched/unwatched on the selected entry; a short tap
               (released before the threshold) still toggles favorite,
               exactly as before -- resolved on key-up rather than
               key-down so a single Y press can only ever do ONE of the
               two actions, never both. */
            if (held & KEY_Y) {
                if (watch_toggle_hold_frames < 0xFFFFFFFFu) {
                    watch_toggle_hold_frames++;
                }
                if (!watch_toggle_fired && watch_toggle_hold_frames >= 72u && g_hfix58_browser.count > 0) {
                    hfix_watchstate_toggle_manual(g_hfix58_browser.entries[g_hfix58_browser.selected].path);
                    g_hfix58_browser.entries[g_hfix58_browser.selected].watch_status =
                        hfix_watchstate_effective(g_hfix58_browser.entries[g_hfix58_browser.selected].path);
                    watch_toggle_fired = true;
                    hfix58_alert_set(
                        g_hfix58_browser.entries[g_hfix58_browser.selected].watch_status == MIVF_WATCH_WATCHED
                            ? "MARKED WATCHED" : "MARKED UNWATCHED", 1);
                    hfix58_browser_redraw();
                }
            } else {
                /* HFIX60: toggle favorite on the selected entry --
                   only for a short tap that released before the
                   long-press threshold ever fired. */
                if ((hidKeysUp() & KEY_Y) && !watch_toggle_fired && g_hfix58_browser.count > 0) {
                    if (g_mivf_settings.remember_favorites) {
                        hfix60_fav_toggle(g_hfix58_browser.entries[g_hfix58_browser.selected].path);
                        hfix58_browser_promote_quick_access();
                        /* Defer preview reload after list reorder. */
                        g_hfix58_preview_deadline = svcGetSystemTick() +
                            (u64)SYSCLOCK_ARM11 / 5ULL;
                        hfix58_browser_redraw();
                    }
                }
                watch_toggle_hold_frames = 0;
                watch_toggle_fired = false;
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

/*
    MIVF_TRANSPORT_ABSTRACTION_V1

    Phase C1 separates a control's semantic action/state from the current
    Cinematic vector drawing. Only the active HFIX58B playback dashboard uses
    this layer. Input, hit boxes, playback state, seeking, NDSP, timing, and
    presentation remain untouched.
*/
typedef enum {
    MIVF_TRANSPORT_PLAY = 0,
    MIVF_TRANSPORT_PAUSE,
    MIVF_TRANSPORT_STOP,
    MIVF_TRANSPORT_REWIND,
    MIVF_TRANSPORT_FORWARD,
    MIVF_TRANSPORT_PREVIOUS,
    MIVF_TRANSPORT_NEXT,
    MIVF_TRANSPORT_VOLUME,
    MIVF_TRANSPORT_SUBTITLES,
    MIVF_TRANSPORT_SETTINGS,
    MIVF_TRANSPORT_COUNT
} MivfTransportAction;

typedef enum {
    MIVF_CONTROL_IDLE = 0,
    MIVF_CONTROL_FOCUSED,
    MIVF_CONTROL_PRESSED,
    MIVF_CONTROL_ACTIVE,
    MIVF_CONTROL_DISABLED
} MivfControlState;

typedef enum {
    MIVF_TRANSPORT_STYLE_CINEMATIC = 0,
    MIVF_TRANSPORT_STYLE_CLASSIC,
    MIVF_TRANSPORT_STYLE_MINIMAL,
    MIVF_TRANSPORT_STYLE_RETRO,
    MIVF_TRANSPORT_STYLE_ACCESSIBLE,
    MIVF_TRANSPORT_STYLE_RADIO,
    MIVF_TRANSPORT_STYLE_CELESTIAL,
    MIVF_TRANSPORT_STYLE_PAPER,
    MIVF_TRANSPORT_STYLE_SYNTHWAVE,
    MIVF_TRANSPORT_STYLE_DIRECTOR,
    MIVF_TRANSPORT_STYLE_CARTRIDGE_CONTROLLER,
    MIVF_TRANSPORT_STYLE_PORTABLE_MONO,
    MIVF_TRANSPORT_STYLE_DUAL_SCREEN_TOUCH,
    MIVF_TRANSPORT_STYLE_INDUSTRIAL_GREEN,
    MIVF_TRANSPORT_STYLE_FLAT_TILE,
    MIVF_TRANSPORT_STYLE_BLUE_WAVE,
    MIVF_TRANSPORT_STYLE_COUNT
} MivfTransportStyleId;
typedef char MivfDeviceUiStyleCountMustMatch[(MIVF_TRANSPORT_STYLE_COUNT == MIVF_DEVICE_UI_STYLE_COUNT) ? 1 : -1];

typedef struct {
    MivfTransportStyleId id;
    int icon_r;
    int icon_g;
    int icon_b;
    int icon_scale_pct;
    bool draw_button_background;
    bool square_frame;
    bool pixel_icon;
    bool use_accessible_outline;
} MivfTransportStyle;

static const MivfTransportStyle g_mivf_transport_styles[MIVF_TRANSPORT_STYLE_COUNT] = {
 {MIVF_TRANSPORT_STYLE_CINEMATIC,245,245,245,100,true,false,false,false},
 {MIVF_TRANSPORT_STYLE_CLASSIC,220,245,255,100,false,false,false,false},
 {MIVF_TRANSPORT_STYLE_MINIMAL,235,244,252,88,false,false,false,false},
 {MIVF_TRANSPORT_STYLE_RETRO,255,238,145,92,true,true,true,false},
 {MIVF_TRANSPORT_STYLE_ACCESSIBLE,255,255,255,116,true,false,false,true},
 {MIVF_TRANSPORT_STYLE_RADIO,246,207,126,100,true,false,false,false},
 {MIVF_TRANSPORT_STYLE_CELESTIAL,210,224,255,96,false,false,false,false},
 {MIVF_TRANSPORT_STYLE_PAPER,43,38,34,94,false,false,false,false},
 {MIVF_TRANSPORT_STYLE_SYNTHWAVE,255,82,214,104,false,false,false,false},
 {MIVF_TRANSPORT_STYLE_DIRECTOR,232,238,244,94,true,true,false,false},
 {MIVF_TRANSPORT_STYLE_CARTRIDGE_CONTROLLER,238,74,68,100,true,true,true,false},
 {MIVF_TRANSPORT_STYLE_PORTABLE_MONO,38,61,35,100,true,false,true,false},
 {MIVF_TRANSPORT_STYLE_DUAL_SCREEN_TOUCH,78,177,224,100,true,false,false,false},
 {MIVF_TRANSPORT_STYLE_INDUSTRIAL_GREEN,94,238,91,108,true,false,false,false},
 {MIVF_TRANSPORT_STYLE_FLAT_TILE,116,218,73,100,true,true,false,false},
 {MIVF_TRANSPORT_STYLE_BLUE_WAVE,104,183,255,100,false,false,false,false}
};

static const char *mivf_transport_style_name(u32 style) {
 static const char *names[MIVF_TRANSPORT_STYLE_COUNT]={"PREMIERE","ORBIT","FOCUS","VIDEO SYSTEM","PLAYBACK CONTROLS","RECEIVER","CELESTIAL","FEATURE","PLAYBACK // CHAPTER","TIMECODE","CARTRIDGE CONTROLLER","PORTABLE MONO","DUAL SCREEN TOUCH","INDUSTRIAL GREEN","FLAT TILE","BLUE WAVE"};
 return names[style<MIVF_TRANSPORT_STYLE_COUNT?style:0u];
}

static const MivfTransportStyle *mivf_transport_current_style(void) {
    u32 style = g_mivf_settings.transport_style;
    if (style >= MIVF_TRANSPORT_STYLE_COUNT) style = 0;
    return &g_mivf_transport_styles[style];
}

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

static MivfControlState mivf_transport_button_state(const MivfButtonSkin *button) {
    if (!button) {
        return MIVF_CONTROL_DISABLED;
    }
    if (button->pressed) {
        return MIVF_CONTROL_PRESSED;
    }
    if (button->hovered) {
        return MIVF_CONTROL_FOCUSED;
    }
    return MIVF_CONTROL_IDLE;
}

static void mivf_transport_draw_icon(
    u8 *fb,
    MivfTransportAction action,
    int x,
    int y,
    int size,
    u16 color
) {
    switch (action) {
        case MIVF_TRANSPORT_PLAY:
            hfix58b_draw_vector_play(fb, x, y, size, color);
            break;
        case MIVF_TRANSPORT_PAUSE:
            hfix58b_draw_vector_pause(fb, x, y, size, color);
            break;
        case MIVF_TRANSPORT_REWIND:
            hfix58b_draw_vector_rewind(fb, x, y, size, color);
            break;
        case MIVF_TRANSPORT_FORWARD:
            hfix58b_draw_vector_forward(fb, x, y, size, color);
            break;
        case MIVF_TRANSPORT_STOP:
        case MIVF_TRANSPORT_PREVIOUS:
        case MIVF_TRANSPORT_NEXT:
        case MIVF_TRANSPORT_VOLUME:
        case MIVF_TRANSPORT_SUBTITLES:
        case MIVF_TRANSPORT_SETTINGS:
        case MIVF_TRANSPORT_COUNT:
        default:
            /* Reserved for later styles/assets. Never draw a misleading icon. */
            break;
    }
}

static void mivf_transport_draw_style_frame(u8 *fb, MivfButtonSkin *button,
                                                const MivfTransportStyle *style,
                                                MivfControlState state) {
    bool hot = state == MIVF_CONTROL_FOCUSED || state == MIVF_CONTROL_PRESSED || state == MIVF_CONTROL_ACTIVE;
    if (!style->draw_button_background) {
        if (hot) hfix58_rect565(fb, button->x + 8, button->y + button->h - 4, button->w - 16, 2,
                                g_mivf_theme_palette.light_r, g_mivf_theme_palette.light_g, g_mivf_theme_palette.light_b);
        return;
    }
    if (!style->square_frame) {
        hfix58b_draw_button(fb, button);
        return;
    }
    hfix58_rect565(fb, button->x + 2, button->y + 2, button->w - 4, button->h - 4,
                    hot ? g_mivf_theme_palette.dark_r : 12,
                    hot ? g_mivf_theme_palette.dark_g : 18,
                    hot ? g_mivf_theme_palette.dark_b : 28);
    hfix58_rect565(fb, button->x + 2, button->y + 2, button->w - 4, 2,
                    hot ? g_mivf_theme_palette.light_r : 88,
                    hot ? g_mivf_theme_palette.light_g : 104,
                    hot ? g_mivf_theme_palette.light_b : 126);
    hfix58_rect565(fb, button->x + 2, button->y + button->h - 4, button->w - 4, 2, 2, 5, 10);
    hfix58_rect565(fb, button->x + 2, button->y + 2, 2, button->h - 4,
                    hot ? g_mivf_theme_r : 55, hot ? g_mivf_theme_g : 68, hot ? g_mivf_theme_b : 84);
    hfix58_rect565(fb, button->x + button->w - 4, button->y + 2, 2, button->h - 4, 2, 5, 10);
}

static void mivf_transport_draw_control(
    u8 *fb,
    MivfButtonSkin *button,
    MivfTransportAction action,
    MivfControlState state,
    int icon_x,
    int icon_y,
    int icon_size,
    const MivfTransportStyle *style
) {
    u16 icon_color;

    if (!fb || !button || !style) {
        return;
    }

    /* State is explicit for future skins; Cinematic parity still uses the
       retained button flags that generated the Beta 1 appearance. */
    button->hovered = (state == MIVF_CONTROL_FOCUSED ||
                       state == MIVF_CONTROL_PRESSED ||
                       state == MIVF_CONTROL_ACTIVE);
    button->pressed = (state == MIVF_CONTROL_PRESSED);

    mivf_transport_draw_style_frame(fb, button, style, state);

    icon_size = (icon_size * style->icon_scale_pct) / 100;
    if (style->pixel_icon) icon_size = (icon_size / 4) * 4;
    icon_color = hfix58_rgb565(style->icon_r, style->icon_g, style->icon_b);
    if (style->use_accessible_outline || g_mivf_theme_palette.strong_focus) {
        u16 edge = hfix58_rgb565(0, 0, 0);
        mivf_transport_draw_icon(fb, action, icon_x - 1, icon_y, icon_size, edge);
        mivf_transport_draw_icon(fb, action, icon_x + 1, icon_y, icon_size, edge);
        mivf_transport_draw_icon(fb, action, icon_x, icon_y - 1, icon_size, edge);
        mivf_transport_draw_icon(fb, action, icon_x, icon_y + 1, icon_size, edge);
    }
    mivf_transport_draw_icon(fb, action, icon_x, icon_y, icon_size, icon_color);
}

/* ==========================================================================
   MIVF_SHOWCASE_FULL_V1 -- deterministic, gated, cross-screen cinematic
   Showcase. Off by default (compile-time MIVF_SHOWCASE_FULL, plus a runtime
   marker-file check so it stays inert even in a build where the flag was
   left on). This is a SEPARATE, additive system from the existing
   Transport Style Preview demo above (mivf_demo_*) -- that demo continues
   to work unmodified. This controller drives real production screens by
   synthesizing the same u32 key bitmask hidKeysDown() would produce for a
   real press, injected at each existing screen's own input-handling point;
   it never duplicates or fakes a feature's rendering/logic.

   Confidence note (kept honest per this project's own evidence discipline):
   the framework below (struct, activation, cancellation, logging, title
   card, and the personalization stage, which only calls the same
   hfix60_apply_theme()/mivf_theme_set_rgb()/plain-field-assignment paths
   Settings itself uses) was written against functions read in full this
   session. The cross-screen synthetic-input stages (LIBRARY/SCENES/RESUME)
   are real, non-fabricated code, but they could not be exercised against a
   running build in this environment (no emulator installed) -- treat them
   as build-verified only until run on real hardware/emulator, per
   MIVF_SHOWCASE_KNOWN_LIMITATIONS.md.
   ========================================================================== */
#ifdef MIVF_SHOWCASE_FULL

typedef enum {
    MIVF_SC_INACTIVE = 0,
    MIVF_SC_NO_PROJECT,   /* lawful demo project missing -- explicit in-app
                             message, never a silent skip straight to title */
    MIVF_SC_INIT,
    MIVF_SC_LIBRARY,
    MIVF_SC_MENU,
    MIVF_SC_SCENES,
    MIVF_SC_PLAYBACK,
    MIVF_SC_DASHBOARD,
    MIVF_SC_PERSONALIZATION,
    MIVF_SC_RESUME,
    MIVF_SC_SCREENSAVER,
    MIVF_SC_TITLE,
    MIVF_SC_DONE
} MivfShowcaseStage;

/* HFIX_SHOWCASE_TIMING_ROBUSTNESS_V1: which real screen loop most recently
   called mivf_showcase_tick()/synth_key() this frame. A stage's synthetic
   sequence is only allowed to fire while the loop it was actually written
   for is the one currently ticking -- see the guards in
   mivf_showcase_synth_key(). Without this, a stage whose real action is
   delayed (e.g. a frame hitch) can have a LATER stage's key presses
   land on whatever real screen is still showing once the state machine's
   own wall-clock timers move on regardless -- this is exactly what caused
   PERSONALIZATION's Settings/Theme/CVD presses to instead land on a still-
   open Scene Selection screen and accidentally launch the wrong chapter,
   observed on real emulator hardware. */
typedef enum {
    MIVF_SC_LOOP_NONE = 0,
    MIVF_SC_LOOP_BROWSER,
    MIVF_SC_LOOP_DVDMENU,
    MIVF_SC_LOOP_PLAYBACK
} MivfShowcaseLoop;

#define MIVF_SC_DEMO_DIR "sdmc:/mivf/demo"
/* MIVF_SC_DEMO_PATH_V1 (single fixed filename) removed this revision: a
   user placing a real project at sdmc:/mivf/showcase_demo.mivf kept
   reporting NOT_FOUND, most likely a naming/placement mismatch against
   that one exact required name. Replaced with a folder scan of
   MIVF_SC_DEMO_DIR -- any supported media file dropped there is found,
   and the real file browser is pointed at that same folder (see the
   scan-override in hfix58_file_browser_select()) so LIBRARY still
   genuinely browses real directory contents instead of a synthesized
   single-file listing. */
/* MIVF_SC_MARKER_PATH removed in this revision: MIVF_SHOWCASE_FULL is now
   the sole activation gate (auto-starts at app launch in a Showcase-
   enabled build). A prior revision additionally required a runtime marker
   file at sdmc:/mivf_showcase.marker; that requirement no longer exists
   and no code checks for that file. Documented here so nobody goes
   looking for a marker-file mechanism that has been deliberately removed,
   not merely forgotten. */

typedef struct {
    bool active;
    bool cancel_requested;
    MivfShowcaseStage stage;
    u64  stage_entry_tick;
    u32  stage_timeout_ms;
    MivfShowcaseLoop current_loop;   /* which real loop ticked last, this frame */
    u32  synth_step;                 /* next not-yet-fired action in the
                                         current stage's key-press schedule;
                                         reset to 0 on every stage entry */
    u32  capture_step;               /* MIVF_SHOWCASE_CAPTURE: next not-yet-taken
                                         screenshot in the flat, stage-ordered
                                         capture schedule for the WHOLE run --
                                         unlike synth_step, deliberately NOT
                                         reset per stage; see mivf_showcase_enter() */
    u32  saved_theme_index;
    bool saved_theme_custom;
    u8   saved_r, saved_g, saved_b;
    u32  saved_color_vision_mode;
    u32  saved_transport_style;
    char demo_project_path[HFIX58_MAX_PATH];
    bool demo_project_found;
} MivfShowcaseCtl;
static MivfShowcaseCtl g_mivf_showcase;

static u32 mivf_showcase_synth_down;   /* set by a stage's _step(), consumed
                                          once by mivf_showcase_synth_key() */

static void mivf_showcase_log(const char *stage, const char *event,
                              const char *classification, const char *extra) {
    printf("SHOWCASE,stage=%s,event=%s,class=%s,extra=%s,ms=%llu\n",
        stage ? stage : "?",
        event ? event : "?",
        classification ? classification : "?",
        extra ? extra : "",
        (unsigned long long)ticks_to_us(svcGetSystemTick() - g_mivf_showcase.stage_entry_tick) / 1000ull);
    if (g_mivf_log) {
        fflush(g_mivf_log);
    }
}

static void mivf_showcase_enter(MivfShowcaseStage stage, u32 timeout_ms) {
    g_mivf_showcase.stage = stage;
    g_mivf_showcase.stage_entry_tick = svcGetSystemTick();
    g_mivf_showcase.stage_timeout_ms = timeout_ms;
    g_mivf_showcase.synth_step = 0;
    /* capture_step is NOT reset here, deliberately -- see the comment above
       g_mivf_showcase_captures[]: it's a single cursor spanning the whole
       flat, stage-ordered capture schedule for the entire run, reset only
       once (via the memset in mivf_showcase_activate()), not per stage. */
    mivf_showcase_synth_down = 0;
}

static bool mivf_showcase_stage_timed_out(void) {
    u64 elapsed_ms = ticks_to_us(svcGetSystemTick() - g_mivf_showcase.stage_entry_tick) / 1000ull;
    return elapsed_ms >= (u64)g_mivf_showcase.stage_timeout_ms;
}

static void mivf_showcase_restore_state(void) {
    /* PERSONALIZATION now drives the real Settings/Theme-Picker/CVD-Picker
       screens (see mivf_showcase_synth_key()'s MIVF_SC_PERSONALIZATION
       case), and its own sequence always closes them well before this
       runs -- but this is also reached from mivf_showcase_cancel() on the
       user's B+START+SELECT chord, which can land mid-sequence. Force
       every real overlay closed through its own real close path (not a
       raw flag reset) so playback-pause state and the settings file both
       end up consistent, not just the in-memory fields below. */
    if (g_mivf_theme_picker.active) {
        mivf_theme_picker_cancel();
    }
    if (g_mivf_cvd_picker.active) {
        mivf_cvd_picker_cancel();
    }
    if (g_hfix59r3_settings_visible) {
        hfix59r3_set_settings_open(false);
    }

    g_mivf_settings.color_vision_mode = g_mivf_showcase.saved_color_vision_mode;
    g_mivf_settings.theme_index = g_mivf_showcase.saved_theme_index;
    g_mivf_settings.theme_custom = g_mivf_showcase.saved_theme_custom;
    g_mivf_settings.transport_style = g_mivf_showcase.saved_transport_style;
    if (g_mivf_showcase.saved_theme_custom) {
        mivf_theme_set_rgb(g_mivf_showcase.saved_r, g_mivf_showcase.saved_g, g_mivf_showcase.saved_b);
    } else {
        hfix60_apply_theme(g_mivf_showcase.saved_theme_index);
    }
    g_mivf_theme_generation++;
    /* The real picker "apply" presses (KEY_A) above already persisted the
       demo's temporary theme/color-vision choices to disk via the same
       MIVF_SettingsSave() the real UI always calls on apply/close -- without
       this, the user's actual settings file would stay stuck on the demo's
       values even after the in-memory/on-screen state above is restored. */
    MIVF_SettingsSave(&g_mivf_settings);
}

/* Case-insensitive "needle appears somewhere in haystack" check, used only
   to prefer a demo file whose name mentions "les" (Les Miserables) when
   more than one candidate sits in MIVF_SC_DEMO_DIR. Plain and local --
   this platform's libc doesn't reliably provide strcasestr. */
static bool mivf_showcase_name_contains_ci(const char *haystack, const char *needle) {
    size_t hn = strlen(haystack);
    size_t nn = strlen(needle);
    size_t i;

    if (nn == 0 || nn > hn) {
        return false;
    }
    for (i = 0; i + nn <= hn; i++) {
        size_t j;
        for (j = 0; j < nn; j++) {
            if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j])) {
                break;
            }
        }
        if (j == nn) {
            return true;
        }
    }
    return false;
}

/* Scans MIVF_SC_DEMO_DIR for any supported media file rather than requiring
   one fixed filename -- drop any real project in that folder and this finds
   it. Preference order: a name containing "les" (Les Miserables) wins
   outright; otherwise the most recently modified file wins ("the newest
   one"); a directory that opens but yields no eligible file, or that
   doesn't exist at all, is NOT_FOUND either way -- same explicit failure
   path as before, just checked against a folder instead of one filename. */
static bool mivf_showcase_find_demo_project(void) {
    DIR *d = opendir(MIVF_SC_DEMO_DIR);
    struct dirent *ent;
    char best_path[HFIX58_MAX_PATH] = {0};
    bool best_is_les = false;
    time_t best_mtime = 0;
    bool found = false;

    if (!d) {
        return false;
    }

    while ((ent = readdir(d)) != NULL) {
        char candidate[HFIX58_MAX_PATH];
        struct stat st;
        bool is_les;

        if (!hfix58_is_supported_media(ent->d_name)) {
            continue;
        }

        snprintf(candidate, sizeof(candidate), "%s/%s", MIVF_SC_DEMO_DIR, ent->d_name);
        if (stat(candidate, &st) != 0) {
            continue;
        }

        is_les = mivf_showcase_name_contains_ci(ent->d_name, "les");

        if (!found) {
            found = true;
        } else if (best_is_les && !is_les) {
            continue; /* an existing "les" match always outranks a non-match */
        } else if (is_les == best_is_les && st.st_mtime <= best_mtime) {
            continue; /* same rank, not newer -- keep current best */
        }

        snprintf(best_path, sizeof(best_path), "%s", candidate);
        best_is_les = is_les;
        best_mtime = st.st_mtime;
    }

    closedir(d);

    if (!found) {
        return false;
    }
    snprintf(g_mivf_showcase.demo_project_path, sizeof(g_mivf_showcase.demo_project_path),
        "%s", best_path);
    return true;
}

/* Called once from hfix58_file_browser_select(), right after its own
   normal hfix58_scan_default_dirs() call. When the Showcase is active and
   found a project in MIVF_SC_DEMO_DIR, this re-scans that same folder
   through the real hfix58_scan_dir() -- the exact function the production
   browser always uses -- so the LIBRARY stage's synthetic DDOWN/A input
   is selecting from a genuine, real directory listing of sdmc:/mivf/demo,
   not a fabricated single-entry list. A normal (non-Showcase) run never
   takes this path, and when Showcase is active but nothing was found in
   the folder, the default-dirs listing from the caller is left standing
   so NO_PROJECT's own message is what the user sees, not a confusing
   empty library. */
static bool mivf_showcase_maybe_scan_demo_dir(void) {
    if (!g_mivf_showcase.active || !g_mivf_showcase.demo_project_found) {
        return false;
    }
    return hfix58_scan_dir(MIVF_SC_DEMO_DIR);
}

/* Only the browser loop needs this as a forward-declared indirection (see
   the enum comment above MivfShowcaseLoop) -- mivf_menu_run() and play()
   are both textually after g_mivf_showcase's real definition, so they set
   current_loop directly at their own hook site. */
static void mivf_showcase_note_loop_browser(void) {
    g_mivf_showcase.current_loop = MIVF_SC_LOOP_BROWSER;
}

#ifdef MIVF_SHOWCASE_CAPTURE
/* HFIX_SHOWCASE_CAPTURE_V1: writes a real, viewable screenshot straight from
   the current 3DS framebuffer -- no PNG/JPEG encoder exists in this codebase,
   so this targets 24-bit uncompressed BMP, the simplest format any image
   viewer can open with zero dependencies. The 3DS framebuffer itself is
   stored column-major with each column's 240 pixels already in bottom-to-top
   order (see hfix58_rect565's own `col[239 - yy]` write pattern above) --
   which, worked through directly, turns out to already match BMP's own
   bottom-up row convention exactly: BMP row index R (0 = first row written)
   corresponds one-to-one to fb[x*240 + R] for every column x, with no
   further flipping needed. */
static bool mivf_showcase_write_bmp565(const char *path, const u8 *fb, int width) {
    const int height = 240;
    const int row_bytes = (width * 3 + 3) & ~3;
    const int pad = row_bytes - width * 3;
    const u32 data_size = (u32)row_bytes * (u32)height;
    const u32 file_size = 54u + data_size;
    u8 header[54];
    u8 row[400 * 3 + 4];
    FILE *f;
    int x, bmp_row;

    if (!fb || !path || width <= 0 || width > 400) {
        return false;
    }

    f = fopen(path, "wb");
    if (!f) {
        return false;
    }

    memset(header, 0, sizeof(header));
    header[0] = 'B'; header[1] = 'M';
    header[2] = (u8)(file_size); header[3] = (u8)(file_size >> 8);
    header[4] = (u8)(file_size >> 16); header[5] = (u8)(file_size >> 24);
    header[10] = 54; /* pixel data offset */
    header[14] = 40; /* BITMAPINFOHEADER size */
    header[18] = (u8)(width); header[19] = (u8)(width >> 8);
    header[20] = (u8)(width >> 16); header[21] = (u8)(width >> 24);
    header[22] = (u8)(height); header[23] = (u8)(height >> 8);
    header[24] = (u8)(height >> 16); header[25] = (u8)(height >> 24);
    header[26] = 1;  /* planes */
    header[28] = 24; /* bits per pixel */

    fwrite(header, 1, sizeof(header), f);

    for (bmp_row = 0; bmp_row < height; bmp_row++) {
        for (x = 0; x < width; x++) {
            u16 px = ((const u16*)fb)[x * 240 + bmp_row];
            u8 r = (u8)(((px >> 11) & 0x1Fu) * 255u / 31u);
            u8 g = (u8)(((px >> 5) & 0x3Fu) * 255u / 63u);
            u8 b = (u8)((px & 0x1Fu) * 255u / 31u);
            row[x * 3 + 0] = b;
            row[x * 3 + 1] = g;
            row[x * 3 + 2] = r;
        }
        if (pad > 0) {
            memset(row + (size_t)width * 3, 0, (size_t)pad);
        }
        fwrite(row, 1, (size_t)row_bytes, f);
    }

    fclose(f);
    return true;
}

static void mivf_showcase_capture_now(const char *name) {
    char path[HFIX58_MAX_PATH];
    u16 fw = 0, fh = 0;
    u8 *top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &fw, &fh);
    u8 *bot = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &fw, &fh);

    snprintf(path, sizeof(path), "sdmc:/mivf/showcase_captures/%s_top.bmp", name);
    mivf_showcase_write_bmp565(path, top, 400);
    snprintf(path, sizeof(path), "sdmc:/mivf/showcase_captures/%s_bot.bmp", name);
    mivf_showcase_write_bmp565(path, bot, 320);
    mivf_showcase_log("CAPTURE", name, "PASS", path);
}

/* One entry per screenshot, evaluated with the exact same monotonic-cursor
   technique as mivf_showcase_fire_next() (see the timing-robustness comment
   above MivfShowcaseLoop): capture_step only ever advances forward, so nothing
   is ever captured twice and a delayed frame just delays the capture instead
   of skipping it. Trigger times are chosen late enough in each stage that
   fades/animations have settled, early enough to land well before that
   stage's own timeout. */
typedef struct {
    MivfShowcaseStage stage;
    u32 trigger_ms;
    const char *name;
} MivfShowcaseCaptureEntry;

static const MivfShowcaseCaptureEntry g_mivf_showcase_captures[] = {
    { MIVF_SC_LIBRARY,         2200, "library" },
    { MIVF_SC_MENU,            2000, "dvd_menu_root" },
    { MIVF_SC_SCENES,          1500, "scene_selection" },
    { MIVF_SC_PLAYBACK,        3000, "playback" },
    { MIVF_SC_DASHBOARD,       1250, "dashboard_premiere" },
    { MIVF_SC_DASHBOARD,       3750, "dashboard_gameboy" },
    { MIVF_SC_DASHBOARD,       6250, "dashboard_receiver" },
    { MIVF_SC_DASHBOARD,       8750, "dashboard_accessible" },
    { MIVF_SC_PERSONALIZATION, 2500, "theme_picker" },
    { MIVF_SC_PERSONALIZATION, 6000, "color_vision_picker" },
    { MIVF_SC_SCREENSAVER,     3000, "screensaver" },
    { MIVF_SC_TITLE,           2000, "title_card" },
};
#define MIVF_SHOWCASE_CAPTURE_COUNT \
    (sizeof(g_mivf_showcase_captures) / sizeof(g_mivf_showcase_captures[0]))

/* Called from every loop hook alongside mivf_showcase_tick(), same as
   mivf_showcase_synth_key() -- so whichever real screen is actually showing
   when a stage's trigger time arrives is exactly what gets captured. */
static void mivf_showcase_maybe_capture(void) {
    u64 elapsed_ms;
    u32 i;

    if (!g_mivf_showcase.active) {
        return;
    }

    elapsed_ms = ticks_to_us(svcGetSystemTick() - g_mivf_showcase.stage_entry_tick) / 1000ull;

    for (i = g_mivf_showcase.capture_step; i < MIVF_SHOWCASE_CAPTURE_COUNT; i++) {
        if (g_mivf_showcase_captures[i].stage != g_mivf_showcase.stage) {
            break; /* schedule is grouped by stage in declaration order */
        }
        if (elapsed_ms < (u64)g_mivf_showcase_captures[i].trigger_ms) {
            break;
        }
        mivf_showcase_capture_now(g_mivf_showcase_captures[i].name);
        g_mivf_showcase.capture_step = i + 1;
    }
}
#endif

static void mivf_showcase_activate(void) {
    /* MIVF_SHOWCASE_FULL is the sole activation gate as of this revision --
       the compile-time #ifdef around this call's one and only call site
       (main(), immediately after mivf_log_open()/mivf_diag_open()) is what
       makes a Showcase-enabled build auto-start. No runtime marker file is
       checked; see the note above MivfShowcaseStage for why. */
    memset(&g_mivf_showcase, 0, sizeof(g_mivf_showcase));
    g_mivf_showcase.saved_color_vision_mode = g_mivf_settings.color_vision_mode;
    g_mivf_showcase.saved_theme_index = g_mivf_settings.theme_index;
    g_mivf_showcase.saved_theme_custom = g_mivf_settings.theme_custom;
    g_mivf_showcase.saved_transport_style = g_mivf_settings.transport_style;
    g_mivf_showcase.saved_r = g_mivf_theme_r;
    g_mivf_showcase.saved_g = g_mivf_theme_g;
    g_mivf_showcase.saved_b = g_mivf_theme_b;

    g_mivf_showcase.demo_project_found = mivf_showcase_find_demo_project();
    g_mivf_showcase.active = true;

#ifdef MIVF_SHOWCASE_CAPTURE
    /* Best-effort: ignore the result if it already exists (EEXIST) or the
       SD card is somehow unwritable here -- mivf_showcase_maybe_capture()'s
       own fopen() calls will simply fail silently later in that case,
       exactly like every other sidecar-write path in this codebase. */
    mkdir("sdmc:/mivf/showcase_captures", 0777);
#endif

    if (!g_mivf_showcase.demo_project_found) {
        /* Explicit, visible failure -- never a silent skip. Shown for a
           restrained hold, then the title card itself, with a subordinate
           status line, so the run is never described (even implicitly) as
           successful. */
        mivf_showcase_enter(MIVF_SC_NO_PROJECT, 4000);
        mivf_showcase_log("INIT", "NO_DEMO_PROJECT", "FAIL", MIVF_SC_DEMO_DIR);
        return;
    }

    mivf_showcase_enter(MIVF_SC_INIT, 1500);
    mivf_showcase_log("INIT", "START", "PASS", g_mivf_showcase.demo_project_path);
}

static void mivf_showcase_cancel(const char *reason) {
    if (!g_mivf_showcase.active) {
        return;
    }
    mivf_showcase_log(
        (g_mivf_showcase.stage == MIVF_SC_INACTIVE) ? "?" : "CANCEL",
        "CANCELLED", "CANCELLED", reason);
    mivf_showcase_restore_state();
    memset(&g_mivf_showcase, 0, sizeof(g_mivf_showcase));
}

/* HFIX_SHOWCASE_PERSONALIZATION_REAL_UI_V1: this stage previously just
   poked g_mivf_settings.transport_style/color_vision_mode directly once a
   second, via the exact same fields the real Settings screen writes but
   never actually opening it -- so nothing about the real Theme Picker or
   Color Vision Picker screens was ever shown. Replaced with a real
   synthetic input sequence (see the MIVF_SC_PERSONALIZATION case in
   mivf_showcase_synth_key() below) that opens Settings with KEY_SELECT,
   navigates to the THEME row and the COLOR VISION row exactly the way a
   real press-by-press user would, and opens/adjusts/applies each real
   picker screen (mivf_theme_picker_input()/mivf_cvd_picker_input(), both
   only reachable from inside play() -- see Settings entry point). No
   separate step-tracking function is needed any more; the timeline lives
   entirely in the single-shot synth_key() triggers. */

/* Advances the Showcase's own stage machine. Called once per frame from
   each relevant existing screen loop (see the small hooks added at those
   loops' own hidKeysDown() call sites). Never blocks; every stage has a
   bounded timeout and a safe forward-only fallback. */
static void mivf_showcase_tick(void) {
    if (!g_mivf_showcase.active) {
        return;
    }
    if (g_mivf_showcase.cancel_requested) {
        mivf_showcase_cancel("USER_CHORD");
        return;
    }

    switch (g_mivf_showcase.stage) {
        case MIVF_SC_NO_PROJECT:
            /* Message rendering happens at whichever loop hook is currently
               active (see the short-circuit blocks added at the browser
               and per-movie-menu loops) -- this only owns the timeout and
               transition to the title card, which will show a subordinate
               failure-status line, never presenting this run as a normal
               completion. */
            if (mivf_showcase_stage_timed_out()) {
                mivf_showcase_log("NO_PROJECT", "DONE", "FAIL", "");
                mivf_showcase_enter(MIVF_SC_TITLE, 7000);
            }
            break;

        case MIVF_SC_INIT:
            if (mivf_showcase_stage_timed_out()) {
                mivf_showcase_log("INIT", "DONE", "PASS", "");
                mivf_showcase_enter(MIVF_SC_LIBRARY, 4500);
            }
            break;

        case MIVF_SC_LIBRARY:
            /* Synthesizes navigation toward the demo project and a launch
               press; see mivf_showcase_synth_key(). Advances on timeout
               regardless, so a browser that never finds the project still
               reaches the title card rather than hanging. */
            if (mivf_showcase_stage_timed_out()) {
                mivf_showcase_log("LIBRARY", "TIMEOUT", "SKIPPED", "");
                mivf_showcase_enter(MIVF_SC_MENU, 5000);
            }
            break;

        case MIVF_SC_MENU:
            if (mivf_showcase_stage_timed_out()) {
                mivf_showcase_log("MENU", "DONE", "PASS", "");
                mivf_showcase_enter(MIVF_SC_SCENES, 6000);
            }
            break;

        case MIVF_SC_SCENES:
            if (mivf_showcase_stage_timed_out()) {
                mivf_showcase_log("SCENES", "TIMEOUT", "SKIPPED", "");
                mivf_showcase_enter(MIVF_SC_PLAYBACK, 7000);
            }
            break;

        case MIVF_SC_PLAYBACK:
            if (g_media_ctl.state == STATE_PLAYING &&
                MIVF_SubtitlesTextAtMs(&g_hfix58s_subtitles,
                    (u32)(ticks_to_us(svcGetSystemTick() - g_mivf_showcase.stage_entry_tick) / 1000ull)) != NULL) {
                mivf_showcase_log("PLAYBACK", "SUBTITLE_SEEN", "PASS", "");
                mivf_showcase_enter(MIVF_SC_DASHBOARD, 10000);
            } else if (mivf_showcase_stage_timed_out()) {
                mivf_showcase_log("PLAYBACK", "TIMEOUT", (g_media_ctl.state == STATE_PLAYING) ? "PASS" : "SKIPPED", "");
                mivf_showcase_enter(MIVF_SC_DASHBOARD, 10000);
            }
            break;

        case MIVF_SC_DASHBOARD:
            /* HFIX_SHOWCASE_DASHBOARD_THEME_TOUR_V1: cycles the REAL
               transport-style dashboard through 4 real, distinct styles.
               This is not a synthetic-input sequence like SCENES/
               PERSONALIZATION below -- mivf_c21_style_id() (main.c ~9904)
               reads g_mivf_settings.transport_style fresh every single
               frame with no indirection, and the real playback dashboard
               renderer (mivf_c21_draw_dashboard(), called every frame from
               the real playback draw path) draws whichever real style that
               value names. Directly setting it here is exactly what a real
               settings change would produce on this exact screen -- there
               is no separate "preview" representation being faked. Style
               names/IDs confirmed by reading the real enum, name table, and
               dispatch switch (main.c ~8385-8438, ~10076) rather than
               assumed: MIVF_TRANSPORT_STYLE_PORTABLE_MONO is the one
               already labeled as the Game Boy style in an existing code
               comment at its one other real-code reference (main.c ~9896),
               and MIVF_TRANSPORT_STYLE_RADIO is the one whose real display name
               is literally "RECEIVER". MIVF_TRANSPORT_STYLE_ACCESSIBLE is
               the only style with use_accessible_outline set, making it
               the evidenced choice for a fourth, high-contrast style.
               Setting the same value repeatedly every frame within a
               window is harmless (unlike a repeated key-press), so this
               doesn't need the single-fire cursor discipline SCENES/
               PERSONALIZATION need -- it only ever needs to be idempotent,
               which a plain last-value-wins comparison already is. */
            {
                u64 elapsed_ms = ticks_to_us(svcGetSystemTick() - g_mivf_showcase.stage_entry_tick) / 1000ull;
                u32 want_style;
                if (elapsed_ms < 2500ull) {
                    want_style = MIVF_TRANSPORT_STYLE_CINEMATIC;      /* Premiere (default) */
                } else if (elapsed_ms < 5000ull) {
                    want_style = MIVF_TRANSPORT_STYLE_PORTABLE_MONO;  /* Game Boy-inspired */
                } else if (elapsed_ms < 7500ull) {
                    want_style = MIVF_TRANSPORT_STYLE_RADIO;          /* Receiver */
                } else {
                    want_style = MIVF_TRANSPORT_STYLE_ACCESSIBLE;    /* high-contrast */
                }
                if (g_mivf_settings.transport_style != want_style) {
                    g_mivf_settings.transport_style = want_style;
                    mivf_showcase_log("DASHBOARD", "STYLE", "PASS", mivf_transport_style_name(want_style));
                }
            }
            if (mivf_showcase_stage_timed_out()) {
                mivf_showcase_log("DASHBOARD", "DONE", "PASS", "");
                /* 9s budget: real Settings navigation to two different rows,
                   cycling visibly through 3 theme presets, plus a real
                   Color Vision Picker -- see mivf_showcase_synth_key()'s
                   MIVF_SC_PERSONALIZATION case (sequence now finishes at
                   6600ms), leaving real margin before this timeout. */
                mivf_showcase_enter(MIVF_SC_PERSONALIZATION, 9000);
            }
            break;

        case MIVF_SC_PERSONALIZATION:
            if (mivf_showcase_stage_timed_out()) {
                mivf_showcase_restore_state();
                mivf_showcase_log("PERSONALIZATION", "DONE", "PASS", "");
                mivf_showcase_enter(MIVF_SC_RESUME, 4000);
            }
            break;

        case MIVF_SC_RESUME:
            if (mivf_showcase_stage_timed_out()) {
                mivf_showcase_log("RESUME", "TIMEOUT", "SKIPPED", "");
                mivf_showcase_enter(MIVF_SC_SCREENSAVER, 5000);
            }
            break;

        case MIVF_SC_SCREENSAVER:
            /* Acceleration itself happens at the per-movie-menu loop hook
               (main.c, the idle_frames/screensaver_active local variables
               near MIVF_MENU_SCREENSAVER_IDLE_FRAMES) -- this stage only
               owns the timeout and the exit/restore transition. See
               mivf_showcase_screensaver_step() below. */
            if (mivf_showcase_stage_timed_out()) {
                mivf_showcase_log("SCREENSAVER", "DONE", "PASS", "");
                mivf_showcase_enter(MIVF_SC_TITLE, 7000);
            }
            break;

        case MIVF_SC_TITLE:
            if (mivf_showcase_stage_timed_out()) {
                mivf_showcase_log("TITLE", "DONE", "PASS", "");
                mivf_showcase_enter(MIVF_SC_DONE, 0);
            }
            break;

        case MIVF_SC_DONE:
        default:
            mivf_showcase_restore_state();
            mivf_showcase_log("DONE", "COMPLETE", "PASS", "");
            g_mivf_showcase.active = false;
            break;
    }
}

/* Called immediately after each screen's own hidKeysDown() call. ORs a
   synthetic bit in only when the current stage requests one for this
   frame; never removes or overrides real input, which is also how a
   genuine B+START+SELECT chord still reaches mivf_showcase_cancel_check()
   below even while synthetic keys are active. */
/* HFIX_SHOWCASE_TIMING_ROBUSTNESS_V1 (continued from the enum comment
   above): fires at most one synthetic key per frame, using a monotonic
   per-stage cursor (g_mivf_showcase.synth_step, reset to 0 by
   mivf_showcase_enter()) rather than inferring "did elapsed_ms just cross
   this threshold" from a fixed lookback window. The previous approach
   (checking `elapsed_ms >= T && elapsed_ms - 16 < T`) assumed consecutive
   frames are never more than ~16ms apart; a single dropped/hitched frame
   (opening a picker, a menu fade, a sound trigger -- all real possibilities
   on this hardware) can make elapsed_ms jump past T in one step, and that
   press is then gone forever, never re-checked. That's confirmed to have
   actually happened: a real emulator run's log showed a chapter launching
   at index 4 instead of the intended index 2, and Settings never opening
   at all -- both explained by dropped presses. A cursor never drops a
   press, only delays it: whichever action is next simply fires on the
   first frame whose elapsed_ms has reached (or passed) its schedule time,
   however late that frame arrives, and the cursor advances so it's never
   fired twice. */
static bool mivf_showcase_fire_next(u64 elapsed_ms, const u32 *fire_ms, const u32 *fire_key, size_t count, u32 *down_bits) {
    if (g_mivf_showcase.synth_step >= count) {
        return false;
    }
    if (elapsed_ms < (u64)fire_ms[g_mivf_showcase.synth_step]) {
        return false;
    }
    *down_bits |= fire_key[g_mivf_showcase.synth_step];
    g_mivf_showcase.synth_step++;
    return true;
}

static void mivf_showcase_synth_key(u32 *down_bits) {
    if (!g_mivf_showcase.active || !down_bits) {
        return;
    }
    switch (g_mivf_showcase.stage) {
        case MIVF_SC_LIBRARY:
            /* Real production selection-wrap logic (main.c ~8165-8207)
               decides where this actually lands -- this only supplies the
               same key bits a physical press would, and only while the
               browser is genuinely the active loop. */
            if (g_mivf_showcase.current_loop == MIVF_SC_LOOP_BROWSER) {
                static const u32 fire_ms[] = { 1500, 3000 };
                static const u32 fire_key[] = { KEY_DDOWN, KEY_A };
                u64 elapsed_ms = ticks_to_us(svcGetSystemTick() - g_mivf_showcase.stage_entry_tick) / 1000ull;
                mivf_showcase_fire_next(elapsed_ms, fire_ms, fire_key, 2, down_bits);
            }
            break;
        case MIVF_SC_SCENES:
            /* MIVF_CAPTURE_CH4_DIRECT_V3: mivf_menu_run performs the
               deterministic production-path handoff after the visible
               Scene Selection dwell. Do not synthesize A here. */
            break;
        case MIVF_SC_PERSONALIZATION:
            /* Drives the REAL Settings screen and the REAL Theme Picker /
               Color Vision Picker screens -- see the long comment above
               mivf_showcase_restore_state()'s dispatch and the research
               that led here: these three screens are all multiplexed
               through hfix59r3_handle_settings_menu(), called once per
               frame from play()'s own loop, only reachable while actual
               playback is active (KEY_SELECT opens it; there's no route
               in from the browser or the DVD-style menu). Settings opens
               on row 0; THEME is row 7 (7x DDOWN); from there, DUP wraps
               backward through row 0 to row 24 then down to row 23
               (COLOR VISION) in 9 presses -- shorter than 16 more DDOWNs
               forward. Gated on current_loop == PLAYBACK: this is the
               exact guard that would have prevented the observed bug
               (these presses landing on a still-open Scene Selection
               screen and launching the wrong chapter) even before the
               cursor fix above made that scenario unlikely in the first
               place -- defense in depth, not just a faster sequence.
               HFIX_SHOWCASE_THEME_CYCLE_V1: KEY_Y jumps to the next of the
               4 built-in presets (mivf_theme_picker_input(), main.c
               ~3274-3279) -- originally pressed once, which only ever
               showed a single jump rather than actually demonstrating
               "the different themes." Now pressed 3 times with real pauses
               between each so every preset it lands on is genuinely
               visible on screen before the final apply, landing on a
               different theme than the one it started on. */
            if (g_mivf_showcase.current_loop == MIVF_SC_LOOP_PLAYBACK) {
                static const u32 fire_ms[] = {
                    200,                                    /* SELECT: open Settings */
                    500, 650, 800, 950, 1100, 1250, 1400,   /* DDOWN x7: row 0 -> THEME (7) */
                    1700,                                    /* A: open Theme Picker */
                    2000,                                    /* Y: preset 1 */
                    2650,                                    /* Y: preset 2 */
                    3300,                                    /* Y: preset 3 */
                    3600,                                    /* R: nudge hue on the final preset */
                    3900,                                    /* A: apply/save theme */
                    4200, 4350, 4500, 4650, 4800, 4950, 5100, 5250, 5400,
                                                             /* DUP x9: row 7 -> wrap -> COLOR VISION (23) */
                    5700,                                    /* A: open Color Vision Picker */
                    6000,                                    /* DRIGHT: cycle to next CVD mode */
                    6300,                                    /* A: apply/save color vision mode */
                    6600                                     /* SELECT: close Settings (saves) */
                };
                static const u32 fire_key[] = {
                    KEY_SELECT,
                    KEY_DDOWN, KEY_DDOWN, KEY_DDOWN, KEY_DDOWN, KEY_DDOWN, KEY_DDOWN, KEY_DDOWN,
                    KEY_A,
                    KEY_Y, KEY_Y, KEY_Y,
                    KEY_R,
                    KEY_A,
                    KEY_DUP, KEY_DUP, KEY_DUP, KEY_DUP, KEY_DUP, KEY_DUP, KEY_DUP, KEY_DUP, KEY_DUP,
                    KEY_A,
                    KEY_DRIGHT,
                    KEY_A,
                    KEY_SELECT
                };
                u64 elapsed_ms = ticks_to_us(svcGetSystemTick() - g_mivf_showcase.stage_entry_tick) / 1000ull;
                mivf_showcase_fire_next(elapsed_ms, fire_ms, fire_key, sizeof(fire_ms) / sizeof(fire_ms[0]), down_bits);
            }
            break;
        default:
            break;
    }
}

/* Checked once per frame alongside mivf_showcase_synth_key() at each hook
   site, using the REAL (not synthetic) key bits so cancellation always
   works regardless of Showcase stage. */
static void mivf_showcase_cancel_check(u32 real_down, u32 real_held) {
    if (!g_mivf_showcase.active) {
        return;
    }
    if ((real_down & KEY_B) && (real_held & KEY_START) && (real_held & KEY_SELECT)) {
        g_mivf_showcase.cancel_requested = true;
    }
}

/* HFIX_SHOWCASE_SCREEN_FILL_BUG_V1: the first hardware/emulator run of this
   feature (real Azahar screenshot) showed the actual bug this fix
   addresses -- the top screen is 400px wide, not 320; filling only 320
   left an un-cleared 80px strip on the right showing whatever the
   library's preview panel had drawn there previously, and the bottom
   screen was never touched at all, so it kept showing the last real
   library frame underneath. Both screens are now explicitly cleared. */
static void mivf_showcase_draw_title_card(u8 *fb_top, u8 *fb_bot) {
    if (fb_bot) {
        hfix58_rect565(fb_bot, 0, 0, 320, 240, 1, 3, 6);
    }
    if (!fb_top) {
        return;
    }
    hfix58_rect565_top(fb_top, 0, 0, 400, 240, 1, 3, 6);
    hfix58_draw_text_shadow(fb_top, 158, 96, "MIVF", 3, 235, 240, 245);
    hfix58_draw_text_shadow(fb_top, 66, 138, "YOUR MOVIES, REIMAGINED FOR NINTENDO 3DS", 1, 200, 210, 220);
    hfix58_draw_text_shadow(fb_top, 66, 156, "IN DEVELOPMENT -- HARDWARE VALIDATION UNDERWAY", 1, 150, 158, 168);
    if (!g_mivf_showcase.demo_project_found) {
        /* Never present this run as a normal completion -- see
           MIVF_SC_NO_PROJECT. Subordinate to the rest of the card, but
           always visible when this is why the run looks the way it does. */
        hfix58_draw_text_shadow(fb_top, 66, 192, "SHOWCASE DEMO PROJECT NOT FOUND", 1, 200, 150, 120);
    }
    hfix58_draw_text_shadow(fb_top, 66, 210, "INTERNAL SHOWCASE BUILD", 1, 90, 96, 104);
}

/* Explicit, restrained failure message -- shown instead of any library/menu
   choreography when the lawful demo project is absent, per the requirement
   that a missing project must never be papered over by silently continuing
   with an unrelated personal movie or an unexplained skip to the title
   card. Same both-screens-cleared fix as the title card above. */
static void mivf_showcase_draw_no_project_message(u8 *fb_top, u8 *fb_bot) {
    if (fb_bot) {
        hfix58_rect565(fb_bot, 0, 0, 320, 240, 1, 3, 6);
    }
    if (!fb_top) {
        return;
    }
    hfix58_rect565_top(fb_top, 0, 0, 400, 240, 1, 3, 6);
    hfix58_draw_text_shadow(fb_top, 60, 96, "SHOWCASE DEMO PROJECT NOT FOUND", 1, 235, 240, 245);
    hfix58_draw_text_shadow(fb_top, 60, 116, "EXPECTED: ANY MEDIA FILE IN SDMC:/MIVF/DEMO", 1, 190, 196, 204);
    hfix58_draw_text_shadow(fb_top, 60, 150, "CANCEL WITH B + START + SELECT", 1, 150, 158, 168);
}

/* Single dispatch point for "does whichever screen loop is currently
   active need to render a Showcase overlay instead of its own normal
   content this frame." Used at every loop hook (including ones textually
   earlier than this function, via the plain-bool forward declaration near
   the browser loop) so none of them need to reference MivfShowcaseStage's
   enum constants or the g_mivf_showcase struct directly -- only this one
   function needs the full type definitions, which is why it lives here
   rather than needing every call site to forward-declare the enum/struct
   too (an enum, unlike a struct, cannot be forward-declared in C without
   defining it, which is what caused this exact call to need refactoring
   into its own function during this revision's build -- see the
   changelog). */
static bool mivf_showcase_render_overlay_if_needed(void) {
    u16 fw = 0, fh = 0;
    u8 *top;
    u8 *bot;

    if (!g_mivf_showcase.active) {
        return false;
    }
    if (g_mivf_showcase.stage != MIVF_SC_NO_PROJECT && g_mivf_showcase.stage != MIVF_SC_TITLE) {
        return false;
    }

    top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &fw, &fh);
    bot = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &fw, &fh);
    if (g_mivf_showcase.stage == MIVF_SC_NO_PROJECT) {
        mivf_showcase_draw_no_project_message(top, bot);
    } else {
        mivf_showcase_draw_title_card(top, bot);
    }
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
    return true;
}

/* Forces the real production idle-screensaver trigger (the idle_frames /
   screensaver_active locals in the per-movie-menu loop, gated by the
   existing MIVF_MENU_SCREENSAVER_IDLE_FRAMES threshold) to fire, without
   touching that threshold or any other idle/autodim behavior. Called only
   from that loop's own hook, where those variables are in scope; a normal
   (non-Showcase) run never executes this path since the call itself is
   `#ifdef MIVF_SHOWCASE_FULL`-gated at the call site. */
static void mivf_showcase_screensaver_accelerate(u32 *idle_frames, u32 threshold) {
    if (g_mivf_showcase.active && g_mivf_showcase.stage == MIVF_SC_SCREENSAVER) {
        if (*idle_frames < threshold) {
            *idle_frames = threshold;
        }
    }
}

/* Cleanly resets the same locals a real keypress would (see the wake path
   a few lines above this hook's call site), so leaving the stage restores
   normal idle tracking for whatever real use follows -- no state is left
   artificially accelerated after this stage ends. */
static void mivf_showcase_screensaver_restore(u32 *idle_frames, bool *screensaver_active) {
    *idle_frames = 0;
    *screensaver_active = false;
}

#endif /* MIVF_SHOWCASE_FULL */

/* Forward declaration: opening the picker now starts Showcase immediately. */
static void mivf_demo_start(void);

static void mivf_transport_picker_open(void) {
    g_mivf_transport_picker.active = true;
    g_mivf_transport_picker.original_style = g_mivf_settings.transport_style;
    g_mivf_transport_picker.preview_style = g_mivf_settings.transport_style;

    /* MIVF_TRANSPORT_SHOWCASE_AUTOSTART_V1:
       Entering Transport Preview begins the real-time 48-second tour
       automatically. SELECT can still restart the tour after completion. */
    mivf_demo_start();
}

static void mivf_transport_picker_preview(u32 style) {
    g_mivf_transport_picker.preview_style = style % MIVF_TRANSPORT_STYLE_COUNT;
    g_mivf_settings.transport_style = g_mivf_transport_picker.preview_style;
    g_mivf_theme_generation++;
}

/* Real-time three-second Showcase timing. This is presentation-only and
   intentionally independent of UI redraw frequency and playback timing. */
#define MIVF_DEMO_SECONDS_PER_STYLE 3u
#define MIVF_DEMO_CONTROL_COUNT 3u
#define MIVF_DEMO_STYLE_TICKS \
    ((u64)SYSCLOCK_ARM11 * (u64)MIVF_DEMO_SECONDS_PER_STYLE)
#define MIVF_DEMO_CONTROL_TICKS \
    (MIVF_DEMO_STYLE_TICKS / (u64)MIVF_DEMO_CONTROL_COUNT)
#define MIVF_DEMO_TOTAL_TICKS \
    ((u64)MIVF_TRANSPORT_STYLE_COUNT * MIVF_DEMO_STYLE_TICKS)
#define MIVF_DEMO_TOTAL_SECONDS \
    ((u32)MIVF_TRANSPORT_STYLE_COUNT * MIVF_DEMO_SECONDS_PER_STYLE)
/* MIVF_AUTOTEST_LOG_V1
   Read-only diagnostic snapshots for the automatic transport Showcase.
   printf is already routed through tee_printf to the normal MIVF log. */
static void mivf_autotest_log(const char *event,u32 scene,u32 control,u32 elapsed_sec){
    printf("AUTOTEST,event=%s,sec=%lu,style=%lu,name=%s,control=%lu,state=%d,frame=%lu,total=%lu,seek=%d,audio_submit=%lu,audio_drop=%lu,meter_l=%lu,meter_r=%lu,volume=%d,subs=%d,speed=%lu,theme=%lu,cvd=%lu,dimmed=%d\n",
        event?event:"UNKNOWN",
        (unsigned long)elapsed_sec,
        (unsigned long)scene,
        mivf_transport_style_name(scene),
        (unsigned long)control,
        (int)g_media_ctl.state,
        (unsigned long)hfix58f_current_frame(),
        (unsigned long)hfix58f_total_frames(),
        hfix58f_seek_active()?1:0,
        (unsigned long)g_audio_submit,
        (unsigned long)g_audio_drop,
        (unsigned long)g_audio_meter_left,
        (unsigned long)g_audio_meter_right,
        g_hfix56_volume_percent,
        g_mivf_settings.show_subtitle_tracks?1:0,
        (unsigned long)mivf_speed_pct(),
        (unsigned long)g_mivf_settings.theme_index,
        (unsigned long)g_mivf_settings.color_vision_mode,
        g_mivf_brightness_dimmed?1:0);
}

static const u8 g_mivf_demo_rgb[MIVF_TRANSPORT_STYLE_COUNT][3]={{112,86,235},{46,183,224},{226,231,238},{82,234,104},{255,216,72},{226,155,83},{116,132,255},{184,54,52},{239,188,36},{92,226,116},{238,74,68},{76,98,61},{78,177,224},{94,238,91},{116,218,73},{104,183,255}};
static void mivf_demo_restore(void){g_mivf_settings.color_vision_mode=g_mivf_transport_picker.saved_color_mode;g_mivf_settings.theme_index=g_mivf_transport_picker.saved_theme_index;g_mivf_settings.theme_custom=g_mivf_transport_picker.saved_custom;if(g_mivf_transport_picker.saved_custom)mivf_theme_set_rgb(g_mivf_transport_picker.saved_r,g_mivf_transport_picker.saved_g,g_mivf_transport_picker.saved_b);else hfix60_apply_theme(g_mivf_transport_picker.saved_theme_index);}
static void mivf_demo_stop(bool restore){if(!g_mivf_transport_picker.demo_active)return;g_mivf_transport_picker.demo_active=false;if(g_mivf_log)fflush(g_mivf_log);if(restore){g_mivf_transport_picker.preview_style=g_mivf_transport_picker.original_style;g_mivf_settings.transport_style=g_mivf_transport_picker.original_style;}mivf_demo_restore();g_mivf_ui_skin.selected_index=1;g_mivf_theme_generation++;}
static void mivf_demo_start(void){
    char message[48];

    g_mivf_transport_picker.demo_active=true;
    g_mivf_transport_picker.demo_start_tick=svcGetSystemTick();
    g_mivf_transport_picker.demo_last_scene=0xFFFFFFFFu;
    g_mivf_transport_picker.demo_last_control=0xFFFFFFFFu;
    g_mivf_transport_picker.demo_last_log_second=0xFFFFFFFFu;
    g_mivf_transport_picker.saved_color_mode=g_mivf_settings.color_vision_mode;
    g_mivf_transport_picker.saved_theme_index=g_mivf_settings.theme_index;
    g_mivf_transport_picker.saved_custom=g_mivf_settings.theme_custom;
    g_mivf_transport_picker.saved_r=g_mivf_theme_r;
    g_mivf_transport_picker.saved_g=g_mivf_theme_g;
    g_mivf_transport_picker.saved_b=g_mivf_theme_b;

    snprintf(
        message,
        sizeof(message),
        "SHOWCASE: %lu SECOND TOUR",
        (unsigned long)MIVF_DEMO_TOTAL_SECONDS
    );
    /* hfix58_alert_set copies the message into its retained alert buffer. */
    hfix58_alert_set(message,1);
    mivf_autotest_log("START",0u,0u,0u);
}
static void mivf_demo_update(void){
    u64 now;
    u64 elapsed;
    u64 scene_elapsed;
    u32 scene;
    u32 control;

    if(!g_mivf_transport_picker.demo_active)return;

    now=svcGetSystemTick();
    elapsed=now-g_mivf_transport_picker.demo_start_tick;

    if(elapsed>=MIVF_DEMO_TOTAL_TICKS){
        mivf_autotest_log("COMPLETE",MIVF_TRANSPORT_STYLE_COUNT-1u,2u,MIVF_DEMO_TOTAL_SECONDS);
        mivf_demo_stop(true);
        hfix58_alert_set("SHOWCASE COMPLETE",1);
        return;
    }

    scene=(u32)(elapsed/MIVF_DEMO_STYLE_TICKS);
    scene_elapsed=elapsed%MIVF_DEMO_STYLE_TICKS;

    if(scene>=(u32)MIVF_TRANSPORT_STYLE_COUNT){
        mivf_demo_stop(true);
        hfix58_alert_set("SHOWCASE COMPLETE",1);
        return;
    }

    control=(u32)(scene_elapsed/MIVF_DEMO_CONTROL_TICKS);
    if(control>=MIVF_DEMO_CONTROL_COUNT){
        control=MIVF_DEMO_CONTROL_COUNT-1u;
    }

    g_mivf_transport_picker.preview_style=scene;
    g_mivf_settings.transport_style=scene;
    g_mivf_ui_skin.selected_index=(int)control;
    hfix58b_sync_hover_state();

    {
        u32 elapsed_sec=(u32)(elapsed/(u64)SYSCLOCK_ARM11);
        if(scene!=g_mivf_transport_picker.demo_last_scene){
            mivf_autotest_log("STYLE",scene,control,elapsed_sec);
        }
        if(control!=g_mivf_transport_picker.demo_last_control){
            mivf_autotest_log("CONTROL",scene,control,elapsed_sec);
            g_mivf_transport_picker.demo_last_control=control;
        }
        if(elapsed_sec!=g_mivf_transport_picker.demo_last_log_second){
            mivf_autotest_log("SNAPSHOT",scene,control,elapsed_sec);
            g_mivf_transport_picker.demo_last_log_second=elapsed_sec;
        }
    }

    if(scene!=g_mivf_transport_picker.demo_last_scene){
        g_mivf_settings.color_vision_mode=MIVF_CVD_STANDARD;
        mivf_theme_set_rgb(
            g_mivf_demo_rgb[scene][0],
            g_mivf_demo_rgb[scene][1],
            g_mivf_demo_rgb[scene][2]
        );
        g_mivf_transport_picker.demo_last_scene=scene;
    }
}

static void mivf_transport_picker_cancel(void) {
    g_mivf_transport_picker.active = false;
    g_mivf_settings.transport_style = g_mivf_transport_picker.original_style;
    g_mivf_theme_generation++;
}

static bool mivf_transport_picker_input(u32 down){if(g_mivf_transport_picker.demo_active){if(down&(KEY_B|KEY_START|KEY_SELECT)){mivf_autotest_log("USER_STOP",g_mivf_transport_picker.preview_style,(u32)g_mivf_ui_skin.selected_index,0u);mivf_demo_stop(true);hfix58_alert_set("SHOWCASE STOPPED",1);}else if(down&KEY_A){u32 keep=g_mivf_transport_picker.preview_style;mivf_autotest_log("USER_HOLD",keep,(u32)g_mivf_ui_skin.selected_index,0u);mivf_demo_stop(false);g_mivf_transport_picker.preview_style=keep;g_mivf_settings.transport_style=keep;hfix58_alert_set("STYLE HELD",1);}return true;}if(down&(KEY_B|KEY_START)){mivf_transport_picker_cancel();return true;}if(down&KEY_SELECT){mivf_demo_start();return true;}if(down&KEY_A){g_mivf_transport_picker.active=false;g_mivf_settings.transport_style=g_mivf_transport_picker.preview_style;MIVF_SettingsSave(&g_mivf_settings);hfix58_alert_set("TRANSPORT STYLE SAVED",1);return true;}if(down&KEY_X){mivf_transport_picker_preview(MIVF_TRANSPORT_STYLE_CINEMATIC);return true;}if(down&(KEY_DLEFT|KEY_L))mivf_transport_picker_preview((g_mivf_transport_picker.preview_style+MIVF_TRANSPORT_STYLE_COUNT-1u)%MIVF_TRANSPORT_STYLE_COUNT);if(down&(KEY_DRIGHT|KEY_R|KEY_Y))mivf_transport_picker_preview((g_mivf_transport_picker.preview_style+1u)%MIVF_TRANSPORT_STYLE_COUNT);return true;}


static void mivf_c21_draw_dashboard(u8 *fb, u32 style, bool preview);

static void mivf_transport_picker_draw(u8 *fb){
    char text[64];
    u32 st;
    u32 left=0;

    if(!fb||!g_mivf_transport_picker.active)return;
    if(g_mivf_transport_picker.demo_active)mivf_demo_update();

    st=g_mivf_transport_picker.preview_style;
    if(st>=MIVF_TRANSPORT_STYLE_COUNT)st=0;

    mivf_c21_draw_dashboard(
        fb,
        st,
        g_mivf_transport_picker.demo_active?false:true
    );

    hfix58_rect565(fb,0,0,320,16,2,5,11);

    if(g_mivf_transport_picker.demo_active){
        u64 now=svcGetSystemTick();
        u64 elapsed=now-g_mivf_transport_picker.demo_start_tick;
        u64 remaining=
            elapsed<MIVF_DEMO_TOTAL_TICKS
                ? MIVF_DEMO_TOTAL_TICKS-elapsed
                : 0;

        left=(u32)(
            (remaining+(u64)SYSCLOCK_ARM11-1u) /
            (u64)SYSCLOCK_ARM11
        );

        snprintf(
            text,
            sizeof(text),
            "SHOWCASE %02lu/%02u %s",
            (unsigned long)(st+1),
            (unsigned)MIVF_TRANSPORT_STYLE_COUNT,
            mivf_transport_style_name(st)
        );
        hfix58_draw_text_shadow(fb,6,4,text,1,245,248,252);

        snprintf(text,sizeof(text),"%lus",(unsigned long)left);
        hfix58_draw_text_shadow(
            fb,284,4,text,1,
            g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b
        );

        hfix58_rect565(fb,0,222,320,18,2,5,11);
        hfix58_draw_text_shadow(
            fb,8,227,
            "A HOLD STYLE  SELECT/B STOP",
            1,216,228,242
        );
    }else{
        snprintf(
            text,
            sizeof(text),
            "%02lu/%02u %s",
            (unsigned long)(st+1),
            (unsigned)MIVF_TRANSPORT_STYLE_COUNT,
            mivf_transport_style_name(st)
        );
        hfix58_draw_text_shadow(fb,8,4,text,1,245,248,252);
        hfix58_rect565(fb,0,222,320,18,2,5,11);
        hfix58_draw_text_shadow(
            fb,8,227,
            "SELECT DEMO  A APPLY  B CANCEL",
            1,216,228,242
        );
    }
}



/* MIVF_TRANSPORT_C25_TEN_EXPERIENCES_V1 -- draw-only, ten authored worlds. */
/* MIVF_TRANSPORT_C27_DEEP_EXPERIENCES_V1
   Authored motion is presentation-only and derived from a monotonic UI tick.
   No playback clock, PTS, seek, decode, NDSP, scheduling, or media state is
   written by this layer. Expensive motion is bounded to small primitives. */
static bool mivf_c27_style_animated(u32 s){
    /* MIVF_ANIM_GATE_CONSISTENCY_AUDIT_V1: Celestial's own renderer (not the
       shared overlay) reads g_mivf_c27_anim_tick directly for its starfield --
       that drift *is* the style's entire background composition, it's cheap
       (42-element bounded integer loop, same cost class as the other four
       below), and a frozen starfield isn't really "Celestial" at all. Added
       deliberately, not as a default. Industrial Green also read the tick
       directly (its ring-dot phosphor color cycle); that effect was
       secondary/decorative rather than load-bearing, so its time dependence
       was removed from the renderer instead of adding a fifth animated style
       -- see cdev_green(). */
    return s==MIVF_TRANSPORT_STYLE_CLASSIC ||       /* Orbit */
           s==MIVF_TRANSPORT_STYLE_RADIO ||         /* Receiver */
           s==MIVF_TRANSPORT_STYLE_PORTABLE_MONO || /* Game Boy */
           s==MIVF_TRANSPORT_STYLE_BLUE_WAVE ||
           s==MIVF_TRANSPORT_STYLE_CELESTIAL;       /* Starfield drift */
}
static int c27_tri(u32 t,int period,int amplitude){int p=(int)(t%(u32)period),h=period/2;return p<h?(p*amplitude/h):((period-p)*amplitude/h);}
static void c27_badge(u8*f,int x,int y,int w,const char*t,int on,int r,int g,int b){hfix58_rect565(f,x,y,w,13,on?r:18,on?g:24,on?b:34);if(on){hfix58_rect565(f,x,y,w,1,230,240,250);hfix58_rect565(f,x,y+12,w,1,230,240,250);hfix58_rect565(f,x,y,1,13,230,240,250);hfix58_rect565(f,x+w-1,y,1,13,230,240,250);}hfix58_draw_text_shadow(f,x+w/2-(int)strlen(t)*3,y+4,t,1,on?255:145,on?255:161,on?255:180);}
static void c27_ticks(u8*f,int x,int y,int w,int count,int r,int g,int b){for(int i=0;i<count;i++){int xx=x+(i*w)/(count-1);hfix58_rect565(f,xx,y,(i%5)?1:2,(i%5)?4:8,r,g,b);}}

static u32 mivf_c21_style_id(void){u32 x=g_mivf_settings.transport_style;return x<MIVF_TRANSPORT_STYLE_COUNT?x:0u;}
static void ztxt(u8*f,int x,int y,const char*t,int r,int g,int b){hfix58_draw_text_shadow(f,x-(int)strlen(t)*3,y,t,1,r,g,b);}
/* MIVF_ALL_THEME_CIRCLE_AA_V1
   Shared edge-only 2x2 supersampling for every circle drawn through zdisc().
   The solid interior remains span-filled for Old 3DS performance; only the
   one-pixel boundary fringe is coverage blended. This automatically improves
   circular buttons, radial controls, orbit nodes, knobs, and timeline thumbs
   in all original and Device UI Phase 1 themes without touching input maps,
   playback state, decoding, seeking, NDSP, timing, or presentation logic. */
static int mivf_circle_aa_coverage_2x2(int px, int py, int radius) {
    int rr = radius * 4;
    int rr2 = rr * rr;
    int hits = 0;
    const int sample[2] = { 1, 3 };
    for (int sy = 0; sy < 2; sy++) {
        int dy = py * 4 + sample[sy];
        for (int sx = 0; sx < 2; sx++) {
            int dx = px * 4 + sample[sx];
            if (dx * dx + dy * dy <= rr2) hits++;
        }
    }
    return hits;
}
static void mivf_circle_aa_pixel(u8 *f, int x, int y, int px, int py,
                                 int radius, int r, int g, int b) {
    int hits = mivf_circle_aa_coverage_2x2(px, py, radius);
    if (hits <= 0) return;
    hfix58_blend_rect565(f, x + px, y + py, 1, 1, r, g, b,
                         hits >= 4 ? 255 : hits * 64);
}
static void zdisc(u8 *f, int x, int y, int n, int r, int g, int b) {
    if (!f || n <= 0) return;
    int n2 = n * n;
    for (int q = -n - 1; q <= n; q++) {
        int d = 0;
        if (q >= -n && q <= n) {
            d = n;
            while (d > 0 && d * d + q * q > n2) d--;
            if (d > 1) hfix58_rect565(f, x - d + 1, y + q, d * 2 - 1, 1, r, g, b);
        }
        /* Supersample both mathematical edge pixels and the outer fringe.
           Duplicate center candidates are harmless but explicitly avoided. */
        int edge[4] = { -d - 1, -d, d, d + 1 };
        for (int i = 0; i < 4; i++) {
            bool duplicate = false;
            for (int j = 0; j < i; j++) if (edge[j] == edge[i]) duplicate = true;
            if (!duplicate) mivf_circle_aa_pixel(f, x, y, edge[i], q, n, r, g, b);
        }
    }
}
static void zframe(u8*f,int x,int y,int w,int h,int r,int g,int b){hfix58_rect565(f,x,y,w,2,r,g,b);hfix58_rect565(f,x,y+h-2,w,2,r,g,b);hfix58_rect565(f,x,y,2,h,r,g,b);hfix58_rect565(f,x+w-2,y,2,h,r,g,b);}
static int zfill(bool p,int w){u32 c=p?158:hfix58f_current_frame(),t=p?300:hfix58f_total_frames();return t?(int)((u64)c*w/t):0;}
static void ztime(u8*f,int x,int y,int w,bool p,int r,int g,int b){char a[16],z[16];u32 c=p?158:hfix58f_current_frame(),t=p?300:hfix58f_total_frames();hfix59r2_format_time(a,sizeof(a),hfix59r2_frame_to_sec(c));hfix59r2_format_time(z,sizeof(z),hfix59r2_frame_to_sec(t));hfix58_draw_text_shadow(f,x,y,a,1,r,g,b);hfix58_draw_text_shadow(f,x+w-(int)strlen(z)*6,y,z,1,r,g,b);}
static void zline(u8*f,int x,int y,int w,int h,bool p,int r,int g,int b){int v=zfill(p,w);hfix58_rect565(f,x,y,w,h,42,49,61);if(v)hfix58_rect565(f,x,y,v,h,r,g,b);zdisc(f,x+v,y+h/2,h+2,r,g,b);}
static void zicon(u8*f,MivfTransportAction a,int x,int y,int n,int r,int g,int b){mivf_transport_draw_icon(f,a,x,y,n,hfix58_rgb565(r,g,b));}
static void zcontrols(u8*f,bool p,int y,int s,int circles){bool q=p?false:g_media_ctl.state==STATE_PLAYING;int x[3]={66,160,254};for(int i=0;i<3;i++){bool on=i==s;MivfTransportAction a=i==0?MIVF_TRANSPORT_REWIND:(i==1?(q?MIVF_TRANSPORT_PAUSE:MIVF_TRANSPORT_PLAY):MIVF_TRANSPORT_FORWARD);if(circles)zdisc(f,x[i],y,i==1?37:27,on?g_mivf_theme_r:24,on?g_mivf_theme_g:31,on?g_mivf_theme_b:43);zicon(f,a,x[i]-(i==1?15:12),y,i==1?30:24,on?255:170,on?255:188,on?255:210);}}
static int cdev_vol_fill(int w);
static void cdev_meter(u8*f,int x,int y,int count,int active,int r,int g,int b,int max_h);
static void device_button(u8*f,int x,int y,int w,int h,bool on,int r,int g,int b);
/* MIVF_ALL_THEME_VISUAL_POLISH_R4 */
/* MIVF_CUSTOMIZATION_V1 (Phase C vertical slice): three small helpers plus
   one Premiere-only controls-row wrapper. zcontrols() itself (above,
   shared with c25_focus) is NOT modified -- these are new, additive
   functions only c25_premiere calls, so every other style's rendering is
   byte-for-byte unaffected. See mivf_customization_gui_20260716/phase_c/
   for the full design/evidence trail. */
/* clip_r > 0 additionally discards any pixel farther than clip_r from
   (cx,cy) -- used so a rectangular underlay asset (sized to the real touch
   hitbox, which is larger than the visual disc) reads as a round button
   face instead of a square poking out from behind the disc. clip_r <= 0
   means no circular clip (used for the dashboard background's full-rect
   blit path, mivf_cust_blit_bg, which is separate and unaffected). */
static void mivf_cust_blit_asset(u8 *f, int cx, int cy, const MivfCustomAsset *a, int clip_r) {
    int x0, y0, xx, yy, clip_r2;
    if (!f || !a || !a->pixels565) return;
    x0 = cx - a->w / 2;
    y0 = cy - a->h / 2;
    clip_r2 = clip_r > 0 ? clip_r * clip_r : -1;
    for (yy = 0; yy < a->h; yy++) {
        for (xx = 0; xx < a->w; xx++) {
            int px, py, bit_idx;
            if (a->mask1bpp) {
                bit_idx = yy * a->w + xx;
                if (!(a->mask1bpp[bit_idx >> 3] & (0x80 >> (bit_idx & 7)))) continue;
            }
            px = x0 + xx; py = y0 + yy;
            if (clip_r2 >= 0) {
                int dx = px - cx, dy = py - cy;
                if (dx * dx + dy * dy > clip_r2) continue;
            }
            if (px < 0 || px >= 320 || py < 0 || py >= 240) continue;
            hfix58_px565(f, px, py, a->pixels565[yy * a->w + xx]);
        }
    }
}
static void mivf_cust_blit_bg(u8 *f, const MivfCustomAsset *a) {
    int xx, yy;
    if (!f || !a || !a->pixels565) return;
    for (yy = 0; yy < a->h && yy < 240; yy++) {
        for (xx = 0; xx < a->w && xx < 320; xx++) {
            hfix58_px565(f, xx, yy, a->pixels565[yy * a->w + xx]);
        }
    }
}
/* Copy of zcontrols()'s three-control loop with customization hooks for
   i==0 (Rewind), i==1 (Play/Pause), and i==2 (Fast Forward). Preserves
   the production layout, hitboxes, actions, and procedural icons exactly;
   only the visible button face, fill, and outline can change when the
   current manifest resolves an override for that semantic control.

   MIVF_CUSTOMIZATION_V1 (Phase C.1 layering fix): a real underlay asset is
   sized to the control's authoritative touch hitbox (64x60/74x78), which
   is larger than the visual disc (radius 27-37) -- drawing the opaque
   built-in disc fill ON TOP of that image (the original order) hid its
   center and left only the square corners visible, confirmed by the
   user's own screenshot. When a real asset exists, the layer order is now:
   ring (outline color, or the same color the disc fill would have used)
   -> the image itself, circularly clipped to disc_r so it reads as a round
   button face -> icon, unchanged. When no asset exists (color-only
   override, or nothing customized at all), rendering is byte-identical to
   before this fix: a single solid disc fill, exactly as the default,
   non-customized build has always drawn it. */
static void mivf_c25_premiere_controls(u8*f,bool p,int y,int s,int circles){
    bool q=p?false:g_media_ctl.state==STATE_PLAYING;
    int x[3]={66,160,254};
    bool active = mivf_customization_active_for_premiere();
    for(int i=0;i<3;i++){
        bool on=i==s;
        MivfTransportAction a=i==0?MIVF_TRANSPORT_REWIND:(i==1?(q?MIVF_TRANSPORT_PAUSE:MIVF_TRANSPORT_PLAY):MIVF_TRANSPORT_FORWARD);
        MivfCtrlId ctrl = (i==1) ? MIVF_CTRL_PLAY_PAUSE : (i==2 ? MIVF_CTRL_FAST_FORWARD : MIVF_CTRL_REWIND);
        int disc_r = i==1?37:27;
        /* C.6: apply an optional authored offset, clamped here
           authoritatively (belt-and-suspenders with the Toolkit's own
           clamp) so a hand-edited or stale manifest can never place a
           control partly off the real 320x240 dashboard canvas. Falls
           through to the original x[i]/y (dx=dy=0) when no override
           exists -- byte-for-byte the pre-C.6 layout. */
        int px = x[i], py = y;
        if (active && ctrl != MIVF_CTRL_COUNT) {
            int dx = 0, dy = 0;
            if (mivf_customization_resolve_position(ctrl, &dx, &dy)) {
                px = x[i] + dx;
                py = y + dy;
                if (px < disc_r + 3) px = disc_r + 3;
                if (px > 320 - disc_r - 3) px = 320 - disc_r - 3;
                if (py < disc_r + 3) py = disc_r + 3;
                if (py > 240 - disc_r - 3) py = 240 - disc_r - 3;
            }
        }
        int fill_r=on?g_mivf_theme_r:24, fill_g=on?g_mivf_theme_g:31, fill_b=on?g_mivf_theme_b:43;
        bool drew_image = false;
        if (active && ctrl != MIVF_CTRL_COUNT) {
            MivfCtrlVisualState vs = on ? MIVF_CTRL_STATE_FOCUSED : MIVF_CTRL_STATE_IDLE;
            const MivfCustomAsset *asset = mivf_customization_resolve_asset(ctrl, vs);
            MivfCtrlColorOverride co = mivf_customization_resolve_color(ctrl, vs, g_mivf_settings.color_vision_mode);
            if (co.has_fill) { fill_r=co.fill_r; fill_g=co.fill_g; fill_b=co.fill_b; }
            if (asset) {
                int ring_r=fill_r, ring_g=fill_g, ring_b=fill_b;
                if (co.has_outline) { ring_r=co.outline_r; ring_g=co.outline_g; ring_b=co.outline_b; }
                zdisc(f, px, py, disc_r+3, ring_r, ring_g, ring_b);
                mivf_cust_blit_asset(f, px, py, asset, disc_r);
                drew_image = true;
            }
        }
        if(circles && !drew_image)zdisc(f,px,py,disc_r,fill_r,fill_g,fill_b);
        zicon(f,a,px-(i==1?15:12),py,i==1?30:24,on?255:170,on?255:188,on?255:210);
    }
}
static void c25_premiere(u8*f,bool p){
    int s=p?1:g_mivf_ui_skin.selected_index;
    bool q=p?false:g_media_ctl.state==STATE_PLAYING;
    char v[24];
    const MivfCustomAsset *bg = mivf_customization_active_for_premiere() ? mivf_customization_get_dashboard_bg() : NULL;
    if (bg) {
        mivf_cust_blit_bg(f, bg);
    } else {
        hfix58_rect565(f,0,0,320,240,1,3,9);
        for(int y=0;y<145;y+=5)hfix58_rect565(f,0,y,320,5,1,3+y/58,9+y/27);
    }
    hfix58_draw_text_shadow(f,18,17,"NOW PLAYING",1,202,181,125);
    hfix58_draw_text_shadow(f,18,36,p?"PREMIERE":"LES MISERABLES",2,245,248,252);
    hfix58_draw_text_shadow(f,18,62,"CH 04  THE BARRICADE",1,143,162,187);
    mivf_c25_premiere_controls(f,p,132,s,1);
    zline(f,30,181,260,5,p,g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);
    ztime(f,30,194,260,p,164,181,202);
    snprintf(v,sizeof(v),"VOL %d%%",g_hfix56_volume_percent);
    hfix58_draw_text_shadow(f,18,220,g_mivf_settings.show_subtitle_tracks?"SUB ON":"SUB OFF",1,132,153,178);
    ztxt(f,160,220,q?"PLAY":"PAUSE",202,181,125);
    hfix58_draw_text_shadow(f,257,220,v,1,132,153,178);
}
static void c25_orbit(u8*f,bool p){int s=p?1:g_mivf_ui_skin.selected_index;bool q=p?false:g_media_ctl.state==STATE_PLAYING;u32 t=g_mivf_c27_anim_tick;hfix58_rect565(f,0,0,320,240,1,5,13);for(int i=0;i<26;i++){int x=(i*73+17+(int)(t/8))%318,y=(i*47+29)%184;hfix58_rect565(f,x,y,1,1,64,95,130);}ztxt(f,160,12,"ORBIT",160,211,240);ztxt(f,160,26,"RADIAL MEDIA COMMAND",88,134,172);/* MIVF_ORBIT_RING_ROTATION_V1: the three orbital rings previously plotted a
   fixed set of 20 dots per ring with no time term at all -- despite the
   "RADIAL MEDIA COMMAND" framing, they never actually orbited. Each ring now
   advances at its own integer phase rate, inner ring fastest (period ~120
   ticks) to outer ring slowest (period ~240 ticks), like real orbital
   mechanics, using the same tri-wave diamond-path math as before. */
for(int rr=48;rr<=72;rr+=12){int ring_idx=(rr-48)/12,div=6+ring_idx*3,phase=(int)((t/(u32)div)%20u)*4;for(int i=0;i<20;i++){int k=(i*4+phase)%80,xx=160+((k<20?k:(k<40?40-k:(k<60?-(k-40):k-80)))*rr)/20,a=(k+20)%80,yy=108+((a<20?a:(a<40?40-a:(a<60?-(a-40):a-80)))*rr)/20;hfix58_rect565(f,xx,yy,1,1,54,103,145);}}zdisc(f,160,108,39,s==1?43:20,s==1?112:45,s==1?178:70);zicon(f,q?MIVF_TRANSPORT_PAUSE:MIVF_TRANSPORT_PLAY,145,108,30,244,250,255);zdisc(f,67,108,27,s==0?34:12,s==0?94:30,s==0?150:52);zicon(f,MIVF_TRANSPORT_REWIND,55,108,24,211,234,248);zdisc(f,253,108,27,s==2?34:12,s==2?94:30,s==2?150:52);zicon(f,MIVF_TRANSPORT_FORWARD,241,108,24,211,234,248);zline(f,42,184,236,4,p,102,216,246);ztime(f,42,198,236,p,124,161,194);hfix58_draw_text_shadow(f,20,220,g_mivf_settings.show_subtitle_tracks?"SUB ON":"SUB OFF",1,97,137,174);ztxt(f,160,220,q?"PLAY":"PAUSE",157,203,231);hfix58_draw_text_shadow(f,272,220,"FIT",1,97,137,174);}
static void c25_focus(u8*f,bool p){int s=p?1:g_mivf_ui_skin.selected_index;bool q=p?false:g_media_ctl.state==STATE_PLAYING;hfix58_rect565(f,0,0,320,240,0,1,4);for(int y=148;y<240;y++)hfix58_rect565(f,0,y,320,1,(y-148)/46,(y-148)/34,(y-148)/25);hfix58_draw_text_shadow(f,22,28,"FOCUS",1,126,144,166);hfix58_draw_text_shadow(f,22,49,p?"PREVIEW":"LES MISERABLES",2,232,239,246);hfix58_draw_text_shadow(f,22,75,"CHAPTER 04",1,112,130,151);zcontrols(f,p,142,s,0);const char*l=s==0?"BACK 10":(s==1?(q?"PAUSE":"PLAY"):"FORWARD 10");ztxt(f,160,178,l,185,202,220);hfix58_rect565(f,132,189,56,2,g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);zline(f,56,207,208,4,p,g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);ztime(f,56,220,208,p,108,125,143);}
static void c25_arcade(u8*f,bool p){int s=p?1:g_mivf_ui_skin.selected_index;bool q=p?false:g_media_ctl.state==STATE_PLAYING;hfix58_rect565(f,0,0,320,240,18,15,28);for(int y=0;y<240;y+=8)for(int x=0;x<320;x+=8)if(((x+y)/8)&1)hfix58_rect565(f,x,y,8,8,24,20,35);zframe(f,8,8,304,224,244,218,116);hfix58_draw_text_shadow(f,20,18,"MIVF VIDEO SYSTEM",1,250,232,146);hfix58_draw_text_shadow(f,250,18,q?"PLAY":"READY",1,128,232,150);hfix58_draw_text_shadow(f,20,43,"CH 04  THE BARRICADE",1,199,190,140);int x[3]={24,111,221},w[3]={76,98,76};const char*l[3]={"REW",q?"PAUSE":"PLAY","FWD"};for(int i=0;i<3;i++){bool on=i==s;hfix58_rect565(f,x[i]+2,91,w[i],66,12,10,18);hfix58_rect565(f,x[i],88,w[i],66,on?242:58,on?215:52,on?116:70);hfix58_rect565(f,x[i]+4,92,w[i]-8,58,25,22,34);ztxt(f,x[i]+w[i]/2,108,l[i],on?255:218,on?240:198,on?160:132);MivfTransportAction a=i==0?MIVF_TRANSPORT_REWIND:(i==1?(q?MIVF_TRANSPORT_PAUSE:MIVF_TRANSPORT_PLAY):MIVF_TRANSPORT_FORWARD);zicon(f,a,x[i]+w[i]/2-11,135,21,on?255:205,on?240:194,on?160:130);}hfix58_draw_text_shadow(f,22,174,"MEDIA ENERGY",1,250,231,145);for(int i=0;i<24;i++){int on=(i+1)*264/24<=zfill(p,264);hfix58_rect565(f,22+i*11,191,8,9,on?g_mivf_theme_r:44,on?g_mivf_theme_g:36,on?g_mivf_theme_b:56);}ztime(f,22,211,264,p,205,188,122);}
static void c25_navigator(u8*f,bool p){int s=p?1:g_mivf_ui_skin.selected_index;bool q=p?false:g_media_ctl.state==STATE_PLAYING;hfix58_rect565(f,0,0,320,240,7,10,14);hfix58_draw_text_shadow(f,16,14,"PLAYBACK CONTROLS",1,245,248,252);hfix58_draw_text_shadow(f,16,34,s==0?"BACK 10 SECONDS":(s==1?(q?"PAUSE MOVIE":"PLAY MOVIE"):"FORWARD 10 SECONDS"),1,179,200,222);int x[3]={9,109,219},w[3]={92,102,92};for(int i=0;i<3;i++){bool on=i==s;hfix58_rect565(f,x[i]+2,67,w[i],80,18,22,28);hfix58_rect565(f,x[i],64,w[i],80,on?g_mivf_theme_r:39,on?g_mivf_theme_g:46,on?g_mivf_theme_b:57);zframe(f,x[i],64,w[i],80,235,241,248);MivfTransportAction a=i==0?MIVF_TRANSPORT_REWIND:(i==1?(q?MIVF_TRANSPORT_PAUSE:MIVF_TRANSPORT_PLAY):MIVF_TRANSPORT_FORWARD);zicon(f,a,x[i]+(i==1?36:28),99,i==1?30:24,255,255,255);ztxt(f,x[i]+w[i]/2,128,i==0?"BACK":(i==1?(q?"PAUSE":"PLAY"):"FORWARD"),255,255,255);}zline(f,38,175,244,8,p,g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);ztime(f,38,195,244,p,230,237,244);hfix58_draw_text_shadow(f,16,220,"D-PAD MOVE  A SELECT  B CLOSE",1,203,220,235);}
static int cdev_audio_meter_bars(u32 level){
 /* Piecewise thresholds give useful movement for quiet dialogue as well as
    loud music without floating point or log10 on the render path. */
 static const u16 threshold[24]={96,160,240,340,480,660,900,1200,1550,1950,2450,3000,
                                3650,4350,5150,6050,7050,8150,9350,10650,12050,13600,15400,17600};
 int bars=0;while(bars<24 && level>=threshold[bars])bars++;return bars;
}
static void c25_radio(u8*f,bool p){
 int s=p?1:g_mivf_ui_skin.selected_index;bool q=p?false:g_media_ctl.state==STATE_PLAYING;int ml=p?0:cdev_audio_meter_bars(g_audio_meter_left),mr=p?0:cdev_audio_meter_bars(g_audio_meter_right);char line[48];
 hfix58_rect565(f,0,0,320,240,34,28,22);for(int y=0;y<240;y+=5)hfix58_rect565(f,0,y,320,1,49,39,29);zframe(f,7,7,306,226,138,112,74);
 hfix58_rect565(f,17,16,286,22,17,17,15);hfix58_draw_text_shadow(f,25,23,"MIVF A/V RECEIVER",1,235,210,159);hfix58_draw_text_shadow(f,247,23,q?"PLAY":"PAUSE",1,q?111:232,q?222:178,q?132:86);
 hfix58_rect565(f,18,45,284,69,8,11,10);zframe(f,18,45,284,69,74,68,52);hfix58_draw_text_shadow(f,29,53,"INPUT  MIVF     PCM 2.0 / 48 kHz",1,184,197,171);ztime(f,29,68,262,p,244,198,104);snprintf(line,sizeof(line),"CH %02d   SUB %s   SPEED %lu.%02lux",g_mivf_chapters_count?4:0,g_mivf_settings.show_subtitle_tracks?"ON":"OFF",(unsigned long)(mivf_speed_pct()/100u),(unsigned long)(mivf_speed_pct()%100u));hfix58_draw_text_shadow(f,29,94,line,1,119,153,129);
 /* MIVF_RECEIVER_METER_SPACING_FIX_V1: the L/R rows previously used the
    shared 12px-tall cdev_meter pattern at y=132/142, whose bottom edges
    (144/154) measurably overlapped both the volume knob (top edge 144) and
    the transport buttons (top edge 151). Compacted to a 6px-tall pattern at
    y=128/136 (bars now span 128-134 and 136-142) so both rows clear the knob
    and buttons with a 2px margin, and no longer overlap each other. Header
    text nudged up from y=123 to y=117 to keep clear of the input panel above
    while making room. Still 24 segments per channel. */
 hfix58_draw_text_shadow(f,22,117,"MASTER VOLUME",1,207,180,123);snprintf(line,sizeof(line),"%d%%",g_hfix56_volume_percent);hfix58_draw_text_shadow(f,86,117,line,1,246,220,166);hfix58_draw_text_shadow(f,22,131,"L",1,126,194,132);cdev_meter(f,31,128,24,ml,83,210,116,6);hfix58_draw_text_shadow(f,22,140,"R",1,126,194,132);cdev_meter(f,31,136,24,mr,83,210,116,6);
 zdisc(f,69,177,33,105,92,70);zdisc(f,69,177,28,30,31,28);hfix58_draw_text_shadow(f,55,172,"VOL",1,199,181,139);hfix58_draw_text_shadow(f,57,184,line,1,247,224,177);
 int x[3]={145,211,277};for(int i=0;i<3;i++){bool on=i==s;device_button(f,x[i]-27,151,54,52,on,91,74,46);MivfTransportAction a=i==0?MIVF_TRANSPORT_REWIND:(i==1?(q?MIVF_TRANSPORT_PAUSE:MIVF_TRANSPORT_PLAY):MIVF_TRANSPORT_FORWARD);zicon(f,a,x[i]-11,176,i==1?24:20,244,230,198);ztxt(f,x[i],207,i==0?"REW":(i==1?"PLAY":"FWD"),194,174,136);}
 hfix58_draw_text_shadow(f,20,222,"L+UP/DOWN VOLUME   SELECT SETTINGS",1,175,154,119);
}
static void c25_celestial(u8*f,bool p){int s=p?1:g_mivf_ui_skin.selected_index;u32 t=g_mivf_c27_anim_tick;bool q=p?false:g_media_ctl.state==STATE_PLAYING;hfix58_rect565(f,0,0,320,240,3,5,20);for(int i=0;i<42;i++){int x=(i*97+11+(int)(t/8)*(1+i%3))%318,y=(i*53+17)%214;hfix58_rect565(f,x,y,(i+(int)(t/10))%15?1:2,1,126+(i%3)*30,146+(i%2)*36,210);}ztxt(f,160,12,"CELESTIAL",192,205,241);ztxt(f,160,28,"CH 04  THE BARRICADE",115,134,176);zline(f,28,58,264,4,p,166,190,255);zdisc(f,160,124,37,s==1?72:31,s==1?91:43,s==1?166:77);zicon(f,q?MIVF_TRANSPORT_PAUSE:MIVF_TRANSPORT_PLAY,145,124,30,250,242,210);zdisc(f,61,124,25,s==0?54:20,s==0?70:31,s==0?130:68);zicon(f,MIVF_TRANSPORT_REWIND,49,124,24,225,232,255);zdisc(f,259,124,25,s==2?54:20,s==2?70:31,s==2?130:68);zicon(f,MIVF_TRANSPORT_FORWARD,247,124,24,225,232,255);hfix58_draw_text_shadow(f,33,174,"BACK",1,104,124,173);ztxt(f,160,174,q?"PLAY":"PAUSE",181,194,230);hfix58_draw_text_shadow(f,250,174,"NEXT",1,104,124,173);ztime(f,28,207,264,p,140,157,200);ztxt(f,160,224,"MEDIA CONSTELLATION",80,97,145);}
static void c25_paper(u8*f,bool p){int s=p?1:g_mivf_ui_skin.selected_index;bool q=p?false:g_media_ctl.state==STATE_PLAYING;hfix58_rect565(f,0,0,320,240,218,210,192);hfix58_rect565(f,10,8,300,224,243,238,225);hfix58_rect565(f,16,14,288,212,232,226,211);hfix58_draw_text_shadow(f,24,22,"LES MISERABLES",2,39,35,32);hfix58_draw_text_shadow(f,24,48,"CHAPTER FOUR",1,103,85,69);hfix58_draw_text_shadow(f,24,63,"THE BARRICADE",1,71,64,57);hfix58_rect565(f,276,14,20,45,g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);hfix58_draw_text_shadow(f,280,29,"04",1,255,255,255);int x[3]={70,160,250};for(int i=0;i<3;i++){bool on=i==s;MivfTransportAction a=i==0?MIVF_TRANSPORT_REWIND:(i==1?(q?MIVF_TRANSPORT_PAUSE:MIVF_TRANSPORT_PLAY):MIVF_TRANSPORT_FORWARD);if(on){hfix58_rect565(f,x[i]-31,112,62,58,221,212,194);zframe(f,x[i]-31,112,62,58,123,102,82);}zicon(f,a,x[i]-(i==1?14:11),138,i==1?30:23,42,38,34);ztxt(f,x[i],177,i==0?"BACK":(i==1?"PLAY":"NEXT"),87,75,64);}hfix58_rect565(f,26,198,268,3,88,78,67);int v=zfill(p,268);hfix58_rect565(f,26,198,v,3,g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);hfix58_rect565(f,24+v,193,5,13,g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);ztime(f,26,211,268,p,79,70,62);}
static void c25_synthwave(u8*f,bool p){int s=p?1:g_mivf_ui_skin.selected_index;bool q=p?false:g_media_ctl.state==STATE_PLAYING;char v[24];hfix58_rect565(f,0,0,320,240,9,11,14);hfix58_rect565(f,0,0,320,25,217,183,35);hfix58_draw_text_shadow(f,12,8,"PLAYBACK",2,16,18,20);hfix58_draw_text_shadow(f,246,8,q?"PLAY":"PAUSE",1,16,18,20);hfix58_rect565(f,12,35,75,68,25,28,31);zframe(f,12,35,75,68,92,99,105);hfix58_draw_text_shadow(f,23,44,"CHAPTER",1,155,163,170);hfix58_draw_text_shadow(f,27,60,"04",3,240,242,244);hfix58_rect565(f,94,35,214,31,25,28,31);hfix58_draw_text_shadow(f,104,43,"LES MISERABLES",1,218,223,228);hfix58_rect565(f,94,72,214,31,25,28,31);hfix58_draw_text_shadow(f,104,80,"THE BARRICADE",1,218,223,228);zline(f,18,119,284,8,p,242,196,37);ztime(f,18,134,284,p,174,181,188);int x[3]={54,160,266};for(int i=0;i<3;i++){bool on=i==s;hfix58_rect565(f,x[i]-43,151,86,51,on?224:28,on?185:31,on?31:35);MivfTransportAction a=i==0?MIVF_TRANSPORT_REWIND:(i==1?(q?MIVF_TRANSPORT_PAUSE:MIVF_TRANSPORT_PLAY):MIVF_TRANSPORT_FORWARD);zicon(f,a,x[i]-13,176,i==1?28:22,on?15:232,on?18:235,on?22:238);ztxt(f,x[i],193,i==0?"PREV":(i==1?"PLAY":"NEXT"),on?20:170,on?22:176,on?24:182);}snprintf(v,sizeof(v),"VOL %d%%",g_hfix56_volume_percent);hfix58_draw_text_shadow(f,12,220,g_mivf_settings.show_subtitle_tracks?"SUB ON":"SUB OFF",1,177,185,192);ztxt(f,160,220,v,177,185,192);hfix58_draw_text_shadow(f,286,220,"FIT",1,177,185,192);}
static void c25_director(u8*f,bool p){int s=p?1:g_mivf_ui_skin.selected_index;bool q=p?false:g_media_ctl.state==STATE_PLAYING;u32 cf=p?158:hfix58f_current_frame(),fpsn=g_hfix59r2_video_fps_num?g_hfix59r2_video_fps_num:30,fpsd=g_hfix59r2_video_fps_den?g_hfix59r2_video_fps_den:1,sec=hfix59r2_frame_to_sec(cf),fr=(u32)(((u64)cf*fpsd)%fpsn);char tc[32],row[64];hfix58_rect565(f,0,0,320,240,14,17,20);hfix58_draw_text_shadow(f,12,10,"TIMECODE / EDIT SUITE",1,225,231,237);hfix58_draw_text_shadow(f,264,10,q?"PLAY":"PAUSE",1,q?98:236,q?221:182,q?120:72);snprintf(tc,sizeof(tc),"%02lu:%02lu:%02lu:%02lu",(unsigned long)(sec/3600),(unsigned long)((sec/60)%60),(unsigned long)(sec%60),(unsigned long)fr);ztxt(f,160,31,tc,98,235,116);hfix58_rect565(f,12,51,296,57,28,32,36);zframe(f,12,51,296,57,66,73,80);snprintf(row,sizeof(row),"FRAME %lu  FPS %lu/%lu",(unsigned long)cf,(unsigned long)fpsn,(unsigned long)fpsd);hfix58_draw_text_shadow(f,22,62,row,1,188,199,208);snprintf(row,sizeof(row),"AUDIO LOCK  VOL %d%%  SUB %s",g_hfix56_volume_percent,g_mivf_settings.show_subtitle_tracks?"ON":"OFF");hfix58_draw_text_shadow(f,22,80,row,1,112,207,144);zline(f,18,126,284,8,p,245,89,81);ztime(f,18,143,284,p,151,163,174);int x[3]={55,160,265};for(int i=0;i<3;i++){bool on=i==s;hfix58_rect565(f,x[i]-43,164,86,45,on?98:29,on?201:34,on?74:39);MivfTransportAction a=i==0?MIVF_TRANSPORT_REWIND:(i==1?(q?MIVF_TRANSPORT_PAUSE:MIVF_TRANSPORT_PLAY):MIVF_TRANSPORT_FORWARD);zicon(f,a,x[i]-13,185,i==1?27:21,242,246,249);ztxt(f,x[i],202,i==0?"FRAME -":(i==1?"PLAY":"FRAME +"),on?255:165,on?255:176,on?255:186);}hfix58_draw_text_shadow(f,12,220,"B A-B LOOP   L/R CHAPTER   X SPEED",1,145,157,168);}

/* MIVF_ALL_THEME_VISUAL_POLISH_R3
   Hardware-faithful original interpretations, concise typography, live status
   modules, and functional volume/progress readouts. No platform logos/assets. */
static int cdev_vol_fill(int w){int v=g_hfix56_volume_percent;if(v<0)v=0;if(v>100)v=100;return v*w/100;}
static void cdev_meter(u8*f,int x,int y,int count,int active,int r,int g,int b,int max_h){
    /* max_h scales the original 4..12px LED-height pattern proportionally, so
       existing callers that still pass 12 render byte-for-byte identical to
       before; a smaller max_h (e.g. Receiver's compact rows) shrinks the whole
       pattern to fit a tighter vertical budget while keeping all 24 segments. */
    for(int i=0;i<count;i++){
        int h=(4+(i%5)*2)*max_h/12;
        if(h<1)h=1;
        hfix58_rect565(f,x+i*7,y+max_h-h,4,h,i<active?r:24,i<active?g:29,i<active?b:28);
    }
}
static void cdev_dpad(u8*f,int cx,int cy,int arm,bool left_on,bool right_on){int lo=left_on?24:52,ro=right_on?24:52;hfix58_rect565(f,cx-arm,cy-9,arm*2,18,46,48,45);hfix58_rect565(f,cx-9,cy-arm,18,arm*2,46,48,45);hfix58_rect565(f,cx-arm+3,cy-6,arm-10,12,lo,lo,lo);hfix58_rect565(f,cx+7,cy-6,arm-10,12,ro,ro,ro);hfix58_rect565(f,cx-6,cy-arm+3,12,arm*2-6,38,40,38);}

/* MIVF_DEVICE_UI_COLLECTION_PHASE1_RENDERERS_V1 */
static void device_button(u8*f,int x,int y,int w,int h,bool on,int r,int g,int b){hfix58_rect565(f,x+2,y+3,w,h,18,18,18);hfix58_rect565(f,x,y,w,h,on?r:r/2,on?g:g/2,on?b:b/2);zframe(f,x,y,w,h,on?255:120,on?255:120,on?255:120);}
static void cdev_cartridge(u8*f,bool p){int s=p?1:g_mivf_ui_skin.selected_index;bool q=p?false:g_media_ctl.state==STATE_PLAYING;hfix58_rect565(f,0,0,320,240,176,176,164);hfix58_rect565(f,8,7,304,226,205,204,191);hfix58_rect565(f,20,17,280,103,55,57,55);hfix58_rect565(f,33,31,254,72,139,155,99);zframe(f,33,31,254,72,44,53,39);hfix58_draw_text_shadow(f,44,40,"MIVF POCKET MEDIA",1,34,49,31);hfix58_draw_text_shadow(f,44,56,q?"PLAY  CH-04":"PAUSE CH-04",2,34,49,31);zline(f,44,84,232,6,p,42,59,36);cdev_dpad(f,64,173,34,s==0,s==2);zicon(f,MIVF_TRANSPORT_REWIND,29,173,20,230,228,211);zicon(f,MIVF_TRANSPORT_FORWARD,76,173,20,230,228,211);zdisc(f,236,171,30,s==1?172:132,s==1?44:49,s==1?69:75);zicon(f,q?MIVF_TRANSPORT_PAUSE:MIVF_TRANSPORT_PLAY,223,171,26,245,236,218);zdisc(f,285,190,19,124,45,70);hfix58_draw_text_shadow(f,215,211,"PLAY",1,83,63,66);hfix58_draw_text_shadow(f,272,216,"SUB",1,83,63,66);hfix58_draw_text_shadow(f,18,220,"POWER",1,q?186:93,q?49:72,q?42:65);}
static void cdev_mono(u8*f,bool p){int s=p?1:g_mivf_ui_skin.selected_index;bool q=p?false:g_media_ctl.state==STATE_PLAYING;char v[16];hfix58_rect565(f,0,0,320,240,169,173,151);hfix58_rect565(f,10,7,300,226,194,196,174);hfix58_rect565(f,28,19,264,109,66,73,62);hfix58_rect565(f,39,31,242,85,153,169,112);hfix58_draw_text_shadow(f,49,39,"DOT MATRIX MEDIA",1,35,52,32);hfix58_draw_text_shadow(f,49,55,q?"PLAY   CH 04":"PAUSE  CH 04",2,35,52,32);zline(f,49,85,222,7,p,37,57,33);ztime(f,49,101,222,p,35,52,32);cdev_dpad(f,67,177,34,s==0,s==2);zdisc(f,233,168,28,s==1?142:121,s==1?43:48,s==1?76:76);zicon(f,q?MIVF_TRANSPORT_PAUSE:MIVF_TRANSPORT_PLAY,220,168,26,241,232,208);zdisc(f,282,190,19,119,47,76);snprintf(v,sizeof(v),"VOL %d",g_hfix56_volume_percent);hfix58_draw_text_shadow(f,194,216,v,1,68,65,61);for(int i=0;i<6;i++)hfix58_rect565(f,274+i*5,132+i*3,2,16,91,92,80);}
static void cdev_dual(u8*f,bool p){int s=p?1:g_mivf_ui_skin.selected_index;bool q=p?false:g_media_ctl.state==STATE_PLAYING;hfix58_rect565(f,0,0,320,240,208,214,218);hfix58_rect565(f,5,6,310,228,238,241,243);hfix58_rect565(f,15,17,290,113,30,39,48);zframe(f,15,17,290,113,88,164,209);hfix58_draw_text_shadow(f,27,28,"TOUCH MEDIA DECK",1,211,234,247);hfix58_draw_text_shadow(f,27,45,"NOW PLAYING   CHAPTER 04",1,144,190,216);zline(f,27,70,266,10,p,75,180,226);ztime(f,27,89,266,p,179,213,232);hfix58_rect565(f,27,108,80,14,56,78,93);hfix58_rect565(f,120,108,80,14,56,78,93);hfix58_rect565(f,213,108,80,14,56,78,93);ztxt(f,67,111,"SUB",220,237,247);ztxt(f,160,111,"AUDIO",220,237,247);ztxt(f,253,111,"SET",220,237,247);int x[3]={15,119,223};for(int i=0;i<3;i++){bool on=i==s;device_button(f,x[i],151,82,64,on,64,155,207);MivfTransportAction a=i==0?MIVF_TRANSPORT_REWIND:(i==1?(q?MIVF_TRANSPORT_PAUSE:MIVF_TRANSPORT_PLAY):MIVF_TRANSPORT_FORWARD);zicon(f,a,x[i]+28,181,i==1?28:23,255,255,255);ztxt(f,x[i]+41,202,i==0?"BACK":(i==1?"PLAY":"NEXT"),255,255,255);}hfix58_draw_text_shadow(f,18,220,"TOUCHSCREEN WORKSPACE",1,78,109,128);}
static void cdev_green(u8*f,bool p){int s=p?1:g_mivf_ui_skin.selected_index;bool q=p?false:g_media_ctl.state==STATE_PLAYING;hfix58_rect565(f,0,0,320,240,2,7,4);for(int i=0;i<7;i++)zframe(f,5+i*3,5+i*3,310-i*6,230-i*6,5,22+i*5,8);hfix58_draw_text_shadow(f,19,18,"GREEN CORE MEDIA",1,112,242,108);hfix58_draw_text_shadow(f,224,18,q?"ACTIVE":"STANDBY",1,82,190,80);zdisc(f,160,116,58,3,28,8);zdisc(f,160,116,47,7,56,15);/* MIVF_ANIM_GATE_CONSISTENCY_AUDIT_V1: this ring's per-dot brightness used
   to cycle with g_mivf_c27_anim_tick (a "phosphor persistence" color pulse),
   but Industrial Green isn't in the animated-redraw gate, so it would jump to
   an arbitrary phase on every event-driven redraw. Removed the tick term;
   the per-dot brightness gradient (still driven by dot index i, unchanged
   ring geometry/composition) is now a fixed pattern instead of a cycling one. */
for(int i=0;i<18;i++){int rr=53;int xx=160+((i<5?i:(i<10?10-i:(i<14?-(i-10):i-18)))*rr)/5;int yy=116+((((i+5)%18)<5?(i+5)%18:((i+5)%18<10?10-(i+5)%18:((i+5)%18<14?-((i+5)%18-10):(i+5)%18-18)))*rr)/5;hfix58_rect565(f,xx,yy,2,2,42,130+i,46);}zdisc(f,160,116,35,s==1?38:15,s==1?171:77,s==1?42:19);zicon(f,q?MIVF_TRANSPORT_PAUSE:MIVF_TRANSPORT_PLAY,145,116,30,222,255,220);zdisc(f,61,116,28,s==0?28:8,s==0?121:48,s==0?31:12);zicon(f,MIVF_TRANSPORT_REWIND,49,116,24,202,255,202);zdisc(f,259,116,28,s==2?28:8,s==2?121:48,s==2?31:12);zicon(f,MIVF_TRANSPORT_FORWARD,247,116,24,202,255,202);hfix58_draw_text_shadow(f,27,174,"MEDIA ENERGY",1,93,195,93);cdev_meter(f,27,191,38,zfill(p,266)*38/266,67,230,72,12);ztime(f,27,211,266,p,118,222,119);}
static void cdev_tiles(u8*f,bool p){int s=p?1:g_mivf_ui_skin.selected_index;bool q=p?false:g_media_ctl.state==STATE_PLAYING;char v[32];hfix58_rect565(f,0,0,320,240,16,19,21);hfix58_rect565(f,8,9,304,105,31,37,40);hfix58_rect565(f,8,9,7,105,110,218,72);hfix58_draw_text_shadow(f,24,20,"NOW PLAYING",1,139,220,98);hfix58_draw_text_shadow(f,24,39,"LES MISERABLES",2,245,248,250);hfix58_draw_text_shadow(f,24,64,"CHAPTER 04 / THE BARRICADE",1,171,183,190);zline(f,24,90,274,9,p,111,218,72);int x[3]={9,113,217};for(int i=0;i<3;i++){bool on=i==s;hfix58_rect565(f,x[i],124,94,66,on?111:38,on?217:45,on?72:49);MivfTransportAction a=i==0?MIVF_TRANSPORT_REWIND:(i==1?(q?MIVF_TRANSPORT_PAUSE:MIVF_TRANSPORT_PLAY):MIVF_TRANSPORT_FORWARD);zicon(f,a,x[i]+34,153,i==1?29:23,255,255,255);ztxt(f,x[i]+47,175,i==0?"BACK":(i==1?"PLAY":"NEXT"),255,255,255);}snprintf(v,sizeof(v),"VOL %d%%",g_hfix56_volume_percent);const char*l[3]={g_mivf_settings.show_subtitle_tracks?"SUB ON":"SUB OFF",v,"ASPECT FIT"};for(int i=0;i<3;i++){hfix58_rect565(f,9+i*104,199,94,32,45,52,56);ztxt(f,56+i*104,211,l[i],192,204,211);} }
static void cdev_wave(u8*f,bool p){int s=p?1:g_mivf_ui_skin.selected_index;bool q=p?false:g_media_ctl.state==STATE_PLAYING;u32 t=g_mivf_c27_anim_tick;hfix58_rect565(f,0,0,320,240,1,6,26);for(int x=0;x<320;x+=4){int y=53+c27_tri((u32)(x+(int)t),56,12);hfix58_rect565(f,x,y,4,2,23,79+y,178);}hfix58_draw_text_shadow(f,18,17,"BLUE WAVE MEDIA",1,215,234,252);hfix58_draw_text_shadow(f,226,17,q?"PLAY":"PAUSE",1,107,188,255);hfix58_draw_text_shadow(f,25,82,"PLAYBACK   CHAPTER   AUDIO   DISPLAY",1,130,174,220);hfix58_rect565(f,24,96,67,2,102,184,255);zline(f,30,112,260,7,p,91,181,255);ztime(f,30,127,260,p,151,190,229);int x[3]={60,160,260};for(int i=0;i<3;i++){bool on=i==s;zdisc(f,x[i],177,i==1?39:33,on?42:10,on?107:39,on?185:77);MivfTransportAction a=i==0?MIVF_TRANSPORT_REWIND:(i==1?(q?MIVF_TRANSPORT_PAUSE:MIVF_TRANSPORT_PLAY):MIVF_TRANSPORT_FORWARD);zicon(f,a,x[i]-13,177,i==1?28:23,226,242,255);ztxt(f,x[i],214,i==0?"BACK":(i==1?"PLAY":"NEXT"),142,184,226);} }

static void mivf_all_theme_status_footer(u8*f,u32 style,bool preview){char left[48],right[48];bool playing=preview?true:g_media_ctl.state==STATE_PLAYING;u32 pct=mivf_speed_pct();int vol=g_hfix56_volume_percent;if(vol<0)vol=0;if(vol>300)vol=300;snprintf(left,sizeof(left),"%s  SUB %s",playing?"PLAY":"PAUSE",g_mivf_settings.show_subtitle_tracks?"ON":"OFF");snprintf(right,sizeof(right),"VOL %d%%  %lu.%02lux",vol,(unsigned long)(pct/100u),(unsigned long)(pct%100u));hfix58_blend_rect565(f,0,228,320,12,0,0,0,150);hfix58_draw_text_shadow(f,7,231,left,1,224,232,242);hfix58_draw_text_shadow(f,212,231,right,1,224,232,242);(void)style;}
/* MIVF_ALL_THEME_ANIMATIONS_R1
   Bounded presentation-only animation overlays. Updated by the existing 20 Hz
   UI redraw gate; integer math only; no allocation, SD I/O, playback writes,
   seek writes, NDSP access, timing changes, or presentation changes. */
static void mivf_anim_pixel(u8*f,int x,int y,int r,int g,int b){if(x>=0&&x<320&&y>=0&&y<240)hfix58_rect565(f,x,y,2,2,r,g,b);}
static void mivf_all_theme_animation_overlay(u8*f,u32 style,bool preview){
 u32 t=g_mivf_c27_anim_tick;bool playing=preview?true:g_media_ctl.state==STATE_PLAYING;int phase=(int)(t%60u);if(!f)return;
 switch(style){
  /* MIVF_TRANSPORT_ANIMATION_SCOPE_V1: only CLASSIC/RADIO/PORTABLE_MONO/
     BLUE_WAVE receive periodic redraws now (mivf_c27_style_animated). The
     other 12 styles below previously drew a time-based sweep/scanner/pulse
     that either (a) never communicated anything real, or (b) would now
     freeze mid-motion at whatever tick happened to be current on the next
     event-driven redraw. Purely decorative sweeps were removed; the two
     cases whose effect was genuinely state-driven (Director's tally,
     Cartridge Controller's power light) were kept but made static -- solid
     when true, absent when false -- since a blink needs periodic ticks this
     style no longer receives. */
  case MIVF_TRANSPORT_STYLE_CINEMATIC:break;/* removed unrelated horizontal sweep + continuous pulse */
  case MIVF_TRANSPORT_STYLE_CLASSIC:{for(int i=0;i<3;i++){int k=(phase+i*20)%60;int x=160+(k<15?k:(k<30?30-k:(k<45?-(k-30):k-60)))*3;int a=(k+15)%60;int y=108+(a<15?a:(a<30?30-a:(a<45?-(a-30):a-60)))*2;mivf_anim_pixel(f,x,y,92+i*35,182+i*18,235);}break;}
  case MIVF_TRANSPORT_STYLE_MINIMAL:break;/* removed continuous breathing-line decoration; Focus stays genuinely minimal */
  case MIVF_TRANSPORT_STYLE_RETRO:break;/* removed unrelated sweep + corner blink */
  case MIVF_TRANSPORT_STYLE_ACCESSIBLE:{int sel=g_mivf_ui_skin.selected_index,x[3]={9,109,219},w[3]={92,102,92};zframe(f,x[sel]-1,63,w[sel]+2,82,220,220,220);break;}/* static focus outline: was a continuous pulse, now event-driven only per the accessibility brief */
  case MIVF_TRANSPORT_STYLE_RADIO:break;/* Receiver's only motion is its audio-reactive L/R meters (MIVF_RECEIVER_AUDIO_REACTIVE_METERS_V1); no decorative time-based lamp/sweep */
  case MIVF_TRANSPORT_STYLE_CELESTIAL:break;/* removed continuous decorative comet trail */
  case MIVF_TRANSPORT_STYLE_PAPER:break;/* removed drifting ribbon + corner blink; Feature/Paper stays mostly static */
  case MIVF_TRANSPORT_STYLE_SYNTHWAVE:break;/* removed unrelated vertical sweep + corner blink */
  case MIVF_TRANSPORT_STYLE_DIRECTOR:{if(playing)hfix58_rect565(f,293,10,5,5,245,89,81);break;}/* static tally-style playing indicator; removed the unrelated vertical sweep and the blink */
  case MIVF_TRANSPORT_STYLE_CARTRIDGE_CONTROLLER:{if(playing)hfix58_rect565(f,18,218,5,5,194,55,46);break;}/* static power light; removed the unrelated cartridge-slot sweep and the blink */
  case MIVF_TRANSPORT_STYLE_PORTABLE_MONO:{int y=34+(int)(t%72u);hfix58_blend_rect565(f,39,y,242,2,51,69,43,35);if(playing&&((t/18u)&1u))hfix58_rect565(f,19,218,5,5,174,48,44);break;}
  case MIVF_TRANSPORT_STYLE_DUAL_SCREEN_TOUCH:break;/* removed unrelated sweep + always-on ripple glow (brief calls for touch-triggered ripple only; deferred, needs real touch-event wiring) */
  case MIVF_TRANSPORT_STYLE_INDUSTRIAL_GREEN:break;/* removed decorative chasing-blip; cdev_green's own ring-dot phosphor cycle is untouched */
  case MIVF_TRANSPORT_STYLE_FLAT_TILE:break;/* removed the named "sweep" highlight */
  case MIVF_TRANSPORT_STYLE_BLUE_WAVE:{for(int i=0;i<5;i++){int x=(int)((t*(2u+i)+i*67u)%318u),y=45+i*13+c27_tri(t+i*9,48,7);mivf_anim_pixel(f,x,y,70+i*20,145+i*15,230);}break;}
  default:break;
 }
}

static void mivf_c21_draw_dashboard(u8*f,u32 s,bool p){if(!f)return;switch(s){case MIVF_TRANSPORT_STYLE_CLASSIC:c25_orbit(f,p);break;case MIVF_TRANSPORT_STYLE_MINIMAL:c25_focus(f,p);break;case MIVF_TRANSPORT_STYLE_RETRO:c25_arcade(f,p);break;case MIVF_TRANSPORT_STYLE_ACCESSIBLE:c25_navigator(f,p);break;case MIVF_TRANSPORT_STYLE_RADIO:c25_radio(f,p);break;case MIVF_TRANSPORT_STYLE_CELESTIAL:c25_celestial(f,p);break;case MIVF_TRANSPORT_STYLE_PAPER:c25_paper(f,p);break;case MIVF_TRANSPORT_STYLE_SYNTHWAVE:c25_synthwave(f,p);break;case MIVF_TRANSPORT_STYLE_DIRECTOR:c25_director(f,p);break;case MIVF_TRANSPORT_STYLE_CARTRIDGE_CONTROLLER:cdev_cartridge(f,p);break;case MIVF_TRANSPORT_STYLE_PORTABLE_MONO:cdev_mono(f,p);break;case MIVF_TRANSPORT_STYLE_DUAL_SCREEN_TOUCH:cdev_dual(f,p);break;case MIVF_TRANSPORT_STYLE_INDUSTRIAL_GREEN:cdev_green(f,p);break;case MIVF_TRANSPORT_STYLE_FLAT_TILE:cdev_tiles(f,p);break;case MIVF_TRANSPORT_STYLE_BLUE_WAVE:cdev_wave(f,p);break;default:c25_premiere(f,p);break;}mivf_all_theme_animation_overlay(f,s,p);}

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
    if (!fb) return;
    hfix58b_ui_init_once();
    mivf_c21_draw_dashboard(fb, mivf_c21_style_id(), false);
    hfix58j_draw_system_overlay(fb, 0);
    hfix_draw_touch_lock_indicator(fb);
    hfix58_draw_alert(fb);
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

    /* MIVF_TRANSPORT_C26_ACTIVE_EXPERIENCES_V1
       Route normal playback through the selected authored experience.
       Settings and Help retain their established full-screen overlays.
       Draw-only: seek, decoder, NDSP, audio clock, rational timing, frame
       scheduling, presentation, and media-control behavior are untouched. */
    if (!g_hfix59r3_settings_visible && !g_hfix62_help_visible) {
        mivf_c21_draw_dashboard(fb, mivf_c21_style_id(), false);
        hfix58j_draw_system_overlay(fb, 0);
        hfix_draw_touch_lock_indicator(fb);
        hfix58_draw_alert(fb);
        return;
    }

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

    if (g_hfix62_help_visible) {
        hfix62_draw_help_overlay(fb);
    }

    /*
        Volume/status module stays above transport panel.
    */

    if (!g_hfix59r3_settings_visible && !g_hfix62_help_visible) {
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

        /* HFIX64: user-facing playback stats/debug overlay disabled.
           Keep the internal counters and CSV diagnostics alive, but do not draw
           the D/B/P/Ag/Dr/L stats line on the bottom screen during playback. */

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
        HFIX62: also hidden while the help overlay is open, same reason.
    */
    if (!g_hfix59r3_settings_visible && !g_hfix62_help_visible) {
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
        audio_clock_new_generation("audio_shutdown");
    } else {
        /* Even if NDSP is unavailable, invalidate software-side metadata so a
           later session can never resolve an entry from this AudioState. */
        audio_clock_new_generation("audio_shutdown_no_channel");
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

/*
    AVSYNC diagnostics (see task: "Finally fix the remaining MIVF audio/video
    sync issue properly"). Bounded, always-on logging of first-N video shows
    and first-N audio submits, plus a one-shot "which came first and by how
    much" summary at startup and after every real (non-preview) seek. This is
    read-only instrumentation -- it does not change playback behavior. It
    exists so real hardware testing can tell us whether the residual offset
    is a fixed startup/seek latency (constant delta_ms every run) or
    something else, instead of guessing.
*/
#define AVSYNC_LOG_LIMIT 20

static u32  g_avsync_video_log_count = 0;
static u32  g_avsync_audio_log_count = 0;
static bool g_avsync_first_video_seen = false;
static bool g_avsync_first_audio_seen = false;
static u64  g_avsync_first_video_tick = 0;
static u64  g_avsync_first_audio_tick = 0;
static u32  g_avsync_first_video_frame = 0;
static u32  g_avsync_first_audio_frame = 0;

/* Reset at the start of every play() session so a previous file's one-shot
   startup report doesn't leak into the next file's log. */
static void avsync_reset_startup(void) {
    g_avsync_video_log_count = 0;
    g_avsync_audio_log_count = 0;
    g_avsync_first_video_seen = false;
    g_avsync_first_audio_seen = false;
    g_avsync_first_video_tick = 0;
    g_avsync_first_audio_tick = 0;
    g_avsync_first_video_frame = 0;
    g_avsync_first_audio_frame = 0;
}

static void avsync_report_start_if_ready(void) {
    if (!g_avsync_first_video_seen || !g_avsync_first_audio_seen) {
        return;
    }

    u64 lo = g_avsync_first_video_tick < g_avsync_first_audio_tick
        ? g_avsync_first_video_tick : g_avsync_first_audio_tick;
    u64 hi = g_avsync_first_video_tick < g_avsync_first_audio_tick
        ? g_avsync_first_audio_tick : g_avsync_first_video_tick;
    long long delta_ms = (long long)(ticks_to_us(hi - lo) / 1000u);
    if (g_avsync_first_audio_tick < g_avsync_first_video_tick) {
        delta_ms = -delta_ms; /* negative: audio was submitted before video first showed */
    }

#ifdef MIVF_AVSYNC_DIAGNOSTICS
    printf(
        "avsync: start first_audio_submit_frame=%lu tick=%llu first_video_show_frame=%lu tick=%llu delta_ms=%lld\n",
        (unsigned long)g_avsync_first_audio_frame,
        (unsigned long long)g_avsync_first_audio_tick,
        (unsigned long)g_avsync_first_video_frame,
        (unsigned long long)g_avsync_first_video_tick,
        delta_ms);
#else
    (void)delta_ms;
#endif
}

/* Seek-side counterpart: armed by hfix58f_execute_pending_seek (real seeks
   only, not preview/scrub), reported once both sides have fired. */
static bool g_avsync_seek_active = false;
static u32  g_avsync_seek_target = 0;
static u32  g_avsync_seek_seekpoint = 0;
static bool g_avsync_seek_first_video_seen = false;
static bool g_avsync_seek_first_audio_seen = false;
static u32  g_avsync_seek_first_video_frame = 0;
static u32  g_avsync_seek_first_audio_frame = 0;

static void avsync_arm_seek(u32 target, u32 seekpoint_frame) {
    g_avsync_seek_active = true;
    g_avsync_seek_target = target;
    g_avsync_seek_seekpoint = seekpoint_frame;
    g_avsync_seek_first_video_seen = false;
    g_avsync_seek_first_audio_seen = false;
    g_avsync_seek_first_video_frame = 0;
    g_avsync_seek_first_audio_frame = 0;
}

static void avsync_report_seek_if_ready(void) {
    if (!g_avsync_seek_active ||
        !g_avsync_seek_first_video_seen ||
        !g_avsync_seek_first_audio_seen) {
        return;
    }

    long delta_frames = (long)g_avsync_seek_first_audio_frame - (long)g_avsync_seek_first_video_frame;

#ifdef MIVF_AVSYNC_DIAGNOSTICS
    printf(
        "avsync: seek target=%lu seekpoint=%lu first_video=%lu first_audio=%lu delta_frames=%ld\n",
        (unsigned long)g_avsync_seek_target,
        (unsigned long)g_avsync_seek_seekpoint,
        (unsigned long)g_avsync_seek_first_video_frame,
        (unsigned long)g_avsync_seek_first_audio_frame,
        delta_frames);
#else
    (void)delta_frames;
#endif

    g_avsync_seek_active = false;
}

/* Periodic drift diagnostics: reports every ~5s of nominal video time
   (never per-frame -- see HFIX72 above for why hammering printf would
   perturb the very timing being measured) comparing how much real wall
   time has elapsed against how much video-nominal time the shown frame
   count implies. A growing drift_ms means video's presentation clock is
   running ahead of (negative) or behind (positive) real elapsed time;
   audio_submit/audio_drop are the existing NDSP submit/drop counters so a
   queue-starvation cause is visible in the same line. Reset at playback
   start and after every real seek lands on its first displayed frame. */
static u64 g_avsync_drift_base_tick = 0;
static u32 g_avsync_drift_base_frame = 0;
static u32 g_avsync_drift_next_frame = 0;
static bool g_avsync_drift_pending_reset = false;

static u32 g_avsync_audioq_next_frame = 0;

static void avsync_drift_reset(u64 now_tick, u32 frame, u32 fpsn, u32 fpsd) {
    u32 frames_per_report = fpsn ? ((5u * fpsn) / (fpsd ? fpsd : 1u)) : 120u;
    if (frames_per_report == 0) {
        frames_per_report = 1u;
    }

    g_avsync_drift_base_tick = now_tick;
    g_avsync_drift_base_frame = frame;
    g_avsync_drift_next_frame = frame + frames_per_report;
    /* Same schedule, but its own counter -- see avsync_audioq_maybe_report
       for why this can't just read g_avsync_drift_next_frame directly. */
    g_avsync_audioq_next_frame = frame + frames_per_report;
}

static void avsync_drift_maybe_report(u64 now_tick, u32 frame, u32 fpsn, u32 fpsd) {
    u32 frames_per_report;
    u32 frame_delta;
    u64 expected_ms;
    u64 real_ms;
    long long drift_ms;

    if (fpsn == 0 || frame < g_avsync_drift_next_frame) {
        return;
    }

    frame_delta = frame - g_avsync_drift_base_frame;
    expected_ms = ((u64)frame_delta * 1000ull * (fpsd ? fpsd : 1u)) / fpsn;
    real_ms = ticks_to_us(now_tick - g_avsync_drift_base_tick) / 1000u;
    drift_ms = (long long)real_ms - (long long)expected_ms;

#ifdef MIVF_AVSYNC_DIAGNOSTICS
    /* HFIX_AVSYNC_DIAG_GATE_V1: this printf previously ran unconditionally in
       every build, every ~5s of playback, for the entire duration of every
       video -- already flagged as a known "always-on diagnostic overhead"
       risk (see MIVF_PUBLIC_RELEASE_FACTS.md). Gated behind this flag so an
       ordinary build pays nothing for it; the threshold check and re-arm
       below are left completely unconditional and unchanged, since other
       code (the early-return guard above, on the next call) depends on
       g_avsync_drift_next_frame actually advancing regardless of whether
       anything gets printed. */
    printf(
        "avsync: drift video_frame=%lu elapsed_video_ms=%llu real_elapsed_ms=%llu drift_ms=%lld audio_submit=%lu audio_drop=%lu\n",
        (unsigned long)frame,
        (unsigned long long)expected_ms,
        (unsigned long long)real_ms,
        drift_ms,
        (unsigned long)g_audio_submit,
        (unsigned long)g_audio_drop);
#else
    (void)drift_ms;
#endif

    frames_per_report = (5u * fpsn) / (fpsd ? fpsd : 1u);
    if (frames_per_report == 0) {
        frames_per_report = 1u;
    }
    g_avsync_drift_next_frame = frame + frames_per_report;
}

/*
    hfix71: manual A/V sync calibration (Settings -> AUDIO SYNC). Holds each
    decoded audio frame's PCM bytes in a small ring for N frames before it
    reaches audio_queue()/NDSP, so audio that currently plays *ahead* of its
    matching video can be pulled back into line without re-encoding. One
    direction only: this can only delay audio further, never advance it
    (advancing would need decoding video+audio ahead of display, which is a
    much bigger structural change). If audio is instead lagging behind video,
    this setting cannot help -- use the encoder's --audio-offset-ms for a
    permanent, bidirectional fix once the right value is known.

    Bounded to MIVF_SETTINGS' audio_offset_ms clamp (0..3000 ms). Depth is in
    whole audio *frames* (matching one decoded IA4M/PC16 packet each), not raw
    ms, since that's the native unit this pipeline already works in.
*/
#define AUDIO_DELAY_MAX_FRAMES 128
#define AUDIO_DELAY_SLOT_BYTES 8192

typedef struct {
    u8 data[AUDIO_DELAY_SLOT_BYTES];
    u32 size; /* 0 = empty slot (treated as silence) */
} AudioDelaySlot;

static AudioDelaySlot g_audio_delay_ring[AUDIO_DELAY_MAX_FRAMES];
static u32 g_audio_delay_ring_depth = 0;  /* configured depth in frames; 0 = disabled/passthrough */
static u32 g_audio_delay_ring_head = 0;
static u32 g_audio_delay_ring_filled = 0; /* real frames stored since (re)configure, saturates at depth */

static void audio_delay_ring_reconfigure(u32 offset_ms) {
    u32 depth = 0;

    if (offset_ms > 0 && audio.rate > 0 && audio.samples_per_frame > 0) {
        u32 frame_ms = (audio.samples_per_frame * 1000u) / audio.rate;
        if (frame_ms == 0) {
            frame_ms = 1;
        }
        depth = offset_ms / frame_ms;
    }

    if (depth > AUDIO_DELAY_MAX_FRAMES) {
        depth = AUDIO_DELAY_MAX_FRAMES;
    }

    g_audio_delay_ring_depth = depth;
    g_audio_delay_ring_head = 0;
    g_audio_delay_ring_filled = 0;

    for (u32 i = 0; i < AUDIO_DELAY_MAX_FRAMES; i++) {
        g_audio_delay_ring[i].size = 0;
    }

    printf("audio_delay_ring: offset_ms=%lu depth_frames=%lu\n",
        (unsigned long)offset_ms, (unsigned long)depth);
}

static void audio_delay_ring_submit(const u8 *data, u32 size) {
    AudioDelaySlot *slot;

    if (g_audio_delay_ring_depth == 0 || !data || size == 0) {
        audio_queue(data, size);
        return;
    }

    slot = &g_audio_delay_ring[g_audio_delay_ring_head];

    if (g_audio_delay_ring_filled >= g_audio_delay_ring_depth) {
        audio_queue(slot->data, slot->size);
    } else {
        g_audio_delay_ring_filled++;
        /* Ring not full yet since (re)configure/seek -- emit silence instead
           of real audio, i.e. "prepend N ms of silence" spread over frames. */
        static const u8 silence[AUDIO_DELAY_SLOT_BYTES] = {0};
        audio_queue(silence, size > AUDIO_DELAY_SLOT_BYTES ? AUDIO_DELAY_SLOT_BYTES : size);
    }

    if (size > AUDIO_DELAY_SLOT_BYTES) {
        size = AUDIO_DELAY_SLOT_BYTES;
    }
    memcpy(slot->data, data, size);
    slot->size = size;

    g_audio_delay_ring_head = (g_audio_delay_ring_head + 1) % g_audio_delay_ring_depth;
}

static bool hfix58_queue_audio_packet(const Stream *a, const u8 *body, u32 psize, u32 frame_no) {
    /* Remember which frame is being submitted so audio_queue_raw_ndsp can tag
       the resulting wavebuf's sequence_id with it (hfix78). Valid because the
       decode+queue path below is synchronous; assumes audio passthrough
       (audio_offset_ms <= 0), which is the sync-calibration case. */
    g_audio_pending_submit_frame = frame_no;

    if (g_avsync_audio_log_count < AVSYNC_LOG_LIMIT) {
#ifdef MIVF_AVSYNC_DIAGNOSTICS
        printf("avsync: audio_submit frame=%lu tick=%llu\n",
            (unsigned long)frame_no, (unsigned long long)svcGetSystemTick());
#endif
        g_avsync_audio_log_count++;
    }

    if (!g_avsync_first_audio_seen) {
        g_avsync_first_audio_seen = true;
        g_avsync_first_audio_tick = svcGetSystemTick();
        g_avsync_first_audio_frame = frame_no;
        avsync_report_start_if_ready();
    }

    if (g_avsync_seek_active && !g_avsync_seek_first_audio_seen) {
        g_avsync_seek_first_audio_seen = true;
        g_avsync_seek_first_audio_frame = frame_no;
        avsync_report_seek_if_ready();
    }
    if (!a || !audio.ready || !body || psize == 0) {
        return false;
    }

    if (memcmp(a->codec, "IA4M", 4) == 0) {
        static s16 ia4m_pcm[4096];
        int ns = decode_ia4m_packet(body, psize, ia4m_pcm, 4096);

        if (ns <= 0) {
            return false;
        }

        audio_delay_ring_submit((const u8*)ia4m_pcm, (u32)(ns * 2));
        return true;
    }

    audio_delay_ring_submit(body, psize);
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

            /* MIVF_RECEIVER_AUDIO_REACTIVE_METERS_V1
               Analyze the exact final PCM16 data sent to NDSP. A sparse 1-in-4
               sample walk keeps this inexpensive on Old 3DS. Mono is mirrored
               to both meters; stereo is measured independently. Fast attack
               and slower release keep speech/music readable without jitter. */
            {
                const s16 *pcm = (const s16*)audio.buf[i];
                u64 sum_l = 0, sum_r = 0;
                u32 peak_l = 0, peak_r = 0, measured = 0;
                u32 stride = 4u;
                for (u32 frame = 0; frame < nsamples; frame += stride) {
                    int lv = pcm[frame * audio.channels];
                    int rv = audio.channels > 1 ? pcm[frame * audio.channels + 1] : lv;
                    u32 la = (u32)(lv < 0 ? -(s32)lv : lv);
                    u32 ra = (u32)(rv < 0 ? -(s32)rv : rv);
                    /* abs(-32768) is 32768 (safe: -(s32)(-32768) fits s32); clamp
                       to the documented 0..32767 meter range. */
                    if (la > 32767u) la = 32767u;
                    if (ra > 32767u) ra = 32767u;
                    sum_l += la; sum_r += ra;
                    if (la > peak_l) peak_l = la;
                    if (ra > peak_r) peak_r = ra;
                    measured++;
                }
                if (measured) {
                    u32 avg_l = (u32)(sum_l / measured);
                    u32 avg_r = (u32)(sum_r / measured);
                    u32 target_l = (avg_l * 3u + peak_l) / 4u;
                    u32 target_r = (avg_r * 3u + peak_r) / 4u;
                    if (target_l > g_audio_meter_left)
                        g_audio_meter_left = (g_audio_meter_left + target_l * 3u) / 4u;
                    else
                        g_audio_meter_left = (g_audio_meter_left * 7u + target_l) / 8u;
                    if (target_r > g_audio_meter_right)
                        g_audio_meter_right = (g_audio_meter_right + target_r * 3u) / 4u;
                    else
                        g_audio_meter_right = (g_audio_meter_right * 7u + target_r) / 8u;
                }
            }

            /* HFIX86 diag: log the actual sample content of the first few
               submitted buffers, so "healthy queue but no sound" can be told
               apart from "real audio isn't reaching NDSP". If peak/rms are
               non-zero here, real audio is going to the DSP and any remaining
               silence is downstream (output routing / host). If they're zero,
               the problem is upstream (decode/gain). Bounded, then silent. */
            if (g_audio_submit_diag_count < 8) {
                const s16 *sp = (const s16*)audio.buf[i];
                u32 count = n / 2;
                int peak = 0;
                u64 sumsq = 0;
                for (u32 s = 0; s < count; s++) {
                    int a = sp[s] < 0 ? -sp[s] : sp[s];
                    if (a > peak) peak = a;
                    sumsq += (u64)((int)sp[s] * (int)sp[s]);
                }
                u32 rms = count ? (u32)hfix_isqrt64(sumsq / count) : 0;
                printf("hfix86_audio_out: submit#%lu nsamples=%lu bytes=%lu peak=%d rms=%lu mastervol=%d\n",
                    (unsigned long)g_audio_submit_diag_count,
                    (unsigned long)nsamples, (unsigned long)n, peak, (unsigned long)rms,
                    (int)(ndspGetMasterVol() * 1000.0f));
                g_audio_submit_diag_count++;
            }

            audio.wb[i].data_pcm16 = (s16*)audio.buf[i];
            audio.wb[i].nsamples = nsamples;
            audio.wb[i].looping = false;

            DSP_FlushDataCache(audio.buf[i], n);
            ndspChnWaveBufAdd(0, &audio.wb[i]);

            /* HFIX88: sequence_id is assigned by ndspChnWaveBufAdd, so map it
               only after submission. Store the exact ID and reset generation;
               a matching modulo slot alone is never considered authoritative. */
            {
                u16 seq = audio.wb[i].sequence_id;
                u32 map_idx = (u32)seq % AUDIO_SEQ_MAP;
                AudioClockMapEntry *entry = &g_audio_clock_map[map_idx];
                entry->valid = true;
                entry->sequence_id = seq;
                entry->generation = g_audio_clock_generation;
                entry->media_frame = g_audio_pending_submit_frame;
                entry->nsamples = nsamples;
                entry->media_sample_start =
                    audio_clock_frame_to_sample(g_audio_pending_submit_frame);
                g_audio_last_submitted_seq = seq;
                g_audio_have_submitted_seq = true;
            }

            audio.next = (i + 1) % AUDIO_BUFS;

            g_audio_submit++;
            g_last_audio_bytes = n;
            g_last_audio_samples = nsamples;
            if (wait > 0) {
                g_audio_wait_events++;
            }

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
    hfix75: NDSP queue-depth diagnostics. Counts wavebuf slots currently
    QUEUED or PLAYING (i.e. submitted to the DSP but not yet finished) --
    this is the actual number of audio frames sitting ahead of what's
    audible right now, independent of whether submission *rate* or video
    *pacing* look correct. A steady-state depth of D buffers means audio
    that was decoded for video frame N doesn't become audible until roughly
    D * (samples_per_frame / rate) seconds later, which the avsync/drift
    diagnostics (frame-index and pacing-clock based) cannot see at all.
*/
static u32 audio_count_pending_wavebufs(void) {
    u32 pending = 0;

    for (int i = 0; i < AUDIO_BUFS; i++) {
        if (audio.buf[i] &&
            (audio.wb[i].status == NDSP_WBUF_QUEUED || audio.wb[i].status == NDSP_WBUF_PLAYING)) {
            pending++;
        }
    }

    return pending;
}

/*
    hfix76: audio-clock sync controller -- the actual fix for the perceived
    "audio gets later over time" problem.

    Root cause (proven by the avsync_audioq: log): one audio wavebuf is
    submitted per presented video frame, paced by the video presentation
    clock (svcGetSystemTick). NDSP drains wavebufs at the DSP's own audio
    clock. Those two clocks are NOT the same oscillator, and the DSP's is a
    touch slower here (~0.5% measured), so audio is produced slightly faster
    than it's consumed. The surplus accumulates in the wavebuf queue:
    pending_wavebufs climbed 1 -> 12 over ~110s of steady playback in the
    log, and each queued buffer is 41.67ms of latency between "submitted near
    video frame N" and "actually audible". Left alone it keeps growing until
    the 48-buffer pool saturates at ~2s of lag (then starts dropping) -- i.e.
    audio that starts in sync drifts seconds late over a few minutes, exactly
    the reported symptom. drift_ms / delta_frames can't see this because they
    track submission, and submission is correct; the latency is entirely
    downstream of it, in the queue.

    Fix: gently steer the DSP playback rate so consumption matches production,
    holding the queue at a small, bounded depth. When the queue is deeper than
    target we nudge the rate up (drain faster); when shallower we nudge it down
    (let it refill). The correction converges to the real production/consumption
    ratio (~0.5% here) and then sits in a dead zone, so the steady-state pitch
    change is a fixed sub-percent (< ~9 cents) shift -- inaudible, and constant,
    so no wobble. This keeps audio perfectly continuous (no drops -- important
    for a wall-to-wall musical) and leaves video pacing untouched, unlike a
    drop-when-full cap (audible gaps in singing) or loop backpressure (video
    judder). Bounded, principled, no NDSP resets or hard flushes in the steady
    state.
*/
/*
    hfix76b: the first cut of this controller was a bang-bang relay (nudge the
    rate a fixed step whenever pending left a [3,6] band). On real logs that
    limit-cycled -- corr swept 988<->1015 with a ~30-60s period and the queue
    depth swung 1..8 (41..333ms) the whole time. A relay driving an integrator
    (the queue is the integral of production-minus-consumption) always limit-
    cycles; that's structural, not a tuning accident. So the audio latency,
    while now bounded (the real win -- no more unbounded growth), was still a
    wobbling ~187ms average instead of a steady, compensable value.

    Replaced with a proper proportional controller on a low-pass-filtered queue
    depth. P control of a single integrator is well-damped and settles instead
    of oscillating; smoothing the measurement first keeps per-frame queue jitter
    from turning into audible per-tick pitch jitter. It holds the queue at a
    steady setpoint with only a small fixed steady-state offset (the constant
    clock-mismatch bias), so the residual latency becomes a stable number a
    fixed compensation can null -- rather than a moving target. Still: no drops,
    video pacing untouched, sub-percent (inaudible) steady pitch trim.
*/
/* hfix76c: the whole "residual audio lag" this session chased was NOT a
   fixed, unmeasurable downstream/host latency -- it's simply this
   controller's own target queue depth. Proven by a flash+beep sync test:
   sync looked correct at the very start of playback (before the queue had
   filled toward the old setpoint of 3.0 buffers/~125ms) and only drifted
   late as the queue ramped up toward that target over the following ~10-20s
   -- exactly matching every avsync_played log this session, which always
   showed audible_lag_ms=0 immediately after a seek/start and climbing from
   there. A time-varying ramp can never be fixed by a single baked-in
   encoder offset (which is why that approach kept not quite working); the
   only real fix is to lower what this controller is holding the queue at.
   Lowered from 3.0 to 1.5 (~62ms target instead of ~125ms).

   hfix76d: but a proportional-ONLY controller cannot actually PARK the queue
   at that setpoint. The hardware log after the 1.5 change proved it: setpoint
   1.5, yet the queue sat flat at pending=3 (queued 125ms, audible_lag 83ms),
   with corr climbing to ~1.007. That is exactly the classic P-controller
   steady-state error: the constant ~0.5% DSP-vs-CPU clock mismatch is a
   standing disturbance, and to counter it the loop MUST hold corr at ~1.007,
   which a pure P term can only produce by maintaining a standing error of
   0.007/KP = ~1.4 buffers above setpoint. So it parks at 1.5 + 1.4 ~= 3, not
   1.5 -- the target is physically unreachable with P alone.

   The fix is an integral term (P -> PI). The integrator accumulates error and
   absorbs the constant clock-bias load itself, driving steady-state error to
   ZERO -- so the queue settles AT 1.5 (~62ms, ~1 frame of audible lag) rather
   than 1.4 buffers above it. And because the integral term is DC, it carries
   that load WITHOUT adding pitch jitter; the small KP is kept only for fast
   response. This is the textbook reason PI (not higher P) is the right tool
   here: raising KP would shrink the standing error but amplify measurement
   jitter into audible pitch wobble, whereas I nulls the error with no wobble.
   The loop ticks at 1 Hz (once per fps frames), so the gains are sized for
   that slow rate; the integral contribution is clamped (anti-windup) so a
   startup transient or a seek can never wind it into a runaway pitch shift. */
#define AUDIO_SYNC_SETPOINT     1.5f    /* target queued buffers (~62ms at 24fps/48k)      */
#define AUDIO_SYNC_KP           0.005f  /* proportional: rate trim per buffer of error     */
#define AUDIO_SYNC_KI           0.0007f /* integral: per-tick(1Hz) accrual; nulls DC bias  */
#define AUDIO_SYNC_EMA_ALPHA    0.3f    /* measurement low-pass; kills per-frame jitter    */
#define AUDIO_SYNC_MAX_CORR     0.03f   /* clamp total correction to +/-3% (safety rail)   */
#define AUDIO_SYNC_I_CLAMP      0.02f   /* clamp integral term alone to +/-2% (anti-windup)*/

static float g_audio_rate_corr = 1.0f;
static float g_audio_pending_ema = AUDIO_SYNC_SETPOINT;
static float g_audio_rate_integ = 0.0f; /* accumulated integral term, expressed as a corr offset */

static float audio_rate_base(void) {
    return (float)audio.rate * (float)mivf_speed_pct() / 100.0f;
}

/* Re-arm to neutral. Called at playback start and after a seek flush, since
   the wavebuf queue is emptied there and the controller should re-converge
   from a clean slate rather than carry a stale correction. */
static void audio_rate_sync_reset(void) {
    g_audio_rate_corr = 1.0f;
    g_audio_pending_ema = AUDIO_SYNC_SETPOINT;
    g_audio_rate_integ = 0.0f;   /* drop the learned bias: the flushed queue re-converges clean */
    if (audio_can_use_ndsp()) {
        ndspChnSetRate(0, audio_rate_base());
    }
}

static void audio_rate_sync_tick(void) {
    if (!audio_can_use_ndsp() || audio.rate == 0) {
        return;
    }

    u32 pending = audio_count_pending_wavebufs();

    /* Low-pass the queue depth before acting on it, so a one-frame spike
       doesn't yank the playback rate (which would be audible). */
    g_audio_pending_ema += AUDIO_SYNC_EMA_ALPHA * ((float)pending - g_audio_pending_ema);

    float error = g_audio_pending_ema - AUDIO_SYNC_SETPOINT;

    /* Integral: accumulate the error as a corr offset. This is what actually
       parks the queue at the setpoint -- it grows until it supplies the exact
       standing correction the constant clock-mismatch needs, at which point
       the error (and thus further accrual) is zero. Clamp it on its own
       (anti-windup) so a long startup fill or a post-seek transient can't wind
       it past a sane pitch trim and take seconds to unwind. */
    g_audio_rate_integ += AUDIO_SYNC_KI * error;
    if (g_audio_rate_integ > AUDIO_SYNC_I_CLAMP) {
        g_audio_rate_integ = AUDIO_SYNC_I_CLAMP;
    } else if (g_audio_rate_integ < -AUDIO_SYNC_I_CLAMP) {
        g_audio_rate_integ = -AUDIO_SYNC_I_CLAMP;
    }

    /* Proportional term kept small (fast response, low jitter); the integral
       carries the DC load with no jitter. */
    float corr = 1.0f + AUDIO_SYNC_KP * error + g_audio_rate_integ;

    if (corr > 1.0f + AUDIO_SYNC_MAX_CORR) {
        corr = 1.0f + AUDIO_SYNC_MAX_CORR;
    } else if (corr < 1.0f - AUDIO_SYNC_MAX_CORR) {
        corr = 1.0f - AUDIO_SYNC_MAX_CORR;
    }

    g_audio_rate_corr = corr;
    ndspChnSetRate(0, audio_rate_base() * corr);
}

/*
    hfix77: video presentation delay -- the user-facing half of A/V sync.

    Even with the queue bounded and steadied by the controller above, audio
    still becomes audible a fixed amount after its matching video frame is
    shown (the submit-queue floor, ~150ms, plus whatever the emulator/host
    output buffer adds -- unmeasurable from in here). Since we can't pull audio
    any earlier than the queue floor, the fix for a *constant* residual is to
    push VIDEO later by the same amount so they realign.

    The top screen is pure video and the UI lives entirely on the bottom
    screen (see blit565_scaled / the draw_* overlays), so we can delay the top
    framebuffer with zero effect on decode state or UI: after each real frame
    is blitted, snapshot it into a ring and blit back the frame from `depth`
    frames ago. Driven by the negative side of the A/V SYNC setting, tunable
    live by ear. Decode is untouched and still sequential -- only what reaches
    the panel is delayed.
*/
#define VIDEO_DELAY_MAX_MS 600

/* hfix77b: TEMPORARILY DISABLED. Reported blinking/flicker on real hardware
   when active. Root cause not yet isolated -- the likely suspect is the
   interaction between this ring's gfxGetFramebuffer() read/write and
   hfix51c_present_finish()'s gfxSwapBuffers(), which runs once per loop
   iteration regardless of got_video: if that call reads the framebuffer on
   the "wrong side" of a swap relative to when this ring snapshots/restores
   it, the ring could read a half-written back-buffer or restore into a
   buffer about to be presented, producing exactly a blink. That needs to be
   isolated on real hardware before re-enabling, not guessed at further from
   here. Until then, video_delay_set_ms always forces depth to 0 -- the
   negative side of the A/V SYNC setting is inert -- and the real fix for the
   audible-lag question is the playback-clock measurement above (hfix78), not
   this. All the alloc/free/reset/apply plumbing stays in place so re-enabling
   is a one-line change (flip this to 1) once the blink is understood. */
#define VIDEO_DELAY_FEATURE_ENABLED 0

static u8   **g_video_delay_ring = NULL;
static int    g_video_delay_slots = 0;
static size_t g_video_delay_slot_bytes = 0;
static int    g_video_delay_head = 0;
static int    g_video_delay_fill = 0;
static int    g_video_delay_depth = 0;   /* active delay in frames */

static void video_delay_free(void) {
    if (g_video_delay_ring) {
        for (int i = 0; i < g_video_delay_slots; i++) {
            free(g_video_delay_ring[i]);
        }
        free(g_video_delay_ring);
        g_video_delay_ring = NULL;
    }
    g_video_delay_slots = 0;
    g_video_delay_slot_bytes = 0;
    g_video_delay_head = 0;
    g_video_delay_fill = 0;
    g_video_delay_depth = 0;
}

/* Allocate the ring sized for VIDEO_DELAY_MAX_MS at this file's fps. Called
   once per playback. If any allocation fails the feature just stays disabled
   (returns false); playback is unaffected. */
static bool video_delay_alloc(u32 fpsn, u32 fpsd) {
    video_delay_free();

    u32 fps = (fpsn && fpsd) ? (fpsn / fpsd) : 30u;
    if (fps == 0) {
        fps = 30u;
    }

    int max_frames = (int)((VIDEO_DELAY_MAX_MS * fps) / 1000u) + 2;
    if (max_frames < 2) {
        max_frames = 2;
    }

    size_t bytes = (size_t)TOP_W * (size_t)TOP_H * 2u;

    g_video_delay_ring = (u8**)malloc((size_t)max_frames * sizeof(u8*));
    if (!g_video_delay_ring) {
        return false;
    }
    for (int i = 0; i < max_frames; i++) {
        g_video_delay_ring[i] = NULL;
    }
    for (int i = 0; i < max_frames; i++) {
        g_video_delay_ring[i] = (u8*)malloc(bytes);
        if (!g_video_delay_ring[i]) {
            video_delay_free();
            return false;
        }
    }

    g_video_delay_slots = max_frames;
    g_video_delay_slot_bytes = bytes;
    g_video_delay_head = 0;
    g_video_delay_fill = 0;
    g_video_delay_depth = 0;
    return true;
}

/* Drop buffered history so post-seek video never shows pre-seek frames. The
   first `depth` frames after a reset present undelayed while the ring
   re-primes -- a brief, self-correcting transient, not stale content. */
static void video_delay_reset(void) {
    g_video_delay_head = 0;
    g_video_delay_fill = 0;
}

static void video_delay_set_ms(int ms, u32 fpsn, u32 fpsd) {
    int depth;

#if !VIDEO_DELAY_FEATURE_ENABLED
    (void)fpsn;
    (void)fpsd;
    g_video_delay_depth = 0;
    if (ms > 0) {
        printf("video_delay: disabled pending blink investigation (requested ms=%d ignored)\n", ms);
    }
    return;
#endif

    if (ms < 0) {
        ms = 0;
    }
    if (ms > VIDEO_DELAY_MAX_MS) {
        ms = VIDEO_DELAY_MAX_MS;
    }

    u32 fps_n = fpsn ? fpsn : 30u;
    u32 fps_d = fpsd ? fpsd : 1u;
    depth = (int)(((u64)ms * (u64)fps_n) / ((u64)fps_d * 1000u));

    if (g_video_delay_slots > 0 && depth > g_video_delay_slots - 1) {
        depth = g_video_delay_slots - 1;
    }
    if (depth < 0) {
        depth = 0;
    }

    g_video_delay_depth = depth;
    printf("video_delay: ms=%d depth_frames=%d slots=%d\n", ms, depth, g_video_delay_slots);
}

/* Called right after a fresh frame has been blitted to the TOP framebuffer.
   Saves it and swaps in the frame from `depth` presents ago. */
static void video_delay_apply(void) {
    if (g_video_delay_depth <= 0 || !g_video_delay_ring || g_video_delay_slots <= 0) {
        return;
    }

    u16 fw = 0, fh = 0;
    u8 *fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &fw, &fh);
    size_t n = (size_t)fw * (size_t)fh * 2u;

    if (!fb || n == 0 || n > g_video_delay_slot_bytes) {
        return;
    }

    memcpy(g_video_delay_ring[g_video_delay_head], fb, n);

    if (g_video_delay_fill >= g_video_delay_depth) {
        int idx = g_video_delay_head - g_video_delay_depth;
        if (idx < 0) {
            idx += g_video_delay_slots;
        }
        memcpy(fb, g_video_delay_ring[idx], n);
    } else {
        g_video_delay_fill++;
    }

    g_video_delay_head = (g_video_delay_head + 1) % g_video_delay_slots;
}

/* Route the single A/V SYNC setting to the two one-directional mechanisms:
   positive -> hold audio (hfix71 delay ring); negative -> delay video
   (hfix77). Exactly one is ever active; the other is set to zero. Reads the
   current video fps from the globals set up in play(), so callers (including
   the Settings UI) don't need to thread it through. */
static void av_sync_offset_apply(int offset_ms) {
    audio_delay_ring_reconfigure(offset_ms > 0 ? (u32)offset_ms : 0u);
    video_delay_set_ms(offset_ms < 0 ? -offset_ms : 0,
                       g_hfix59r2_video_fps_num, g_hfix59r2_video_fps_den);
}

/*
    HFIX88: trustworthy audio-playback clock.

    Reading seq then sample_pos once is racy: NDSP can advance to the next
    wavebuf between those calls. Read seq/sample/seq and accept only a stable
    sequence. Then require an exact sequence_id and current generation match.
*/
static bool audio_clock_snapshot(AudioClockSnapshot *out) {
    if (!out || !audio_can_use_ndsp() || !g_audio_have_submitted_seq ||
        !ndspChnIsPlaying(0)) {
        return false;
    }

    for (int tries = 0; tries < 4; tries++) {
        u16 seq_before = ndspChnGetWaveBufSeq(0);
        u32 sample_pos = ndspChnGetSamplePos(0);
        u16 seq_after = ndspChnGetWaveBufSeq(0);

        if (seq_before != seq_after) {
            continue;
        }

        u32 map_idx = (u32)seq_before % AUDIO_SEQ_MAP;
        const AudioClockMapEntry *entry = &g_audio_clock_map[map_idx];

        if (!entry->valid ||
            entry->sequence_id != seq_before ||
            entry->generation != g_audio_clock_generation ||
            entry->nsamples == 0) {
            return false;
        }

        /* A stable sequence with a position beyond this buffer is not a valid
           pair for our media-clock purposes. Retry in case NDSP is crossing a
           boundary; never clamp mismatched metadata into a believable value. */
        if (sample_pos > entry->nsamples) {
            continue;
        }

        out->sequence_id = seq_before;
        out->generation = entry->generation;
        out->media_frame = entry->media_frame;
        out->nsamples = entry->nsamples;
        out->sample_pos = sample_pos;
        out->media_sample = entry->media_sample_start + (u64)sample_pos;
        return true;
    }

    return false;
}

/* Compatibility wrapper for existing callers. */
static bool audio_get_playing_frame(u32 *out_frame, u32 *out_sample_pos) {
    AudioClockSnapshot snap;
    if (!audio_clock_snapshot(&snap)) {
        return false;
    }
    if (out_frame) {
        *out_frame = snap.media_frame;
    }
    if (out_sample_pos) {
        *out_sample_pos = snap.sample_pos;
    }
    return true;
}

/* Highest current-generation frame among wavebuf headers marked DONE. */
static u32 audio_last_done_frame(void) {
    u32 last_done = 0;
    bool have = false;

    for (int i = 0; i < AUDIO_BUFS; i++) {
        if (!audio.buf[i] || audio.wb[i].status != NDSP_WBUF_DONE) {
            continue;
        }

        u16 seq = audio.wb[i].sequence_id;
        const AudioClockMapEntry *entry =
            &g_audio_clock_map[(u32)seq % AUDIO_SEQ_MAP];

        if (!entry->valid || entry->sequence_id != seq ||
            entry->generation != g_audio_clock_generation) {
            continue;
        }

        if (!have || entry->media_frame > last_done) {
            last_done = entry->media_frame;
            have = true;
        }
    }

    return have ? last_done : 0;
}

static void avsync_played_report(u32 video_frame) {
    AudioClockSnapshot snap;
    bool have_playing = audio_clock_snapshot(&snap);
    u32 last_done = audio_last_done_frame();
    long audible_lag_frames = have_playing
        ? (long)video_frame - (long)snap.media_frame : 0;
    long long audible_lag_samples = 0;
    long long audible_lag_us = 0;
    long long audible_lag_ms = 0;

    if (have_playing && audio.rate > 0) {
        u64 video_sample = audio_clock_frame_to_sample(video_frame);
        audible_lag_samples = (long long)video_sample -
                              (long long)snap.media_sample;
        audible_lag_us = (audible_lag_samples * 1000000ll) /
                         (long long)audio.rate;
        audible_lag_ms = audible_lag_us / 1000ll;
    }

#ifdef MIVF_AVSYNC_DIAGNOSTICS
    /* See the HFIX_AVSYNC_DIAG_GATE_V1 comment at avsync_drift_maybe_report --
       same rationale. Only the printf is gated; audio_clock_snapshot() and
       audio_last_done_frame() above remain fully unconditional and
       unchanged, since this is a read-only diagnostic query into the
       frozen audio-clock system, not something safe to skip calling. */
    printf(
        "avsync_played: video_frame=%lu last_submitted_audio_frame=%lu last_done_audio_frame=%lu "
        "estimated_playing_audio_frame=%lu sample_pos=%lu playing_known=%d audible_lag_frames=%ld "
        "audible_lag_samples=%lld audible_lag_us=%lld audible_lag_ms=%lld playing_seq=%u "
        "clock_generation=%lu mapped_nsamples=%lu\n",
        (unsigned long)video_frame,
        (unsigned long)g_audio_pending_submit_frame,
        (unsigned long)last_done,
        (unsigned long)(have_playing ? snap.media_frame : 0u),
        (unsigned long)(have_playing ? snap.sample_pos : 0u),
        have_playing ? 1 : 0,
        audible_lag_frames,
        audible_lag_samples,
        audible_lag_us,
        audible_lag_ms,
        (unsigned int)(have_playing ? snap.sequence_id : 0u),
        (unsigned long)(have_playing ? snap.generation : g_audio_clock_generation),
        (unsigned long)(have_playing ? snap.nsamples : 0u));
#else
    (void)last_done; (void)audible_lag_frames; (void)audible_lag_samples;
    (void)audible_lag_us; (void)audible_lag_ms;
#endif
}

static void avsync_audioq_report(u32 video_frame) {
    u32 pending = audio_count_pending_wavebufs();
    u32 free_bufs = (u32)AUDIO_BUFS - pending;
    u32 queued_audio_ms = (audio.rate > 0)
        ? (u32)(((u64)pending * (u64)audio.samples_per_frame * 1000ull) / audio.rate)
        : 0;

    /* rate_corr_ppt = the sync controller's current correction in parts per
       thousand (1000 = neutral). It should settle near the real clock
       mismatch (~1005 for the measured ~0.5%) and then hold, with
       queued_audio_ms bounded instead of climbing. */
    u32 rate_corr_ppt = (u32)(g_audio_rate_corr * 1000.0f + 0.5f);

    /* hfix76d/e: g_perf_late_count tracks hfix59r3_present_video_frame's
       "late" branch (video presentation more than 2 frame durations behind
       schedule), which permanently re-anchors the video pacing clock to
       "now" instead of catching up -- see the HFIX72 comment near the top
       of this file for the mechanism. That branch was only ever suspected
       from a scrubbing-specific stall pattern and has never been checked
       during ordinary continuous playback, because the counter existed but
       was never surfaced in any diagnostic output. Surfacing it here (as a
       cumulative count since session start) turns "could this be causing
       the reported hour-long drift" into something an actual long,
       uninterrupted playback log can confirm or rule out, instead of more
       speculation. */
#ifdef MIVF_AVSYNC_DIAGNOSTICS
    /* See the HFIX_AVSYNC_DIAG_GATE_V1 comment at avsync_drift_maybe_report --
       same rationale. All the values above remain computed unconditionally
       (they're cheap local arithmetic and counter reads, not the expensive
       part -- the printf itself is). */
    printf(
        "avsync_audioq: video_frame=%lu submit_frame=%lu pending_wavebufs=%lu free_wavebufs=%lu "
        "queued_audio_ms=%lu audio_submit=%lu audio_drop=%lu audio_wait_events=%lu rate_corr_ppt=%lu "
        "late_count=%lu\n",
        (unsigned long)video_frame,
        (unsigned long)g_avsync_first_audio_frame,
        (unsigned long)pending,
        (unsigned long)free_bufs,
        (unsigned long)queued_audio_ms,
        (unsigned long)g_audio_submit,
        (unsigned long)g_audio_drop,
        (unsigned long)g_audio_wait_events,
        (unsigned long)rate_corr_ppt,
        (unsigned long)g_perf_late_count);
#else
    (void)pending; (void)free_bufs; (void)queued_audio_ms; (void)rate_corr_ppt;
#endif

    avsync_played_report(video_frame);
}

/* Same ~5s-of-nominal-video-time cadence as avsync_drift_maybe_report, and
   deliberately reusing its frame-count schedule (g_avsync_drift_next_frame)
   rather than a second independent counter, so the two lines land together
   in the log and are trivial to compare side by side. Also fired once
   immediately after a seek lands (see the call in hfix59r3_present_video_frame
   right after avsync_drift_reset). */
static void avsync_audioq_maybe_report(u32 video_frame, bool force) {
    if (!force && video_frame < g_avsync_audioq_next_frame) {
        return;
    }

    avsync_audioq_report(video_frame);

    if (!force) {
        /* Mirrors avsync_drift_maybe_report's own re-arm math so the two
           stay on the same ~5s cadence without sharing (and each
           self-consuming) a single counter. */
        u32 fpsn = g_hfix59r2_video_fps_num ? g_hfix59r2_video_fps_num : 30u;
        u32 fpsd = g_hfix59r2_video_fps_den ? g_hfix59r2_video_fps_den : 1u;
        u32 frames_per_report = (5u * fpsn) / fpsd;
        if (frames_per_report == 0) {
            frames_per_report = 1u;
        }
        g_avsync_audioq_next_frame = video_frame + frames_per_report;
    }
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
        g_hfix56_left_gain_percent == 100 &&
        g_hfix56_right_gain_percent == 100 &&
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
            Mono -> stereo upmix (force_stereo duplicating a mono source to
            two speakers). P.A1: once duplicated, the two outputs are real,
            independent NDSP left/right channels, so left_gain_percent and
            right_gain_percent are applied independently here -- this is
            the one case where a mono *source* still gets real per-channel
            attenuation, because the *output* genuinely is stereo.
        */
        for (u32 i = 0; i < sample_frames; i++) {
            out[i * 2 + 0] = hfix56_apply_gain_channel(in[i], g_hfix56_volume_percent,
                                                        g_hfix56_left_gain_percent, g_hfix56_limiter_enabled);
            out[i * 2 + 1] = hfix56_apply_gain_channel(in[i], g_hfix56_volume_percent,
                                                        g_hfix56_right_gain_percent, g_hfix56_limiter_enabled);
        }
    } else if (src_ch == 2 && out_ch == 2) {
        /*
            True stereo path. P.A1: left_gain_percent/right_gain_percent
            applied independently per channel, combined with master volume
            in one gain calculation (hfix56_gain.h) -- verified equivalent
            to the pre-P.A1 hfix56_apply_gain_one() at 100/100 by
            source/test_hfix56_gain.c's compatibility tests.
        */
        for (u32 i = 0; i < sample_frames; i++) {
            out[i * 2 + 0] = hfix56_apply_gain_channel(in[i * 2 + 0], g_hfix56_volume_percent,
                                                        g_hfix56_left_gain_percent, g_hfix56_limiter_enabled);
            out[i * 2 + 1] = hfix56_apply_gain_channel(in[i * 2 + 1], g_hfix56_volume_percent,
                                                        g_hfix56_right_gain_percent, g_hfix56_limiter_enabled);
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

    /* hfix77: the fresh frame is now in the TOP framebuffer -- delay it by
       swapping in an older one if the A/V SYNC setting asks for video delay.
       No-op (returns immediately) when depth is 0. */
    video_delay_apply();

    u8 *tmp = *prev;
    *prev = *frame;
    *frame = tmp;

    if (have_prev) {
        *have_prev = true;
    }

    u32 shown_frame = *shown;

    (*shown)++;
    hfix58s_subtitles_set_frame_time(*shown, fpsn_abs, fpsd_abs);

    u64 now_tick = svcGetSystemTick();

    if (g_avsync_video_log_count < AVSYNC_LOG_LIMIT) {
#ifdef MIVF_AVSYNC_DIAGNOSTICS
        printf("avsync: video_show frame=%lu tick=%llu\n",
            (unsigned long)shown_frame, (unsigned long long)now_tick);
#endif
        g_avsync_video_log_count++;
    }

    bool avsync_is_first_video_ever = !g_avsync_first_video_seen;

    if (!g_avsync_first_video_seen) {
        g_avsync_first_video_seen = true;
        g_avsync_first_video_tick = now_tick;
        g_avsync_first_video_frame = shown_frame;
        avsync_report_start_if_ready();
    }

    if (g_avsync_seek_active && !g_avsync_seek_first_video_seen) {
        g_avsync_seek_first_video_seen = true;
        g_avsync_seek_first_video_frame = shown_frame;
        avsync_report_seek_if_ready();
    }

    /* Covers every jump -- preview/scrub or real -- not just real seeks, so
       the drift baseline never compares "time since session start" against
       a frame number from after a scrub. See hfix58f_execute_pending_seek. */
    if (g_avsync_drift_pending_reset || avsync_is_first_video_ever) {
        g_avsync_drift_pending_reset = false;
        avsync_drift_reset(now_tick, shown_frame, fpsn_abs, fpsd_abs);
        avsync_audioq_maybe_report(shown_frame, true);
    }

    avsync_drift_maybe_report(now_tick, shown_frame, fpsn_abs, fpsd_abs);
    avsync_audioq_maybe_report(shown_frame, false);

    /* hfix76: run the audio-clock sync controller ~once per second. The modulo
       gate self-aligns across seeks (no per-session counter to reset) and is
       coarse on purpose -- adjusting the DSP rate at most once a second keeps
       the correction gradual and the steady state a fixed sub-percent offset
       rather than a per-frame wobble. */
    if (fpsn_abs > 0 && (shown_frame % fpsn_abs) == 0) {
        audio_rate_sync_tick();
    }

    if (now_tick > *next_frame_tick + frame_ticks_abs * 2) {
        *next_frame_tick = now_tick + frame_ticks_abs;
        g_perf_late_count++;
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

/* HFIX67: set when hfix58f_build_seek_index skipped its up-front metadata
   scan because the file is bigger than HFIX58F_SYNC_INDEX_FAST_LIMIT_BYTES
   (and no embedded footer / .idx cache covered it) -- e.g. a full-length
   movie encode with no seek-index sidecar. In this mode g_hfix58f_seek.count
   is 0 (no real index), so a later seek request falls back to a small,
   bounded on-demand scan (hfix67_approx_seek_scan) instead of failing
   outright. This never re-scans the whole file. */
static bool g_hfix58f_seek_large_file_mode = false;
static u32 g_hfix58f_index_first_offset = 0;



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
    printf("idx: request target=%lu ready=%d count=%lu large=%d total=%lu\n",
        (unsigned long)target_frame, g_hfix58f_seek.ready ? 1 : 0,
        (unsigned long)g_hfix58f_seek.count, g_hfix58f_seek_large_file_mode ? 1 : 0,
        (unsigned long)g_hfix58f_seek.total_frames);
    target_frame = hfix58j_clamp_seek_target(target_frame);

    /* HFIX67: a large file with no real index (g_hfix58f_seek_large_file_mode)
       still gets a pending seek -- hfix58f_execute_pending_seek will try the
       bounded on-demand scan before giving up. Only fail here outright when
       there is truly no index and no way to build one on demand. */
    if ((!g_hfix58f_seek.ready || g_hfix58f_seek.count == 0) && !g_hfix58f_seek_large_file_mode) {
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

/* hfix84: load the ".chapthumbs" sidecar written by
   tools/mivf_build_chapter_thumbs.py. Format (little-endian), 16-byte header:
     bytes 0..3   "MCTH"
     u32          version (only 1 defined)
     u32          count
     u16          thumb_w
     u16          thumb_h
     count * (thumb_w * thumb_h) u16 RGB565LE pixels, one thumbnail per
     chapter, in chapter order.
   Deliberately strict: any mismatch (wrong magic/version/dimensions, count
   that doesn't match the already-loaded .chapters list, short read) just
   leaves g_mivf_chapthumbs_count at 0 and the Scene Selection screen falls
   back to its existing text-only rendering -- a stale or hand-edited
   sidecar must never show the wrong thumbnail next to a chapter. */
static void mivf_menu_load_chapter_thumbs(const char *video_path) {
    char path[HFIX58_MAX_PATH];
    FILE *fp;
    u8 hdr[16];
    u32 version, count;
    u16 w, h;
    size_t need;

    g_mivf_chapthumbs_count = 0;

    if (!video_path || !*video_path || g_mivf_chapters_count <= 0) {
        return;
    }

    hfix60_make_sidecar_path(path, sizeof(path), video_path, ".chapthumbs");
    if (!path[0]) {
        return;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        return;
    }

    if (fread(hdr, 1, sizeof(hdr), fp) != sizeof(hdr) ||
        memcmp(hdr, "MCTH", 4) != 0) {
        fclose(fp);
        return;
    }

    version = le32(hdr + 4);
    count = le32(hdr + 8);
    w = (u16)le16(hdr + 12);
    h = (u16)le16(hdr + 14);

    if (version != 1 ||
        w != MIVF_CHAPTHUMB_W || h != MIVF_CHAPTHUMB_H ||
        count == 0 || count > (u32)MIVF_CHAP_MAX ||
        count != (u32)g_mivf_chapters_count) {
        fclose(fp);
        return;
    }

    need = (size_t)count * (size_t)MIVF_CHAPTHUMB_W * (size_t)MIVF_CHAPTHUMB_H * sizeof(u16);
    if (fread(g_mivf_chapthumbs, 1, need, fp) != need) {
        fclose(fp);
        g_mivf_chapthumbs_count = 0;
        return;
    }

    fclose(fp);
    g_mivf_chapthumbs_count = (int)count;
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

/* ------------------------------------------------------------------------- */
/* HFIX66: DVD-style MIVF Menu Packs -- static MVP runtime.                   */
/*                                                                           */
/* Scope, deliberately: a ".menu.ini" sidecar switches browser selection      */
/* from immediate playback to a small Play/Resume/Chapters/Back screen.      */
/* There is no ISO/VIDEO_TS/IFO parsing here, no DVD VM, no subpicture       */
/* overlays -- just a hand-rolled INI and the existing RGB565 draw helpers.  */
/* An invalid or missing sidecar always falls back to normal playback.       */
/* ------------------------------------------------------------------------- */

static bool mivf_menu_chapters_sidecar_exists(const char *movie_path) {
    char path[HFIX58_MAX_PATH];
    FILE *f;

    hfix60_make_sidecar_path(path, sizeof(path), movie_path, ".chapters");
    if (!path[0]) {
        return false;
    }

    f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    fclose(f);
    return true;
}

/* ------------------------------------------------------------------------- */
/* HFIX74: MIVF Asset Bundle (MASB) -- read-only, Phase A.                   */
/*                                                                           */
/* An append-only footer + directory + payloads written after all real      */
/* movie/index data, so a player (or any other tool) that doesn't know       */
/* about MASB can simply ignore the trailing bytes and still play the       */
/* file normally. This lets a single .mivf eventually carry every sidecar   */
/* (menu.ini, chapters, nfo, menu_bg.cover, idx, ...) internally, without    */
/* ever touching the existing page-based movie data format.                 */
/*                                                                           */
/* Phase A implements the reader plus exactly one consumer: menu_bg.cover.   */
/* Sidecar files still take priority over the embedded copy (see            */
/* mivf_menu_load_background below) -- this keeps the existing dev workflow */
/* of editing a sidecar without repacking the whole .mivf. Other keys       */
/* (menu.ini, chapters, nfo, idx) are intentionally NOT wired to a loader    */
/* yet; that's later phases, once the text-sidecar parsers are adapted to   */
/* read from an in-memory buffer as well as a FILE*.                        */
/*                                                                           */
/* Deliberately NOT implemented in Phase A: CRC32 verification at load time */
/* (the packer tool still writes it, so a later phase can start enforcing   */
/* it without a format version bump) and any compression -- matching how    */
/* small unqualified profile these assets have expects. */
#define MIVF_ASSET_FOOTER_MAGIC   0x4253414Du /* "MASB" little-endian */
#define MIVF_ASSET_FOOTER_VERSION 1u
#define MIVF_ASSET_FOOTER_SIZE    64u
#define MIVF_ASSET_DIR_MAGIC      0x44424D41u /* "MABD" little-endian */
#define MIVF_ASSET_DIR_VERSION    1u
#define MIVF_ASSET_DIR_HEADER_SIZE 16u
#define MIVF_ASSET_ENTRY_SIZE     64u
#define MIVF_ASSET_KEY_MAX        32u
#define MIVF_ASSET_MAX_ENTRIES    64u

typedef struct {
    bool valid;
    u64 bundle_offset;
    u64 bundle_size;
    u32 entry_count;
} MivfAssetBundle;

/* Reads and validates the trailing MASB footer. Does not keep the file
   open -- called at most a couple of times per menu load, never in a
   per-frame path, so the extra open/close is not a concern. */
static bool mivf_assets_probe(const char *movie_path, MivfAssetBundle *bundle) {
    FILE *f;
    long end_pos;
    u8 footer[MIVF_ASSET_FOOTER_SIZE];
    u32 magic, version, entry_count;
    u64 bundle_offset, bundle_size;

    memset(bundle, 0, sizeof(*bundle));

    f = fopen(movie_path, "rb");
    if (!f) {
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    end_pos = ftell(f);
    if (end_pos < (long)MIVF_ASSET_FOOTER_SIZE) {
        fclose(f);
        return false;
    }

    if (fseek(f, end_pos - (long)MIVF_ASSET_FOOTER_SIZE, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }
    if (fread(footer, 1, sizeof(footer), f) != sizeof(footer)) {
        fclose(f);
        return false;
    }
    fclose(f);

    magic = le32(footer + 0);
    version = le32(footer + 4);
    bundle_offset = le64(footer + 8);
    bundle_size = le64(footer + 16);
    entry_count = le32(footer + 24);

    if (magic != MIVF_ASSET_FOOTER_MAGIC || version != MIVF_ASSET_FOOTER_VERSION) {
        return false;
    }
    if (entry_count == 0 || entry_count > MIVF_ASSET_MAX_ENTRIES) {
        return false;
    }
    if (bundle_offset >= (u64)end_pos ||
        bundle_offset + bundle_size + (u64)MIVF_ASSET_FOOTER_SIZE != (u64)end_pos) {
        return false;
    }

    bundle->valid = true;
    bundle->bundle_offset = bundle_offset;
    bundle->bundle_size = bundle_size;
    bundle->entry_count = entry_count;
    return true;
}

/* Looks up a single entry by key in the asset directory. *out_offset is
   absolute (already added to bundle_offset), ready to fseek() to directly. */
static bool mivf_assets_find(const char *movie_path, const MivfAssetBundle *bundle,
                              const char *key, u64 *out_offset, u64 *out_size) {
    FILE *f;
    u8 dir_hdr[MIVF_ASSET_DIR_HEADER_SIZE];
    u32 dmagic, dversion, dcount;
    u32 i;

    if (!bundle->valid) {
        return false;
    }

    f = fopen(movie_path, "rb");
    if (!f) {
        return false;
    }

    if (fseek(f, (long)bundle->bundle_offset, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }
    if (fread(dir_hdr, 1, sizeof(dir_hdr), f) != sizeof(dir_hdr)) {
        fclose(f);
        return false;
    }

    dmagic = le32(dir_hdr + 0);
    dversion = le32(dir_hdr + 4);
    dcount = le32(dir_hdr + 8);

    if (dmagic != MIVF_ASSET_DIR_MAGIC || dversion != MIVF_ASSET_DIR_VERSION || dcount != bundle->entry_count) {
        fclose(f);
        return false;
    }

    for (i = 0; i < dcount; i++) {
        u8 entry[MIVF_ASSET_ENTRY_SIZE];
        char entry_key[MIVF_ASSET_KEY_MAX + 1];
        u64 rel_offset, size;

        if (fread(entry, 1, sizeof(entry), f) != sizeof(entry)) {
            fclose(f);
            return false;
        }

        memcpy(entry_key, entry, MIVF_ASSET_KEY_MAX);
        entry_key[MIVF_ASSET_KEY_MAX] = 0;

        if (strcmp(entry_key, key) != 0) {
            continue;
        }

        rel_offset = le64(entry + 40);
        size = le64(entry + 48);
        fclose(f);

        *out_offset = bundle->bundle_offset + rel_offset;
        *out_size = size;
        return true;
    }

    fclose(f);
    return false;
}

/* Convenience wrapper for a fixed-size binary asset (currently only
   menu_bg.cover): probe, find, and read directly into dst if the stored
   size matches expected_size exactly. */
static bool mivf_assets_load_exact(const char *movie_path, const char *asset_key,
                                    void *dst, size_t expected_size) {
    MivfAssetBundle bundle;
    u64 offset, size;
    FILE *f;
    size_t got;

    if (!mivf_assets_probe(movie_path, &bundle)) {
        return false;
    }
    if (!mivf_assets_find(movie_path, &bundle, asset_key, &offset, &size)) {
        return false;
    }
    if (size != (u64)expected_size) {
        return false;
    }

    f = fopen(movie_path, "rb");
    if (!f) {
        return false;
    }
    if (fseek(f, (long)offset, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }

    got = fread(dst, 1, expected_size, f);
    fclose(f);
    return got == expected_size;
}

static bool mivf_menu_exists_for_movie(const char *movie_path) {
    char path[HFIX58_MAX_PATH];
    FILE *f;

    hfix60_make_sidecar_path(path, sizeof(path), movie_path, ".menu.ini");
    if (!path[0]) {
        return false;
    }

    f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    fclose(f);
    return true;
}

/* HFIX74: sidecar wins over the embedded MASB asset -- lets a background
   still be replaced during development without repacking the whole .mivf.
   Falls back to the embedded "menu_bg.cover" asset if no valid sidecar is
   present; mivf_menu_draw_top's own has_background==false path handles the
   case where neither exists. */
static bool mivf_menu_load_background(const char *movie_path) {
    char path[HFIX58_MAX_PATH];
    FILE *f;
    size_t need = (size_t)MIVF_MENU_BG_W * (size_t)MIVF_MENU_BG_H * 2u;
    size_t got;

    hfix60_make_sidecar_path(path, sizeof(path), movie_path, ".menu_bg.cover");
    if (path[0]) {
        f = fopen(path, "rb");
        if (f) {
            got = fread(g_mivf_menu_bg, 1, need, f);
            fclose(f);
            if (got == need) {
                return true;
            }
        }
    }

    return mivf_assets_load_exact(movie_path, "menu_bg.cover", g_mivf_menu_bg, need);
}

/* hfix85: optional custom bounce image for the idle screensaver -- exact
   raw RGB565 size, no header, same simplicity as ".menu_bg.cover". Absent
   or wrong-sized sidecar just means the default drawn MIVF mark bounces
   instead (see mivf_menu_draw_screensaver); this never blocks the menu. */
static bool mivf_menu_load_screensaver_image(const char *movie_path) {
    char path[HFIX58_MAX_PATH];
    FILE *f;
    size_t need = (size_t)MIVF_SCREENSAVER_W * (size_t)MIVF_SCREENSAVER_H * 2u;
    size_t got;

    hfix60_make_sidecar_path(path, sizeof(path), movie_path, ".screensaver.cover");
    if (!path[0]) {
        return false;
    }

    f = fopen(path, "rb");
    if (!f) {
        return false;
    }

    got = fread(g_mivf_screensaver_img, 1, need, f);
    fclose(f);
    return got == need;
}

static MivfMenuAction mivf_menu_parse_action(const char *s) {
    if (!strcmp(s, "play")) return MIVF_MENU_ACTION_PLAY;
    if (!strcmp(s, "resume")) return MIVF_MENU_ACTION_RESUME;
    if (!strcmp(s, "chapters")) return MIVF_MENU_ACTION_CHAPTERS;
    if (!strcmp(s, "back")) return MIVF_MENU_ACTION_BACK;
    return MIVF_MENU_ACTION_NONE;
}

/* Trims trailing whitespace/CR/LF in place and returns a pointer past any
   leading whitespace -- same idiom as hfix60_chapters_load's line handling. */
static char *mivf_menu_ini_trim(char *s) {
    size_t ln = strlen(s);

    while (ln > 0 && (s[ln - 1] == '\n' || s[ln - 1] == '\r' ||
                       s[ln - 1] == ' '  || s[ln - 1] == '\t')) {
        s[--ln] = 0;
    }

    while (*s == ' ' || *s == '\t') {
        s++;
    }

    return s;
}

/* hfix79: cheap standalone total-frame-count probe for the resume progress
   bar. Reads only the 64-byte header + stream descriptors (read_header/
   read_stream, the same parse play() itself uses) -- no page/packet scan, no
   seek-index build, just enough to get duration + video fps. This is the
   same total-frames formula hfix58f_total_frames() uses during real
   playback, duplicated here (rather than called) because that function is
   defined later in this file and depends on play()-time globals that
   haven't been populated yet at menu-load time. */
static u32 mivf_menu_probe_total_frames(const char *movie_path) {
    FILE *f = fopen(movie_path, "rb");
    Header h;
    u32 fpsn = 0;
    u32 fpsd = 1;

    if (!f) {
        return 0;
    }

    if (read_header(f, &h) != 0) {
        fclose(f);
        return 0;
    }

    for (u32 i = 0; i < h.streams; i++) {
        Stream s;

        if (read_stream(f, &s) != 0) {
            break;
        }
        if (s.type == 1 && fpsn == 0) {
            fpsn = s.fpsn;
            fpsd = s.fpsd ? s.fpsd : 1;
        }
    }

    fclose(f);

    if (h.duration == 0 || fpsn == 0) {
        return 0;
    }

    {
        u64 den = 30000ull * (u64)fpsd;
        u64 frames = (h.duration * (u64)fpsn + den - 1) / den;
        return (frames > 0xffffffffull) ? 0xffffffffu : (u32)frames;
    }
}

/* Only a safe subset is parsed: [MIVF_MENU]'s title=, and [BUTTON id]
   sections' label=/x=/y=/w=/h=/action=. Unrecognized keys (background=,
   style=, up=/down=/etc. from the wider spec) are intentionally ignored for
   this MVP -- background is always the fixed ".menu_bg.cover" sidecar, never
   an arbitrary path from the ini, so there is no path-traversal surface. */
static bool mivf_menu_load_for_movie(const char *movie_path, MivfMenu *menu) {
    char path[HFIX58_MAX_PATH];
    FILE *f;
    char line[192];
    MivfMenuButton *cur_button = NULL;

    memset(menu, 0, sizeof(*menu));
    menu->selected = -1;
    snprintf(menu->movie_path, sizeof(menu->movie_path), "%s", movie_path);

    hfix60_make_sidecar_path(path, sizeof(path), movie_path, ".menu.ini");
    if (!path[0]) {
        return false;
    }

    f = fopen(path, "rb");
    if (!f) {
        return false;
    }

    while (fgets(line, sizeof(line), f)) {
        char *p = mivf_menu_ini_trim(line);
        char *eq;
        char *key;
        char *val;

        if (*p == 0 || *p == '#' || *p == ';') {
            continue;
        }

        if (*p == '[') {
            char *end = strchr(p, ']');
            char *id;

            if (!end) {
                continue;
            }
            *end = 0;
            p++;

            if (!strncmp(p, "BUTTON", 6)) {
                id = p + 6;
                while (*id == ' ') {
                    id++;
                }

                if (menu->button_count < MIVF_MENU_MAX_BUTTONS) {
                    cur_button = &menu->buttons[menu->button_count++];
                    memset(cur_button, 0, sizeof(*cur_button));
                    snprintf(cur_button->id, sizeof(cur_button->id), "%s", id);
                    cur_button->enabled = true;
                } else {
                    cur_button = NULL;
                }
            } else {
                cur_button = NULL;
            }
            continue;
        }

        eq = strchr(p, '=');
        if (!eq) {
            continue;
        }
        *eq = 0;
        key = p;
        val = eq + 1;

        if (cur_button) {
            if (!strcmp(key, "label")) {
                snprintf(cur_button->label, sizeof(cur_button->label), "%s", val);
            } else if (!strcmp(key, "x")) {
                cur_button->x = atoi(val);
            } else if (!strcmp(key, "y")) {
                cur_button->y = atoi(val);
            } else if (!strcmp(key, "w")) {
                cur_button->w = atoi(val);
            } else if (!strcmp(key, "h")) {
                cur_button->h = atoi(val);
            } else if (!strcmp(key, "action")) {
                cur_button->action = mivf_menu_parse_action(val);
            }
        } else {
            if (!strcmp(key, "title")) {
                snprintf(menu->title, sizeof(menu->title), "%s", val);
            }
        }
    }

    fclose(f);

    if (menu->button_count == 0) {
        return false;
    }

    if (menu->title[0] == 0) {
        const char *base = strrchr(movie_path, '/');
        char *dot;

        base = base ? base + 1 : movie_path;
        snprintf(menu->title, sizeof(menu->title), "%.60s", base);
        dot = strrchr(menu->title, '.');
        if (dot) {
            *dot = 0;
        }
    }

    for (int i = 0; i < menu->button_count; i++) {
        MivfMenuButton *b = &menu->buttons[i];

        if (b->action == MIVF_MENU_ACTION_NONE) {
            b->enabled = false;
        }
        if (b->label[0] == 0) {
            snprintf(b->label, sizeof(b->label), "%s", b->id[0] ? b->id : "BUTTON");
        }
        /* HFIX72: the top-screen root menu auto-layouts its button stack
           (mivf_menu_draw_top_root) instead of trusting these coordinates --
           bottom-screen-authored rects collided with the title/divider zone
           once buttons moved to the top screen. x/y/w/h are still parsed
           and clamped here against TOP_W/TOP_H for a possible future
           explicit/custom layout mode, just not read by the current
           top-screen rendering path. */
        if (b->w <= 0) b->w = 120;
        if (b->h <= 0) b->h = 22;
        if (b->w > TOP_W) b->w = TOP_W;
        if (b->h > TOP_H) b->h = TOP_H;
        if (b->x < 0) b->x = 0;
        if (b->y < 0) b->y = 0;
        if (b->x + b->w > TOP_W) b->x = TOP_W - b->w;
        if (b->y + b->h > TOP_H) b->y = TOP_H - b->h;
    }

    /* Resume needs an actual bookmark; Chapters needs the sidecar to exist
       and to actually contain at least one entry. Load chapter labels now
       (dummy 30/1 fps -- only .label is used here; play() reloads this same
       sidecar with the real fps before actually seeking to a chapter). */
    {
        MivfBookmark bookmark;
        bool have_bookmark = MIVF_BookmarkLoad(movie_path, &bookmark) &&
            bookmark.video_path[0] &&
            !strcmp(bookmark.video_path, movie_path) &&
            bookmark.frame > 0;
        bool have_chapters_sidecar = mivf_menu_chapters_sidecar_exists(movie_path);

        if (have_chapters_sidecar) {
            hfix60_chapters_load(movie_path, 30, 1);
            mivf_menu_load_chapter_thumbs(movie_path);
        } else {
            g_mivf_chapters_count = 0;
            g_mivf_chapthumbs_count = 0;
        }

        for (int i = 0; i < menu->button_count; i++) {
            MivfMenuButton *b = &menu->buttons[i];

            if (b->action == MIVF_MENU_ACTION_RESUME && !have_bookmark) {
                b->enabled = false;
            }
            if (b->action == MIVF_MENU_ACTION_CHAPTERS && g_mivf_chapters_count <= 0) {
                b->enabled = false;
            }
        }

        /* hfix79: resume progress bar data. Only bother probing total_frames
           (a real file open + header/stream parse) when there's actually a
           bookmark to show progress for. */
        if (have_bookmark) {
            u32 total = mivf_menu_probe_total_frames(movie_path);

            if (total > 1 && bookmark.frame < total) {
                menu->resume_bookmark_frame = bookmark.frame;
                menu->resume_total_frames = total;
                menu->has_resume_progress = true;
            }
        }
    }

    menu->has_background = mivf_menu_load_background(movie_path);
    menu->has_screensaver_image = mivf_menu_load_screensaver_image(movie_path);
    menu->valid = true;
    return true;
}

/* MIVF_PHASE7_CINEMATIC_GLASS_V1
   Shared, integer-only visual primitives for the DVD-menu presentation.
   These helpers wrap the existing RGB565 fill/blend/text functions; no new
   framebuffer, image decoder, floating-point animation, or navigation state. */
static int mivf_ui_clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int mivf_ui_tri(u32 tick, u32 period, int amplitude) {
    u32 half;
    u32 phase;
    if (period < 2u || amplitude <= 0) return 0;
    half = period / 2u;
    phase = tick % period;
    if (phase > half) phase = period - phase;
    return (int)(((u64)phase * (u64)amplitude) / (u64)half);
}

static void mivf_ui_top_vignette(u8 *fb) {
    /* Eight inexpensive inset rings: strongest at the edge, transparent in
       the content region. This reads as a cinematic lens vignette without a
       400x240 per-pixel blend pass. */
    for (int i = 0; i < 8; i++) {
        int a = 42 - i * 4;
        hfix58s_top_blend_rect565(fb, i, i, TOP_W - i * 2, 1, 0, 0, 0, a);
        hfix58s_top_blend_rect565(fb, i, TOP_H - 1 - i, TOP_W - i * 2, 1, 0, 0, 0, a);
        hfix58s_top_blend_rect565(fb, i, i, 1, TOP_H - i * 2, 0, 0, 0, a);
        hfix58s_top_blend_rect565(fb, TOP_W - 1 - i, i, 1, TOP_H - i * 2, 0, 0, 0, a);
    }
}

static void mivf_ui_top_glass(u8 *fb, int x, int y, int w, int h, int alpha) {
    hfix58s_top_blend_rect565(fb, x + 2, y + 3, w, h, 0, 0, 0, 90);
    hfix58s_top_blend_rect565(fb, x, y, w, h, 5, 10, 20, alpha);
    hfix58s_top_rect565(fb, x, y, w, 1, 80, 96, 120);
    hfix58s_top_rect565(fb, x, y + h - 1, w, 1, 18, 26, 40);
    hfix58s_top_rect565(fb, x, y, 1, h, 54, 66, 84);
    hfix58s_top_rect565(fb, x + w - 1, y, 1, h, 16, 24, 38);
    hfix58s_top_blend_rect565(fb, x + 2, y + 2, w - 4, 5, 110, 135, 170, 18);
}

static void mivf_ui_bottom_glass(u8 *fb, int x, int y, int w, int h) {
    hfix58_blend_rect565(fb, x + 2, y + 3, w, h, 0, 0, 0, 100);
    hfix58_blend_rect565(fb, x, y, w, h, 10, 18, 32, 230);
    hfix58_rect565(fb, x, y, w, 1, 72, 90, 116);
    hfix58_rect565(fb, x, y + h - 1, w, 1, 18, 26, 42);
    hfix58_rect565(fb, x, y, 1, h, 52, 66, 88);
    hfix58_rect565(fb, x + w - 1, y, 1, h, 18, 28, 44);
}

static void mivf_ui_bottom_badge(u8 *fb, int x, int y, const char *text,
                                 int r, int g, int b) {
    int w = (int)strlen(text) * 6 + 10;
    hfix58_blend_rect565(fb, x, y, w, 15, r, g, b, 100);
    hfix58_rect565(fb, x, y, w, 1, r, g, b);
    hfix58_draw_text_shadow(fb, x + 5, y + 4, text, 1, 226, 238, 250);
}

static void mivf_ui_bottom_footer(u8 *fb, const char *left, const char *right) {
    int rw = right ? (int)strlen(right) * 6 : 0;
    hfix58_rect565(fb, 0, 215, 320, 25, 3, 7, 14);
    hfix58_rect565(fb, 0, 215, 320, 1, 34, 48, 68);
    if (left) hfix58_draw_text_shadow(fb, 14, 225, left, 1, 170, 190, 214);
    if (right) hfix58_draw_text_shadow(fb, 306 - rw, 225, right, 1, 170, 190, 214);
}

static void mivf_ui_format_frame_time(char *out, size_t out_sz, u32 frame,
                                      u32 total_frames) {
    u32 sec = 0;
    u32 min;
    if (!out || out_sz == 0) return;
    if (total_frames > 0) {
        /* The menu does not open the video clock here; this is intentionally
           a progress percentage helper, not a fabricated wall-clock time. */
        u32 pct = (u32)(((u64)frame * 100ull) / (u64)total_frames);
        if (pct > 100u) pct = 100u;
        /* MIVF_PHASE7_1_SUPPORTED_PROGRESS_TEXT_V1 */
        snprintf(out, out_sz, "WATCHED %lu PCT", (unsigned long)pct);
        return;
    }
    min = sec / 60u;
    snprintf(out, out_sz, "%lu:%02lu", (unsigned long)min,
             (unsigned long)(sec % 60u));
}

/* HFIX70: small diamond flourish flanking centered titles -- a cheap,
   recognizable "cinematic menu" accent that doesn't need new font glyphs. */
static void mivf_menu_top_diamond(u8 *fb, int cx, int cy, int r, int g, int b) {
    for (int i = 0; i <= 2; i++) {
        int half = 2 - i;
        hfix58s_top_rect565(fb, cx - half, cy - 2 + i, half * 2 + 1, 1, r, g, b);
        if (i > 0) {
            hfix58s_top_rect565(fb, cx - half, cy + 2 - i, half * 2 + 1, 1, r, g, b);
        }
    }
}

/* HFIX71: background/title only -- used as-is while browsing Scene
   Selection (which stays bottom-screen-primary), and as the base layer
   for mivf_menu_draw_top_root below. */
/* HFIX72: fixed title-zone heights for each background mode, so the button
   auto-layout below always has a guaranteed collision-free start position
   regardless of title length or button count. The old generated-fallback
   layout centered its title at TOP_H/2, which is exactly where the button
   stack needs to live -- that was the root cause of the title/button
   overlap. Both modes now confine title+divider to a fixed strip near the
   top instead. */
#define MIVF_MENU_TOP_CONTENT_Y_BG 44
#define MIVF_MENU_TOP_CONTENT_Y_FALLBACK 54

/* hfix82: slow "Ken Burns" zoom+pan loop over the existing still background
   -- no new decode pipeline, no higher-res source art needed. Zooms in by
   shrinking the sampled window (integer math) and drifts the window's
   center within the margin the zoom creates, easing via triangle waves
   (same idiom already used for the button-select pulse glow elsewhere in
   this file) rather than pulling in sin/cos for a purely cosmetic effect.
   Zoom and pan use deliberately different periods so the motion doesn't
   read as a mechanical loop. */

/* hfix82 / MIVF_PHASE8_1_KEN_BURNS_SMOOTH_V1
   EASED / FIXED-POINT KEN BURNS: longer cycles and smoothstep easing reduce
   visible whole-pixel stepping. Still one top-screen RGB565 pass only, still
   integer-only, still no new images or playback-side state. */
#undef MIVF_MENU_BG_ZOOM_PERIOD
#undef MIVF_MENU_BG_PAN_PERIOD
#undef MIVF_MENU_BG_ZOOM_PCT
#define MIVF_MENU_BG_ZOOM_PERIOD 720  /* slower, cinematic in/out cycle */
#define MIVF_MENU_BG_PAN_PERIOD  960  /* different long pan cycle */
#define MIVF_MENU_BG_ZOOM_PCT    7    /* smaller zoom preserves sharpness */
static u32 mivf_menu_smoothstep_q16(u32 phase, u32 period) {
    u32 half;
    u32 t;
    u64 tt;
    if (period < 2u) return 0;
    half = period / 2u;
    phase %= period;
    if (phase > half) phase = period - phase;
    t = half ? (u32)(((u64)phase << 16) / (u64)half) : 0;
    tt = ((u64)t * (u64)t) >> 16;
    return (u32)((tt * (3ull * 65536ull - 2ull * (u64)t)) >> 16);
}

static void mivf_menu_draw_background_animated(u8 *fb, u32 pulse) {
    u32 zoom_ease = mivf_menu_smoothstep_q16(pulse, MIVF_MENU_BG_ZOOM_PERIOD);
    u32 pan_ease_x = mivf_menu_smoothstep_q16(pulse + MIVF_MENU_BG_PAN_PERIOD / 5u,
        MIVF_MENU_BG_PAN_PERIOD);
    u32 pan_ease_y = mivf_menu_smoothstep_q16(pulse + MIVF_MENU_BG_PAN_PERIOD / 2u,
        MIVF_MENU_BG_PAN_PERIOD);
    int max_margin_x = MIVF_MENU_BG_W * MIVF_MENU_BG_ZOOM_PCT / 100;
    int max_margin_y = MIVF_MENU_BG_H * MIVF_MENU_BG_ZOOM_PCT / 100;
    int margin_x = (int)(((u64)max_margin_x * (u64)zoom_ease) >> 16);
    int margin_y = (int)(((u64)max_margin_y * (u64)zoom_ease) >> 16);
    int pan_x = margin_x ? (int)(((s64)margin_x * ((s64)pan_ease_x - 32768)) / 32768) : 0;
    int pan_y = margin_y ? (int)(((s64)margin_y * ((s64)pan_ease_y - 32768)) / 32768) : 0;
    int src_x0 = margin_x + pan_x;
    int src_y0 = margin_y + pan_y;
    int src_w = MIVF_MENU_BG_W - 2 * margin_x;
    int src_h = MIVF_MENU_BG_H - 2 * margin_y;
    if (src_w <= 0 || src_w > MIVF_MENU_BG_W || src_h <= 0 || src_h > MIVF_MENU_BG_H) {
        src_x0 = 0; src_y0 = 0;
        src_w = MIVF_MENU_BG_W; src_h = MIVF_MENU_BG_H;
    }
    if (src_x0 < 0) src_x0 = 0;
    if (src_y0 < 0) src_y0 = 0;
    if (src_x0 + src_w > MIVF_MENU_BG_W) src_x0 = MIVF_MENU_BG_W - src_w;
    if (src_y0 + src_h > MIVF_MENU_BG_H) src_y0 = MIVF_MENU_BG_H - src_h;

    u32 step_x = ((u32)src_w << 16) / (u32)MIVF_MENU_BG_W;
    u32 step_y = ((u32)src_h << 16) / (u32)MIVF_MENU_BG_H;
    u32 sy_acc = (u32)src_y0 << 16;
    for (int yy = 0; yy < MIVF_MENU_BG_H; yy++) {
        int sy = (int)(sy_acc >> 16);
        const u16 *row = &g_mivf_menu_bg[(size_t)sy * MIVF_MENU_BG_W];
        u32 sx_acc = (u32)src_x0 << 16;
        for (int xx = 0; xx < MIVF_MENU_BG_W; xx++) {
            hfix58s_top_px565(fb, xx, yy, row[sx_acc >> 16]);
            sx_acc += step_x;
        }
        sy_acc += step_y;
    }
}
static void mivf_menu_draw_top(u8 *fb, const MivfMenu *menu, u32 pulse) {
    /* MIVF_PHASE7_2_TITLE_COMPOSITION_V1 */
    if (menu->has_background) {
        mivf_menu_draw_background_animated(fb, pulse);
        for (int i=0;i<8;i++) {
            hfix58s_top_blend_rect565(fb,0,i*4,TOP_W,4,2,6,14,62-i*5);
            hfix58s_top_blend_rect565(fb,0,TOP_H-1-i*3,TOP_W,3,2,6,14,42-i*4);
        }
    } else {
        for (int yy=0;yy<TOP_H;yy++) {
            int shade=5+(yy*24)/TOP_H;
            hfix58s_top_rect565(fb,0,yy,TOP_W,1,shade/4,shade/2,shade+8);
        }
        for (int i=0;i<6;i++)
            hfix58s_top_blend_rect565(fb,56+i*8,0,TOP_W-112-i*16,TOP_H,
                g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b,5);
    }
    mivf_ui_top_vignette(fb);

    /* Compact two-line title card: title and disc identity are now one
       composition rather than unrelated left/right labels. */
    mivf_ui_top_glass(fb,12,8,TOP_W-24,34,130);
    hfix58s_top_draw_text_shadow(fb,25,15,menu->title,1,242,248,255);
    hfix58s_top_draw_text_shadow(fb,25,29,"MIVF DISC EXPERIENCE",1,122,146,176);
    hfix58s_top_rect565(fb,20,41,88,2,
        g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);
    mivf_menu_top_diamond(fb,TOP_W-31,25,
        g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);
}

/* MIVF_CUSTOMIZATION_V1 (Phase C.1): masked blit for the top-screen root
   DVD-menu, mirroring mivf_cust_blit_asset() (the bottom-screen dashboard
   variant, above near c25_premiere) but writing through hfix58s_top_px565
   instead of hfix58_px565 -- the two framebuffers are addressed by separate
   primitives throughout main.c, so this is a real, necessary duplication of
   the blit loop, not a redundant one. */
static void mivf_cust_blit_asset_top(u8 *f, int cx, int cy, const MivfCustomAsset *a) {
    int x0, y0, xx, yy;
    if (!f || !a || !a->pixels565) return;
    x0 = cx - a->w / 2;
    y0 = cy - a->h / 2;
    for (yy = 0; yy < a->h; yy++) {
        for (xx = 0; xx < a->w; xx++) {
            int px, py, bit_idx;
            if (a->mask1bpp) {
                bit_idx = yy * a->w + xx;
                if (!(a->mask1bpp[bit_idx >> 3] & (0x80 >> (bit_idx & 7)))) continue;
            }
            px = x0 + xx; py = y0 + yy;
            if (px < 0 || px >= TOP_W || py < 0 || py >= TOP_H) continue;
            hfix58s_top_px565(f, px, py, a->pixels565[yy * a->w + xx]);
        }
    }
}

static void mivf_menu_draw_button_top(u8 *fb, const char *label, int cx, int y, bool enabled, bool is_selected, u32 pulse, MivfMenuAction action) {
    /* MIVF_PHASE7_2_DEFINITIVE_UI_V1
       Minimal focus language: a warm rail, compact diamond, restrained glass,
       and text. Idle rows are borderless so the artwork and composition lead. */
    enum { ROW_W = 222, ROW_H = 20 };
    int x = cx - ROW_W / 2;
    int text_x = x + 27;
    int tr = enabled ? 194 : 88;
    int tg = enabled ? 208 : 95;
    int tb = enabled ? 225 : 105;

    /* MIVF_CUSTOMIZATION_V1 (Phase C.1): MIVF_CTRL_MOVIE_MENU_BACK is the
       only customizable action on this screen (see mivf_customization.h's
       enum comment for why). Every other action (PLAY/RESUME/CHAPTERS)
       draws through this exact same function completely unmodified below --
       this block only ever adds an underlay drawn BEHIND the existing
       highlight/text, never replaces them, and only when a real override
       resolved. No manifest, wrong action, or failed load: byte-identical
       to the pre-Phase-C.1 rendering. */
    if (action == MIVF_MENU_ACTION_BACK && mivf_customization_active_for_menu()) {
        MivfCtrlVisualState vs = (is_selected && enabled) ? MIVF_CTRL_STATE_FOCUSED : MIVF_CTRL_STATE_IDLE;
        const MivfCustomAsset *back_asset = mivf_customization_resolve_asset(MIVF_CTRL_MOVIE_MENU_BACK, vs);
        MivfCtrlColorOverride back_co = mivf_customization_resolve_color(MIVF_CTRL_MOVIE_MENU_BACK, vs, g_mivf_settings.color_vision_mode);
        if (back_asset) {
            mivf_cust_blit_asset_top(fb, cx, y + 5, back_asset);
        }
        if (back_co.has_fill && !(is_selected && enabled)) {
            /* Idle rows have no built-in highlight rect at all (deliberately
               borderless per the comment above) -- a customized Back fill
               is the one case this screen draws a rect while idle. Selected
               rows keep their existing built-in highlight untouched; a Back
               fill override there would fight the existing focus language,
               so it is intentionally not applied while selected. */
            hfix58s_top_blend_rect565(fb, x, y - 5, ROW_W, ROW_H,
                back_co.fill_r, back_co.fill_g, back_co.fill_b, 60);
        }
    }

    if (is_selected && enabled) {
        int breathe = mivf_ui_tri(pulse >> 1, 36u, 10);
        hfix58s_top_blend_rect565(fb, x + 2, y - 4, ROW_W, ROW_H,
            0, 0, 0, 88);
        hfix58s_top_blend_rect565(fb, x, y - 5, ROW_W, ROW_H,
            g_mivf_theme_r, g_mivf_theme_g, g_mivf_theme_b, 45 + breathe);
        hfix58s_top_rect565(fb, x, y - 5, g_mivf_theme_palette.strong_focus ? 6 : 3, ROW_H,
            g_mivf_theme_palette.light_r,g_mivf_theme_palette.light_g,g_mivf_theme_palette.light_b);
        mivf_menu_top_diamond(fb, x + 14, y + 3,
            g_mivf_theme_palette.light_r,g_mivf_theme_palette.light_g,g_mivf_theme_palette.light_b);
        tr=255; tg=249; tb=236;
    }
    hfix58s_top_draw_text_shadow(fb, text_x, y, label, 1, tr, tg, tb);
}

static void mivf_menu_draw_top_root(u8 *fb, const MivfMenu *menu, u32 pulse) {
    /* MIVF_PHASE7_2_FLOATING_MENU_V1
       A softer floating menu: no hard outer frame and no underline on every
       row. The selected row carries the only strong geometry. */
    enum { ROW_H=27, PANEL_W=252 };
    int content_y = 61;
    int cx = TOP_W/2;
    int panel_h = menu->button_count*ROW_H+25;

    /* Preserve prior provenance markers replaced with this renderer. */
    /* MIVF_PHASE7_ROOT_MENU_V1 */
    /* MIVF_PHASE7_1_ROOT_REFINEMENT_V1 */
    mivf_menu_draw_top(fb,menu,pulse);
    panel_h=mivf_ui_clampi(panel_h,48,TOP_H-content_y-12);

    hfix58s_top_blend_rect565(fb,(TOP_W-PANEL_W)/2+3,content_y-11,
        PANEL_W,panel_h,0,0,0,72);
    hfix58s_top_blend_rect565(fb,(TOP_W-PANEL_W)/2,content_y-14,
        PANEL_W,panel_h,5,10,20,82);
    hfix58s_top_draw_text_shadow(fb,(TOP_W-PANEL_W)/2+15,content_y-7,
        "MAIN MENU",1,132,154,184);
    hfix58s_top_rect565(fb,(TOP_W-PANEL_W)/2+80,content_y-4,
        PANEL_W-96,1,40,54,74);

    for(int i=0;i<menu->button_count;i++) {
        const MivfMenuButton *b=&menu->buttons[i];
        mivf_menu_draw_button_top(fb,b->label,cx,
            content_y+14+i*ROW_H,b->enabled,i==menu->selected,pulse,b->action);
    }
}

/* Short, human-readable description of what the selected root-menu button
   does, shown on the bottom-screen info panel. */
static const char *mivf_menu_action_description(MivfMenuAction action) {
    switch (action) {
        case MIVF_MENU_ACTION_PLAY:     return "START FROM THE BEGINNING";
        case MIVF_MENU_ACTION_RESUME:   return "CONTINUE WHERE YOU LEFT OFF";
        case MIVF_MENU_ACTION_CHAPTERS: return "BROWSE MOVIE SCENES";
        case MIVF_MENU_ACTION_BACK:     return "RETURN TO THE FILE BROWSER";
        default:                        return "";
    }
}

/* HFIX70: shared header treatment for both bottom-screen menu panels -- a
   title, a small bullet divider, and (when total_pages > 1) a right-aligned
   "PAGE X/Y" indicator, so Scene Selection reads like a real DVD scene menu
   instead of a plain list. total_pages <= 1 draws no page indicator. */
static void mivf_menu_draw_panel_header(u8 *fb, const char *title, int page, int total_pages) {
    hfix58_draw_text_shadow(fb, 18, 17, title, 1, 238, 246, 255);
    hfix58_rect565(fb, 18, 29, 42, 2,
        g_mivf_theme_r, g_mivf_theme_g, g_mivf_theme_b);
    hfix58_rect565(fb, 64, 30, 154, 1, 45, 60, 80);
    if (total_pages > 1) {
        char page_str[32];
        int w;
        snprintf(page_str, sizeof(page_str), "PAGE %d OF %d", page, total_pages);
        w = (int)strlen(page_str) * 6;
        hfix58_draw_text_shadow(fb, 302 - w, 17, page_str, 1,
            132, 154, 182);
        hfix58_rect565(fb, 302 - w, 29, w, 1,
            g_mivf_theme_r, g_mivf_theme_g, g_mivf_theme_b);
    }
}

/* HFIX71: bottom screen is now a helper/info panel for the root menu --
   the interactive buttons live on the top screen instead. */
static void mivf_menu_draw_info_bottom(u8 *fb, const MivfMenu *menu) {
    /* MIVF_PHASE7_2_CONTEXT_DASHBOARD_V1
       The top screen owns focus; this screen explains the selected action.
       Information is grouped into description, context, and controls. */
    const MivfMenuButton *sel=NULL;
    bool have_resume=false;
    char progress[40];
    char meta[48];

    /* Preserve prior provenance marker replaced with this renderer. */
    /* MIVF_PHASE7_DISC_DASHBOARD_V1 */
    hfix58_rect565(fb,0,0,320,240,2,5,11);
    for(int x=0;x<320;x+=24) hfix58_rect565(fb,x,0,1,217,5,10,19);
    mivf_ui_bottom_glass(fb,8,7,304,204);
    mivf_menu_draw_panel_header(fb,"DISC DASHBOARD",1,1);

    if(menu->selected>=0 && menu->selected<menu->button_count)
        sel=&menu->buttons[menu->selected];
    for(int i=0;i<menu->button_count;i++)
        if(menu->buttons[i].action==MIVF_MENU_ACTION_RESUME &&
           menu->buttons[i].enabled) have_resume=true;

    if(sel) {
        hfix58_rect565(fb,17,47,3,42,g_mivf_theme_palette.light_r,g_mivf_theme_palette.light_g,g_mivf_theme_palette.light_b);
        hfix58_draw_text_shadow(fb,28,51,sel->label,1,
            sel->enabled?241:132,sel->enabled?248:140,sel->enabled?255:150);
        hfix58_draw_text_shadow(fb,28,69,
            mivf_menu_action_description(sel->action),1,154,178,204);
        if(!sel->enabled)
            hfix58_draw_text_shadow(fb,235,52,"NOT AVAILABLE",1,192,112,118);
    }

    hfix58_rect565(fb,17,101,286,1,32,44,62);
    if(sel && sel->action==MIVF_MENU_ACTION_RESUME &&
       menu->has_resume_progress && menu->resume_total_frames>0) {
        int fill=(int)(((u64)menu->resume_bookmark_frame*286ull)/
            (u64)menu->resume_total_frames);
        fill=mivf_ui_clampi(fill,0,286);
        hfix58_draw_text_shadow(fb,17,115,"CONTINUE WATCHING",1,174,196,220);
        hfix58_rect565(fb,17,136,286,7,14,22,35);
        if(fill>0) hfix58_rect565(fb,17,136,fill,7,
            g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);
        if(fill>2) hfix58_rect565(fb,17+fill-2,134,3,11,255,224,125);
        mivf_ui_format_frame_time(progress,sizeof(progress),
            menu->resume_bookmark_frame,menu->resume_total_frames);
        hfix58_draw_text_shadow(fb,17,153,progress,1,204,220,237);
    } else if(sel && sel->action==MIVF_MENU_ACTION_CHAPTERS) {
        snprintf(meta,sizeof(meta),"BROWSE %d CHAPTERS",g_mivf_chapters_count);
        hfix58_draw_text_shadow(fb,17,115,meta,1,190,209,228);
        hfix58_draw_text_shadow(fb,17,136,"SIX SCENES PER PAGE",1,136,158,184);
        hfix58_rect565(fb,17,156,286,2,
            g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);
    } else if(sel && sel->action==MIVF_MENU_ACTION_BACK) {
        hfix58_draw_text_shadow(fb,17,115,"RETURN TO THE LIBRARY",1,190,209,228);
        hfix58_draw_text_shadow(fb,17,136,
            have_resume?"CURRENT PROGRESS IS AVAILABLE":"NO SAVED PROGRESS",1,
            136,158,184);
    } else {
        hfix58_draw_text_shadow(fb,17,115,"PLAY FROM THE BEGINNING",1,190,209,228);
        snprintf(meta,sizeof(meta),"%d CHAPTERS   MIVF DISC",g_mivf_chapters_count);
        hfix58_draw_text_shadow(fb,17,136,meta,1,136,158,184);
        if(have_resume)
            hfix58_draw_text_shadow(fb,17,157,"RESUME READY",1,126,205,162);
    }

    hfix58_draw_text_shadow(fb,17,190,"D-PAD NAVIGATE",1,126,148,176);
    mivf_ui_bottom_footer(fb,"A SELECT","B LIBRARY");
}

/* hfix84: blits one chapter's pre-rendered thumbnail (see
   mivf_menu_load_chapter_thumbs) at (x0,y0) on the bottom screen. */
static void mivf_menu_blit_chapthumb(u8 *fb, int x0, int y0, int chapter_idx) {
    const u16 *src = &g_mivf_chapthumbs[(size_t)chapter_idx * MIVF_CHAPTHUMB_W * MIVF_CHAPTHUMB_H];

    for (int yy = 0; yy < MIVF_CHAPTHUMB_H; yy++) {
        for (int xx = 0; xx < MIVF_CHAPTHUMB_W; xx++) {
            hfix58_px565(fb, x0 + xx, y0 + yy, src[(size_t)yy * MIVF_CHAPTHUMB_W + xx]);
        }
    }
}

static void mivf_menu_draw_chapters_bottom(u8 *fb, int selected) {
    /* Preserve the Phase 7 provenance marker because Phase 7.1 replaces the
       complete Scene Selection renderer that originally contained it. */
    /* MIVF_PHASE7_SCENE_BROWSER_V1 */
    /* MIVF_PHASE7_1_CHAPTER_REFINEMENT_V1
       Compact 112px cards fit the native 96px thumbnails without the large
       unused right-side block visible in the first Phase 7 screenshots. */
    bool have_thumbs = (g_mivf_chapthumbs_count > 0) &&
        (g_mivf_chapthumbs_count == g_mivf_chapters_count);
    int start = (selected / MIVF_MENU_CHAPTERS_VISIBLE_ROWS) *
        MIVF_MENU_CHAPTERS_VISIBLE_ROWS;
    int page = (start / MIVF_MENU_CHAPTERS_VISIBLE_ROWS) + 1;
    int total = (g_mivf_chapters_count +
        MIVF_MENU_CHAPTERS_VISIBLE_ROWS - 1) /
        MIVF_MENU_CHAPTERS_VISIBLE_ROWS;
    if (total < 1) total = 1;

    hfix58_rect565(fb, 0, 0, 320, 240, 2, 5, 11);
    mivf_ui_bottom_glass(fb, 8, 7, 304, 202);
    mivf_menu_draw_panel_header(fb, "SCENE SELECTION", page, total);

    if (have_thumbs) {
        enum {
            COLS = 2, ITEM_W = 112, ITEM_H = 58,
            GAP_X = 24, GAP_Y = 2, X0 = 36, Y0 = 37
        };
        for (int slot = 0; slot < MIVF_MENU_CHAPTERS_VISIBLE_ROWS &&
             start + slot < g_mivf_chapters_count; slot++) {
            int idx = start + slot;
            int col = slot % COLS;
            int row = slot / COLS;
            int x = X0 + col * (ITEM_W + GAP_X);
            int y = Y0 + row * (ITEM_H + GAP_Y);
            int tx = x + (ITEM_W - MIVF_CHAPTHUMB_W) / 2;
            int ty = y + 2;
            bool sel = idx == selected;
            char number[16];

            hfix58_blend_rect565(fb, x + 2, y + 3, ITEM_W, ITEM_H,
                0, 0, 0, 100);
            hfix58_rect565(fb, x, y, ITEM_W, ITEM_H, 7, 12, 21);
            mivf_menu_blit_chapthumb(fb, tx, ty, idx);

            /* Keep almost the entire picture visible; only a small number
               badge overlays the lower-left corner. The full selected label
               is shown contextually on the top screen. */
            snprintf(number, sizeof(number), "%02d", idx + 1);
            hfix58_blend_rect565(fb, tx + 3, ty + MIVF_CHAPTHUMB_H - 15,
                24, 13, 0, 0, 0, 190);
            hfix58_draw_text_shadow(fb, tx + 7,
                ty + MIVF_CHAPTHUMB_H - 12, number, 1,
                238, 243, 250);

            if (sel) {
                hfix58_rect565(fb, x - 2, y - 2, ITEM_W + 4, 2,
                    255, 220, 112);
                hfix58_rect565(fb, x - 2, y + ITEM_H, ITEM_W + 4, 2,
                    255, 220, 112);
                hfix58_rect565(fb, x - 2, y - 2, 2, ITEM_H + 4,
                    255, 220, 112);
                hfix58_rect565(fb, x + ITEM_W, y - 2, 2, ITEM_H + 4,
                    255, 220, 112);
            } else {
                hfix58_blend_rect565(fb, tx, ty,
                    MIVF_CHAPTHUMB_W, MIVF_CHAPTHUMB_H,
                    0, 0, 0, 18);
            }
        }
    } else {
        for (int slot = 0; slot < 6 &&
             start + slot < g_mivf_chapters_count; slot++) {
            int idx = start + slot;
            int y = 43 + slot * 27;
            bool sel = idx == selected;
            char line[64];
            hfix58_blend_rect565(fb, 16, y - 3, 288, 22,
                sel ? g_mivf_theme_r : 8,
                sel ? g_mivf_theme_g : 14,
                sel ? g_mivf_theme_b : 24, sel ? 84 : 150);
            hfix58_rect565(fb, 16, y - 3, sel ? 3 : 1, 22,
                sel ? 255 : 40, sel ? 220 : 52, sel ? 112 : 70);
            snprintf(line, sizeof(line), "%02d   %.38s",
                idx + 1, g_mivf_chapters[idx].label);
            hfix58_draw_text_shadow(fb, 27, y, line, 1,
                sel ? 255 : 198, sel ? 248 : 211,
                sel ? 232 : 228);
        }
    }
    mivf_ui_bottom_footer(fb, "A PLAY   B BACK",
        total > 1 ? "L/R PAGE" : "");
}

/* ------------------------------------------------------------------------- */
/* HFIX80: DVD menu sound effects                                            */
/* ------------------------------------------------------------------------- */
/*
    Movie audio (audio_queue_raw_ndsp/AudioState) exclusively uses NDSP
    channel 0, and only exists once play() calls audio_init_from_stream --
    the menu runs before that, so channel 0 isn't configured yet. Menu SFX
    uses channel 1 instead, which nothing else in this codebase ever touches
    (grep confirms it), so there's no interaction with movie playback audio
    at all, in either direction.

    Tones are synthesized once, procedurally (sine + linear decay envelope,
    using the same integer/triangle-wave idioms already used elsewhere in
    this file rather than pulling in <math.h> for a one-shot need) -- no new
    asset files, no MASB/encoder changes, nothing to ship or embed.
*/
#define MENU_SFX_CHANNEL 1
#define MENU_SFX_RATE    22050
#define MENU_SFX_RING    3   /* rotating wavebuf headers per tone, so a fast
                                double-tap (D-pad auto-repeat) can't try to
                                reuse a wavebuf still QUEUED/PLAYING */

/* Quarter-sine lookup (0..90 degrees, 256 steps, Q15) -- enough resolution
   for an audio envelope/tone and avoids <math.h> for a single use site. */
static const s16 g_menu_sfx_qsin[257] = {
    0,201,402,603,804,1005,1206,1407,1608,1809,2009,2210,2410,2611,2811,3011,
    3211,3410,3610,3809,4008,4206,4405,4603,4801,4999,5196,5393,5590,5787,5983,6179,
    6374,6569,6764,6958,7152,7346,7539,7731,7923,8115,8306,8496,8687,8876,9065,9254,
    9441,9629,9815,10001,10187,10371,10555,10739,10921,11103,11284,11464,11644,11823,12001,12178,
    12355,12530,12705,12879,13052,13224,13395,13565,13735,13903,14071,14237,14403,14567,14731,14893,
    15055,15215,15375,15533,15690,15847,16002,16156,16309,16461,16612,16761,16910,17057,17203,17348,
    17491,17634,17775,17915,18053,18191,18327,18461,18595,18727,18857,18987,19115,19241,19366,19490,
    19612,19733,19853,19971,20087,20202,20315,20427,20538,20647,20754,20860,20964,21067,21168,21268,
    21365,21462,21556,21649,21741,21830,21918,22004,22089,22172,22253,22332,22409,22485,22559,22631,
    22701,22770,22836,22901,22964,23025,23085,23142,23198,23252,23304,23354,23402,23449,23493,23536,
    23576,23615,23652,23687,23720,23751,23780,23807,23832,23855,23877,23896,23913,23929,23942,23954,
    23963,23971,23977,23980,23982,23982,23980,23976,23970,23962,23952,23940,23927,23911,23893,23874,
    23852,23829,23803,23776,23747,23716,23683,23648,23611,23572,23532,23489,23445,23399,23351,23301,
    23249,23196,23140,23083,23024,22963,22901,22836,22770,22702,22633,22561,22488,22413,22337,22258,
    22178,22096,22013,21927,21840,21751,21661,21569,21475,21379,21282,21183,21083,20981,20877,20772,
    20665,20557,20447,20335,20222,20108,19992,19875,19756,19636,19514,19391,19266,19140,19013,18884,
    18754
};

static s16 menu_sfx_sin_q15(u32 phase_deg_x4) {
    u32 wrapped = phase_deg_x4 % 1440u; /* 360 degrees, quarter-degree steps */
    u32 quarter = wrapped / 360u;       /* which 90-degree quadrant (0..3) */
    u32 rem = wrapped % 360u;           /* 0..359, quarter-degree steps within quadrant -> 0..256 index */
    u32 idx = rem * 256u / 360u;
    s32 v = g_menu_sfx_qsin[idx];

    if (quarter == 1) v = g_menu_sfx_qsin[256 - idx];
    else if (quarter == 2) v = -(s32)g_menu_sfx_qsin[idx];
    else if (quarter == 3) v = -(s32)g_menu_sfx_qsin[256 - idx];

    return (s16)v;
}

typedef struct {
    s16 *data;
    u32 nsamples;
    ndspWaveBuf wb[MENU_SFX_RING];
    int next;
} MenuSfxTone;

static MenuSfxTone g_menu_sfx_move;
static MenuSfxTone g_menu_sfx_select;
static MenuSfxTone g_menu_sfx_back;
static bool g_menu_sfx_ready = false;

/* freq_start/end in Hz, ms duration, decay_shift controls how fast the
   envelope falls off (higher = shorter/snappier). Allocates from linear
   memory since NDSP DMAs directly from it, matching audio_queue_raw_ndsp's
   own buffers. */
static bool menu_sfx_synth(MenuSfxTone *tone, int freq_start, int freq_end, int ms, int decay_shift) {
    u32 n = (u32)((MENU_SFX_RATE * ms) / 1000);

    if (n == 0) {
        return false;
    }

    tone->data = (s16*)linearAlloc(n * sizeof(s16));
    if (!tone->data) {
        return false;
    }

    for (u32 i = 0; i < n; i++) {
        /* Linear frequency sweep from freq_start to freq_end over the tone. */
        int freq = freq_start + (int)(((s64)(freq_end - freq_start) * (s64)i) / (s64)n);
        u32 phase = (u32)(((u64)i * (u64)freq * 1440ull) / (u64)MENU_SFX_RATE);
        s32 sample = menu_sfx_sin_q15(phase);

        /* Simple exponential-ish decay via repeated halving, cheap and
           avoids float entirely: envelope = 32768 >> (i * decay_shift / n). */
        u32 decay_steps = (u32)(((u64)i * (u64)decay_shift) / (u64)n);
        s32 env = (decay_steps < 15) ? (32768 >> decay_steps) : 1;

        tone->data[i] = (s16)((sample * env) >> 15);
    }

    tone->nsamples = n;
    tone->next = 0;
    for (int i = 0; i < MENU_SFX_RING; i++) {
        memset(&tone->wb[i], 0, sizeof(ndspWaveBuf));
    }

    DSP_FlushDataCache(tone->data, n * sizeof(s16));
    return true;
}

static void menu_sfx_init(void) {
    if (g_menu_sfx_ready || !g_ndsp_ready) {
        return;
    }

    ndspChnReset(MENU_SFX_CHANNEL);
    ndspChnSetInterp(MENU_SFX_CHANNEL, NDSP_INTERP_LINEAR);
    ndspChnSetFormat(MENU_SFX_CHANNEL, NDSP_FORMAT_MONO_PCM16);
    ndspChnSetRate(MENU_SFX_CHANNEL, (float)MENU_SFX_RATE);

    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 0.5f;
    mix[1] = 0.5f;
    ndspChnSetMix(MENU_SFX_CHANNEL, mix);

    bool ok = true;
    ok = menu_sfx_synth(&g_menu_sfx_move, 1400, 1500, 40, 9) && ok;
    ok = menu_sfx_synth(&g_menu_sfx_select, 900, 1500, 130, 6) && ok;
    ok = menu_sfx_synth(&g_menu_sfx_back, 1100, 700, 90, 7) && ok;

    g_menu_sfx_ready = ok;
}

static void menu_sfx_play(MenuSfxTone *tone) {
    if (!g_menu_sfx_ready || !tone->data) {
        return;
    }

    ndspWaveBuf *wb = &tone->wb[tone->next];

    if (wb->status != NDSP_WBUF_FREE && wb->status != NDSP_WBUF_DONE) {
        /* All ring slots busy (extremely fast repeat presses) -- drop this
           one rather than corrupt a wavebuf still in flight. */
        return;
    }

    memset(wb, 0, sizeof(*wb));
    wb->data_pcm16 = tone->data;
    wb->nsamples = tone->nsamples;
    wb->looping = false;
    ndspChnWaveBufAdd(MENU_SFX_CHANNEL, wb);

    tone->next = (tone->next + 1) % MENU_SFX_RING;
}

static void menu_sfx_move(void)   { menu_sfx_play(&g_menu_sfx_move); }
static void menu_sfx_select(void) { menu_sfx_play(&g_menu_sfx_select); }
static void menu_sfx_back(void)   { menu_sfx_play(&g_menu_sfx_back); }

/* ------------------------------------------------------------------------- */
/* HFIX85: idle-menu bouncing-logo screensaver                               */
/* ------------------------------------------------------------------------- */
#define MIVF_MENU_SCREENSAVER_IDLE_FRAMES 1200 /* ~20-40s depending on actual loop rate */

static const u8 g_mivf_screensaver_palette[][3] = {
    {255, 80, 80}, {80, 200, 255}, {120, 255, 120}, {255, 220, 90},
    {200, 120, 255}, {255, 150, 60}, {90, 255, 220}, {255, 100, 200},
};
#define MIVF_SCREENSAVER_PALETTE_COUNT \
    ((int)(sizeof(g_mivf_screensaver_palette) / sizeof(g_mivf_screensaver_palette[0])))

static int g_mivf_screensaver_x = 20;
static int g_mivf_screensaver_y = 20;
static int g_mivf_screensaver_vx = 2;
static int g_mivf_screensaver_vy = 2;
static int g_mivf_screensaver_color_idx = 0;
/* Advanced Screensaver Customization: frames since this activation, used
   only for the optional fade-in. Reset alongside the rest of the physics
   state so a fresh activation always fades in the same way. */
static u32 g_mivf_screensaver_active_frames = 0;

static void mivf_menu_screensaver_reset(void) {
    int speed = (int)g_mivf_settings.screensaver_speed;
    if (speed < 1) speed = 1;
    g_mivf_screensaver_x = 20;
    g_mivf_screensaver_y = 20;
    g_mivf_screensaver_vx = speed;
    g_mivf_screensaver_vy = speed;
    g_mivf_screensaver_color_idx = 0;
    g_mivf_screensaver_active_frames = 0;
}

/* Advances the bounce physics by one frame and draws it. Top screen shows
   the bouncing mark/image on black; bottom screen goes dark with a plain
   wake hint, matching a real screensaver minimizing what's lit up. */
static void mivf_menu_draw_screensaver(u8 *fb_top, u8 *fb_bot, const MivfMenu *menu) {
    enum { BOX_W=MIVF_SCREENSAVER_W, BOX_H=MIVF_SCREENSAVER_H };
    bool bounced=false; int cr,cg,cb;
    /* MIVF_PHASE7_SCREENSAVER_V1 */
    /* Advanced Screensaver Customization: static/reduce-motion mode skips
       the position update entirely (frozen frame, no bounce) but still
       redraws every frame -- color cycling, fade, and the wake hint all
       keep working; only the physical movement is disabled. */
    if (!g_mivf_settings.screensaver_reduce_motion) {
        g_mivf_screensaver_x+=g_mivf_screensaver_vx; g_mivf_screensaver_y+=g_mivf_screensaver_vy;
        if(g_mivf_screensaver_x<=4){g_mivf_screensaver_x=4;g_mivf_screensaver_vx=-g_mivf_screensaver_vx;bounced=true;}
        else if(g_mivf_screensaver_x+BOX_W>=TOP_W-4){g_mivf_screensaver_x=TOP_W-4-BOX_W;g_mivf_screensaver_vx=-g_mivf_screensaver_vx;bounced=true;}
        if(g_mivf_screensaver_y<=4){g_mivf_screensaver_y=4;g_mivf_screensaver_vy=-g_mivf_screensaver_vy;bounced=true;}
        else if(g_mivf_screensaver_y+BOX_H>=TOP_H-4){g_mivf_screensaver_y=TOP_H-4-BOX_H;g_mivf_screensaver_vy=-g_mivf_screensaver_vy;bounced=true;}
    }
    if(bounced)g_mivf_screensaver_color_idx=(g_mivf_screensaver_color_idx+1)%MIVF_SCREENSAVER_PALETTE_COUNT;
    if (g_mivf_screensaver_active_frames < 0xFFFFFFFFu) g_mivf_screensaver_active_frames++;
    if(g_mivf_screensaver_color_idx==0){cr=g_mivf_theme_r;cg=g_mivf_theme_g;cb=g_mivf_theme_b;}else{cr=g_mivf_screensaver_palette[g_mivf_screensaver_color_idx][0];cg=g_mivf_screensaver_palette[g_mivf_screensaver_color_idx][1];cb=g_mivf_screensaver_palette[g_mivf_screensaver_color_idx][2];}
    if(fb_top){
        /* Deep theatre background with a faint horizon and star-like points. */
        hfix58s_top_rect565(fb_top,0,0,TOP_W,TOP_H,0,1,5);
        for(int y=0;y<TOP_H;y+=12)hfix58s_top_rect565(fb_top,0,y,TOP_W,1,1,4,10);
        for(int i=0;i<12;i++){
            int sx=(i*73+29)%TOP_W,sy=(i*41+17)%TOP_H;
            hfix58s_top_rect565(fb_top,sx,sy,1,1,34,48,70);
        }
        /* Soft shadow/reflection under the traveling card. */
        hfix58s_top_blend_rect565(fb_top,g_mivf_screensaver_x+7,g_mivf_screensaver_y+BOX_H+4,BOX_W-14,5,cr,cg,cb,26);
        if(menu->has_screensaver_image){
            for(int yy=0;yy<BOX_H;yy++)for(int xx=0;xx<BOX_W;xx++)
                hfix58s_top_px565(fb_top,g_mivf_screensaver_x+xx,g_mivf_screensaver_y+yy,g_mivf_screensaver_img[(size_t)yy*BOX_W+xx]);
        } else {
            hfix58s_top_blend_rect565(fb_top,g_mivf_screensaver_x,g_mivf_screensaver_y,BOX_W,BOX_H,cr/4,cg/4,cb/4,230);
            mivf_menu_top_diamond(fb_top,g_mivf_screensaver_x+BOX_W/2,g_mivf_screensaver_y+16,cr,cg,cb);
            hfix58s_top_draw_text_shadow(fb_top,g_mivf_screensaver_x+BOX_W/2-12,g_mivf_screensaver_y+28,"MIVF",1,cr,cg,cb);
        }
        hfix58s_top_rect565(fb_top,g_mivf_screensaver_x-2,g_mivf_screensaver_y-2,BOX_W+4,2,cr,cg,cb);
        hfix58s_top_rect565(fb_top,g_mivf_screensaver_x-2,g_mivf_screensaver_y+BOX_H,BOX_W+4,2,cr,cg,cb);
        hfix58s_top_rect565(fb_top,g_mivf_screensaver_x-2,g_mivf_screensaver_y-2,2,BOX_H+4,cr,cg,cb);
        hfix58s_top_rect565(fb_top,g_mivf_screensaver_x+BOX_W,g_mivf_screensaver_y-2,2,BOX_H+4,cr,cg,cb);
        if(bounced){
            for(int i=0;i<4;i++){
                hfix58s_top_rect565(fb_top,g_mivf_screensaver_x-5-i*2,g_mivf_screensaver_y+BOX_H/2,1,1,cr,cg,cb);
                hfix58s_top_rect565(fb_top,g_mivf_screensaver_x+BOX_W+5+i*2,g_mivf_screensaver_y+BOX_H/2,1,1,cr,cg,cb);
            }
        }
        /* Advanced Screensaver Customization: fade in from black over the
           configured number of frames (0 = off, the default, byte-for-byte
           the pre-existing hard-cut activation). A full-screen black blend
           whose alpha decreases to 0 reads as "fading in" without touching
           any of the physics/color logic above. */
        if (g_mivf_settings.screensaver_fade_frames > 0 &&
            g_mivf_screensaver_active_frames < g_mivf_settings.screensaver_fade_frames) {
            u32 remaining = g_mivf_settings.screensaver_fade_frames - g_mivf_screensaver_active_frames;
            int alpha = (int)((255u * remaining) / g_mivf_settings.screensaver_fade_frames);
            if (alpha > 255) alpha = 255;
            if (alpha > 0) {
                hfix58s_top_blend_rect565(fb_top,0,0,TOP_W,TOP_H,0,0,0,alpha);
            }
        }
    }
    if(fb_bot){
        hfix58_rect565(fb_bot,0,0,320,240,0,1,5);
        hfix58_draw_text_shadow(fb_bot,112,178,"MIVF IDLE",1,64,82,108);
        hfix58_draw_text_shadow(fb_bot,95,220,"PRESS ANY BUTTON",1,125,145,172);
        hfix58_rect565(fb_bot,95,233,130,1,24,36,54);
    }
}

/* MIVF_PHASE7_1_NATIVE_FRAME_CACHE_V1
   Cache complete frames in the framebuffer's native layout. A cache hit is
   a single memcpy, avoiding repeated 400x240 background sampling, text, and
   blend work. Input is still scanned every VBlank and buffers are still
   swapped every loop. The top image advances one visual bucket every eight
   loops, while page/selection changes invalidate immediately. */
#define MIVF_MENU_TOP_CACHE_BYTES ((size_t)TOP_W * (size_t)TOP_H * 2u)
#define MIVF_MENU_BOTTOM_CACHE_BYTES ((size_t)320u * (size_t)240u * 2u)
static u8 g_mivf_menu_top_frame_cache[MIVF_MENU_TOP_CACHE_BYTES];
static u8 g_mivf_menu_bottom_frame_cache[MIVF_MENU_BOTTOM_CACHE_BYTES];
static bool g_mivf_menu_top_frame_valid = false;
static bool g_mivf_menu_bottom_frame_valid = false;
static bool g_mivf_menu_cache_chapters = false;
static int g_mivf_menu_cache_top_selection = -999;
static int g_mivf_menu_cache_bottom_selection = -999;
static u32 g_mivf_menu_cache_anim_bucket = 0xffffffffu;
static u32 g_mivf_menu_cache_theme_generation = 0;

static void mivf_menu_perf_cache_reset(void) {
    g_mivf_menu_top_frame_valid = false;
    g_mivf_menu_bottom_frame_valid = false;
    g_mivf_menu_cache_chapters = false;
    g_mivf_menu_cache_top_selection = -999;
    g_mivf_menu_cache_bottom_selection = -999;
    g_mivf_menu_cache_anim_bucket = 0xffffffffu;
}

static void mivf_menu_draw_chapter_context_top(u8 *fb, int selected,
                                                int page, int total_pages) {
    /* Preserve Phase 7.1 scene provenance because this definitive renderer
       replaces the complete chapter-context function. */
    /* MIVF_PHASE7_1_CHAPTER_REFINEMENT_V1 */
    /* MIVF_PHASE7_2_SCENE_COMPOSITION_V1 */
    char number[24];
    char label[48];
    char progress[32];
    int progress_w;
    if(!fb || selected<0 || selected>=g_mivf_chapters_count) return;
    snprintf(number,sizeof(number),"CHAPTER %02u",(unsigned)(selected+1));
    snprintf(label,sizeof(label),"%.36s",g_mivf_chapters[selected].label);
    snprintf(progress,sizeof(progress),"%d OF %d",selected+1,g_mivf_chapters_count);
    progress_w=(int)strlen(progress)*6;

    hfix58s_top_blend_rect565(fb,58,174,284,40,3,8,17,132);
    hfix58s_top_rect565(fb,58,174,3,40,
        g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);
    hfix58s_top_draw_text_shadow(fb,72,181,number,1,255,226,132);
    /* Avoid repeating generic labels such as "Chapter 1" at equal weight. */
    if(strncmp(label,"Chapter ",8)!=0 && strncmp(label,"CHAPTER ",8)!=0)
        hfix58s_top_draw_text_shadow(fb,72,197,label,1,235,243,252);
    hfix58s_top_draw_text_shadow(fb,326-progress_w,197,progress,1,132,154,182);

    /* Cheap chapter progression rail. */
    hfix58s_top_rect565(fb,72,211,246,2,30,43,60);
    if(g_mivf_chapters_count>1) {
        int px=(246*selected)/(g_mivf_chapters_count-1);
        hfix58s_top_rect565(fb,72,211,px+1,2,
            g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);
        hfix58s_top_rect565(fb,72+px,209,3,6,g_mivf_theme_palette.light_r,g_mivf_theme_palette.light_g,g_mivf_theme_palette.light_b);
    }
    (void)page; (void)total_pages;
}

static void mivf_menu_render_frame(const MivfMenu *menu, bool in_chapters, int chapter_selected, u32 pulse, int fade_alpha) {
    u16 fw = 0, fh = 0;
    u8 *fb_top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &fw, &fh);
    u8 *fb_bot = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &fw, &fh);
    int top_selection = in_chapters ? chapter_selected : menu->selected;
    int bottom_selection = top_selection;
    u32 anim_bucket = pulse >> 2; /* MIVF_PHASE8_1 smoother menu background cadence */
    bool page_changed = !g_mivf_menu_top_frame_valid ||
        g_mivf_menu_cache_chapters != in_chapters ||
        g_mivf_menu_cache_theme_generation != g_mivf_theme_generation;
    bool top_dirty = fade_alpha > 0 || page_changed ||
        g_mivf_menu_cache_top_selection != top_selection ||
        g_mivf_menu_cache_anim_bucket != anim_bucket;
    bool bottom_dirty = fade_alpha > 0 ||
        !g_mivf_menu_bottom_frame_valid ||
        g_mivf_menu_cache_chapters != in_chapters ||
        g_mivf_menu_cache_bottom_selection != bottom_selection;

    if (fb_top) {
        if (top_dirty) {
            if (in_chapters) {
                int start = (chapter_selected /
                    MIVF_MENU_CHAPTERS_VISIBLE_ROWS) *
                    MIVF_MENU_CHAPTERS_VISIBLE_ROWS;
                int page = start / MIVF_MENU_CHAPTERS_VISIBLE_ROWS + 1;
                int total = (g_mivf_chapters_count +
                    MIVF_MENU_CHAPTERS_VISIBLE_ROWS - 1) /
                    MIVF_MENU_CHAPTERS_VISIBLE_ROWS;
                if (total < 1) total = 1;
                mivf_menu_draw_top(fb_top, menu, pulse);
                mivf_menu_draw_chapter_context_top(fb_top,
                    chapter_selected, page, total);
            } else {
                mivf_menu_draw_top_root(fb_top, menu, pulse);
            }
            if (fade_alpha > 0) {
                hfix58s_top_blend_rect565(fb_top, 0, 0,
                    TOP_W, TOP_H, 0, 0, 0, fade_alpha);
            }
            if (fade_alpha == 0) {
                memcpy(g_mivf_menu_top_frame_cache, fb_top,
                    MIVF_MENU_TOP_CACHE_BYTES);
                g_mivf_menu_top_frame_valid = true;
                g_mivf_menu_cache_top_selection = top_selection;
                g_mivf_menu_cache_anim_bucket = anim_bucket;
            }
        } else {
            memcpy(fb_top, g_mivf_menu_top_frame_cache,
                MIVF_MENU_TOP_CACHE_BYTES);
        }
    }

    if (fb_bot) {
        if (bottom_dirty) {
            if (in_chapters) {
                mivf_menu_draw_chapters_bottom(fb_bot, chapter_selected);
            } else {
                mivf_menu_draw_info_bottom(fb_bot, menu);
            }
            if (fade_alpha > 0) {
                hfix58_blend_rect565(fb_bot, 0, 0, 320, 240,
                    0, 0, 0, fade_alpha);
            }
            if (fade_alpha == 0) {
                memcpy(g_mivf_menu_bottom_frame_cache, fb_bot,
                    MIVF_MENU_BOTTOM_CACHE_BYTES);
                g_mivf_menu_bottom_frame_valid = true;
                g_mivf_menu_cache_bottom_selection = bottom_selection;
            }
        } else {
            memcpy(fb_bot, g_mivf_menu_bottom_frame_cache,
                MIVF_MENU_BOTTOM_CACHE_BYTES);
        }
    }

    if (fade_alpha == 0 && (top_dirty || bottom_dirty)) {
        g_mivf_menu_cache_chapters = in_chapters;
        g_mivf_menu_cache_theme_generation = g_mivf_theme_generation;
    }
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
}

#define MIVF_MENU_FADE_STEPS 10

static void mivf_menu_fade(const MivfMenu *menu, bool in_chapters, int chapter_selected, u32 pulse, bool fade_out) {
    for (int step = 1; step <= MIVF_MENU_FADE_STEPS; step++) {
        int t = fade_out ? step : (MIVF_MENU_FADE_STEPS - step);
        int alpha = (t * 255) / MIVF_MENU_FADE_STEPS;
        mivf_menu_render_frame(menu, in_chapters, chapter_selected, pulse, alpha);
    }
}

/* Runs the interactive menu loop. Returns PLAY once a launch-worthy action
   fires (having set g_mivf_launch_mode/g_mivf_launch_chapter_index as a
   side effect), or BACK to return to the browser without playing anything. */
static MivfMenuResult mivf_menu_run(MivfMenu *menu) {
    MivfMenuResult result;
    bool in_chapters = false;
    int chapter_selected = 0;
    u32 pulse = 0;
    bool have_last;
    bool screensaver_active = false;
    u32 idle_frames = 0;

    menu_sfx_init();
    mivf_menu_perf_cache_reset();

    /* hfix81: restore the last-highlighted button/chapter for this exact
       movie, if its menu was shown earlier this session. */
    have_last = (g_mivf_menu_last_path[0] != 0) &&
        !strcmp(g_mivf_menu_last_path, menu->movie_path);

    menu->selected = -1;
    if (have_last && g_mivf_menu_last_button >= 0 &&
        g_mivf_menu_last_button < menu->button_count &&
        menu->buttons[g_mivf_menu_last_button].enabled) {
        menu->selected = g_mivf_menu_last_button;
    }
    if (menu->selected < 0) {
        for (int i = 0; i < menu->button_count; i++) {
            if (menu->buttons[i].enabled) {
                menu->selected = i;
                break;
            }
        }
    }
    if (menu->selected < 0) {
        menu->selected = 0;
    }

    if (have_last && g_mivf_menu_last_in_chapters && g_mivf_chapters_count > 0) {
        in_chapters = true;
        chapter_selected = g_mivf_menu_last_chapter;
        if (chapter_selected < 0) {
            chapter_selected = 0;
        }
        if (chapter_selected >= g_mivf_chapters_count) {
            chapter_selected = g_mivf_chapters_count - 1;
        }
    }

    mivf_menu_fade(menu, in_chapters, chapter_selected, pulse, false);

    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
#ifdef MIVF_SHOWCASE_FULL
        mivf_showcase_tick();
        g_mivf_showcase.current_loop = MIVF_SC_LOOP_DVDMENU;
        /* MIVF_CAPTURE_CH4_DIRECT_V3: display real Scene Selection on
           Chapter 4, then return PLAY with the chapter directive committed.
           Ordinary builds never enter this active Showcase-only branch. */
        if (g_mivf_showcase.active && g_mivf_showcase.stage == MIVF_SC_SCENES) {
            u64 capture_elapsed_ms = ticks_to_us(
                svcGetSystemTick() - g_mivf_showcase.stage_entry_tick) / 1000ull;
            if (g_mivf_chapters_count < 4) {
                printf("capture: Chapter 4 unavailable chapters=%d\n", g_mivf_chapters_count);
            } else {
                in_chapters = true;
                chapter_selected = 3;
                if (capture_elapsed_ms >= 3500ull) {
                    g_mivf_launch_mode = MIVF_LAUNCH_CHAPTER;
                    g_mivf_launch_chapter_index = chapter_selected;
                    printf("capture: direct chapter launch index=%d frame=%lu label=%s\n",
                        chapter_selected,
                        (unsigned long)g_mivf_chapters[chapter_selected].frame,
                        g_mivf_chapters[chapter_selected].label);
                    result = MIVF_MENU_RESULT_PLAY;
                    goto mivf_menu_exit;
                }
            }
        }
        mivf_showcase_synth_key(&down);
        mivf_showcase_cancel_check(down, hidKeysHeld());
#ifdef MIVF_SHOWCASE_CAPTURE
        mivf_showcase_maybe_capture();
#endif
        if (mivf_showcase_render_overlay_if_needed()) {
            continue;
        }
        if (screensaver_active && g_mivf_showcase.active && g_mivf_showcase.stage != MIVF_SC_SCREENSAVER) {
            /* The Showcase has moved past the SCREENSAVER stage (timed out
               in mivf_showcase_tick() above) -- wake it through the exact
               same path a real keypress uses, so nothing is left rendering
               a stage the controller no longer thinks is active. */
            mivf_showcase_screensaver_restore(&idle_frames, &screensaver_active);
            mivf_menu_fade(menu, in_chapters, chapter_selected, pulse, false);
        }
#endif
        pulse++;

        /* hfix85: idle screensaver -- scoped to the root menu view only
           (Scene Selection keeps its own paging state, and exiting the
           screensaver back into the middle of a scrub/page felt like more
           complexity than this playful feature is worth). Any key wakes it
           and is consumed by waking it, not also treated as a menu action
           (that's what `continue` below is for). */
        if (down != 0) {
            if (screensaver_active) {
                screensaver_active = false;
                mivf_menu_fade(menu, in_chapters, chapter_selected, pulse, false);
            }
            idle_frames = 0;
        } else if (!in_chapters) {
            idle_frames++;
#ifdef MIVF_SHOWCASE_FULL
            mivf_showcase_screensaver_accelerate(&idle_frames, g_mivf_settings.screensaver_idle_frames);
#endif
            if (!screensaver_active && idle_frames >= g_mivf_settings.screensaver_idle_frames) {
                mivf_menu_fade(menu, false, 0, pulse, true);
                screensaver_active = true;
                mivf_menu_screensaver_reset();
#ifdef MIVF_SHOWCASE_FULL
                if (g_mivf_showcase.active && g_mivf_showcase.stage == MIVF_SC_SCREENSAVER) {
                    mivf_showcase_log("SCREENSAVER", "ACTIVATED", "PASS", "");
                }
#endif
            }
        }

        if (screensaver_active) {
            u16 fw = 0, fh = 0;
            u8 *fb_top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &fw, &fh);
            u8 *fb_bot = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &fw, &fh);

            mivf_menu_draw_screensaver(fb_top, fb_bot, menu);
            gfxFlushBuffers();
            gfxSwapBuffers();
            gspWaitForVBlank();
            continue;
        }

        if (in_chapters) {
            if ((down & KEY_DUP) && chapter_selected > 0) {
                chapter_selected--;
                menu_sfx_move();
            }
            if ((down & KEY_DDOWN) && chapter_selected < g_mivf_chapters_count - 1) {
                chapter_selected++;
                menu_sfx_move();
            }
            if (down & KEY_L) {
                int prev = chapter_selected;
                chapter_selected -= MIVF_MENU_CHAPTERS_VISIBLE_ROWS;
                if (chapter_selected < 0) {
                    chapter_selected = 0;
                }
                if (chapter_selected != prev) {
                    menu_sfx_move();
                }
            }
            if (down & KEY_R) {
                int prev = chapter_selected;
                chapter_selected += MIVF_MENU_CHAPTERS_VISIBLE_ROWS;
                if (chapter_selected > g_mivf_chapters_count - 1) {
                    chapter_selected = g_mivf_chapters_count - 1;
                }
                if (chapter_selected != prev) {
                    menu_sfx_move();
                }
            }
            if ((down & KEY_A) && g_mivf_chapters_count > 0) {
                menu_sfx_select();
                g_mivf_launch_mode = MIVF_LAUNCH_CHAPTER;
                g_mivf_launch_chapter_index = chapter_selected;
                result = MIVF_MENU_RESULT_PLAY;
                goto mivf_menu_exit;
            }
            if (down & (KEY_B | KEY_START)) {
                menu_sfx_back();
                mivf_menu_fade(menu, true, chapter_selected, pulse, true);
                in_chapters = false;
                mivf_menu_fade(menu, false, 0, pulse, false);
            }
        } else {
            if (down & (KEY_DUP | KEY_DDOWN) && menu->button_count > 0) {
                int dir = (down & KEY_DUP) ? -1 : 1;
                int prev = menu->selected;
                for (int i = 1; i <= menu->button_count; i++) {
                    int idx = ((menu->selected + dir * i) % menu->button_count + menu->button_count) % menu->button_count;
                    if (menu->buttons[idx].enabled) {
                        menu->selected = idx;
                        break;
                    }
                }
                if (menu->selected != prev) {
                    menu_sfx_move();
                }
            }

            /* HFIX71: touch-select removed here -- button rects are now top-
               screen coordinates, and touch input only ever comes from the
               bottom screen, so bottom-screen touch coordinates no longer
               correspond to button positions. D-pad + A/B is the supported
               input for the top-screen root menu. */

            if ((down & KEY_A) && menu->selected >= 0 && menu->selected < menu->button_count) {
                MivfMenuButton *b = &menu->buttons[menu->selected];

                if (b->enabled) {
                    switch (b->action) {
                        case MIVF_MENU_ACTION_PLAY:
                            menu_sfx_select();
                            g_mivf_launch_mode = MIVF_LAUNCH_START_OVER;
                            result = MIVF_MENU_RESULT_PLAY;
                            goto mivf_menu_exit;
                        case MIVF_MENU_ACTION_RESUME:
                            menu_sfx_select();
                            g_mivf_launch_mode = MIVF_LAUNCH_RESUME;
                            result = MIVF_MENU_RESULT_PLAY;
                            goto mivf_menu_exit;
                        case MIVF_MENU_ACTION_CHAPTERS:
                            menu_sfx_select();
                            mivf_menu_fade(menu, false, 0, pulse, true);
                            in_chapters = true;
                            chapter_selected = 0;
                            mivf_menu_fade(menu, true, chapter_selected, pulse, false);
                            break;
                        case MIVF_MENU_ACTION_BACK:
                            menu_sfx_back();
                            result = MIVF_MENU_RESULT_BACK;
                            goto mivf_menu_exit;
                        default:
                            break;
                    }
                }
            }

            if (down & (KEY_B | KEY_START)) {
                menu_sfx_back();
                result = MIVF_MENU_RESULT_BACK;
                goto mivf_menu_exit;
            }
        }

        mivf_menu_render_frame(menu, in_chapters, chapter_selected, pulse, 0);
    }

    return MIVF_MENU_RESULT_BACK;

mivf_menu_exit:
    /* hfix81: remember where the cursor was, whichever way we're leaving,
       so the next visit to this same movie's menu resumes there. */
    snprintf(g_mivf_menu_last_path, sizeof(g_mivf_menu_last_path), "%s", menu->movie_path);
    g_mivf_menu_last_button = menu->selected;
    g_mivf_menu_last_in_chapters = in_chapters;
    g_mivf_menu_last_chapter = chapter_selected;

    mivf_menu_fade(menu, in_chapters, chapter_selected, pulse, true);
    return result;
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

    if (g_hfix59r3_settings_visible || g_hfix62_help_visible ||
        g_mivf_theme_picker.active || g_mivf_cvd_picker.active ||
        g_mivf_transport_picker.active) {
        if ((up & KEY_TOUCH) != 0) g_mivf_anim.is_touch_scrubbing = false;
        return;
    }

    if ((held & KEY_TOUCH) != 0) {
        const MivfTouchLayout *layout = hfix57_current_touch_layout();
        int timeline_x = layout->timeline.x;
        int timeline_w = layout->timeline.w;
        int timeline_y = layout->timeline.y;
        int timeline_h = layout->timeline.h;

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

/* hfix58f_total_frames() itself always returns a real value (falling
   back to 1 to avoid divide-by-zero in ITS callers' progress-bar math)
   -- that fallback of 1 is not a trustworthy total for watch-state
   purposes (it would make frame 1 of any video register as "complete").
   Mirrors the exact same trust preconditions hfix58f_total_frames()
   itself checks, returning 0 (this module's "unknown" sentinel) when
   none of them hold instead of a degenerate 1. */
static u32 hfix_watchstate_trustworthy_total(void) {
    if (g_hfix59r2_duration_ticks != 0 && g_hfix59r2_video_fps_num != 0) {
        return hfix58f_total_frames();
    }
    if (g_hfix58f_seek.total_frames) {
        return hfix58f_total_frames();
    }
    if (g_media_ctl.total_frames) {
        return hfix58f_total_frames();
    }
    return 0;
}

/* Continue Watching index reconciliation, shared by both save paths --
   IN_PROGRESS notes/promotes; anything else (WATCHED or a rare
   UNWATCHED) removes, satisfying "automatic removal on completion". */
static void hfix_watchstate_reconcile_continue(const char *path, u32 status) {
    if (status == MIVF_WATCH_IN_PROGRESS) {
        hfix_continue_note(path);
    } else {
        hfix_continue_remove(path);
    }
}

static void hfix_watchstate_checkpoint(u32 shown_frame) {
    MivfWatchState state;
    u32 total = hfix_watchstate_trustworthy_total();

    memset(&state, 0, sizeof(state));
    state.last_frame = shown_frame;
    state.total_frames = total;
    state.status = hfix_watchstate_compute_status(shown_frame, total, false);
    state.last_played_unix = (u32)time(NULL);
    MIVF_WatchStateSave(MIVF_PATH, &state);
    hfix_watchstate_reconcile_continue(MIVF_PATH, state.status);
}

static void hfix_watchstate_finish(u32 shown_frame, bool reached_eof) {
    MivfWatchState state;
    u32 total = hfix_watchstate_trustworthy_total();

    memset(&state, 0, sizeof(state));
    state.last_frame = shown_frame;
    state.total_frames = total;
    state.status = hfix_watchstate_compute_status(shown_frame, total, reached_eof);
    state.last_played_unix = (u32)time(NULL);
    MIVF_WatchStateSave(MIVF_PATH, &state);
    hfix_watchstate_reconcile_continue(MIVF_PATH, state.status);
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

/* HFIX73: robust .idx diagnostics and sidecar loader.
   The standalone generator writes a normal sidecar next to the movie
   (cars.mivf -> cars.idx).  Older code tended to fail silently, which made
   chapter launches and manual seek look like they were broken menu features.
   Read the header explicitly and accept both possible on-disk seek-point
   strides: 16 bytes (ARM padded u32+u64 struct) and 12 bytes (packed). */
static bool hfix58j_try_load_seek_cache(const char *cache_path, u64 file_size, u32 first_offset) {
    FILE *fp = NULL;
    u32 magic = 0, version = 0, count = 0, stored_first = 0;
    u64 stored_size = 0;
    long end_pos = 0;
    long data_bytes = 0;
    int stride = 0;

    printf("idx: try sidecar %s\n", cache_path ? cache_path : "(null)");

    if (!cache_path || !cache_path[0]) {
        printf("idx: reject empty path\n");
        return false;
    }

    fp = fopen(cache_path, "rb");
    if (!fp) {
        printf("idx: open failed %s\n", cache_path);
        return false;
    }

    if (fread(&magic, 1, 4, fp) != 4 ||
        fread(&version, 1, 4, fp) != 4 ||
        fread(&stored_size, 1, 8, fp) != 8 ||
        fread(&stored_first, 1, 4, fp) != 4 ||
        fread(&count, 1, 4, fp) != 4) {
        printf("idx: reject short header\n");
        fclose(fp);
        return false;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        printf("idx: reject fseek end failed\n");
        fclose(fp);
        return false;
    }
    end_pos = ftell(fp);
    if (end_pos < 0) {
        printf("idx: reject ftell failed\n");
        fclose(fp);
        return false;
    }

    printf("idx: hdr magic=%08lx version=%lu stored_size=%llu runtime_size=%llu stored_first=%lu runtime_first=%lu count=%lu bytes=%ld\n",
        (unsigned long)magic, (unsigned long)version,
        (unsigned long long)stored_size, (unsigned long long)file_size,
        (unsigned long)stored_first, (unsigned long)first_offset,
        (unsigned long)count, end_pos);

    if (magic != HFIX58J_IDX_MAGIC || version != HFIX58J_IDX_VERSION) {
        printf("idx: reject magic/version expected magic=%08lx version=%lu\n",
            (unsigned long)HFIX58J_IDX_MAGIC, (unsigned long)HFIX58J_IDX_VERSION);
        fclose(fp);
        return false;
    }
    if (stored_size != file_size || stored_first != first_offset) {
        printf("idx: reject file binding mismatch\n");
        fclose(fp);
        return false;
    }
    if (count == 0 || count > HFIX58F_MAX_SEEK_POINTS) {
        printf("idx: reject invalid count=%lu max=%lu\n",
            (unsigned long)count, (unsigned long)HFIX58F_MAX_SEEK_POINTS);
        fclose(fp);
        return false;
    }

    data_bytes = end_pos - 24;
    if (data_bytes == (long)count * 16L) {
        stride = 16;
    } else if (data_bytes == (long)count * 12L) {
        stride = 12;
    } else {
        printf("idx: reject record bytes=%ld not count*16=%ld or count*12=%ld\n",
            data_bytes, (long)count * 16L, (long)count * 12L);
        fclose(fp);
        return false;
    }

    if (fseek(fp, 24, SEEK_SET) != 0) {
        printf("idx: reject seek data failed\n");
        fclose(fp);
        return false;
    }

    memset(&g_hfix58f_seek, 0, sizeof(g_hfix58f_seek));
    g_hfix58f_seek.count = count;
    g_hfix58f_seek.total_frames = 0;

    for (u32 i = 0; i < count; i++) {
        u32 frame = 0;
        u64 offset = 0;
        if (fread(&frame, 1, 4, fp) != 4) {
            printf("idx: reject short frame at %lu\n", (unsigned long)i);
            fclose(fp);
            memset(&g_hfix58f_seek, 0, sizeof(g_hfix58f_seek));
            return false;
        }
        if (stride == 16) {
            u32 pad = 0;
            if (fread(&pad, 1, 4, fp) != 4 || fread(&offset, 1, 8, fp) != 8) {
                printf("idx: reject short padded record at %lu\n", (unsigned long)i);
                fclose(fp);
                memset(&g_hfix58f_seek, 0, sizeof(g_hfix58f_seek));
                return false;
            }
        } else {
            if (fread(&offset, 1, 8, fp) != 8) {
                printf("idx: reject short packed record at %lu\n", (unsigned long)i);
                fclose(fp);
                memset(&g_hfix58f_seek, 0, sizeof(g_hfix58f_seek));
                return false;
            }
        }
        g_hfix58f_seek.points[i].frame = frame;
        g_hfix58f_seek.points[i].file_offset = offset;
        if (frame + 1u > g_hfix58f_seek.total_frames) {
            g_hfix58f_seek.total_frames = frame + 1u;
        }
    }

    fclose(fp);
    g_hfix58f_seek.ready = true;
    g_hfix58f_seek_large_file_mode = false;

    printf("idx: loaded count=%lu stride=%d first_frame=%lu first_off=%llu last_frame=%lu last_off=%llu\n",
        (unsigned long)g_hfix58f_seek.count, stride,
        (unsigned long)g_hfix58f_seek.points[0].frame,
        (unsigned long long)g_hfix58f_seek.points[0].file_offset,
        (unsigned long)g_hfix58f_seek.points[g_hfix58f_seek.count - 1].frame,
        (unsigned long long)g_hfix58f_seek.points[g_hfix58f_seek.count - 1].file_offset);

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
    g_hfix58f_seek_large_file_mode = false;
    g_hfix58f_index_first_offset = first_offset;

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
        printf("idx: large file no sidecar/embedded index; entering fallback mode size=%llu first=%lu total=%lu\n",
            (unsigned long long)file_size, (unsigned long)first_offset,
            (unsigned long)g_hfix58f_seek.total_frames);
        g_hfix58f_seek_large_file_mode = true;

        if (saved >= 0) {
            fseek(f, saved, SEEK_SET);
        }

        /*
            Movie-sized uncached files must start playback immediately. A full
            first-run seek-index scan can walk gigabytes and looks like a hang.
            Existing .idx files are still used, and small files still build one.
            g_hfix58f_seek_large_file_mode lets a later seek request fall back
            to a small bounded on-demand scan (HFIX67) instead of just failing.
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

/* HFIX67: on-demand fallback for files where no seek index exists at all
   (g_hfix58f_seek_large_file_mode). Interpolates a byte-offset guess from
   target_frame/total_frames (assuming roughly uniform bytes-per-frame),
   backs up a safety margin, then scans forward for the first real sync
   point -- capped to HFIX67_APPROX_SEEK_SCAN_PAGES pages, so this costs at
   most a small, bounded read at the moment a seek is requested. It never
   walks the whole file. On success the found point is written into
   g_hfix58f_seek.points[0] and treated as a valid (if approximate) index,
   so hfix58f_find_seekpoint/hfix58f_execute_pending_seek need no changes --
   they just see a 1-entry index for this seek. */
#define HFIX67_APPROX_SEEK_SCAN_PAGES 600
#define HFIX67_APPROX_SEEK_BACKUP_BYTES (512ull * 1024ull)

static bool hfix67_approx_seek_scan(FILE *f, const Stream *v, u32 target_frame) {
    if (!f || !v || g_hfix58f_seek.total_frames == 0) {
        return false;
    }

    long saved = ftell(f);
    bool have_best = false;
    Hfix58FSeekPoint best;
    memset(&best, 0, sizeof(best));

    if (fseek(f, 0, SEEK_END) != 0) {
        if (saved >= 0) fseek(f, saved, SEEK_SET);
        return false;
    }
    long end_pos_l = ftell(f);
    if (end_pos_l <= 0) {
        if (saved >= 0) fseek(f, saved, SEEK_SET);
        return false;
    }
    u64 file_size = (u64)end_pos_l;
    u32 first_offset = g_hfix58f_index_first_offset;

    if (first_offset >= file_size) {
        if (saved >= 0) fseek(f, saved, SEEK_SET);
        return false;
    }

    double frac = (double)target_frame / (double)g_hfix58f_seek.total_frames;
    if (frac < 0.0) frac = 0.0;
    if (frac > 1.0) frac = 1.0;

    u64 est = (u64)first_offset + (u64)(frac * (double)(file_size - first_offset));
    u64 pos = (est > (u64)first_offset + HFIX67_APPROX_SEEK_BACKUP_BYTES)
        ? est - HFIX67_APPROX_SEEK_BACKUP_BYTES
        : (u64)first_offset;

    for (int page_i = 0; page_i < HFIX67_APPROX_SEEK_SCAN_PAGES && pos + MIVF_PAGE_HEADER_SIZE < file_size; page_i++) {
        u8 page_hdr[MIVF_PAGE_HEADER_SIZE];

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

        if (page_end > file_size) {
            break;
        }

        u64 pkt_pos = page_payload_start;
        bool stop_after_page = false;

        for (u16 i = 0; i < packets && pkt_pos + 16 <= page_end; i++) {
            u8 pkt_hdr[16];

            if (fseek(f, (long)pkt_pos, SEEK_SET) != 0) {
                break;
            }
            if (fread(pkt_hdr, 1, sizeof(pkt_hdr), f) != sizeof(pkt_hdr)) {
                break;
            }

            u8 pkt_stream = pkt_hdr[0];
            u8 flags = pkt_hdr[1];
            u16 hsize = le16(pkt_hdr + 2);
            u32 psize = le32(pkt_hdr + 8);
            u32 pframe = le32(pkt_hdr + 12);

            if (hsize < 16 || psize > payload || pkt_pos + hsize + psize > page_end) {
                break;
            }

            if (pkt_stream != v->id) {
                pkt_pos += (u64)hsize + (u64)psize;
                continue;
            }

            bool sync = false;
            u8 body_head[16];
            memset(body_head, 0, sizeof(body_head));

            if (psize >= 4 && pkt_pos + hsize + 4 <= page_end) {
                u32 probe = psize < (u32)sizeof(body_head) ? psize : (u32)sizeof(body_head);

                if (fseek(f, (long)(pkt_pos + hsize), SEEK_SET) == 0) {
                    (void)fread(body_head, 1, probe, f);
                }
            }

            if (body_head[0] == 'M' && body_head[1] == '2' && body_head[2] == 'Y' && body_head[3] == '0') {
                sync = true;
            }
            if (hfix58f_body_is_m2y_delta_codec(body_head, psize)) {
                sync = hfix58f_body_is_m2y_keyframe(body_head, psize);
            }
            if (body_head[0] == 'M' && body_head[1] == '1' && body_head[2] == 'P' && body_head[3] == '0') {
                sync = true;
            }
            if (!hfix58f_body_is_m2y_delta_codec(body_head, psize) &&
                (flags & 1) != 0 &&
                body_head[0] == 'M' &&
                (body_head[1] == '2' || body_head[1] == '1')) {
                sync = true;
            }
            if (!strcmp(v->codec, "RAWV")) {
                u32 raw_size = (u32)v->w * (u32)v->h * 2u;
                if (raw_size != 0 && psize == raw_size) {
                    sync = true;
                }
            }

            if (sync) {
                if (!have_best || (pframe <= target_frame && pframe > best.frame)) {
                    best.frame = pframe;
                    best.file_offset = pos;
                    have_best = true;
                }
                if (pframe >= target_frame) {
                    stop_after_page = true;
                }
                break;
            }

            pkt_pos += (u64)hsize + (u64)psize;
        }

        if (stop_after_page) {
            break;
        }

        pos = page_end;
    }

    if (saved >= 0) {
        fseek(f, saved, SEEK_SET);
    }

    if (!have_best) {
        return false;
    }

    g_hfix58f_seek.points[0] = best;
    g_hfix58f_seek.count = 1;
    g_hfix58f_seek.ready = true;
    return true;
}

static const Hfix58FSeekPoint *hfix58f_find_seekpoint(u32 target_frame) {
    printf("idx: find target=%lu ready=%d count=%lu total=%lu\n",
        (unsigned long)target_frame, g_hfix58f_seek.ready ? 1 : 0,
        (unsigned long)g_hfix58f_seek.count, (unsigned long)g_hfix58f_seek.total_frames);
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
    /* Clear any decoded-but-not-yet-submitted audio in the manual sync delay
       ring, and drop the video-delay ring's buffered frames, so neither can
       leak pre-seek content across the jump. Routed through av_sync_offset_apply
       so a negative (video-delay) setting is handled correctly instead of
       wrapping through the u32 audio path. */
    av_sync_offset_apply(g_mivf_settings.audio_offset_ms);
    video_delay_reset();

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

    /* HFIX88: audio_configure_ndsp_channel() already advanced the clock
       generation and cleared every map entry immediately after ndspChnReset.
       The clock remains unknown until the first new post-seek submission. */

    /* Queue was just emptied -- restart the sync controller neutral so it
       re-converges for the new position instead of holding a stale rate.
       (audio_configure_ndsp_channel above already set the base rate; this
       just clears the correction factor to match.) */
    audio_rate_sync_reset();
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

    if (!sp && g_hfix58f_seek_large_file_mode) {
        /* HFIX67: no real index for this file -- try a small bounded
           on-demand scan near the requested frame before giving up. */
        if (hfix67_approx_seek_scan(f, v, g_hfix58f_seek_target)) {
            sp = hfix58f_find_seekpoint(g_hfix58f_seek_target);
        }
    }

    if (!sp) {
        hfix58_alert_set("SEEK FAILED", 2);
        return false;
    }

    if (!preview_seek) {
        avsync_arm_seek(g_hfix58f_seek_target, sp->frame);
    }

    /* Any jump -- preview (scrub) or real -- discontinues the frame/tick
       relationship the drift baseline assumes, so it must be re-armed here
       too, independent of the seek-alignment report above (which is only
       meaningful for real seeks). Without this, a scrub burst leaves the
       drift diagnostic comparing "time since session start" against a
       frame number that just jumped tens of thousands of frames, producing
       meaningless multi-minute "drift" numbers that look alarming but
       aren't real -- see the log analysis that caught this. */
    g_avsync_drift_pending_reset = true;

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
    if (!fb) return;
    /* C2.4 timeline is owned by the selected experience. */
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


/* HFIX64_RESUME_PROMPT: ask before applying a saved bookmark.
   This reuses the existing bookmark/seek pipeline. A resumes from the saved
   frame; B/START starts from the beginning and clears the bookmark. */
static bool hfix64_resume_prompt(u32 saved_frame, u32 fpsn, u32 fpsd) {
    /* MIVF_PHASE8_RESUME_MODAL_V1
       Polished two-choice modal preserving the original bool contract:
       true=resume, false=start over. B/START retain start-over behavior. */
    char time_line[48];
    char title[64];
    u32 fps_num=fpsn?fpsn:30u;
    u32 fps_den=fpsd?fpsd:1u;
    u64 seconds=((u64)saved_frame*(u64)fps_den)/(u64)fps_num;
    u32 hh=(u32)(seconds/3600u),mm=(u32)((seconds/60u)%60u),ss=(u32)(seconds%60u);
    int selected=0;
    menu_sfx_init();
    mivf8_ellipsis(title,sizeof(title),hfix58_preview_basename(MIVF_PATH),34);
    if(hh>0) snprintf(time_line,sizeof(time_line),"SAVED AT %lu:%02lu:%02lu",
        (unsigned long)hh,(unsigned long)mm,(unsigned long)ss);
    else snprintf(time_line,sizeof(time_line),"SAVED AT %lu:%02lu",
        (unsigned long)mm,(unsigned long)ss);

    while(aptMainLoop()) {
        hidScanInput();
        u32 down=hidKeysDown();
#ifdef MIVF_SHOWCASE_FULL
        mivf_showcase_cancel_check(down, hidKeysHeld());
        if (g_mivf_showcase.active && g_mivf_showcase.stage == MIVF_SC_RESUME) {
            /* Accept resume shortly after the prompt is reached -- selected
               defaults to 0 (Resume), so a plain synthesized A confirms it,
               through the exact same return path a real press uses. */
            down |= KEY_A;
        }
#endif
        if(down&(KEY_DUP|KEY_DDOWN|KEY_DLEFT|KEY_DRIGHT)) {
            selected=1-selected; menu_sfx_move();
        }
        if(down&KEY_A) {
            menu_sfx_select(); return selected==0;
        }
        if(down&(KEY_B|KEY_START)) {
            menu_sfx_back(); return false;
        }

        u16 fw=0,fh=0;
        u8 *top=gfxGetFramebuffer(GFX_TOP,GFX_LEFT,&fw,&fh);
        u8 *bot=gfxGetFramebuffer(GFX_BOTTOM,GFX_LEFT,&fw,&fh);
        if(top) {
            hfix58s_top_rect565(top,0,0,TOP_W,TOP_H,2,5,11);
            for(int y=0;y<TOP_H;y+=16) hfix58s_top_rect565(top,0,y,TOP_W,1,4,9,18);
            mivf8_top_panel(top,28,34,344,170,182);
            hfix58s_top_draw_text_shadow(top,48,52,"CONTINUE WATCHING?",2,242,248,255);
            hfix58s_top_rect565(top,48,78,86,2,
                g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);
            hfix58s_top_draw_text_shadow(top,48,96,title,1,220,234,248);
            hfix58s_top_draw_text_shadow(top,48,119,time_line,1,158,184,210);
            hfix58s_top_draw_text_shadow(top,48,141,"YOUR BOOKMARK IS READY",1,126,205,162);
            mivf8_top_progress(top,48,164,286,saved_frame,
                g_hfix59r2_duration_ticks?
                (u32)((g_hfix59r2_duration_ticks*(u64)fps_num)/
                (30000ull*(u64)fps_den)):saved_frame+1u);
        }
        if(bot) {
            hfix58_rect565(bot,0,0,320,240,2,5,11);
            mivf8_bottom_panel(bot,18,24,284,176);
            hfix58_draw_text_shadow(bot,36,42,"PLAYBACK CHOICE",1,238,246,255);
            hfix58_rect565(bot,36,56,64,2,
                g_mivf_theme_r,g_mivf_theme_g,g_mivf_theme_b);
            mivf8_bottom_focus_row(bot,36,80,248,"RESUME","SAVED POSITION",selected==0);
            mivf8_bottom_focus_row(bot,36,112,248,"START OVER","FROM BEGINNING",selected==1);
            hfix58_draw_text_shadow(bot,36,158,
                selected==0?"CONTINUE FROM YOUR BOOKMARK":"KEEP BOOKMARK AND PLAY FROM START",
                1,142,166,194);
            mivf8_bottom_footer(bot,"A SELECT","B START OVER");
        }
        gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();
    }
    return false;
}

static int play(void) {
    /*
        Audit finding (audit_correction_20260718_081500): nothing
        previously cleared a leftover alert when a new play() attempt
        began. hfix58_alert_set()'s messages live for a fixed 180-frame
        window regardless of what happens next -- if a user re-selected
        a valid file within roughly 3 seconds of seeing a prior file's
        error message, that stale message could still be on-screen
        (hfix58_draw_alert() is called from playback UI, not just the
        browser) for the first moments of the new, successfully-playing
        file. Safe to clear unconditionally here: nothing has rendered
        a frame yet at this point, so if this attempt also fails, the
        pending-error path below sets a fresh message with no visible
        flicker; if it succeeds, the stale message is correctly gone
        before real playback begins.
    */
    hfix58_alert_clear();

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

    g_hfix59r2_video_fps_num = v.fpsn ? v.fpsn : 30;
    g_hfix59r2_video_fps_den = v.fpsd ? v.fpsd : 1;

    if (ha) {
        audio_init_from_stream(&a);
        audio_rate_sync_reset();
        /* hfix77: size the video-delay ring for this file's fps, then apply the
           A/V SYNC setting (fps globals above are already set, so
           av_sync_offset_apply can compute the video-delay depth). */
        video_delay_alloc(g_hfix59r2_video_fps_num, g_hfix59r2_video_fps_den);
        av_sync_offset_apply(g_mivf_settings.audio_offset_ms);
    } else {
        printf("no audio stream\n");
    }

    g_hfix58f_media_end_offset = 0;

    hfix58s_subtitles_load_for_video(MIVF_PATH);
    hfix60_chapters_load(MIVF_PATH, v.fpsn, v.fpsd);
    mivf_customization_on_dashboard_enter(MIVF_PATH);

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
        video_delay_free();
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
            video_delay_free();
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

    /* Reset per-playback-session perf counters so max values from
       a previous file don't mislead the debug overlay. */
    hfix58_perf_diag_reset();
    avsync_reset_startup();
    g_audio_submit_diag_count = 0; /* HFIX86: re-log submitted-buffer content for each new file */

    /* HFIX66: a DVD-style menu (if one ran before this play()) sets a one-shot
       launch directive that takes priority over the normal resume prompt --
       the menu already asked/decided, so re-prompting here would be
       redundant. MIVF_LAUNCH_DEFAULT (normal browser selection, or no menu
       sidecar) falls through to the existing unmodified prompt behavior. */
    if (g_mivf_launch_mode == MIVF_LAUNCH_CHAPTER) {
        printf("idx: launch chapter index=%d chapters=%d\n", g_mivf_launch_chapter_index, g_mivf_chapters_count);
        if (g_mivf_launch_chapter_index >= 0 && g_mivf_launch_chapter_index < g_mivf_chapters_count) {
            hfix58j_request_absolute_seek(g_mivf_chapters[g_mivf_launch_chapter_index].frame);
            (void)hfix58f_execute_pending_seek(
                &stream, f, &v, &m2y0, &m2y0_prev, &m2y0_have_prev,
                frame, prev, fsz, &have_prev, &hfix51c_last_direct_yuv,
                &shown, &next_frame_tick, frame_ticks_abs);
        }
    } else if (g_mivf_launch_mode == MIVF_LAUNCH_RESUME) {
        MivfBookmark bookmark;
        if (MIVF_BookmarkLoad(MIVF_PATH, &bookmark) &&
            bookmark.video_path[0] &&
            !strcmp(bookmark.video_path, MIVF_PATH) &&
            bookmark.frame > 0) {
            hfix58j_request_absolute_seek(bookmark.frame);
            (void)hfix58f_execute_pending_seek(
                &stream, f, &v, &m2y0, &m2y0_prev, &m2y0_have_prev,
                frame, prev, fsz, &have_prev, &hfix51c_last_direct_yuv,
                &shown, &next_frame_tick, frame_ticks_abs);
        }
    } else if (g_mivf_launch_mode == MIVF_LAUNCH_START_OVER) {
        /* Play from the beginning; the menu already chose this, so the
           bookmark (if any) is left untouched and the prompt is skipped. */
    } else if (g_mivf_settings.resume_enabled) {
        MivfBookmark bookmark;
        if (MIVF_BookmarkLoad(MIVF_PATH, &bookmark) &&
            bookmark.video_path[0] &&
            !strcmp(bookmark.video_path, MIVF_PATH) &&
            bookmark.frame > 0) {
            printf("resume prompt: found bookmark frame=%lu\n", (unsigned long)bookmark.frame);

            if (hfix64_resume_prompt(bookmark.frame, v.fpsn, v.fpsd)) {
                printf("resume prompt: resume accepted frame=%lu\n", (unsigned long)bookmark.frame);
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
            } else {
                printf("resume prompt: start over selected; clearing bookmark\n");
                MIVF_BookmarkClear(MIVF_PATH);
            }
        }
    }

    /* One-shot: never let a menu-driven launch mode leak into the next file
       (auto-advance, or a later normal browser selection). */
    g_mivf_launch_mode = MIVF_LAUNCH_DEFAULT;
    g_mivf_launch_chapter_index = -1;

    wait_stream_prebuffer(&stream);

    /* Reset per-playback feature state. */
    g_mivf_ab_a = MIVF_AB_UNSET;
    g_mivf_ab_b = MIVF_AB_UNSET;
    g_mivf_ab_state = 0;
    g_mivf_play_reached_eof = false;
    g_mivf_sleep_fired = false;
    /* Defensive: a checkpoint request from a *previous* title's
       suspend/sleep/exit hook should never carry over and trigger an
       immediate, position-zero save for a title that hasn't played a
       single frame yet. */
    g_mivf_bookmark_checkpoint_requested = false;
    /* Session-only touch lock: never carry a locked/held state over from
       a previous title into a fresh play() session. */
    g_mivf_touch_locked = false;
    g_mivf_touch_lock_hold_frames = 0;
    g_mivf_touch_lock_gesture_fired = false;
    g_mivf_next_periodic_checkpoint_tick = svcGetSystemTick() +
        (u64)SYSCLOCK_ARM11 * MIVF_PERIODIC_CHECKPOINT_INTERVAL_SEC;
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

        /* Consume any lifecycle checkpoint request set by aptHook
           (ONSUSPEND/ONSLEEP/ONEXIT). This is the only place that
           performs the actual file write -- a context already proven
           safe for bookmark I/O, since the pre-existing post-loop save
           below has always run from here. Mirrors that save's exact
           guard so a checkpoint never fires with resume disabled or
           after the title already reached EOF. */
        if (g_mivf_bookmark_checkpoint_requested) {
            g_mivf_bookmark_checkpoint_requested = false;
            mivf_bookmark_checkpoint_if_eligible(shown);
        }

        /* Periodic checkpoint: bounds data loss from a crash or hang,
           which never reaches any lifecycle hook. Only while actively
           playing -- a paused position isn't changing, so there's
           nothing new to protect until playback resumes. */
        if (g_media_ctl.state == STATE_PLAYING &&
            svcGetSystemTick() >= g_mivf_next_periodic_checkpoint_tick) {
            g_mivf_next_periodic_checkpoint_tick = svcGetSystemTick() +
                (u64)SYSCLOCK_ARM11 * MIVF_PERIODIC_CHECKPOINT_INTERVAL_SEC;
            mivf_bookmark_checkpoint_if_eligible(shown);
        }

        u32 h_keys_down = hidKeysDown();
        u32 h_keys_held = hidKeysHeld();
        u32 h_keys_up = hidKeysUp();
        /* Don't evaluate the lock gesture while a keyboard-driven modal
           with its own L/R meaning is open (e.g. the theme picker's hue
           nudge) -- avoids an accidental toggle from unrelated L/R use.
           Touch is already inert in those modals regardless of lock
           state, so nothing is lost by skipping the check here. */
        if (!g_hfix59r3_settings_visible && !g_hfix62_help_visible &&
            !g_mivf_theme_picker.active && !g_mivf_cvd_picker.active &&
            !g_mivf_transport_picker.active) {
            hfix_touch_lock_update(h_keys_held);
        }
#ifdef MIVF_SHOWCASE_FULL
        mivf_showcase_tick();
        g_mivf_showcase.current_loop = MIVF_SC_LOOP_PLAYBACK;
        mivf_showcase_synth_key(&h_keys_down);
        mivf_showcase_cancel_check(h_keys_down, h_keys_held);
#ifdef MIVF_SHOWCASE_CAPTURE
        mivf_showcase_maybe_capture();
#endif
        if (g_mivf_showcase.active && g_mivf_showcase.stage == MIVF_SC_RESUME) {
            /* Save a Showcase-scoped bookmark against the demo project's own
               path (isolated from any real movie's bookmark simply by the
               demo project having a distinct filename -- MIVF_BookmarkSave/
               Load key off the video path, not a separate namespace), then
               exit playback the same way a real B/START press would. */
            MIVF_BookmarkSave(g_mivf_showcase.demo_project_path, shown);
            h_keys_down |= KEY_START;
        }
#endif

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

            hfix59r3_handle_settings_menu(settings_keys, h_keys_held);
            hfix59r3_tick_idle(hfix59r3_activity);
        } else if (g_hfix62_help_visible) {
            hfix62_handle_help_menu(h_keys_down);
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
        if (page_wait_us > g_perf_page_us_max) g_perf_page_us_max = page_wait_us;

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
            /* Track longest gap between audio queue calls. */
            if (g_perf_audio_gap_tick) {
                u64 gap_ms = ticks_to_us(svcGetSystemTick() - g_perf_audio_gap_tick) / 1000u;
                if (gap_ms > g_perf_audio_gap_ms_max) g_perf_audio_gap_ms_max = gap_ms;
            }
            g_perf_audio_gap_tick = svcGetSystemTick();
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
        if (parse_us > g_perf_decode_us_max) g_perf_decode_us_max = parse_us;

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
            if (blit_us > g_perf_blit_us_max) g_perf_blit_us_max = blit_us;

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

    printf("lifecycle: play() loop exited, starting cleanup\n");

    mivf_customization_on_dashboard_exit();

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
    printf("lifecycle: play() calling mivf_stream_close (stops/joins reader thread)\n");
    mivf_stream_close(&stream);
    printf("lifecycle: play() mivf_stream_close returned\n");
    hfix52a_y2r_shutdown();
    audio_shutdown();
    video_delay_free();
    hfix58s_subtitles_unload();

    if (g_mivf_settings.resume_enabled && !g_mivf_play_reached_eof) {
        MIVF_BookmarkSave(MIVF_PATH, shown);
    } else {
        MIVF_BookmarkClear(MIVF_PATH);
    }
    /* Watch-state records here regardless of resume_enabled -- this is
       the one authoritative place a real EOF (reached_eof=true) gets
       turned into a WATCHED record, independent of the bookmark's own
       (unrelated) resume_enabled gate. */
    hfix_watchstate_finish(shown, g_mivf_play_reached_eof);

    fclose(f);

    return 0;
}

/* ------------------------------------------------------------------------- */
/* Main                                                                       */
/* ------------------------------------------------------------------------- */

int main(void) {
    u64 lifecycle_start_tick = svcGetSystemTick();

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
#ifdef MIVF_SHOWCASE_FULL
    mivf_showcase_activate();
#endif

    /* First log write of the run -- ticks_to_us() elapsed since main() started
       covers gfxInitDefault/ptmuInit/aptInit/gspLcdInit above (the log file
       itself isn't open yet during those, but the tick delta still captures
       their cost). */
    printf("lifecycle: startup t+%llu us -- gfx/ptmu/apt/gsp init done, log opened\n",
        (unsigned long long)ticks_to_us(svcGetSystemTick() - lifecycle_start_tick));

    app_audio_system_init();

    printf("lifecycle: startup t+%llu us -- audio system init done\n",
        (unsigned long long)ticks_to_us(svcGetSystemTick() - lifecycle_start_tick));

    MIVF_SettingsInit(&g_mivf_settings);
    MIVF_SettingsLoad(&g_mivf_settings);
    MIVF_SettingsClamp(&g_mivf_settings);
    /* HFIX64: the playback stats overlay is no longer user-facing.  Force the
       setting off at runtime so older settings.ini files cannot keep drawing it. */
    g_mivf_settings.debug_overlay_enabled = false;
    g_mivf_settings_loaded = true;
    g_mivf_brightness_active = 5u;
    aptHook(&g_mivf_apt_hook, hfix59r3_apt_hook, NULL);
    hfix59r3_apply_runtime_settings();
    GSPLCD_PowerOnAllBacklights();
    hfix59r3_apply_screen_brightness(false);

    printf("lifecycle: startup t+%llu us -- settings loaded, apt hook registered, entering main loop\n",
        (unsigned long long)ticks_to_us(svcGetSystemTick() - lifecycle_start_tick));

    /* HFIX58D: scrubbed full bottom-console printf statement. */
    /* HFIX58D: scrubbed full bottom-console printf statement. */

    /* MIVF_PHASE2_RETURN_TO_MENU_V1
       This flag is deliberately local to the outer application loop: it is
       launch-origin policy, not playback state. play() and all of its decode,
       seek, NDSP, and timing behavior remain unchanged. */
    bool playback_launched_from_menu = false;

    while (aptMainLoop()) {
        /*
            HFIX58A_R5_BROWSER_BEFORE_PLAY:
            play() is no-argument on this branch and reads MIVF_PATH.
            MIVF_PATH is redirected to g_hfix58_selected_media above.
        */
        if (!g_hfix58_has_selected_media) {
            printf("lifecycle: entering file browser\n");
            bool browser_r = hfix58_file_browser_select(g_hfix58_selected_media, sizeof(g_hfix58_selected_media));
            printf("lifecycle: file browser returned (%s)\n", browser_r ? "selected" : "exit");

            if (!browser_r) {
                break;
            }

            g_hfix58_has_selected_media = true;
            /* A fresh browser selection has no DVD-menu return origin. */
            playback_launched_from_menu = false;
        }

        if (hfix58_media_kind(MIVF_PATH) == HFIX58_MEDIA_MOFLEX) {
            printf("lifecycle: entering moflex playback: %s\n", MIVF_PATH);
            MoflexResult result = play_moflex_selected_media(MIVF_PATH);
            printf("lifecycle: moflex playback returned (%d)\n", (int)result);

            if (result == MOFLEX_QUIT_EXIT) {
                break;
            }

            g_hfix58_has_selected_media = false;
            continue;
        }

        /* HFIX66: a ".menu.ini" sidecar routes through the DVD-style menu
           instead of straight into play(). Invalid/missing sidecar always
           falls back to normal playback below, unchanged. */
        if (mivf_menu_exists_for_movie(MIVF_PATH)) {
            MivfMenu menu;

            if (mivf_menu_load_for_movie(MIVF_PATH, &menu)) {
                mivf_customization_on_dashboard_enter(MIVF_PATH);
#ifdef MIVF_SHOWCASE_FULL
                printf("capture: customization active=%d path=%s\n",
                    mivf_customization_active_for_menu() ? 1 : 0, MIVF_PATH);
#endif
                printf("lifecycle: entering dvd-style menu: %s\n", MIVF_PATH);
                MivfMenuResult mr = mivf_menu_run(&menu);
                printf("lifecycle: dvd-style menu returned (%d)\n", (int)mr);

                if (mr == MIVF_MENU_RESULT_BACK) {
                    mivf_customization_on_dashboard_exit();
                    playback_launched_from_menu = false;
                    g_hfix58_has_selected_media = false;
                    continue;
                }
                /* MIVF_MENU_RESULT_PLAY: g_mivf_launch_mode is already set
                   (START_OVER / RESUME / CHAPTER). Remember the origin so
                   normal playback cleanup returns to this movie's menu. The
                   existing hfix81 globals restore root/chapter selection. */
                playback_launched_from_menu = true;
            } else {
                printf("lifecycle: invalid/unreadable menu.ini, falling back to normal playback: %s\n", MIVF_PATH);
            }
        }

        printf("lifecycle: entering play(): %s\n", MIVF_PATH);
        int r = play();
        printf("lifecycle: play() returned (%d)\n", r);

        /*
            play()'s early-exit codes (-1..-7) were previously silent to
            the user: consoleInit() is disabled (see below), so every
            diagnostic printf() above was invisible on real hardware,
            and the caller here treated any negative return exactly
            like a normal end-of-playback -- a bad file just bounced
            back to the browser with no explanation. Recorded as a
            *pending* message rather than shown immediately, because
            hfix58_file_browser_select() unconditionally clears any
            active alert the instant it's entered (its own first line)
            -- setting it here would just be wiped before the browser
            ever renders a frame. See where g_mivf_pending_error_alert
            is consumed, right after that clear. Grouped into the three
            distinctions a user can actually act on; the finer-grained
            internal codes (bad header vs. stream read error vs. no
            video stream, etc.) aren't meaningfully different to them.
        */
        if (r < 0) {
            switch (r) {
                case -1:
                    snprintf(g_mivf_pending_error_alert, sizeof(g_mivf_pending_error_alert), "COULD NOT OPEN FILE");
                    break;
                case -6:
                case -7:
                    snprintf(g_mivf_pending_error_alert, sizeof(g_mivf_pending_error_alert), "NOT ENOUGH MEMORY TO PLAY THIS FILE");
                    break;
                default:
                    snprintf(g_mivf_pending_error_alert, sizeof(g_mivf_pending_error_alert), "FILE APPEARS CORRUPTED OR UNSUPPORTED");
                    break;
            }
        }

        if (r == 1) {
            break;
        }

        /* Menu-origin playback returns to the same title's DVD menu after
           either manual exit or natural EOF. Consume the flag before looping
           so a later browser/direct launch cannot inherit the return policy.
           This check intentionally precedes auto-advance: a disc menu owns its
           post-playback navigation, while browser-origin playback retains the
           existing playlist behavior below. */
        if (playback_launched_from_menu) {
            playback_launched_from_menu = false;
            g_hfix58_has_selected_media = true;
            continue;
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

    printf("lifecycle: main() outer loop exited, shutting down\n");

    MIVF_SettingsSave(&g_mivf_settings);
    audio_shutdown();
    app_audio_system_shutdown();
    gspLcdExit();
    printf("lifecycle: calling aptExit\n");
    aptExit();
    printf("lifecycle: aptExit returned\n");
    ptmuExit();
    mivf_diag_close();
    printf("lifecycle: main() returning cleanly\n");
    mivf_log_close();
    gfxExit();
    return 0;
}
