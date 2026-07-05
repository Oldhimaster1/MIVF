#include "mivf_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

static void mivf_copy_key(char *dst, size_t dst_sz, const char *src) {
    if (!dst || dst_sz == 0) {
        return;
    }

    if (!src) {
        dst[0] = 0;
        return;
    }

    strncpy(dst, src, dst_sz - 1);
    dst[dst_sz - 1] = 0;
}

static char *mivf_trim(char *s) {
    char *end;

    if (!s) {
        return s;
    }

    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
        s++;
    }

    end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }

    *end = 0;
    return s;
}

static bool mivf_mkdir_one(const char *path) {
    if (!path || !*path) {
        return false;
    }

    if (mkdir(path, 0777) == 0) {
        return true;
    }

    return errno == EEXIST;
}

static bool mivf_mkdirs(const char *path) {
    char tmp[512];
    char *cursor;

    if (!path || !*path) {
        return false;
    }

    snprintf(tmp, sizeof(tmp), "%s", path);

    for (cursor = tmp; *cursor; cursor++) {
        if (*cursor != '/') {
            continue;
        }

        if (cursor == tmp + 5 && tmp[4] == ':') {
            continue;
        }

        *cursor = 0;
        if (tmp[0]) {
            mivf_mkdir_one(tmp);
        }
        *cursor = '/';
    }

    return mivf_mkdir_one(tmp);
}

static void mivf_path_dirname(const char *path, char *out, size_t out_sz) {
    const char *slash;

    if (!out || out_sz == 0) {
        return;
    }

    out[0] = 0;
    if (!path || !*path) {
        return;
    }

    slash = strrchr(path, '/');
    if (!slash) {
        return;
    }

    if (slash == path) {
        snprintf(out, out_sz, "/");
        return;
    }

    snprintf(out, out_sz, "%.*s", (int)(slash - path), path);
}

static bool mivf_ensure_parent_dirs(const char *path) {
    char dir[512];

    mivf_path_dirname(path, dir, sizeof(dir));
    if (!dir[0]) {
        return false;
    }

    return mivf_mkdirs(dir);
}

bool MIVF_AppDataEnsureLayout(void) {
    mivf_mkdirs(MIVF_APPDATA_DIR);
    mivf_mkdirs(MIVF_BOOKMARK_PATH);
    mivf_mkdirs(MIVF_LOG_DIR);
    mivf_mkdirs(MIVF_CACHE_DIR);
    mivf_mkdirs(MIVF_BENCHMARK_DIR);
    return true;
}

static FILE *mivf_open_read_with_legacy(const char *path, const char *legacy_path, bool *used_legacy) {
    FILE *fp;

    if (used_legacy) {
        *used_legacy = false;
    }

    fp = fopen(path, "rb");
    if (fp || !legacy_path) {
        return fp;
    }

    fp = fopen(legacy_path, "rb");
    if (fp && used_legacy) {
        *used_legacy = true;
    }

    return fp;
}

void MIVF_SettingsInit(MivfSettings *settings) {
    if (!settings) {
        return;
    }

    memset(settings, 0, sizeof(*settings));
    settings->settings_menu_enabled = true;
    settings->autosleep_allowed = true;
    settings->autodim_enabled = true;
    settings->autodim_timeout_frames = 60u * 15u;
    settings->autodim_brightness = 1;
    settings->active_brightness = 5;
    settings->resume_enabled = true;
    settings->remember_favorites = true;
    settings->show_subtitle_tracks = true;
    settings->show_chapters = false;
    settings->force_stereo = true;
    settings->debug_overlay_enabled = false;
    settings->subtitle_track_index = 0;
    settings->subtitle_delay_ms = 0;
    settings->subtitle_position = 0;
    settings->theme_index = 0;
    settings->font_scale = 1;
    settings->aspect_mode = 0;
    settings->auto_advance = false;
    settings->playback_speed_idx = 2; /* 1.0x */
    settings->sleep_timer_min = 0;    /* off */
}

