#pragma once

#include <3ds.h>
#include <stdbool.h>

#define MIVF_APPDATA_DIR "sdmc:/3ds/mivf_player_3ds/appdata"

#define MIVF_SETTINGS_PATH MIVF_APPDATA_DIR "/settings.ini"
#define MIVF_SETTINGS_LEGACY_PATH "sdmc:/mivf_settings.ini"

#define MIVF_BOOKMARK_PATH MIVF_APPDATA_DIR "/bookmarks"
#define MIVF_BOOKMARK_LEGACY_PATH "sdmc:/mivf_bookmarks"

#define MIVF_FAVORITES_PATH MIVF_APPDATA_DIR "/favorites.ini"
#define MIVF_FAVORITES_LEGACY_PATH "sdmc:/mivf_favorites"

#define MIVF_RECENTS_PATH MIVF_APPDATA_DIR "/recents.ini"
#define MIVF_RECENTS_LEGACY_PATH "sdmc:/mivf_recents"

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
    u32 font_scale;
    u32 aspect_mode;          /* 0 = FIT, 1 = STRETCH, 2 = NATIVE */
    bool auto_advance;        /* play the next file in the folder when one ends */
    u32 playback_speed_idx;   /* index into the playback-speed table (default 1.0x) */
    u32 sleep_timer_min;      /* 0 = off, otherwise minutes before auto-pause */
    u32 volume_percent;       /* MIVF_PHASE6_PERSIST_VOLUME_V1: 0..300, default 100 */
} MivfSettings;

typedef struct {
    char video_path[256];
    u32 frame;
} MivfBookmark;

void MIVF_SettingsInit(MivfSettings *settings);
void MIVF_SettingsClamp(MivfSettings *settings);
bool MIVF_AppDataEnsureLayout(void);
bool MIVF_SettingsLoad(MivfSettings *settings);
bool MIVF_SettingsSave(const MivfSettings *settings);

bool MIVF_BookmarkLoad(const char *video_path, MivfBookmark *bookmark);
bool MIVF_BookmarkSave(const char *video_path, u32 frame);
bool MIVF_BookmarkClear(const char *video_path);
