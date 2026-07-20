/*
    MIVF_CUSTOMIZATION_V1 (Phase C vertical slice)

    Implements the manifest parser, static asset pool, and resolver described
    in mivf_customization.h. See mivf_customization_gui_20260716/ for the full
    design package. This file has zero dependency on main.c's internals
    beyond standard libctru/libc -- it never calls a static main.c function
    and never reads g_mivf_settings, g_media_ctl, or any input state
    directly. main.c feeds it a video_path and the current color-vision mode
    (a plain u32) and consumes resolved, read-only data back.

    Two small pieces of logic are intentionally DUPLICATED from main.c rather
    than called into, both documented at the point of duplication:
      1. The sidecar-path convention (hfix60_make_sidecar_path's algorithm).
      2. The color-vision-mode accent substitution table
         (mivf_theme_set_rgb's switch, main.c:2867-2877).
    Both are small, stable, already-frozen pieces of logic. Duplicating them
    keeps this module able to resolve colors/paths without mutating global
    theme state or reaching into main.c's statics -- see
    CUSTOMIZATION_PRECEDENCE.md for why a per-dashboard manifest accent must
    never bleed into every other screen's colors via the global palette.
*/

#include "mivf_customization.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---- real hitbox-derived asset caps, from main.c's g_mivf_touch_layouts[0]
   (MIVF_TRANSPORT_STYLE_CINEMATIC), main.c:2332 --
   {{34,107,64,60},{123,97,74,78},{222,107,64,60}} -- main[2]=Forward (64x60),
   main[1]=Play/Pause (74x78). Real bottom-screen dashboard canvas is 320x240
   (c25_premiere's own hfix58_rect565(f,0,0,320,240,...) background fill),
   NOT the 400x240 this project's own Phase B design docs assumed before this
   integration read the real function body -- corrected here. */
#define MIVF_CUST_BG_W 320
#define MIVF_CUST_BG_H 240
#define MIVF_CUST_FF_W 64
#define MIVF_CUST_FF_H 60
#define MIVF_CUST_PP_W 74
#define MIVF_CUST_PP_H 78

/* Real rendered rectangle for the root DVD-menu's Back row: ROW_W/ROW_H
   inside mivf_menu_draw_button_top() (main.c:13416), centered at
   cx = TOP_W/2 for every button, Y position varying by button index --
   that per-index Y is already correctly computed by the existing render
   loop (mivf_menu_draw_top_root, main.c:13462-13466); only the *size* is a
   fixed constant, which is what this module needs, since main.c passes the
   real y in at call time. This is a real, hardware-verified constant, not
   a guess -- see BACK_RENDER_GEOMETRY.md. */
#define MIVF_CUST_BACK_W 222
#define MIVF_CUST_BACK_H 20

#define MIVF_CUST_ASSET_MAGIC "MVCA"
#define MIVF_CUST_ASSET_VERSION 1u
#define MIVF_CUST_MANIFEST_MAX_BYTES 4096
#define MIVF_CUST_NAME_MAX 48
#define MIVF_CUST_PATH_MAX 512

enum {
    MIVF_CUST_FALLBACK_NONE = 0,
    MIVF_CUST_FALLBACK_NO_MANIFEST,
    MIVF_CUST_FALLBACK_BAD_SCHEMA,
    MIVF_CUST_FALLBACK_ASSET_MISSING,
    MIVF_CUST_FALLBACK_ASSET_BAD_SIZE,
    MIVF_CUST_FALLBACK_ASSET_BAD_MAGIC
};

/* ---- static pool: fixed, no allocation, no allocation-failure mode ---- */
static u16 g_cust_bg_pixels[MIVF_CUST_BG_W * MIVF_CUST_BG_H];
static bool g_cust_bg_valid;

static u16 g_cust_ff_pixels[MIVF_CTRL_STATE_COUNT][MIVF_CUST_FF_W * MIVF_CUST_FF_H];
static u8  g_cust_ff_mask[MIVF_CTRL_STATE_COUNT][(MIVF_CUST_FF_W * MIVF_CUST_FF_H + 7) / 8];
static bool g_cust_ff_valid[MIVF_CTRL_STATE_COUNT];

/* Real hitbox is g_mivf_touch_layouts[CINEMATIC].main[0] = {34,107,64,60} --
   identical WxH to Forward's main[2]={222,107,64,60}, mirrored position.
   Same MIVF_CUST_FF_W/H constants apply; a separate pool is still needed
   since Rewind and Forward are independently-loadable assets. */
static u16 g_cust_rw_pixels[MIVF_CTRL_STATE_COUNT][MIVF_CUST_FF_W * MIVF_CUST_FF_H];
static u8  g_cust_rw_mask[MIVF_CTRL_STATE_COUNT][(MIVF_CUST_FF_W * MIVF_CUST_FF_H + 7) / 8];
static bool g_cust_rw_valid[MIVF_CTRL_STATE_COUNT];

static u16 g_cust_pp_pixels[MIVF_CTRL_STATE_COUNT][MIVF_CUST_PP_W * MIVF_CUST_PP_H];
static u8  g_cust_pp_mask[MIVF_CTRL_STATE_COUNT][(MIVF_CUST_PP_W * MIVF_CUST_PP_H + 7) / 8];
static bool g_cust_pp_valid[MIVF_CTRL_STATE_COUNT];

static u16 g_cust_back_pixels[MIVF_CTRL_STATE_COUNT][MIVF_CUST_BACK_W * MIVF_CUST_BACK_H];
static u8  g_cust_back_mask[MIVF_CTRL_STATE_COUNT][(MIVF_CUST_BACK_W * MIVF_CUST_BACK_H + 7) / 8];
static bool g_cust_back_valid[MIVF_CTRL_STATE_COUNT];

/* [0]=FAST_FORWARD, [1]=PLAY_PAUSE, [2]=MOVIE_MENU_BACK, [3]=REWIND, indexed by MivfCtrlVisualState */
static bool g_cust_has_fill[4][MIVF_CTRL_STATE_COUNT];
static u8   g_cust_fill[4][MIVF_CTRL_STATE_COUNT][3];
static bool g_cust_has_outline[4][MIVF_CTRL_STATE_COUNT];
static u8   g_cust_outline[4][MIVF_CTRL_STATE_COUNT][3];

/* C.6: position is state-independent, one slot per control (same [0]=FF,
   [1]=PP, [2]=BACK (unused, position is Premiere-transport-only), [3]=RW
   indexing as the arrays above, for consistency). */
static bool g_cust_has_position[4];
static int  g_cust_dx[4], g_cust_dy[4];

static bool g_cust_has_accent, g_cust_has_palette_outline;
static u8 g_cust_accent[3], g_cust_palette_outline[3];

static bool g_cust_manifest_loaded;
static char g_cust_loaded_for_video[MIVF_CUST_PATH_MAX];
static MivfCustomizationStats g_cust_stats;

/* ---- small local duplicates (see file header for why) ---- */

static u32 mivf_cust_le32(const u8 *p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}
static u16 mivf_cust_le16(const u8 *p) {
    return (u16)(p[0] | (p[1] << 8));
}

static void mivf_cust_make_sidecar_path(char *out, size_t out_sz, const char *base_path, const char *ext) {
    char *dot;
    if (!out || out_sz == 0) return;
    out[0] = 0;
    if (!base_path || !ext) return;
    snprintf(out, out_sz, "%s", base_path);
    dot = strrchr(out, '.');
    if (dot && dot > out) {
        snprintf(dot, out_sz - (size_t)(dot - out), "%s", ext);
    } else {
        size_t len = strlen(out);
        if (len + strlen(ext) < out_sz) strcat(out, ext);
    }
}

/* Resolves a bare asset name (never a path -- see CUSTOMIZATION_RISK_REGISTER.md)
   to <dir-of-video_path>/<name>.mivfasset. Rejects any name containing a path
   separator, "..", or a colon outright. */
static bool mivf_cust_resolve_asset_path(char *out, size_t out_sz, const char *video_path, const char *bare_name) {
    const char *slash, *bslash, *last_sep;
    size_t dir_len;

    out[0] = 0;
    if (!bare_name || !bare_name[0]) return false;
    if (strchr(bare_name, '/') || strchr(bare_name, '\\') ||
        strchr(bare_name, ':') || strstr(bare_name, "..")) {
        return false;
    }

    slash = strrchr(video_path, '/');
    bslash = strrchr(video_path, '\\');
    last_sep = slash;
    if (bslash && (!last_sep || bslash > last_sep)) last_sep = bslash;

    dir_len = last_sep ? (size_t)(last_sep - video_path + 1) : 0;
    if (dir_len >= out_sz) return false;

    memcpy(out, video_path, dir_len);
    out[dir_len] = 0;

    if (dir_len + strlen(bare_name) + strlen(".mivfasset") >= out_sz) return false;
    strcat(out, bare_name);
    strcat(out, ".mivfasset");
    return true;
}

/* Duplicate of main.c's mivf_theme_set_rgb() CVD substitution table
   (main.c:2867-2877), deliberately not called into -- see file header. Pure
   function: never touches g_mivf_theme_palette or any global theme state. */
static void mivf_cust_apply_cvd(u8 base_r, u8 base_g, u8 base_b, u32 cvd_mode,
                                 u8 *out_r, u8 *out_g, u8 *out_b) {
    switch (cvd_mode) {
        case 1: *out_r = 40;  *out_g = 170; *out_b = 245; return; /* PROTAN */
        case 2: *out_r = 65;  *out_g = 135; *out_b = 245; return; /* DEUTAN */
        case 3: *out_r = 225; *out_g = 80;  *out_b = 205; return; /* TRITAN */
        case 4: { /* MONOCHROME */
            int y = ((int)base_r * 54 + (int)base_g * 183 + (int)base_b * 19) >> 8;
            if (y < 150) y = 205;
            *out_r = *out_g = *out_b = (u8)y;
            return;
        }
        case 5: *out_r = 255; *out_g = 224; *out_b = 70; return; /* HIGH_CONTRAST */
        default: *out_r = base_r; *out_g = base_g; *out_b = base_b; return; /* STANDARD */
    }
}

/* ---- asset loading, strict exact-size-or-reject (matches .chapthumbs'
   real precedent at main.c:12452-12518) ---- */

static bool mivf_cust_load_asset(const char *path, int expect_w, int expect_h,
                                  u16 *out_pixels, u8 *out_mask, size_t mask_bytes,
                                  int *reason_out) {
    FILE *fp;
    u8 hdr[12];
    u32 version;
    u16 w, h;
    size_t px_need, mask_need;

    *reason_out = MIVF_CUST_FALLBACK_ASSET_MISSING;

    fp = fopen(path, "rb");
    if (!fp) return false;

    if (fread(hdr, 1, sizeof(hdr), fp) != sizeof(hdr) ||
        memcmp(hdr, MIVF_CUST_ASSET_MAGIC, 4) != 0) {
        fclose(fp);
        *reason_out = MIVF_CUST_FALLBACK_ASSET_BAD_MAGIC;
        return false;
    }

    version = mivf_cust_le32(hdr + 4);
    w = mivf_cust_le16(hdr + 8);
    h = mivf_cust_le16(hdr + 10);

    if (version != MIVF_CUST_ASSET_VERSION || w != (u16)expect_w || h != (u16)expect_h) {
        fclose(fp);
        *reason_out = MIVF_CUST_FALLBACK_ASSET_BAD_SIZE;
        return false;
    }

    px_need = (size_t)w * (size_t)h * sizeof(u16);
    /* Background assets pass out_mask=NULL/mask_bytes=0 (no mask section in
       the file at all -- see mivf_make_dashboard_bg.py). Only control
       assets (out_mask != NULL) have a mask to size-check and read; doing
       this check unconditionally previously rejected every valid
       no-mask background file (caught by test_mivf_customization.c). */
    mask_need = out_mask ? (size_t)((w * h + 7) / 8) : 0;
    if (out_mask && mask_need > mask_bytes) {
        fclose(fp);
        *reason_out = MIVF_CUST_FALLBACK_ASSET_BAD_SIZE;
        return false;
    }

    /* File and framebuffer are both little-endian RGB565 on this
       platform/toolchain, stored verbatim -- same convention as .chapthumbs. */
    if (fread((u8 *)out_pixels, 1, px_need, fp) != px_need) {
        fclose(fp);
        *reason_out = MIVF_CUST_FALLBACK_ASSET_BAD_SIZE;
        return false;
    }

    if (out_mask && fread(out_mask, 1, mask_need, fp) != mask_need) {
        fclose(fp);
        *reason_out = MIVF_CUST_FALLBACK_ASSET_BAD_SIZE;
        return false;
    }

    fclose(fp);
    *reason_out = MIVF_CUST_FALLBACK_NONE;
    return true;
}

/* ---- manifest parsing: versioned KEY=VALUE, see CUSTOMIZATION_SCHEMA.md ---- */

typedef struct {
    bool has_dashboard_bg;
    char dashboard_bg_name[MIVF_CUST_NAME_MAX];
    bool has_accent;
    u8 accent[3];
    bool has_outline;
    u8 outline[3];

    bool ff_has_underlay[MIVF_CTRL_STATE_COUNT];
    char ff_underlay_name[MIVF_CTRL_STATE_COUNT][MIVF_CUST_NAME_MAX];
    bool ff_has_fill[MIVF_CTRL_STATE_COUNT];
    u8 ff_fill[MIVF_CTRL_STATE_COUNT][3];
    bool ff_has_outline[MIVF_CTRL_STATE_COUNT];
    u8 ff_outline[MIVF_CTRL_STATE_COUNT][3];
    /* C.6: position is state-independent (one offset per control, not per
       Idle/Focused) -- Premiere-only, see mivf_customization_resolve_position(). */
    bool ff_has_position;
    int ff_dx, ff_dy;

    bool pp_has_underlay[MIVF_CTRL_STATE_COUNT];
    char pp_underlay_name[MIVF_CTRL_STATE_COUNT][MIVF_CUST_NAME_MAX];
    bool pp_has_fill[MIVF_CTRL_STATE_COUNT];
    u8 pp_fill[MIVF_CTRL_STATE_COUNT][3];
    bool pp_has_outline[MIVF_CTRL_STATE_COUNT];
    u8 pp_outline[MIVF_CTRL_STATE_COUNT][3];
    bool pp_has_position;
    int pp_dx, pp_dy;

    /* MIVF_CTRL_MOVIE_MENU_BACK -- the real DVD-menu root screen's Back
       row, a deliberately distinct context from the two transport controls
       above (see mivf_customization.h's enum comment). Same CONTROL= block
       syntax, different control-name token ("BACK"), reusing the identical
       parser/validation machinery rather than a parallel flat-key scheme. */
    bool back_has_underlay[MIVF_CTRL_STATE_COUNT];
    char back_underlay_name[MIVF_CTRL_STATE_COUNT][MIVF_CUST_NAME_MAX];
    bool back_has_fill[MIVF_CTRL_STATE_COUNT];
    u8 back_fill[MIVF_CTRL_STATE_COUNT][3];
    bool back_has_outline[MIVF_CTRL_STATE_COUNT];
    u8 back_outline[MIVF_CTRL_STATE_COUNT][3];

    bool rw_has_underlay[MIVF_CTRL_STATE_COUNT];
    char rw_underlay_name[MIVF_CTRL_STATE_COUNT][MIVF_CUST_NAME_MAX];
    bool rw_has_fill[MIVF_CTRL_STATE_COUNT];
    u8 rw_fill[MIVF_CTRL_STATE_COUNT][3];
    bool rw_has_outline[MIVF_CTRL_STATE_COUNT];
    u8 rw_outline[MIVF_CTRL_STATE_COUNT][3];
    bool rw_has_position;
    int rw_dx, rw_dy;
} MivfCustParsedManifest;

static bool mivf_cust_parse_rgb(const char *v, u8 *out) {
    int r, g, b;
    if (sscanf(v, "%d,%d,%d", &r, &g, &b) != 3) return false;
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) return false;
    out[0] = (u8)r; out[1] = (u8)g; out[2] = (u8)b;
    return true;
}

