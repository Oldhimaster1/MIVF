#pragma once

#include <3ds.h>
#include <stdbool.h>

#define MIVF_SETTINGS_PATH "sdmc:/mivf_settings.ini"
#define MIVF_BOOKMARK_PATH "sdmc:/mivf_bookmarks"

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
    bool force_stereo;
    bool debug_overlay_enabled;
    u32 subtitle_track_index;
    u32 theme_index;
    u32 font_scale;
    u32 aspect_mode;          /* 0 = FIT, 1 = STRETCH, 2 = NATIVE */
    bool auto_advance;        /* play the next file in the folder when one ends */
    u32 playback_speed_idx;   /* index into the playback-speed table (default 1.0x) */
    u32 sleep_timer_min;      /* 0 = off, otherwise minutes before auto-pause */
} MivfSettings;

typedef struct {
    char video_path[256];
    u32 frame;
} MivfBookmark;

void MIVF_SettingsInit(MivfSettings *settings);
void MIVF_SettingsClamp(MivfSettings *settings);
bool MIVF_SettingsLoad(MivfSettings *settings);
bool MIVF_SettingsSave(const MivfSettings *settings);

bool MIVF_BookmarkLoad(const char *video_path, MivfBookmark *bookmark);
bool MIVF_BookmarkSave(const char *video_path, u32 frame);
bool MIVF_BookmarkClear(const char *video_path);
