#include "mivf_subtitles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void mivf_subtitle_trim_inplace(char *s) {
    char *start;
    char *end;

    if (!s) {
        return;
    }

    start = s;

    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }

    end = s + strlen(s);

    while (end > s && isspace((unsigned char)end[-1])) {
        end--;
    }

    *end = '\0';
}

static bool mivf_parse_2(const char *s, uint32_t *out) {
    if (!s ||
        !isdigit((unsigned char)s[0]) ||
        !isdigit((unsigned char)s[1])) {
        return false;
    }

    *out = (uint32_t)((s[0] - '0') * 10 + (s[1] - '0'));
    return true;
}

static bool mivf_parse_3(const char *s, uint32_t *out) {
    if (!s ||
        !isdigit((unsigned char)s[0]) ||
        !isdigit((unsigned char)s[1]) ||
        !isdigit((unsigned char)s[2])) {
        return false;
    }

    *out = (uint32_t)((s[0] - '0') * 100 +
                      (s[1] - '0') * 10 +
                      (s[2] - '0'));
    return true;
}

static bool mivf_parse_srt_time(const char *s, uint32_t *out_ms) {
    uint32_t hh;
    uint32_t mm;
    uint32_t ss;
    uint32_t ms;

    if (!s || !out_ms) {
        return false;
    }

    if (!mivf_parse_2(s + 0, &hh)) return false;
    if (s[2] != ':') return false;
    if (!mivf_parse_2(s + 3, &mm)) return false;
    if (s[5] != ':') return false;
    if (!mivf_parse_2(s + 6, &ss)) return false;
    if (s[8] != ',' && s[8] != '.') return false;
    if (!mivf_parse_3(s + 9, &ms)) return false;

    if (mm >= 60 || ss >= 60 || ms >= 1000) {
        return false;
    }

    *out_ms = (((hh * 60u) + mm) * 60u + ss) * 1000u + ms;
    return true;
}

static bool mivf_parse_srt_range(
    const char *line,
    uint32_t *start_ms,
    uint32_t *end_ms
) {
    const char *arrow;

    if (!line || !start_ms || !end_ms) {
        return false;
    }

    arrow = strstr(line, "-->");

    if (!arrow) {
        return false;
    }

    if (!mivf_parse_srt_time(line, start_ms)) {
        return false;
    }

    arrow += 3;

    while (*arrow && isspace((unsigned char)*arrow)) {
        arrow++;
    }

    if (!mivf_parse_srt_time(arrow, end_ms)) {
        return false;
    }

    if (*end_ms <= *start_ms) {
        return false;
    }

    return true;
}

static void mivf_strip_basic_tags(char *s) {
    char out[MIVF_SUBTITLE_MAX_TEXT];
    size_t oi = 0;
    size_t i;
    bool in_tag = false;

    if (!s) {
        return;
    }

    for (i = 0; s[i] && oi + 1 < sizeof(out); i++) {
        char c = s[i];

        if (c == '<') {
            in_tag = true;
            continue;
        }

        if (c == '>') {
            in_tag = false;
            continue;
        }

        if (!in_tag) {
            out[oi++] = c;
        }
    }

    out[oi] = '\0';
    strncpy(s, out, MIVF_SUBTITLE_MAX_TEXT - 1);
    s[MIVF_SUBTITLE_MAX_TEXT - 1] = '\0';
}

static void mivf_append_subtitle_text(char *dst, const char *src) {
    size_t dl;
    size_t sl;

    if (!dst || !src || !*src) {
        return;
    }

    dl = strlen(dst);
    sl = strlen(src);

    if (dl > 0) {
        if (dl + 3 >= MIVF_SUBTITLE_MAX_TEXT) {
            return;
        }

        strcat(dst, " | ");
        dl += 3;
    }

    if (dl + sl >= MIVF_SUBTITLE_MAX_TEXT) {
        strncat(dst, src, MIVF_SUBTITLE_MAX_TEXT - dl - 1);
        dst[MIVF_SUBTITLE_MAX_TEXT - 1] = '\0';
        return;
    }

    strcat(dst, src);
}

static bool mivf_subtitles_reserve(MivfSubtitles *subs, uint32_t needed) {
    MivfSubtitleEntry *next;
    uint32_t next_capacity;

    if (!subs) {
        return false;
    }

    if (needed <= subs->capacity) {
        return true;
    }

    next_capacity = subs->capacity ? subs->capacity * 2u : 128u;

    while (next_capacity < needed) {
        next_capacity *= 2u;
    }

    if (next_capacity > 8192u) {
        return false;
    }

    next = (MivfSubtitleEntry *)realloc(
        subs->entries,
        sizeof(MivfSubtitleEntry) * next_capacity
    );

    if (!next) {
        return false;
    }

    subs->entries = next;
    subs->capacity = next_capacity;
    return true;
}