/* C.6: dx/dy are clamped to a generous but bounded range at parse time --
   the authoritative clamp against each control's real screen geometry
   happens once, in mivf_c25_premiere_controls, using real per-control
   bounds; this is just a sanity backstop against a corrupted/hand-edited
   manifest (matches mivf_cust_parse_rgb's own "malformed value skips just
   this field" philosophy, never aborts the whole manifest). */
static bool mivf_cust_parse_xy(const char *v, int *dx, int *dy) {
    int x, y;
    if (sscanf(v, "%d,%d", &x, &y) != 2) return false;
    if (x < -160 || x > 160 || y < -120 || y > 120) return false;
    *dx = x; *dy = y;
    return true;
}

static void mivf_cust_trim(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' ')) s[--n] = 0;
}

/* Strict: unknown keys ignored, malformed values skip that one field only,
   never abort the whole parse. Matches MIVF_SettingsLoad's own philosophy
   (mivf_settings.c:276-345). Returns false only if the schema line itself is
   missing/wrong -- the one case that rejects the entire manifest. Not
   reentrant (uses strtok, not strtok_r) -- fine, this parser only ever runs
   from mivf_customization_on_dashboard_enter, never nested/threaded. */
static bool mivf_cust_parse_manifest_text(char *buf, MivfCustParsedManifest *out) {
    char *line;
    bool schema_ok = false;
    MivfCtrlId cur_ctrl = MIVF_CTRL_COUNT; /* "no block open" sentinel */
    MivfCtrlVisualState cur_state = MIVF_CTRL_STATE_IDLE;
    bool in_block = false;

    memset(out, 0, sizeof(*out));

    line = strtok(buf, "\n");
    while (line) {
        char *eq, *key, *val;
        mivf_cust_trim(line);

        /* MIVF_CAPTURE_THEME_CH4_FIX_V1: tolerate a UTF-8 BOM and harmless leading whitespace. */
        if ((unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB &&
            (unsigned char)line[2] == 0xBF) {
            memmove(line, line + 3, strlen(line + 3) + 1);
        }
        while (*line == ' ' || *line == '\t') memmove(line, line + 1, strlen(line));

        if (line[0] == 0 || line[0] == '#') { line = strtok(NULL, "\n"); continue; }

        if (!schema_ok) {
            /* strlen("MIVFTHEME_SCHEMA=") == 17 -- do not "round up" this
               constant, an off-by-one here previously made the schema check
               always fail (caught by test_mivf_customization.c). */
            if (strncmp(line, "MIVFTHEME_SCHEMA=", 17) == 0) {
                if (strcmp(line + 17, "1") == 0) { schema_ok = true; line = strtok(NULL, "\n"); continue; }
                return false; /* unrecognized schema version: reject the whole manifest */
            }
            return false; /* first meaningful line must be the schema key */
        }

        eq = strchr(line, '=');
        if (!eq) { line = strtok(NULL, "\n"); continue; }
        *eq = 0;
        key = line;
        val = eq + 1;
        mivf_cust_trim(key);
        while (*val == ' ' || *val == '\t') val++;
        mivf_cust_trim(val);

        if (strcmp(key, "CONTROL") == 0) {
            in_block = true;
            cur_state = MIVF_CTRL_STATE_IDLE;
            if (strcmp(val, "FAST_FORWARD") == 0) cur_ctrl = MIVF_CTRL_FAST_FORWARD;
            else if (strcmp(val, "PLAY_PAUSE") == 0) cur_ctrl = MIVF_CTRL_PLAY_PAUSE;
            else if (strcmp(val, "BACK") == 0) cur_ctrl = MIVF_CTRL_MOVIE_MENU_BACK;
            else if (strcmp(val, "REWIND") == 0) cur_ctrl = MIVF_CTRL_REWIND;
            else cur_ctrl = MIVF_CTRL_COUNT; /* unsupported control id in this slice: fields ignored */
        } else if (strcmp(key, "CONTROL.END") == 0) {
            in_block = false;
            cur_ctrl = MIVF_CTRL_COUNT;
        } else if (in_block && strcmp(key, "CONTROL.STATE") == 0) {
            cur_state = (strcmp(val, "FOCUSED") == 0) ? MIVF_CTRL_STATE_FOCUSED : MIVF_CTRL_STATE_IDLE;
        } else if (in_block && cur_ctrl == MIVF_CTRL_FAST_FORWARD && strcmp(key, "CONTROL.UNDERLAY") == 0) {
            snprintf(out->ff_underlay_name[cur_state], MIVF_CUST_NAME_MAX, "%s", val);
            out->ff_has_underlay[cur_state] = true;
        } else if (in_block && cur_ctrl == MIVF_CTRL_FAST_FORWARD && strcmp(key, "CONTROL.FILL") == 0) {
            out->ff_has_fill[cur_state] = mivf_cust_parse_rgb(val, out->ff_fill[cur_state]);
        } else if (in_block && cur_ctrl == MIVF_CTRL_FAST_FORWARD && strcmp(key, "CONTROL.OUTLINE") == 0) {
            out->ff_has_outline[cur_state] = mivf_cust_parse_rgb(val, out->ff_outline[cur_state]);
        } else if (in_block && cur_ctrl == MIVF_CTRL_FAST_FORWARD && strcmp(key, "CONTROL.POSITION") == 0) {
            out->ff_has_position = mivf_cust_parse_xy(val, &out->ff_dx, &out->ff_dy);
        } else if (in_block && cur_ctrl == MIVF_CTRL_PLAY_PAUSE && strcmp(key, "CONTROL.UNDERLAY") == 0) {
            snprintf(out->pp_underlay_name[cur_state], MIVF_CUST_NAME_MAX, "%s", val);
            out->pp_has_underlay[cur_state] = true;
        } else if (in_block && cur_ctrl == MIVF_CTRL_PLAY_PAUSE && strcmp(key, "CONTROL.FILL") == 0) {
            out->pp_has_fill[cur_state] = mivf_cust_parse_rgb(val, out->pp_fill[cur_state]);
        } else if (in_block && cur_ctrl == MIVF_CTRL_PLAY_PAUSE && strcmp(key, "CONTROL.OUTLINE") == 0) {
            out->pp_has_outline[cur_state] = mivf_cust_parse_rgb(val, out->pp_outline[cur_state]);
        } else if (in_block && cur_ctrl == MIVF_CTRL_PLAY_PAUSE && strcmp(key, "CONTROL.POSITION") == 0) {
            out->pp_has_position = mivf_cust_parse_xy(val, &out->pp_dx, &out->pp_dy);
        } else if (in_block && cur_ctrl == MIVF_CTRL_MOVIE_MENU_BACK && strcmp(key, "CONTROL.UNDERLAY") == 0) {
            snprintf(out->back_underlay_name[cur_state], MIVF_CUST_NAME_MAX, "%s", val);
            out->back_has_underlay[cur_state] = true;
        } else if (in_block && cur_ctrl == MIVF_CTRL_MOVIE_MENU_BACK && strcmp(key, "CONTROL.FILL") == 0) {
            out->back_has_fill[cur_state] = mivf_cust_parse_rgb(val, out->back_fill[cur_state]);
        } else if (in_block && cur_ctrl == MIVF_CTRL_MOVIE_MENU_BACK && strcmp(key, "CONTROL.OUTLINE") == 0) {
            out->back_has_outline[cur_state] = mivf_cust_parse_rgb(val, out->back_outline[cur_state]);
        } else if (in_block && cur_ctrl == MIVF_CTRL_REWIND && strcmp(key, "CONTROL.UNDERLAY") == 0) {
            snprintf(out->rw_underlay_name[cur_state], MIVF_CUST_NAME_MAX, "%s", val);
            out->rw_has_underlay[cur_state] = true;
        } else if (in_block && cur_ctrl == MIVF_CTRL_REWIND && strcmp(key, "CONTROL.FILL") == 0) {
            out->rw_has_fill[cur_state] = mivf_cust_parse_rgb(val, out->rw_fill[cur_state]);
        } else if (in_block && cur_ctrl == MIVF_CTRL_REWIND && strcmp(key, "CONTROL.OUTLINE") == 0) {
            out->rw_has_outline[cur_state] = mivf_cust_parse_rgb(val, out->rw_outline[cur_state]);
        } else if (in_block && cur_ctrl == MIVF_CTRL_REWIND && strcmp(key, "CONTROL.POSITION") == 0) {
            out->rw_has_position = mivf_cust_parse_xy(val, &out->rw_dx, &out->rw_dy);
        } else if (!in_block && strcmp(key, "DASHBOARD_BG") == 0) {
            snprintf(out->dashboard_bg_name, MIVF_CUST_NAME_MAX, "%s", val);
            out->has_dashboard_bg = true;
        } else if (!in_block && strcmp(key, "PALETTE_ACCENT") == 0) {
            out->has_accent = mivf_cust_parse_rgb(val, out->accent);
        } else if (!in_block && strcmp(key, "PALETTE_OUTLINE") == 0) {
            out->has_outline = mivf_cust_parse_rgb(val, out->outline);
        }
        /* every other key (THEME_NAME, THEME_AUTHOR, CONTROL.SCALE_MODE,
           CONTROL.ANCHOR, CONTROL.OPACITY, and any future/unknown key) is
           silently ignored -- matches CUSTOMIZATION_SCHEMA.md's table. */

        line = strtok(NULL, "\n");
    }

    return schema_ok;
}

/* ---- public lifecycle API ---- */

static void mivf_cust_invalidate(void) {
    int i;
    g_cust_bg_valid = false;
    for (i = 0; i < MIVF_CTRL_STATE_COUNT; i++) {
        g_cust_ff_valid[i] = false;
        g_cust_pp_valid[i] = false;
        g_cust_back_valid[i] = false;
        g_cust_rw_valid[i] = false;
        g_cust_has_fill[0][i] = g_cust_has_fill[1][i] = g_cust_has_fill[2][i] = g_cust_has_fill[3][i] = false;
        g_cust_has_outline[0][i] = g_cust_has_outline[1][i] = g_cust_has_outline[2][i] = g_cust_has_outline[3][i] = false;
    }
    g_cust_has_position[0] = g_cust_has_position[1] = g_cust_has_position[2] = g_cust_has_position[3] = false;
    g_cust_has_accent = false;
    g_cust_has_palette_outline = false;
    g_cust_manifest_loaded = false;
    g_cust_stats.invalidation_count++;
}

void mivf_customization_on_dashboard_exit(void) {
    mivf_cust_invalidate();
    g_cust_loaded_for_video[0] = 0;
}

void mivf_customization_on_dashboard_enter(const char *video_path) {
    char manifest_path[MIVF_CUST_PATH_MAX];
    FILE *fp;
    char buf[MIVF_CUST_MANIFEST_MAX_BYTES + 1];
    size_t n;
    MivfCustParsedManifest parsed;
    u64 t0, t1;
    int s;

    if (!video_path || !video_path[0]) return;

    /* Cheap idempotency: re-entering with the same video_path this session
       does not re-parse or reload anything. */
    if (g_cust_manifest_loaded && strcmp(g_cust_loaded_for_video, video_path) == 0) {
        return;
    }

    mivf_cust_invalidate();
    g_cust_stats.last_fallback_reason = MIVF_CUST_FALLBACK_NO_MANIFEST;
    snprintf(g_cust_loaded_for_video, sizeof(g_cust_loaded_for_video), "%s", video_path);

    t0 = svcGetSystemTick();

    /* Per-project manifest only in this slice (CUSTOMIZATION_PRECEDENCE.md
       step 5) -- the global sdmc:/mivf/theme.mivftheme default is a Phase B
       "open decision" not yet resolved, so it is intentionally not read
       here rather than guessed at (see PHASE_C_ENTRY_CRITERIA.md item 3). */
    mivf_cust_make_sidecar_path(manifest_path, sizeof(manifest_path), video_path, ".mivftheme");
    if (!manifest_path[0]) return;

    fp = fopen(manifest_path, "rb");
    if (!fp) return; /* no manifest: built-in rendering, not an error */

    n = fread(buf, 1, MIVF_CUST_MANIFEST_MAX_BYTES, fp);
    fclose(fp);
    if (n == 0 || n >= MIVF_CUST_MANIFEST_MAX_BYTES) {
        g_cust_stats.last_fallback_reason = MIVF_CUST_FALLBACK_BAD_SCHEMA;
        return;
    }
    buf[n] = 0;

    if (!mivf_cust_parse_manifest_text(buf, &parsed)) {
        g_cust_stats.last_fallback_reason = MIVF_CUST_FALLBACK_BAD_SCHEMA;
        return;
    }

    g_cust_manifest_loaded = true;
    g_cust_stats.last_fallback_reason = MIVF_CUST_FALLBACK_NONE;

    g_cust_has_accent = parsed.has_accent;
    if (parsed.has_accent) memcpy(g_cust_accent, parsed.accent, 3);
    g_cust_has_palette_outline = parsed.has_outline;
    if (parsed.has_outline) memcpy(g_cust_palette_outline, parsed.outline, 3);

    if (parsed.has_dashboard_bg) {
        char asset_path[MIVF_CUST_PATH_MAX];
        int reason;
        if (mivf_cust_resolve_asset_path(asset_path, sizeof(asset_path), video_path, parsed.dashboard_bg_name)) {
            g_cust_bg_valid = mivf_cust_load_asset(asset_path, MIVF_CUST_BG_W, MIVF_CUST_BG_H,
                                                    g_cust_bg_pixels, NULL, 0, &reason);
            if (!g_cust_bg_valid) g_cust_stats.last_fallback_reason = reason;
        }
    }

    g_cust_has_position[0] = parsed.ff_has_position;
    g_cust_dx[0] = parsed.ff_dx; g_cust_dy[0] = parsed.ff_dy;
    g_cust_has_position[1] = parsed.pp_has_position;
    g_cust_dx[1] = parsed.pp_dx; g_cust_dy[1] = parsed.pp_dy;
    g_cust_has_position[3] = parsed.rw_has_position;
    g_cust_dx[3] = parsed.rw_dx; g_cust_dy[3] = parsed.rw_dy;

    for (s = 0; s < MIVF_CTRL_STATE_COUNT; s++) {
        g_cust_has_fill[0][s] = parsed.ff_has_fill[s];
        if (parsed.ff_has_fill[s]) memcpy(g_cust_fill[0][s], parsed.ff_fill[s], 3);
        g_cust_has_outline[0][s] = parsed.ff_has_outline[s];
        if (parsed.ff_has_outline[s]) memcpy(g_cust_outline[0][s], parsed.ff_outline[s], 3);

        g_cust_has_fill[1][s] = parsed.pp_has_fill[s];
        if (parsed.pp_has_fill[s]) memcpy(g_cust_fill[1][s], parsed.pp_fill[s], 3);
        g_cust_has_outline[1][s] = parsed.pp_has_outline[s];
        if (parsed.pp_has_outline[s]) memcpy(g_cust_outline[1][s], parsed.pp_outline[s], 3);

        g_cust_has_fill[2][s] = parsed.back_has_fill[s];
        if (parsed.back_has_fill[s]) memcpy(g_cust_fill[2][s], parsed.back_fill[s], 3);
        g_cust_has_outline[2][s] = parsed.back_has_outline[s];
        if (parsed.back_has_outline[s]) memcpy(g_cust_outline[2][s], parsed.back_outline[s], 3);

        g_cust_has_fill[3][s] = parsed.rw_has_fill[s];
        if (parsed.rw_has_fill[s]) memcpy(g_cust_fill[3][s], parsed.rw_fill[s], 3);
        g_cust_has_outline[3][s] = parsed.rw_has_outline[s];
        if (parsed.rw_has_outline[s]) memcpy(g_cust_outline[3][s], parsed.rw_outline[s], 3);

        if (parsed.ff_has_underlay[s]) {
            char asset_path[MIVF_CUST_PATH_MAX];
            int reason;
            if (mivf_cust_resolve_asset_path(asset_path, sizeof(asset_path), video_path, parsed.ff_underlay_name[s])) {
                g_cust_ff_valid[s] = mivf_cust_load_asset(asset_path, MIVF_CUST_FF_W, MIVF_CUST_FF_H,
                                                           g_cust_ff_pixels[s], g_cust_ff_mask[s],
                                                           sizeof(g_cust_ff_mask[s]), &reason);
                if (!g_cust_ff_valid[s]) g_cust_stats.last_fallback_reason = reason;
            }
        }
        if (parsed.pp_has_underlay[s]) {
            char asset_path[MIVF_CUST_PATH_MAX];
            int reason;
            if (mivf_cust_resolve_asset_path(asset_path, sizeof(asset_path), video_path, parsed.pp_underlay_name[s])) {
                g_cust_pp_valid[s] = mivf_cust_load_asset(asset_path, MIVF_CUST_PP_W, MIVF_CUST_PP_H,
                                                           g_cust_pp_pixels[s], g_cust_pp_mask[s],
                                                           sizeof(g_cust_pp_mask[s]), &reason);
                if (!g_cust_pp_valid[s]) g_cust_stats.last_fallback_reason = reason;
            }
        }
        if (parsed.back_has_underlay[s]) {
            char asset_path[MIVF_CUST_PATH_MAX];
            int reason;
            if (mivf_cust_resolve_asset_path(asset_path, sizeof(asset_path), video_path, parsed.back_underlay_name[s])) {
                g_cust_back_valid[s] = mivf_cust_load_asset(asset_path, MIVF_CUST_BACK_W, MIVF_CUST_BACK_H,
                                                             g_cust_back_pixels[s], g_cust_back_mask[s],
                                                             sizeof(g_cust_back_mask[s]), &reason);
                /* A malformed/missing Back asset must never invalidate the
                   dashboard's own fill/outline/underlay state resolved just
                   above in this same loop -- each control's fallback is
                   independent by construction (separate valid[] flags),
                   matching CUSTOMIZATION_SCHEMA.md's "partial override"
                   and "invalid section doesn't block the rest" rules. */
                if (!g_cust_back_valid[s]) g_cust_stats.last_fallback_reason = reason;
            }
        }
        if (parsed.rw_has_underlay[s]) {
            char asset_path[MIVF_CUST_PATH_MAX];
            int reason;
            if (mivf_cust_resolve_asset_path(asset_path, sizeof(asset_path), video_path, parsed.rw_underlay_name[s])) {
                g_cust_rw_valid[s] = mivf_cust_load_asset(asset_path, MIVF_CUST_FF_W, MIVF_CUST_FF_H,
                                                           g_cust_rw_pixels[s], g_cust_rw_mask[s],
                                                           sizeof(g_cust_rw_mask[s]), &reason);
                if (!g_cust_rw_valid[s]) g_cust_stats.last_fallback_reason = reason;
            }
        }
    }

    t1 = svcGetSystemTick();
    g_cust_stats.last_load_us = (u32)(((t1 - t0) * 1000000ULL) / SYSCLOCK_ARM11);

    /* Budget raised again to cover Rewind's two states (2 x 8,160 =
       16,320 bytes, identical size to Forward's since main[0]/main[2]
       share the same 64x60 real hitbox). Total static pool is now
       229,642 bytes (see PHASE_C1_SUMMARY.md's memory table for the full
       breakdown). Playback (dashboard+FF+PP+Rewind) and menu (Back) assets
       are NOT slot-shared in this pass -- both are small static pools that
       coexist for the lifetime of one movie's session regardless of which
       screen is currently showing. Dynamic slot-sharing between the menu
       and dashboard screens remains a real, identified, but unimplemented
       optimization -- not required for correctness here. */
    g_cust_stats.configured_budget_bytes = 260000;
    g_cust_stats.allocated_bytes =
        (u32)(sizeof(g_cust_bg_pixels) + sizeof(g_cust_ff_pixels) + sizeof(g_cust_ff_mask) +
              sizeof(g_cust_pp_pixels) + sizeof(g_cust_pp_mask) +
              sizeof(g_cust_back_pixels) + sizeof(g_cust_back_mask) +
              sizeof(g_cust_rw_pixels) + sizeof(g_cust_rw_mask));
    if (g_cust_stats.allocated_bytes > g_cust_stats.peak_bytes) g_cust_stats.peak_bytes = g_cust_stats.allocated_bytes;
    g_cust_stats.cache_slots_used =
        (u32)(g_cust_bg_valid ? 1 : 0) +
        (u32)(g_cust_ff_valid[0] ? 1 : 0) + (u32)(g_cust_ff_valid[1] ? 1 : 0) +
        (u32)(g_cust_pp_valid[0] ? 1 : 0) + (u32)(g_cust_pp_valid[1] ? 1 : 0) +
        (u32)(g_cust_back_valid[0] ? 1 : 0) + (u32)(g_cust_back_valid[1] ? 1 : 0) +
        (u32)(g_cust_rw_valid[0] ? 1 : 0) + (u32)(g_cust_rw_valid[1] ? 1 : 0);
}

bool mivf_customization_active_for_premiere(void) {
    return g_cust_manifest_loaded;
}

bool mivf_customization_active_for_menu(void) {
    return g_cust_manifest_loaded;
}

const MivfCustomAsset *mivf_customization_get_dashboard_bg(void) {
    static MivfCustomAsset asset;
    if (!g_cust_bg_valid) return NULL;
    asset.pixels565 = g_cust_bg_pixels;
    asset.mask1bpp = NULL;
    asset.w = MIVF_CUST_BG_W;
    asset.h = MIVF_CUST_BG_H;
    return &asset;
}

const MivfCustomAsset *mivf_customization_resolve_asset(MivfCtrlId ctrl, MivfCtrlVisualState state) {
    static MivfCustomAsset asset;
    if (ctrl == MIVF_CTRL_FAST_FORWARD) {
        int s = state;
        if (!g_cust_ff_valid[s] && s == MIVF_CTRL_STATE_FOCUSED) s = MIVF_CTRL_STATE_IDLE;
        if (!g_cust_ff_valid[s]) return NULL;
        asset.pixels565 = g_cust_ff_pixels[s];
        asset.mask1bpp = g_cust_ff_mask[s];
        asset.w = MIVF_CUST_FF_W;
        asset.h = MIVF_CUST_FF_H;
        return &asset;
    }
    if (ctrl == MIVF_CTRL_PLAY_PAUSE) {
        int s = state;
        if (!g_cust_pp_valid[s] && s == MIVF_CTRL_STATE_FOCUSED) s = MIVF_CTRL_STATE_IDLE;
        if (!g_cust_pp_valid[s]) return NULL;
        asset.pixels565 = g_cust_pp_pixels[s];
        asset.mask1bpp = g_cust_pp_mask[s];
        asset.w = MIVF_CUST_PP_W;
        asset.h = MIVF_CUST_PP_H;
        return &asset;
    }
    if (ctrl == MIVF_CTRL_MOVIE_MENU_BACK) {
        int s = state;
        if (!g_cust_back_valid[s] && s == MIVF_CTRL_STATE_FOCUSED) s = MIVF_CTRL_STATE_IDLE;
        if (!g_cust_back_valid[s]) return NULL;
        asset.pixels565 = g_cust_back_pixels[s];
        asset.mask1bpp = g_cust_back_mask[s];
        asset.w = MIVF_CUST_BACK_W;
        asset.h = MIVF_CUST_BACK_H;
        return &asset;
    }
    if (ctrl == MIVF_CTRL_REWIND) {
        int s = state;
        if (!g_cust_rw_valid[s] && s == MIVF_CTRL_STATE_FOCUSED) s = MIVF_CTRL_STATE_IDLE;
        if (!g_cust_rw_valid[s]) return NULL;
        asset.pixels565 = g_cust_rw_pixels[s];
        asset.mask1bpp = g_cust_rw_mask[s];
        asset.w = MIVF_CUST_FF_W;
        asset.h = MIVF_CUST_FF_H;
        return &asset;
    }
    return NULL; /* every reserved control id: always NULL in this slice */
}

MivfCtrlColorOverride mivf_customization_resolve_color(MivfCtrlId ctrl, MivfCtrlVisualState state, u32 color_vision_mode) {
    MivfCtrlColorOverride out;
    int idx = -1;
    memset(&out, 0, sizeof(out));

    if (ctrl == MIVF_CTRL_FAST_FORWARD) idx = 0;
    else if (ctrl == MIVF_CTRL_PLAY_PAUSE) idx = 1;
    else if (ctrl == MIVF_CTRL_MOVIE_MENU_BACK) idx = 2;
    else if (ctrl == MIVF_CTRL_REWIND) idx = 3;

    if (idx >= 0 && g_cust_has_fill[idx][state]) {
        mivf_cust_apply_cvd(g_cust_fill[idx][state][0], g_cust_fill[idx][state][1], g_cust_fill[idx][state][2],
                             color_vision_mode, &out.fill_r, &out.fill_g, &out.fill_b);
        out.has_fill = true;
    } else if (g_cust_has_accent) {
        /* Per-control fill missing: fall back to the manifest's global
           accent (CUSTOMIZATION_PRECEDENCE.md step 7 falling through to
           step 4), still CVD-transformed, never the raw authored value. */
        mivf_cust_apply_cvd(g_cust_accent[0], g_cust_accent[1], g_cust_accent[2],
                             color_vision_mode, &out.fill_r, &out.fill_g, &out.fill_b);
        out.has_fill = true;
    }

    if (idx >= 0 && g_cust_has_outline[idx][state]) {
        mivf_cust_apply_cvd(g_cust_outline[idx][state][0], g_cust_outline[idx][state][1], g_cust_outline[idx][state][2],
                             color_vision_mode, &out.outline_r, &out.outline_g, &out.outline_b);
        out.has_outline = true;
    } else if (g_cust_has_palette_outline) {
        mivf_cust_apply_cvd(g_cust_palette_outline[0], g_cust_palette_outline[1], g_cust_palette_outline[2],
                             color_vision_mode, &out.outline_r, &out.outline_g, &out.outline_b);
        out.has_outline = true;
    }

    return out;
}

bool mivf_customization_resolve_position(MivfCtrlId ctrl, int *out_dx, int *out_dy) {
    int idx = -1;

    if (ctrl == MIVF_CTRL_FAST_FORWARD) idx = 0;
    else if (ctrl == MIVF_CTRL_PLAY_PAUSE) idx = 1;
    else if (ctrl == MIVF_CTRL_REWIND) idx = 3;
    /* MIVF_CTRL_MOVIE_MENU_BACK deliberately excluded: C.6 is scoped to the
       three functional playback-dashboard controls only, matching
       mivf_customization_active_for_premiere()'s own existing scope. */

    if (idx < 0 || !g_cust_has_position[idx]) {
        if (out_dx) *out_dx = 0;
        if (out_dy) *out_dy = 0;
        return false;
    }

    if (out_dx) *out_dx = g_cust_dx[idx];
    if (out_dy) *out_dy = g_cust_dy[idx];
    return true;
}

MivfCustomizationStats mivf_customization_get_stats(void) {
    return g_cust_stats;
}
