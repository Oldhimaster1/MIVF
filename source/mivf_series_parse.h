#pragma once

/*
    Pure, platform-independent series/season/episode filename parsing.

    No <3ds.h> dependency by design -- kept host-testable, see
    tools/test_series_parse.c. source/main.c is the only caller.

    Recognizes exactly one convention: "SxxEyy" (case-insensitive)
    directly in the filename. The alternate "NxM" notation is
    deliberately NOT recognized -- it collides with resolution tags like
    "1920x1080" that appear in many real filenames, and this project's
    own standing rule ("do not silently assign uncertain files to a
    series") rules out a pattern with that kind of false-positive rate.
    A file with no SxxEyy match is simply left unparsed, never guessed.
*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline bool mivf_series_is_ascii_digit(char c) {
    return c >= '0' && c <= '9';
}

/* Trims trailing separators (space/./-/_) immediately before match_pos,
   copies what remains into out, and normalizes remaining '.'/'_' to
   spaces for a human-readable series name. Empty result (nothing
   meaningful before the match) leaves out[0] = 0. */
static inline void mivf_series_extract_prefix(const char *name, size_t match_pos,
                                               char *out, size_t out_sz) {
    size_t end = match_pos;
    size_t copy_len, i;

    if (!out || out_sz == 0) {
        return;
    }
    out[0] = 0;
    if (!name) {
        return;
    }

    while (end > 0 && (name[end - 1] == ' ' || name[end - 1] == '.' ||
                        name[end - 1] == '-' || name[end - 1] == '_')) {
        end--;
    }
    if (end == 0) {
        return;
    }

    copy_len = end < out_sz - 1 ? end : out_sz - 1;
    memcpy(out, name, copy_len);
    out[copy_len] = 0;
    for (i = 0; i < copy_len; i++) {
        if (out[i] == '.' || out[i] == '_') {
            out[i] = ' ';
        }
    }
}

/* Returns true and fills series/season/episode only on a genuine
   "SxxEyy" match with a non-empty series-name prefix; otherwise returns
   false and leaves series[0]=0, *season=0, *episode=0 -- an
   unrecognized or ambiguous filename is never guessed. */
static inline bool mivf_series_parse(const char *name, char *series, size_t series_sz,
                                      uint32_t *season, uint32_t *episode) {
    size_t len, i;

    if (series_sz) {
        series[0] = 0;
    }
    if (season) *season = 0;
    if (episode) *episode = 0;

    if (!name || !season || !episode) {
        return false;
    }
    len = strlen(name);

    for (i = 0; i + 1 < len; i++) {
        char c = name[i];
        size_t j, k;
        uint32_t s = 0, e = 0;
        int s_digits = 0, e_digits = 0;

        if (c != 'S' && c != 's') {
            continue;
        }
        j = i + 1;
        while (j < len && mivf_series_is_ascii_digit(name[j]) && s_digits < 4) {
            s = s * 10u + (uint32_t)(name[j] - '0');
            j++;
            s_digits++;
        }
        if (s_digits == 0 || j >= len || (name[j] != 'E' && name[j] != 'e')) {
            continue;
        }
        k = j + 1;
        while (k < len && mivf_series_is_ascii_digit(name[k]) && e_digits < 4) {
            e = e * 10u + (uint32_t)(name[k] - '0');
            k++;
            e_digits++;
        }
        if (e_digits == 0) {
            continue;
        }

        mivf_series_extract_prefix(name, i, series, series_sz);
        if (!series[0]) {
            continue; /* nothing before the match -- refuse rather than guess */
        }
        *season = s;
        *episode = e;
        return true;
    }

    return false;
}