void MIVF_SettingsClamp(MivfSettings *settings) {
    if (!settings) {
        return;
    }

    if (settings->autodim_timeout_frames < 30u) {
        settings->autodim_timeout_frames = 30u;
    }

    if (settings->autodim_timeout_frames > 60u * 60u * 10u) {
        settings->autodim_timeout_frames = 60u * 60u * 10u;
    }

    if (settings->autodim_brightness > 5u) {
        settings->autodim_brightness = 5u;
    }

    if (settings->active_brightness < 1u) {
        settings->active_brightness = 1u;
    }

    if (settings->active_brightness > 5u) {
        settings->active_brightness = 5u;
    }

    if (settings->font_scale < 1u) {
        settings->font_scale = 1u;
    }

    if (settings->font_scale > 3u) {
        settings->font_scale = 3u;
    }

    if (settings->aspect_mode > 2u) {
        settings->aspect_mode = 0u;
    }

    if (settings->playback_speed_idx > 5u) {
        settings->playback_speed_idx = 2u;
    }

    if (settings->sleep_timer_min > 600u) {
        settings->sleep_timer_min = 600u;
    }

    if (settings->subtitle_delay_ms < -5000) {
        settings->subtitle_delay_ms = -5000;
    }

    if (settings->subtitle_delay_ms > 5000) {
        settings->subtitle_delay_ms = 5000;
    }

    if (settings->subtitle_position > 2u) {
        settings->subtitle_position = 0u;
    }
}

static void mivf_settings_set_defaults(MivfSettings *settings) {
    MIVF_SettingsInit(settings);
}

bool MIVF_SettingsLoad(MivfSettings *settings) {
    FILE *fp;
    char line[256];
    bool used_legacy = false;

    if (!settings) {
        return false;
    }

    mivf_settings_set_defaults(settings);

    fp = mivf_open_read_with_legacy(MIVF_SETTINGS_PATH, MIVF_SETTINGS_LEGACY_PATH, &used_legacy);
    if (!fp) {
        return false;
    }

    while (fgets(line, sizeof(line), fp)) {
        char *eq;
        char *key;
        char *val;

        key = mivf_trim(line);
        if (!*key || *key == '#') {
            continue;
        }

        eq = strchr(key, '=');
        if (!eq) {
            continue;
        }

        *eq = 0;
        val = mivf_trim(eq + 1);
        key = mivf_trim(key);

        if (!strcmp(key, "settings_menu_enabled")) settings->settings_menu_enabled = atoi(val) != 0;
        else if (!strcmp(key, "autosleep_allowed")) settings->autosleep_allowed = atoi(val) != 0;
        else if (!strcmp(key, "autodim_enabled")) settings->autodim_enabled = atoi(val) != 0;
        else if (!strcmp(key, "autodim_timeout_frames")) settings->autodim_timeout_frames = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "autodim_brightness")) settings->autodim_brightness = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "active_brightness")) settings->active_brightness = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "resume_enabled")) settings->resume_enabled = atoi(val) != 0;
        else if (!strcmp(key, "remember_favorites")) settings->remember_favorites = atoi(val) != 0;
        else if (!strcmp(key, "show_subtitle_tracks")) settings->show_subtitle_tracks = atoi(val) != 0;
        else if (!strcmp(key, "show_chapters")) settings->show_chapters = atoi(val) != 0;
        else if (!strcmp(key, "force_stereo")) settings->force_stereo = atoi(val) != 0;
        else if (!strcmp(key, "debug_overlay_enabled")) settings->debug_overlay_enabled = atoi(val) != 0;
        else if (!strcmp(key, "subtitle_track_index")) settings->subtitle_track_index = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "subtitle_delay_ms")) settings->subtitle_delay_ms = atoi(val);
        else if (!strcmp(key, "subtitle_position")) settings->subtitle_position = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "theme_index")) settings->theme_index = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "font_scale")) settings->font_scale = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "aspect_mode")) settings->aspect_mode = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "auto_advance")) settings->auto_advance = atoi(val) != 0;
        else if (!strcmp(key, "playback_speed_idx")) settings->playback_speed_idx = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "sleep_timer_min")) settings->sleep_timer_min = (u32)strtoul(val, NULL, 10);
    }

    fclose(fp);
    MIVF_SettingsClamp(settings);

    if (used_legacy) {
        MIVF_SettingsSave(settings);
    }

    return true;
}

