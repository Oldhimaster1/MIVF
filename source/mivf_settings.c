#include "mivf_settings.h"
#include "mivf_bookmark_io.h"

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
    settings->chapter_markers_enabled = true;
    settings->force_stereo = true;
    settings->debug_overlay_enabled = false;
    settings->subtitle_track_index = 0;
    settings->subtitle_delay_ms = 0;
    settings->subtitle_position = 0;
    settings->audio_offset_ms = 0;
    settings->theme_index = 0;
    settings->theme_custom = false;
    settings->theme_r = 70;
    settings->theme_g = 120;
    settings->theme_b = 210;
    settings->color_vision_mode = 0;
    settings->transport_style = 0;
    settings->font_scale = 1;
    settings->aspect_mode = 0;
    settings->auto_advance = false;
    settings->playback_speed_idx = 2; /* 1.0x */
    settings->sleep_timer_min = 0;    /* off */
    settings->volume_percent = 100;
    settings->left_gain_percent = 100;
    settings->right_gain_percent = 100;
    /* Advanced Screensaver Customization: defaults exactly reproduce the
       pre-existing hardcoded behavior (vx=vy=2, MIVF_MENU_SCREENSAVER_IDLE_FRAMES=1200,
       no fade, full motion) so an existing settings.ini with none of these
       keys sees zero behavior change. */
    settings->screensaver_speed = 2;
    settings->screensaver_idle_frames = 1200;
    settings->screensaver_reduce_motion = false;
    settings->screensaver_fade_frames = 0;
    settings->library_sort_mode = 0;
    settings->library_filter_mode = 0;
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

    if (settings->theme_index > 3u) { settings->theme_index = 0u; }
    if (settings->theme_r > 255u) { settings->theme_r = 255u; }
    if (settings->theme_g > 255u) { settings->theme_g = 255u; }
    if (settings->theme_b > 255u) { settings->theme_b = 255u; }
    if (settings->color_vision_mode > 5u) { settings->color_vision_mode = 0u; }
    /* MIVF_TRANSPORT_STYLE_PERSIST_FIX_V1: the real style count (16, 0..15)
       is only defined via the MivfTransportStyleId enum in main.c, which this
       translation unit has no path to include -- so this bound is a literal,
       not MIVF_TRANSPORT_STYLE_COUNT. If the style roster changes again,
       update this literal to match. Previously ">9u", stale from an earlier
       10-style roster; that silently reset styles 10..15 (Cartridge
       Controller, Portable Mono, Dual Screen Touch, Industrial Green, Flat
       Tile, Blue Wave) to 0 every time settings were clamped, including on
       every app launch right after MIVF_SettingsLoad(). */
    if (settings->transport_style >= 16u) { settings->transport_style = 0u; }
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

    if (settings->volume_percent > 300u) {
        settings->volume_percent = 300u;
    }

    /* P.A1: attenuation only, never amplification -- capped at 100, unlike
       volume_percent's existing boost range above. */
    if (settings->left_gain_percent > 100u) {
        settings->left_gain_percent = 100u;
    }

    if (settings->right_gain_percent > 100u) {
        settings->right_gain_percent = 100u;
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

    if (settings->audio_offset_ms < -600) {
        settings->audio_offset_ms = -600;
    }

    if (settings->audio_offset_ms > 3000) {
        settings->audio_offset_ms = 3000;
    }

    /* Advanced Screensaver Customization: bounded ranges enforced here
       regardless of source (settings.ini hand-edit, a future Toolkit
       export, or a corrupted file) -- mirrors every other field in this
       function, never trusts a loaded value without a range check. */
    if (settings->screensaver_speed < 1u) {
        settings->screensaver_speed = 1u;
    }
    if (settings->screensaver_speed > 5u) {
        settings->screensaver_speed = 5u;
    }
    if (settings->screensaver_idle_frames < 300u) {
        settings->screensaver_idle_frames = 300u;
    }
    if (settings->screensaver_idle_frames > 7200u) {
        settings->screensaver_idle_frames = 7200u;
    }
    if (settings->screensaver_fade_frames > 60u) {
        settings->screensaver_fade_frames = 60u;
    }
    if (settings->library_sort_mode > 3u) {
        settings->library_sort_mode = 0u;
    }
    if (settings->library_filter_mode > 3u) {
        settings->library_filter_mode = 0u;
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
        else if (!strcmp(key, "chapter_markers_enabled")) settings->chapter_markers_enabled = atoi(val) != 0;
        else if (!strcmp(key, "force_stereo")) settings->force_stereo = atoi(val) != 0;
        else if (!strcmp(key, "debug_overlay_enabled")) settings->debug_overlay_enabled = atoi(val) != 0;
        else if (!strcmp(key, "subtitle_track_index")) settings->subtitle_track_index = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "subtitle_delay_ms")) settings->subtitle_delay_ms = atoi(val);
        else if (!strcmp(key, "subtitle_position")) settings->subtitle_position = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "audio_offset_ms")) settings->audio_offset_ms = atoi(val);
        else if (!strcmp(key, "screensaver_speed")) settings->screensaver_speed = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "screensaver_idle_frames")) settings->screensaver_idle_frames = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "screensaver_reduce_motion")) settings->screensaver_reduce_motion = atoi(val) != 0;
        else if (!strcmp(key, "screensaver_fade_frames")) settings->screensaver_fade_frames = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "library_sort_mode")) settings->library_sort_mode = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "library_filter_mode")) settings->library_filter_mode = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "theme_index")) settings->theme_index = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "theme_custom")) settings->theme_custom = atoi(val) != 0;
        else if (!strcmp(key, "theme_r")) settings->theme_r = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "theme_g")) settings->theme_g = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "theme_b")) settings->theme_b = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "color_vision_mode")) settings->color_vision_mode = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "transport_style")) settings->transport_style = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "font_scale")) settings->font_scale = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "aspect_mode")) settings->aspect_mode = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "auto_advance")) settings->auto_advance = atoi(val) != 0;
        else if (!strcmp(key, "playback_speed_idx")) settings->playback_speed_idx = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "sleep_timer_min")) settings->sleep_timer_min = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "volume_percent")) settings->volume_percent = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "left_gain_percent")) settings->left_gain_percent = (u32)strtoul(val, NULL, 10);
        else if (!strcmp(key, "right_gain_percent")) settings->right_gain_percent = (u32)strtoul(val, NULL, 10);
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
    fprintf(fp, "chapter_markers_enabled=%d\n", settings->chapter_markers_enabled ? 1 : 0);
    fprintf(fp, "force_stereo=%d\n", settings->force_stereo ? 1 : 0);
    fprintf(fp, "debug_overlay_enabled=%d\n", settings->debug_overlay_enabled ? 1 : 0);
    fprintf(fp, "subtitle_track_index=%lu\n", (unsigned long)settings->subtitle_track_index);
    fprintf(fp, "subtitle_delay_ms=%d\n", settings->subtitle_delay_ms);
    fprintf(fp, "subtitle_position=%lu\n", (unsigned long)settings->subtitle_position);
    fprintf(fp, "audio_offset_ms=%d\n", settings->audio_offset_ms);
    fprintf(fp, "screensaver_speed=%lu\n", (unsigned long)settings->screensaver_speed);
    fprintf(fp, "screensaver_idle_frames=%lu\n", (unsigned long)settings->screensaver_idle_frames);
    fprintf(fp, "screensaver_reduce_motion=%d\n", settings->screensaver_reduce_motion ? 1 : 0);
    fprintf(fp, "screensaver_fade_frames=%lu\n", (unsigned long)settings->screensaver_fade_frames);
    fprintf(fp, "library_sort_mode=%lu\n", (unsigned long)settings->library_sort_mode);
    fprintf(fp, "library_filter_mode=%lu\n", (unsigned long)settings->library_filter_mode);
    fprintf(fp, "theme_index=%lu\n", (unsigned long)settings->theme_index);
    fprintf(fp, "theme_custom=%d\n", settings->theme_custom ? 1 : 0);
    fprintf(fp, "theme_r=%lu\n", (unsigned long)settings->theme_r);
    fprintf(fp, "theme_g=%lu\n", (unsigned long)settings->theme_g);
    fprintf(fp, "theme_b=%lu\n", (unsigned long)settings->theme_b);
    fprintf(fp, "color_vision_mode=%lu\n", (unsigned long)settings->color_vision_mode);
    fprintf(fp, "transport_style=%lu\n", (unsigned long)settings->transport_style);
    fprintf(fp, "font_scale=%lu\n", (unsigned long)settings->font_scale);
    fprintf(fp, "aspect_mode=%lu\n", (unsigned long)settings->aspect_mode);
    fprintf(fp, "auto_advance=%d\n", settings->auto_advance ? 1 : 0);
    fprintf(fp, "playback_speed_idx=%lu\n", (unsigned long)settings->playback_speed_idx);
    fprintf(fp, "sleep_timer_min=%lu\n", (unsigned long)settings->sleep_timer_min);
    fprintf(fp, "volume_percent=%lu\n", (unsigned long)settings->volume_percent);
    fprintf(fp, "left_gain_percent=%lu\n", (unsigned long)settings->left_gain_percent);
    fprintf(fp, "right_gain_percent=%lu\n", (unsigned long)settings->right_gain_percent);

    fclose(fp);
    return true;
}

