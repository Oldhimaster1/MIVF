#pragma once

#include <3ds.h>
#include <stdbool.h>

#define MIVF_APPDATA_DIR "sdmc:/3ds/mivf_player_3ds/appdata"

#define MIVF_SETTINGS_PATH MIVF_APPDATA_DIR "/settings.ini"
#define MIVF_SETTINGS_LEGACY_PATH "sdmc:/mivf_settings.ini"

#define MIVF_BOOKMARK_PATH MIVF_APPDATA_DIR "/bookmarks"
#define MIVF_BOOKMARK_LEGACY_PATH "sdmc:/mivf_bookmarks"

#define MIVF_WATCHSTATE_PATH MIVF_APPDATA_DIR "/watchstate"

#define MIVF_FAVORITES_PATH MIVF_APPDATA_DIR "/favorites.ini"
#define MIVF_FAVORITES_LEGACY_PATH "sdmc:/mivf_favorites"

#define MIVF_RECENTS_PATH MIVF_APPDATA_DIR "/recents.ini"
#define MIVF_RECENTS_LEGACY_PATH "sdmc:/mivf_recents"

#define MIVF_CONTINUE_WATCHING_PATH MIVF_APPDATA_DIR "/continue_watching.ini"

#define MIVF_ADDED_DATES_PATH MIVF_APPDATA_DIR "/added_dates.ini"

#define MIVF_LOG_DIR MIVF_APPDATA_DIR "/logs"
#define MIVF_LOG_PATH MIVF_LOG_DIR "/mivf.log"

#define MIVF_CACHE_DIR MIVF_APPDATA_DIR "/cache"
#define MIVF_BENCHMARK_DIR MIVF_APPDATA_DIR "/benchmarks"

typedef struct {
    bool settings_menu_enabled;
    bool autosleep_allowed;
    bool autodim_enabled;
    u32 autodim_timeout_frames;
    u32 autodim_brightness;
    u32 active_brightness;
    bool resume_enabled;
    bool remember_favorites;
    bool show_subtitle_tracks;
    bool show_chapters;
    bool chapter_markers_enabled; /* draw chapter tick marks on the playback timeline */
    bool force_stereo;
    bool debug_overlay_enabled;
    u32 subtitle_track_index;
    int subtitle_delay_ms;     /* negative = earlier, positive = later */
    u32 subtitle_position;     /* 0 = low, 1 = middle, 2 = high */
    int audio_offset_ms;       /* A/V sync trim, -600..+3000 ms. Positive holds
                                   decoded audio this many ms before NDSP (audio
                                   later). Negative delays VIDEO presentation by
                                   |ms| instead (audio effectively earlier),
                                   which is what cancels the NDSP queue latency.
                                   Same sign convention as encoder
                                   --audio-offset-ms. See hfix71 / hfix77. */
    u32 theme_index;
    bool theme_custom;      /* true = use theme_r/theme_g/theme_b instead of preset */
    u32 theme_r;            /* custom accent, 0..255 */
    u32 theme_g;            /* custom accent, 0..255 */
    u32 theme_b;            /* custom accent, 0..255 */
    u32 color_vision_mode; /* 0 standard, 1 protan, 2 deutan, 3 tritan, 4 mono, 5 high contrast */
    u32 transport_style;   /* 0..15: 0 Premiere, 1 Orbit, 2 Focus, 3 Video System,
                               4 Playback Controls, 5 Receiver, 6 Celestial, 7 Feature,
                               8 Playback // Chapter, 9 Timecode, 10 Cartridge Controller,
                               11 Portable Mono, 12 Dual Screen Touch, 13 Industrial Green,
                               14 Flat Tile, 15 Blue Wave. Authoritative names/count live in
                               MivfTransportStyleId in main.c; keep this list and
                               MIVF_SettingsClamp's bound in sync if that roster changes. */
    u32 font_scale;
    u32 aspect_mode;          /* 0 = FIT, 1 = STRETCH, 2 = NATIVE */
    bool auto_advance;        /* play the next file in the folder when one ends */
    u32 playback_speed_idx;   /* index into the playback-speed table (default 1.0x) */
    u32 sleep_timer_min;      /* 0 = off, otherwise minutes before auto-pause */
    u32 volume_percent;       /* MIVF_PHASE6_PERSIST_VOLUME_V1: 0..300, default 100 */
    u32 left_gain_percent;    /* P.A1: independent left-channel attenuation, 0..100,
                                  default 100. Attenuation only -- never amplifies.
                                  Combines with volume_percent as
                                  (sample * volume_percent * left_gain_percent) / 10000
                                  (see source/hfix56_gain.h). Applies only when the
                                  live NDSP output is stereo (real stereo source, or
                                  a mono source with force_stereo on) -- a true mono
                                  output has no independent left/right to attenuate. */
    u32 right_gain_percent;   /* P.A1: independent right-channel attenuation, 0..100,
                                  default 100. Same semantics as left_gain_percent. */
    /* Advanced Screensaver Customization (global settings; the bounce
       image itself remains a PER-TITLE sidecar, .screensaver.cover,
       unchanged -- see mivf_menu_load_screensaver_image). */
    u32 screensaver_speed;         /* 1..5, default 2 (matches the prior hardcoded vx=vy=2) */
    u32 screensaver_idle_frames;   /* 300..7200, default 1200 (matches the prior MIVF_MENU_SCREENSAVER_IDLE_FRAMES) */
    bool screensaver_reduce_motion; /* true = freeze position, no bounce/movement, default false */
    u32 screensaver_fade_frames;   /* 0..60, default 0 (off); frames to fade in from black on activation */
    /* Library Sort/Filter/Search: session choices worth remembering
       across launches. Search text itself is deliberately NOT persisted
       here (a query is ephemeral -- see g_hfix58_search_query, in-memory
       only, cleared each browser session). */
    u32 library_sort_mode;    /* 0=Name (default/legacy), 1=Date Added, 2=Last Played, 3=By Series */
    u32 library_filter_mode;  /* 0=All (default/legacy), 1=Unwatched, 2=In Progress, 3=Watched */
} MivfSettings;