bool MIVF_SettingsSave(const MivfSettings *settings) {
    FILE *fp;

    if (!settings) {
        return false;
    }

    MIVF_AppDataEnsureLayout();
    mivf_ensure_parent_dirs(MIVF_SETTINGS_PATH);

    fp = fopen(MIVF_SETTINGS_PATH, "wb");
    if (!fp) {
        return false;
    }

    fprintf(fp, "settings_menu_enabled=%d\n", settings->settings_menu_enabled ? 1 : 0);
    fprintf(fp, "autosleep_allowed=%d\n", settings->autosleep_allowed ? 1 : 0);
    fprintf(fp, "autodim_enabled=%d\n", settings->autodim_enabled ? 1 : 0);
    fprintf(fp, "autodim_timeout_frames=%lu\n", (unsigned long)settings->autodim_timeout_frames);
    fprintf(fp, "autodim_brightness=%lu\n", (unsigned long)settings->autodim_brightness);
    fprintf(fp, "active_brightness=%lu\n", (unsigned long)settings->active_brightness);
    fprintf(fp, "resume_enabled=%d\n", settings->resume_enabled ? 1 : 0);
    fprintf(fp, "remember_favorites=%d\n", settings->remember_favorites ? 1 : 0);
    fprintf(fp, "show_subtitle_tracks=%d\n", settings->show_subtitle_tracks ? 1 : 0);
    fprintf(fp, "show_chapters=%d\n", settings->show_chapters ? 1 : 0);
    fprintf(fp, "force_stereo=%d\n", settings->force_stereo ? 1 : 0);
    fprintf(fp, "debug_overlay_enabled=%d\n", settings->debug_overlay_enabled ? 1 : 0);
    fprintf(fp, "subtitle_track_index=%lu\n", (unsigned long)settings->subtitle_track_index);
    fprintf(fp, "subtitle_delay_ms=%d\n", settings->subtitle_delay_ms);
    fprintf(fp, "subtitle_position=%lu\n", (unsigned long)settings->subtitle_position);
    fprintf(fp, "theme_index=%lu\n", (unsigned long)settings->theme_index);
    fprintf(fp, "font_scale=%lu\n", (unsigned long)settings->font_scale);
    fprintf(fp, "aspect_mode=%lu\n", (unsigned long)settings->aspect_mode);
    fprintf(fp, "auto_advance=%d\n", settings->auto_advance ? 1 : 0);
    fprintf(fp, "playback_speed_idx=%lu\n", (unsigned long)settings->playback_speed_idx);
    fprintf(fp, "sleep_timer_min=%lu\n", (unsigned long)settings->sleep_timer_min);

    fclose(fp);
    return true;
}

static void mivf_make_bookmark_path(const char *video_path, char *out, size_t out_sz, bool legacy) {
    const char *base;
    const char *root;

    if (!out || out_sz == 0) {
        return;
    }

    out[0] = 0;
    if (!video_path || !*video_path) {
        return;
    }

    base = strrchr(video_path, '/');
    if (!base) {
        base = video_path;
    } else {
        base++;
    }

    root = legacy ? MIVF_BOOKMARK_LEGACY_PATH : MIVF_BOOKMARK_PATH;
    snprintf(out, out_sz, "%s.%s.bookmark", root, base);
}

bool MIVF_BookmarkLoad(const char *video_path, MivfBookmark *bookmark) {
    char path[512];
    char legacy_path[512];
    char line[256];
    FILE *fp;
    bool used_legacy = false;

    if (!bookmark) {
        return false;
    }

    memset(bookmark, 0, sizeof(*bookmark));
    mivf_make_bookmark_path(video_path, path, sizeof(path), false);
    if (!path[0]) {
        return false;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        mivf_make_bookmark_path(video_path, legacy_path, sizeof(legacy_path), true);
        fp = fopen(legacy_path, "rb");
        if (!fp) {
            return false;
        }

        used_legacy = true;
    }

    if (!fgets(bookmark->video_path, sizeof(bookmark->video_path), fp)) {
        fclose(fp);
        return false;
    }

    bookmark->video_path[strcspn(bookmark->video_path, "\r\n")] = 0;
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return false;
    }

    bookmark->frame = (u32)strtoul(line, NULL, 10);
    fclose(fp);

    if (used_legacy) {
        MIVF_BookmarkSave(video_path, bookmark->frame);
    }

    return bookmark->video_path[0] != 0;
}

bool MIVF_BookmarkSave(const char *video_path, u32 frame) {
    char path[512];
    FILE *fp;

    mivf_make_bookmark_path(video_path, path, sizeof(path), false);
    if (!path[0] || !video_path || !*video_path) {
        return false;
    }

    MIVF_AppDataEnsureLayout();
    mivf_ensure_parent_dirs(path);

    fp = fopen(path, "wb");
    if (!fp) {
        return false;
    }

    fprintf(fp, "%s\n%lu\n", video_path, (unsigned long)frame);
    fclose(fp);
    return true;
}

bool MIVF_BookmarkClear(const char *video_path) {
    char path[512];
    char legacy_path[512];

    mivf_make_bookmark_path(video_path, path, sizeof(path), false);
    if (!path[0]) {
        return false;
    }

    mivf_make_bookmark_path(video_path, legacy_path, sizeof(legacy_path), true);

    if (remove(path) == 0) {
        remove(legacy_path);
        return true;
    }

    if (remove(legacy_path) == 0) {
        return true;
    }

    return false;
}
