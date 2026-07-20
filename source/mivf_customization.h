#pragma once

/*
    MIVF_CUSTOMIZATION_V1 (Phase C vertical slice)

    Public API for the Premiere-theme dashboard customization resolver.
    See mivf_customization_gui_20260716/ (repo root) for the full Phase B
    design package this implements: CUSTOMIZATION_SCHEMA.md,
    CUSTOMIZATION_PRECEDENCE.md, CUSTOMIZATION_CACHE_DESIGN.md,
    CUSTOMIZATION_ASSET_FORMAT_DECISION.md.

    Scope, deliberately: MIVF_CTRL_FAST_FORWARD, MIVF_CTRL_PLAY_PAUSE, and
    MIVF_CTRL_REWIND (Cinematic/Premiere dashboard) and
    MIVF_CTRL_MOVIE_MENU_BACK (the real,
    functional DVD-style root menu's Back button, MIVF_MENU_ACTION_BACK --
    see main.c:12937-12943, 334-339) are resolvable in this slice. Every
    other control ID exists so the enum is stable for a future phase, but
    mivf_customization_resolve_asset() always returns NULL for them today --
    they must never be presented as functional. MIVF_CTRL_MOVIE_MENU_BACK is
    a deliberately distinct identity from the transport-dashboard controls:
    it lives on a different screen (the root DVD-style menu, D-pad/index
    selected, no touch hitbox), rendered by a different function
    (mivf_menu_draw_button_top(), not any of the 16 mivf_c21_draw_dashboard
    styles), and was never added to MivfTransportAction -- Back is not, and
    must never be presented as, a playback-dashboard control.

    This module never touches input, hitboxes, playback state, NDSP, or
    timing. It never allocates from the heap (one static pool, sized for this
    slice only) and never performs file I/O outside the two lifecycle calls
    below, never inside a per-frame draw path.
*/

#include <3ds.h>
#include <stdbool.h>

typedef enum {
    MIVF_CTRL_PLAY_PAUSE = 0,
    MIVF_CTRL_FAST_FORWARD,
    MIVF_CTRL_REWIND,      /* real, resolvable: g_mivf_touch_layouts[CINEMATIC].main[0], same 64x60 as Fast Forward */
    MIVF_CTRL_PREVIOUS,    /* reserved id; corresponding action is unwired upstream */
    MIVF_CTRL_NEXT,        /* reserved id; corresponding action is unwired upstream */
    MIVF_CTRL_STOP,        /* reserved id; corresponding action is unwired upstream */
    MIVF_CTRL_VOLUME,      /* reserved id; corresponding action is unwired upstream */
    MIVF_CTRL_SUBTITLES,   /* reserved id; corresponding action is unwired upstream */
    MIVF_CTRL_MOVIE_MENU_BACK, /* real, functional: MIVF_MENU_ACTION_BACK on the root DVD-style menu */
    MIVF_CTRL_COUNT
} MivfCtrlId;

typedef enum {
    MIVF_CTRL_STATE_IDLE = 0,
    MIVF_CTRL_STATE_FOCUSED,
    MIVF_CTRL_STATE_COUNT
} MivfCtrlVisualState;

/* Resolved raster override for one (control, state). Pixels are RGB565LE,
   row-major, top-to-bottom (the caller is responsible for framebuffer
   column-major conversion, same as every other draw helper in main.c).
   mask1bpp is row-major, MSB-first, 1 = draw pixel, 0 = skip. w/h are the
   asset's real dimensions, always <= the control's authoritative hitbox
   size for this style (enforced at load time, never at blit time). */
typedef struct {
    const u16 *pixels565;
    const u8  *mask1bpp;
    int w, h;
} MivfCustomAsset;

/* Resolved semantic-color override, already passed through the same
   color-vision substitution table main.c's mivf_theme_set_rgb() uses (see
   mivf_customization.c's own comment on why this is a deliberate, documented
   duplication rather than a call into main.c's global theme state). */
typedef struct {
    bool has_fill;
    u8 fill_r, fill_g, fill_b;
    bool has_outline;
    u8 outline_r, outline_g, outline_b;
} MivfCtrlColorOverride;