typedef struct {
    char video_path[256];
    u32 frame;
} MivfBookmark;

/* Watch-state model: a genuinely separate persisted store from
   MivfBookmark, NOT derived from bookmark presence -- a natural EOF
   always clears the bookmark (see the play() teardown), so a finished
   title would be indistinguishable from a never-started one if this
   reused bookmark presence/absence as its signal. Uses the exact same
   identity hashing as bookmarks (mivf_bookmark_identity_path, see
   mivf_settings.c) so two titles never collide and one video's watch
   state is never silently attributed to a different, same-named file. */
typedef enum {
    MIVF_WATCH_UNWATCHED = 0,
    MIVF_WATCH_IN_PROGRESS = 1,
    MIVF_WATCH_WATCHED = 2
} MivfWatchStatus;

typedef struct {
    char video_path[256];
    u32 last_frame;
    u32 total_frames;    /* 0 = unknown/untrustworthy -- never used alone to infer WATCHED */
    u32 status;           /* MivfWatchStatus */
    u32 last_played_unix;
    bool manual;           /* true once the user has explicitly set this via the long-press
                               toggle in the library, rather than automatic playback tracking */
} MivfWatchState;

void MIVF_SettingsInit(MivfSettings *settings);
void MIVF_SettingsClamp(MivfSettings *settings);
bool MIVF_AppDataEnsureLayout(void);
bool MIVF_SettingsLoad(MivfSettings *settings);
bool MIVF_SettingsSave(const MivfSettings *settings);

bool MIVF_BookmarkLoad(const char *video_path, MivfBookmark *bookmark);
bool MIVF_BookmarkSave(const char *video_path, u32 frame);
bool MIVF_BookmarkClear(const char *video_path);

bool MIVF_WatchStateLoad(const char *video_path, MivfWatchState *out);
bool MIVF_WatchStateSave(const char *video_path, const MivfWatchState *state);
bool MIVF_WatchStateClear(const char *video_path);