static bool mivf_subtitles_add(
    MivfSubtitles *subs,
    uint32_t start_ms,
    uint32_t end_ms,
    const char *text
) {
    MivfSubtitleEntry *e;

    if (!subs || !text || !*text) {
        return false;
    }

    if (!mivf_subtitles_reserve(subs, subs->count + 1u)) {
        return false;
    }

    e = &subs->entries[subs->count++];
    e->start_ms = start_ms;
    e->end_ms = end_ms;

    strncpy(e->text, text, MIVF_SUBTITLE_MAX_TEXT - 1);
    e->text[MIVF_SUBTITLE_MAX_TEXT - 1] = '\0';

    return true;
}

void MIVF_SubtitlesInit(MivfSubtitles *subs) {
    if (!subs) {
        return;
    }

    memset(subs, 0, sizeof(*subs));
}

void MIVF_SubtitlesFree(MivfSubtitles *subs) {
    if (!subs) {
        return;
    }

    free(subs->entries);
    memset(subs, 0, sizeof(*subs));
}

bool MIVF_SubtitlesMakeSidecarPath(
    const char *video_path,
    char *out,
    size_t out_sz
) {
    const char *dot;
    size_t base_len;

    if (!video_path || !out || out_sz == 0) {
        return false;
    }

    if (strlen(video_path) >= MIVF_SUBTITLE_MAX_PATH) {
        return false;
    }

    dot = strrchr(video_path, '.');

    if (dot) {
        base_len = (size_t)(dot - video_path);
    } else {
        base_len = strlen(video_path);
    }

    if (base_len + 4 + 1 > out_sz) {
        return false;
    }

    memcpy(out, video_path, base_len);
    out[base_len] = '\0';
    strcat(out, ".srt");

    return true;
}

bool MIVF_SubtitlesLoadForVideo(MivfSubtitles *subs, const char *video_path) {
    char srt_path[MIVF_SUBTITLE_MAX_PATH];

    if (!MIVF_SubtitlesMakeSidecarPath(video_path, srt_path, sizeof(srt_path))) {
        return false;
    }

    return MIVF_SubtitlesLoadSrt(subs, srt_path);
}

bool MIVF_SubtitlesLoadSrt(MivfSubtitles *subs, const char *srt_path) {
    FILE *f;
    char line[512];
    char text[MIVF_SUBTITLE_MAX_TEXT];
    uint32_t start_ms = 0;
    uint32_t end_ms = 0;
    bool have_range = false;

    if (!subs || !srt_path || !*srt_path) {
        return false;
    }

    MIVF_SubtitlesFree(subs);
    MIVF_SubtitlesInit(subs);

    f = fopen(srt_path, "r");

    if (!f) {
        return false;
    }

    text[0] = '\0';

    while (fgets(line, sizeof(line), f)) {
        bool index_line = true;
        size_t i;

        line[strcspn(line, "\r\n")] = '\0';
        mivf_subtitle_trim_inplace(line);

        if (line[0] == '\0') {
            if (have_range && text[0]) {
                mivf_strip_basic_tags(text);
                mivf_subtitles_add(subs, start_ms, end_ms, text);
            }

            have_range = false;
            text[0] = '\0';
            continue;
        }

        for (i = 0; line[i]; i++) {
            if (!isdigit((unsigned char)line[i])) {
                index_line = false;
                break;
            }
        }

        if (index_line) {
            continue;
        }

        if (strstr(line, "-->")) {
            if (have_range && text[0]) {
                mivf_strip_basic_tags(text);
                mivf_subtitles_add(subs, start_ms, end_ms, text);
                text[0] = '\0';
            }

            have_range = mivf_parse_srt_range(line, &start_ms, &end_ms);
            continue;
        }

        if (have_range) {
            mivf_append_subtitle_text(text, line);
        }
    }

    if (have_range && text[0]) {
        mivf_strip_basic_tags(text);
        mivf_subtitles_add(subs, start_ms, end_ms, text);
    }

    fclose(f);

    subs->cursor = 0;
    subs->loaded = subs->count > 0;

    if (!subs->loaded) {
        MIVF_SubtitlesFree(subs);
        return false;
    }

    return true;
}

const char *MIVF_SubtitlesTextAtMs(MivfSubtitles *subs, uint32_t now_ms) {
    uint32_t i;

    if (!subs || !subs->loaded || subs->count == 0 || !subs->entries) {
        return NULL;
    }

    if (subs->cursor < subs->count) {
        MivfSubtitleEntry *e = &subs->entries[subs->cursor];

        if (now_ms >= e->start_ms && now_ms < e->end_ms) {
            return e->text;
        }

        while (subs->cursor + 1u < subs->count &&
               now_ms >= subs->entries[subs->cursor].end_ms) {
            subs->cursor++;
            e = &subs->entries[subs->cursor];

            if (now_ms >= e->start_ms && now_ms < e->end_ms) {
                return e->text;
            }
        }
    }

    for (i = 0; i < subs->count; i++) {
        MivfSubtitleEntry *e = &subs->entries[i];

        if (now_ms >= e->start_ms && now_ms < e->end_ms) {
            subs->cursor = i;
            return e->text;
        }
    }

    return NULL;
}
