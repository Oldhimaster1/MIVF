/*
    P.A1: host-compilable test harness for hfix56_gain.h.

    Lives in tools/, NOT source/ -- the 3DS Makefile globs every .c file
    under source/ for the player build, and this file's own main() would
    collide with
    main.c's real main() if it sat alongside it (confirmed: an earlier
    placement under source/ produced a real link error, "multiple
    definition of `main'", during the P.A1 build). tools/ already hosts
    other host-compiled, non-3DS C programs (m2y2_transcode.c,
    mivf_ia4m_stream.c), so this follows established convention.

    Build (plain gcc, no devkitARM/libctru needed -- this is pure C99):
        gcc -O2 -std=c99 -Wall -o test_hfix56_gain tools/test_hfix56_gain.c
        ./test_hfix56_gain

    This is the ONLY place hfix56_apply_gain_one is duplicated outside
    main.c -- `legacy_apply_gain_one` below is a byte-for-byte copy of the
    real function (main.c, near line 5896, static inline int
    hfix56_apply_gain_one(int sample)), kept here ONLY so the compatibility
    tests can assert real equivalence against the *actual shipped logic*,
    not a paraphrase of it. If main.c's hfix56_apply_gain_one ever changes,
    this copy must be updated to match, or the compatibility tests below
    are testing against a stale target.
*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../source/hfix56_gain.h"

static int g_failures = 0;

#define CHECK(cond, fmt, ...) \
    do { \
        if (!(cond)) { \
            printf("FAIL: " fmt "\n", ##__VA_ARGS__); \
            g_failures++; \
        } \
    } while (0)

/* Verbatim copy of main.c's hfix56_apply_gain_one (its own clamp helper
   inlined here as clamp_s16, matching main.c's hfix56_clamp_s16_i32). */
static int32_t clamp_s16(int32_t v) {
    if (v < -32768) return -32768;
    if (v > 32767) return 32767;
    return v;
}

static int legacy_apply_gain_one(int sample, int volume_percent, int limiter_enabled) {
    int v = (sample * volume_percent) / 100;

    if (limiter_enabled) {
        const int knee = 28000;
        if (v > knee) {
            v = knee + ((v - knee) >> 2);
        } else if (v < -knee) {
            v = -knee + ((v + knee) >> 2);
        }
    }

    return (int)clamp_s16(v);
}

/* --- 1. Pure gain/clamp tests ------------------------------------------ */

static void test_basic_values(void) {
    CHECK(hfix56_apply_gain_channel(0, 100, 100, 0) == 0, "sample 0 must stay 0");
    CHECK(hfix56_apply_gain_channel(32767, 100, 100, 0) == 32767, "positive max at 100/100/no-limiter must pass through");
    CHECK(hfix56_apply_gain_channel(-32768, 100, 100, 0) == -32768, "negative max at 100/100/no-limiter must pass through");
    CHECK(hfix56_apply_gain_channel(100, 100, 100, 0) == 100, "small positive sample unchanged at 100/100");
    CHECK(hfix56_apply_gain_channel(-100, 100, 100, 0) == -100, "small negative sample unchanged at 100/100");
}

static void test_master_and_channel_combinations(void) {
    /* master 80, channel 50 -> effective 40% */
    CHECK(hfix56_apply_gain_channel(10000, 80, 50, 0) == (int16_t)((10000LL * 80 * 50) / 10000),
          "master80/channel50 combined formula");
    CHECK(hfix56_apply_gain_channel(1000, 0, 100, 0) == 0, "master 0 must silence regardless of channel");
    CHECK(hfix56_apply_gain_channel(1000, 100, 0, 0) == 0, "channel 0 must silence that channel");
    CHECK(hfix56_apply_gain_channel(1000, 0, 0, 0) == 0, "both zero must silence");
    /* existing master boost range (>100) combined with channel attenuation */
    CHECK(hfix56_apply_gain_channel(1000, 200, 50, 0) == (int16_t)((1000LL * 200 * 50) / 10000),
          "master boost (200%%) with channel attenuation (50%%)");
}

static void test_limiter_boundary(void) {
    /* Above knee (28000) with limiter on must compress, matching the exact
       legacy knee formula: knee + ((v-knee)>>2). */
    int without_limiter = hfix56_apply_gain_channel(30000, 100, 100, 0);
    int with_limiter = hfix56_apply_gain_channel(30000, 100, 100, 1);
    CHECK(without_limiter == 30000, "without limiter, no compression below s16 max");
    CHECK(with_limiter == 28000 + ((30000 - 28000) >> 2), "limiter compresses exactly at the known knee formula");
    CHECK(with_limiter < without_limiter, "limiter must reduce a sample above the knee");

    /* Just below the knee: limiter must be a no-op. */
    CHECK(hfix56_apply_gain_channel(27999, 100, 100, 1) == 27999, "just below the knee, limiter must not alter the sample");
    CHECK(hfix56_apply_gain_channel(28000, 100, 100, 1) == 28000, "exactly at the knee, limiter must not alter the sample");
    CHECK(hfix56_apply_gain_channel(28001, 100, 100, 1) == 28000 + ((1) >> 2), "just above the knee, limiter engages");

    /* Symmetric negative knee. */
    CHECK(hfix56_apply_gain_channel(-30000, 100, 100, 1) == -28000 + ((-30000 + 28000) >> 2),
          "negative knee compresses symmetrically");
}