/*
    Three identity tiers, tried in this order on load and always upgraded
    forward on write:

      CURRENT   root + basename + FNV-1a hash of the full path.
                Collision-resistant: two different files that happen to
                share a basename ("ShowA/ep01.mivf" vs "ShowB/ep01.mivf")
                now resolve to different files instead of silently
                clobbering one another's resume position.
      PRE_HASH  root + basename only. What every bookmark written before
                this hardening pass used -- kept as a read fallback so
                existing SD cards aren't orphaned.
      LEGACY    the original pre-appdata-restructure root + basename
                only. Unchanged from before this pass.
*/
typedef enum {
    MIVF_BOOKMARK_TIER_CURRENT = 0,
    MIVF_BOOKMARK_TIER_PRE_HASH = 1,
    MIVF_BOOKMARK_TIER_LEGACY = 2
} MivfBookmarkTier;

static void mivf_make_bookmark_path(const char *video_path, char *out, size_t out_sz, MivfBookmarkTier tier) {
    const char *base;

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

    if (tier == MIVF_BOOKMARK_TIER_CURRENT) {
        mivf_bookmark_identity_path(MIVF_BOOKMARK_PATH, video_path, base, "bookmark", out, out_sz);
        return;
    }

    snprintf(out, out_sz, "%s.%s.bookmark",
             tier == MIVF_BOOKMARK_TIER_LEGACY ? MIVF_BOOKMARK_LEGACY_PATH : MIVF_BOOKMARK_PATH, base);
}

