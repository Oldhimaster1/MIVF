/*
    Host-compilable test harness for source/mivf_series_parse.h.

    Lives in tools/, not source/, for the same reason as
    test_bookmark_identity.c/test_hfix56_gain.c: the 3DS Makefile globs
    every .c file under source/ for the player build, and a second
    main() there would collide with main.c's real main().

    Build (plain gcc, no devkitARM/libctru needed):
        gcc -O2 -std=c99 -Wall -Wextra -Werror -o test_series_parse tools/test_series_parse.c
        ./test_series_parse
*/
#include <stdio.h>
#include <string.h>

#include "../source/mivf_series_parse.h"

static int g_failures = 0;

#define CHECK(cond, fmt, ...) \
    do { \
        if (!(cond)) { \
            printf("FAIL: " fmt "\n", ##__VA_ARGS__); \
            g_failures++; \
        } \
    } while (0)

/* --- real, well-formed patterns -------------------------------------- */

static void test_standard_dash_separated(void) {
    char series[64]; uint32_t season = 0, episode = 0;
    bool ok = mivf_series_parse("Show Name - S01E02 - Title.mivf", series, sizeof(series), &season, &episode);
    CHECK(ok, "expected a match");
    CHECK(strcmp(series, "Show Name") == 0, "expected series 'Show Name', got '%s'", series);
    CHECK(season == 1, "expected season 1, got %u", season);
    CHECK(episode == 2, "expected episode 2, got %u", episode);
}

static void test_dot_separated(void) {
    char series[64]; uint32_t season = 0, episode = 0;
    bool ok = mivf_series_parse("Show.Name.S02E10.1080p.mivf", series, sizeof(series), &season, &episode);
    CHECK(ok, "expected a match");
    CHECK(strcmp(series, "Show Name") == 0, "dots before the match should normalize to spaces, got '%s'", series);
    CHECK(season == 2 && episode == 10, "expected S02E10, got S%02uE%02u", season, episode);
}

static void test_lowercase_pattern(void) {
    char series[64]; uint32_t season = 0, episode = 0;
    bool ok = mivf_series_parse("show s03e07.mivf", series, sizeof(series), &season, &episode);
    CHECK(ok, "lowercase 's03e07' should still match");
    CHECK(season == 3 && episode == 7, "expected S03E07, got S%02uE%02u", season, episode);
}

static void test_multi_digit_numbers(void) {
    char series[64]; uint32_t season = 0, episode = 0;
    bool ok = mivf_series_parse("Long Running Show - S12E345.mivf", series, sizeof(series), &season, &episode);
    CHECK(ok, "expected a match for multi-digit season/episode");
    CHECK(season == 12 && episode == 345, "expected S12E345, got S%02uE%02u", season, episode);
}

/* --- deliberately unrecognized / refused rather than guessed ---------- */

static void test_no_pattern_is_unrecognized(void) {
    char series[64]; uint32_t season = 99, episode = 99;
    bool ok = mivf_series_parse("Just A Movie.mivf", series, sizeof(series), &season, &episode);
    CHECK(!ok, "a plain filename with no SxxEyy must not match");
    CHECK(series[0] == 0, "series must be cleared on no-match");
    CHECK(season == 0 && episode == 0, "season/episode must be reset to 0 on no-match, not left stale");
}

static void test_resolution_tag_is_not_misparsed_as_nxm(void) {
    /* This project deliberately does NOT implement "NxM" notation --
       confirms a filename containing a resolution-looking token doesn't
       accidentally produce a false match via some other path. */
    char series[64]; uint32_t season = 0, episode = 0;
    bool ok = mivf_series_parse("Movie.1920x1080.mivf", series, sizeof(series), &season, &episode);
    CHECK(!ok, "a resolution tag must never be misread as a series pattern");
}

static void test_match_with_nothing_before_it_is_refused(void) {
    char series[64]; uint32_t season = 0, episode = 0;
    bool ok = mivf_series_parse("S01E02.mivf", series, sizeof(series), &season, &episode);
    CHECK(!ok, "a match with no series-name prefix must be refused, not silently assigned an empty series");
}

static void test_s_not_followed_by_digit_is_skipped(void) {
    /* The literal letter 'S' inside "Series" itself must not falsely
       trigger -- the parser should keep scanning past it to find the
       real SxxEyy match later in the string. */
    char series[64]; uint32_t season = 0, episode = 0;
    bool ok = mivf_series_parse("Series Title - S05E01.mivf", series, sizeof(series), &season, &episode);
    CHECK(ok, "expected the real match after the false 'S' in 'Series' to still be found");
    CHECK(season == 5 && episode == 1, "expected S05E01, got S%02uE%02u", season, episode);
}

static void test_empty_and_null_are_handled_safely(void) {
    char series[64]; uint32_t season = 0, episode = 0;
    CHECK(mivf_series_parse("", series, sizeof(series), &season, &episode) == false, "empty string must not crash or match");
    CHECK(mivf_series_parse(NULL, series, sizeof(series), &season, &episode) == false, "NULL must not crash");
}

/* --- determinism -------------------------------------------------------- */

static void test_deterministic_across_repeated_calls(void) {
    char a[64], b[64]; uint32_t sa, ea, sb, eb;
    mivf_series_parse("Show - S01E01.mivf", a, sizeof(a), &sa, &ea);
    mivf_series_parse("Show - S01E01.mivf", b, sizeof(b), &sb, &eb);
    CHECK(strcmp(a, b) == 0 && sa == sb && ea == eb, "the same filename must always parse identically");
}

int main(void) {
    test_standard_dash_separated();
    test_dot_separated();
    test_lowercase_pattern();
    test_multi_digit_numbers();
    test_no_pattern_is_unrecognized();
    test_resolution_tag_is_not_misparsed_as_nxm();
    test_match_with_nothing_before_it_is_refused();
    test_s_not_followed_by_digit_is_skipped();
    test_empty_and_null_are_handled_safely();
    test_deterministic_across_repeated_calls();

    if (g_failures == 0) {
        printf("ALL SERIES PARSE TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