static void test_invalid_channel_values_clamp_safely(void) {
    CHECK(hfix56_apply_gain_channel(10000, 100, -50, 0) == hfix56_apply_gain_channel(10000, 100, 0, 0),
          "negative channel_percent must clamp to 0, never go negative-gain/invert");
    CHECK(hfix56_apply_gain_channel(10000, 100, 500, 0) == hfix56_apply_gain_channel(10000, 100, 100, 0),
          "channel_percent above 100 must clamp to 100, never amplify beyond master alone");
}

/* --- 2. Compatibility tests: new(sample, master, channel=100) ==
   legacy(sample, master) -- the single most important regression
   property. Broad sample x master matrix, not a handful of spot checks. */

static void test_legacy_equivalence(void) {
    static const int samples[] = {0, 1, -1, 100, -100, 1000, -1000, 12345, -12345,
                                   27999, 28000, 28001, 30000, -30000, 32767, -32768};
    static const int masters[] = {0, 1, 10, 50, 80, 99, 100, 101, 150, 200, 250, 300};
    int limiter, mi, si;

    for (limiter = 0; limiter <= 1; limiter++) {
        for (mi = 0; mi < (int)(sizeof(masters) / sizeof(masters[0])); mi++) {
            for (si = 0; si < (int)(sizeof(samples) / sizeof(samples[0])); si++) {
                int sample = samples[si];
                int master = masters[mi];
                int expected = legacy_apply_gain_one(sample, master, limiter);
                int actual = hfix56_apply_gain_channel((int16_t)sample, master, 100, limiter);
                CHECK(actual == expected,
                      "equivalence failed: sample=%d master=%d limiter=%d -> new=%d legacy=%d",
                      sample, master, limiter, actual, expected);
            }
        }
    }
}

/* --- 3. Stereo interleaving / channel-order simulation ------------------ */

static void test_stereo_interleave_pattern(void) {
    /* Simulates the real main.c integration pattern (in[i*2+0]=L,
       in[i*2+1]=R) without touching main.c -- verifies the *design*, not
       main.c's literal bytes (that's what the build+manual review cover).
       Uses identical L/R input magnitudes (mirrored sign) so a channel
       swap would be silently invisible if we only compared against a
       second call with the same arguments -- instead this checks against
       the hand-computed formula directly, and against what the *other*
       channel's gain would have produced, to genuinely catch a swap. */
    int16_t in[8] = {1000, 1000, 2000, 2000, 3000, 3000, 4000, 4000};
    int16_t out[8];
    int master = 100, left = 80, right = 40, limiter = 0;
    int i;

    for (i = 0; i < 4; i++) {
        out[i * 2 + 0] = hfix56_apply_gain_channel(in[i * 2 + 0], master, left, limiter);
        out[i * 2 + 1] = hfix56_apply_gain_channel(in[i * 2 + 1], master, right, limiter);
    }

    for (i = 0; i < 4; i++) {
        int32_t hand_left = (int32_t)((in[i * 2 + 0] * (int64_t)master * left) / 10000);
        int32_t hand_right = (int32_t)((in[i * 2 + 1] * (int64_t)master * right) / 10000);
        int16_t swapped_left = hfix56_apply_gain_channel(in[i * 2 + 0], master, right, limiter);

        CHECK(out[i * 2 + 0] == (int16_t)hand_left, "frame %d: left output must match the hand-computed 80%% formula", i);
        CHECK(out[i * 2 + 1] == (int16_t)hand_right, "frame %d: right output must match the hand-computed 40%% formula", i);
        CHECK(out[i * 2 + 0] != swapped_left, "frame %d: left sample must use LEFT gain (80%%), not right gain (40%%) -- catches a channel swap", i);
    }
    CHECK(sizeof(out) / sizeof(out[0]) == sizeof(in) / sizeof(in[0]), "sample count must be preserved (8 in, 8 out)");
}

/* --- 4. Mono behavior: no L/R identity at all, master-only path unaffected */

static void test_mono_path_uses_master_only(void) {
    /* True mono output has no per-channel identity -- the design keeps
       mono samples on the *existing* hfix56_apply_gain_one path
       (channel=100 equivalent), never applying left/right gain. This test
       documents and enforces that design intent at the pure-function
       level. */
    int sample = 12345, master = 80;
    CHECK(hfix56_apply_gain_channel((int16_t)sample, master, 100, 0) == legacy_apply_gain_one(sample, master, 0),
          "mono path (channel=100, i.e. 'no per-channel attenuation') must match the master-only legacy path");
}

int main(void) {
    test_basic_values();
    test_master_and_channel_combinations();
    test_limiter_boundary();
    test_invalid_channel_values_clamp_safely();
    test_legacy_equivalence();
    test_stereo_interleave_pattern();
    test_mono_path_uses_master_only();

    if (g_failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