bool MIVF_BookmarkLoad(const char *video_path, MivfBookmark *bookmark) {
    static const MivfBookmarkTier kFallbackTiers[] = {
        MIVF_BOOKMARK_TIER_CURRENT, MIVF_BOOKMARK_TIER_PRE_HASH, MIVF_BOOKMARK_TIER_LEGACY
    };
    char path[512];
    char line[256];
    FILE *fp = NULL;
    size_t i;
    bool used_fallback = false;

    if (!bookmark) {
        return false;
    }

    memset(bookmark, 0, sizeof(*bookmark));

    for (i = 0; i < sizeof(kFallbackTiers) / sizeof(kFallbackTiers[0]); i++) {
        mivf_make_bookmark_path(video_path, path, sizeof(path), kFallbackTiers[i]);
        if (!path[0]) {
            return false;
        }

        fp = fopen(path, "rb");
        if (fp) {
            used_fallback = (kFallbackTiers[i] != MIVF_BOOKMARK_TIER_CURRENT);
            break;
        }
    }

    if (!fp) {
        return false;
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

    /*
        Upgrade a pre-hardening/legacy bookmark into the new
        collision-resistant tier -- but only if its stored identity
        actually matches the path we were asked to load. If it doesn't
        match, this IS the historical basename-collision bug (a
        different file's bookmark happened to already occupy this
        pre-hash path); leave it alone rather than writing another
        movie's frame number under this path's new identity. The
        caller's own identity check (comparing bookmark->video_path
        against its own path) already handles the mismatched case
        exactly as it did before this change.
    */
    if (used_fallback && video_path && !strcmp(bookmark->video_path, video_path)) {
        MIVF_BookmarkSave(video_path, bookmark->frame);
    }

    return bookmark->video_path[0] != 0;
}

bool MIVF_BookmarkSave(const char *video_path, u32 frame) {
    char path[512];
    char text[300];

    mivf_make_bookmark_path(video_path, path, sizeof(path), MIVF_BOOKMARK_TIER_CURRENT);
    if (!path[0] || !video_path || !*video_path) {
        return false;
    }

    MIVF_AppDataEnsureLayout();
    mivf_ensure_parent_dirs(path);

    snprintf(text, sizeof(text), "%s\n%lu\n", video_path, (unsigned long)frame);
    return mivf_atomic_write_text(path, text);
}

bool MIVF_BookmarkClear(const char *video_path) {
    char path[512];
    char pre_hash_path[512];
    char legacy_path[512];
    bool cleared = false;

    mivf_make_bookmark_path(video_path, path, sizeof(path), MIVF_BOOKMARK_TIER_CURRENT);
    if (!path[0]) {
        return false;
    }

    mivf_make_bookmark_path(video_path, pre_hash_path, sizeof(pre_hash_path), MIVF_BOOKMARK_TIER_PRE_HASH);
    mivf_make_bookmark_path(video_path, legacy_path, sizeof(legacy_path), MIVF_BOOKMARK_TIER_LEGACY);

    if (remove(path) == 0) {
        cleared = true;
    }
    if (remove(pre_hash_path) == 0) {
        cleared = true;
    }
    if (remove(legacy_path) == 0) {
        cleared = true;
    }

    return cleared;
}

/*
    Watch-state persistence. Single-tier only (CURRENT-equivalent) --
    unlike bookmarks, there is no pre-existing legacy format to fall back
    to for a feature that never existed before this pass. Reuses
    mivf_bookmark_identity_path (mivf_bookmark_io.h) directly rather than
    duplicating the FNV-1a collision-resistant identity scheme bookmarks
    already established.
*/
static void mivf_make_watchstate_path(const char *video_path, char *out, size_t out_sz) {
    const char *base;

    if (!out || out_sz == 0) {
        return;
    }

    out[0] = 0;
    if (!video_path || !*video_path) {
        return;
    }

    base = strrchr(video_path, '/');
    base = base ? base + 1 : video_path;

    mivf_bookmark_identity_path(MIVF_WATCHSTATE_PATH, video_path, base, "watchstate", out, out_sz);
}

bool MIVF_WatchStateLoad(const char *video_path, MivfWatchState *out) {
    char path[512];
    char line[256];
    FILE *fp;

    if (!out) {
        return false;
    }

    memset(out, 0, sizeof(*out));

    mivf_make_watchstate_path(video_path, path, sizeof(path));
    if (!path[0]) {
        return false;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        return false;
    }

    if (!fgets(out->video_path, sizeof(out->video_path), fp)) { fclose(fp); return false; }
    out->video_path[strcspn(out->video_path, "\r\n")] = 0;

    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return false; }
    out->last_frame = (u32)strtoul(line, NULL, 10);

    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return false; }
    out->total_frames = (u32)strtoul(line, NULL, 10);

    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return false; }
    out->status = (u32)strtoul(line, NULL, 10);

    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return false; }
    out->last_played_unix = (u32)strtoul(line, NULL, 10);

    if (fgets(line, sizeof(line), fp)) {
        out->manual = strtoul(line, NULL, 10) != 0;
    }

    fclose(fp);

    /* Same identity-mismatch discipline as MIVF_BookmarkLoad's caller
       contract: a hash collision or corrupted file is detectable by the
       stored path not matching what was asked for. Refuse rather than
       silently attributing one title's watch history to another. */
    return out->video_path[0] != 0 && video_path && !strcmp(out->video_path, video_path);
}

bool MIVF_WatchStateSave(const char *video_path, const MivfWatchState *state) {
    char path[512];
    char text[400];

    if (!state || !video_path || !*video_path) {
        return false;
    }

    mivf_make_watchstate_path(video_path, path, sizeof(path));
    if (!path[0]) {
        return false;
    }

    MIVF_AppDataEnsureLayout();
    mivf_ensure_parent_dirs(path);

    snprintf(text, sizeof(text), "%s\n%lu\n%lu\n%lu\n%lu\n%d\n",
        video_path,
        (unsigned long)state->last_frame,
        (unsigned long)state->total_frames,
        (unsigned long)state->status,
        (unsigned long)state->last_played_unix,
        state->manual ? 1 : 0);

    return mivf_atomic_write_text(path, text);
}

bool MIVF_WatchStateClear(const char *video_path) {
    char path[512];

    mivf_make_watchstate_path(video_path, path, sizeof(path));
    if (!path[0]) {
        return false;
    }

    return remove(path) == 0;
}