/* Lifecycle hook, shared by both customizable screens for one movie. Call
   once when either screen for a given video_path is entered: main.c's
   play() (immediately after the existing sidecar loads --
   hfix58s_subtitles_load_for_video / hfix60_chapters_load), AND the root
   DVD-style menu path (immediately before mivf_menu_run(), main.c's main
   loop) -- both read the SAME per-project .mivftheme sidecar for the same
   movie, so this is one shared load, not two. Performs at most one
   manifest parse and bounded fixed-pool loads: one dashboard background
   plus Idle/Focused assets for Rewind, Play/Pause, Fast Forward, and the
   separate movie-menu Back control. Safe to call repeatedly (skips
   re-parsing when video_path matches the already-loaded session, so a
   menu-then-play sequence for the same movie only loads once). Loading is
   CVD-agnostic (raw authored bytes are cached as-is);
   mivf_customization_resolve_color() below is where color_vision_mode is
   applied, every time it's called. */
void mivf_customization_on_dashboard_enter(const char *video_path);

/* Exit lifecycle hook. Call when leaving either customizable screen back to
   the browser with nothing else for this movie pending (main.c's play(),
   in its cleanup section; and the root DVD-menu's MIVF_MENU_RESULT_BACK
   branch, since that path never reaches play() at all). Marks the static
   cache invalid; performs no I/O and no heap free (the pool is static,
   never malloc'd). Safe to call even if nothing was loaded. */
void mivf_customization_on_dashboard_exit(void);

/* True only when a manifest was successfully loaded for the current
   session AND the active dashboard style is Cinematic/Premiere. Governs
   the three functional dashboard controls -- Rewind, Play/Pause, and Fast
   Forward. See mivf_customization_active_for_menu() for the menu's separate
   Back control, which
   is not style-gated (the DVD-menu has no per-style variants). Every other
   function below is safe to call regardless, and returns the
   built-in-fallback answer (NULL / has_*=false) when this is false. */
bool mivf_customization_active_for_premiere(void);

/* True only when a manifest was successfully loaded for the current
   session (regardless of transport_style, since the root DVD-menu is not
   style-gated). Governs MIVF_CTRL_MOVIE_MENU_BACK only. */
bool mivf_customization_active_for_menu(void);

/* NULL if no manifest, no dashboard-background override, or the loaded
   asset failed validation. Dimensions are always exactly 320x240 (the real
   bottom-screen dashboard canvas size -- see mivf_customization.c's loader
   comment for why this corrects an earlier, unverified 400x240 assumption). */
const MivfCustomAsset *mivf_customization_get_dashboard_bg(void);

/* NULL if no override exists for this exact (ctrl, state). Internally falls
   back FOCUSED -> IDLE -> NULL, once, deterministically -- never partial. */
const MivfCustomAsset *mivf_customization_resolve_asset(MivfCtrlId ctrl, MivfCtrlVisualState state);

/* Always returns a struct; has_fill/has_outline are false when no override
   applies, in which case the caller must use its existing built-in color.
   Falls back from a missing per-control fill/outline to the manifest's
   global PALETTE_ACCENT/PALETTE_OUTLINE before giving up entirely (see
   CUSTOMIZATION_PRECEDENCE.md). color_vision_mode is passed by value, same
   reasoning as mivf_customization_on_dashboard_enter above -- every
   returned color has already been through the same CVD substitution table
   main.c's own theme system uses, per CUSTOMIZATION_PRECEDENCE.md's binding
   rule that a manifest can never bypass an accessibility setting. */
MivfCtrlColorOverride mivf_customization_resolve_color(MivfCtrlId ctrl, MivfCtrlVisualState state, u32 color_vision_mode);

/* C.6: optional per-control pixel offset (dx, dy), Premiere-transport-only
   (Rewind/Play-Pause/Fast Forward -- not the DVD-menu Back control). False
   with *out_dx=*out_dy=0 when no override exists; caller must add these to
   its existing hardcoded position, never replace it wholesale, so a
   manifest with no C.6 authoring is byte-for-byte the pre-C.6 layout. */
bool mivf_customization_resolve_position(MivfCtrlId ctrl, int *out_dx, int *out_dy);

/* Diagnostics only (Phase C evidence requirement) -- never used for control
   flow. All fields are 0 when the module has never attempted a load. */
typedef struct {
    u32 configured_budget_bytes;
    u32 allocated_bytes;
    u32 peak_bytes;
    u32 cache_slots_used;
    u32 invalidation_count;
    u32 last_load_us;
    int last_fallback_reason; /* 0 = none, see mivf_customization.c for the enum */
} MivfCustomizationStats;
MivfCustomizationStats mivf_customization_get_stats(void);
