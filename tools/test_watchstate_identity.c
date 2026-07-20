/*
    Host-compilable test harness for the watch-state identity scheme
    (source/mivf_settings.c's mivf_make_watchstate_path, which is a thin
    static wrapper this test cannot call directly -- so it exercises the
    exact same call shape via mivf_bookmark_identity_path directly,
    proving watch-state's reuse of the bookmark identity primitive is
    collision-resistant the same way test_bookmark_identity.c already
    proves for bookmarks themselves).

    MIVF_WatchStateLoad/Save/Clear themselves cannot be host-tested here:
    they live in mivf_settings.c, which includes <3ds.h> via
    mivf_settings.h -- same documented limitation as other player-only
    logic this session (Touch Lock, Screensaver settings). This test
    covers the one piece that IS pure and host-testable: the identity
    scheme watch-state deliberately reuses rather than duplicates.

    Build (plain gcc, no devkitARM/libctru needed):
        gcc -O2 -std=c99 -Wall -Wextra -Werror -o test_watchstate_identity tools/test_watchstate_identity.c
        ./test_watchstate_identity
*/
#include <stdio.h>
#include <string.h>

#include "../source/mivf_bookmark_io.h"

static int g_failures = 0;

#define CHECK(cond, fmt, ...) \
    do { \
        if (!(cond)) { \
            printf("FAIL: " fmt "\n", ##__VA_ARGS__); \
            g_failures++; \
        } \
    } while (0)

#define WATCHSTATE_ROOT "sdmc:/3ds/mivf_player_3ds/appdata/watchstate"
#define BOOKMARK_ROOT "sdmc:/3ds/mivf_player_3ds/appdata/bookmarks"

static void test_watchstate_paths_disambiguate_shared_basenames(void) {
    char a[512], b[512], c[512];

    mivf_bookmark_identity_path(WATCHSTATE_ROOT, "sdmc:/mivf/ShowA/ep01.mivf", "ep01.mivf", "watchstate", a, sizeof(a));
    mivf_bookmark_identity_path(WATCHSTATE_ROOT, "sdmc:/mivf/ShowB/ep01.mivf", "ep01.mivf", "watchstate", b, sizeof(b));
    mivf_bookmark_identity_path(WATCHSTATE_ROOT, "sdmc:/mivf/ep01.mivf", "ep01.mivf", "watchstate", c, sizeof(c));

    CHECK(strcmp(a, b) != 0, "two different shows' same-named episode must not share a watch-state path");
    CHECK(strcmp(a, c) != 0, "a nested and a root-level same-named file must not share a watch-state path");
    CHECK(strcmp(b, c) != 0, "all three distinct sources must resolve to distinct paths");
}

static void test_watchstate_path_is_deterministic(void) {
    char a[512], b[512];
    mivf_bookmark_identity_path(WATCHSTATE_ROOT, "sdmc:/mivf/movie.mivf", "movie.mivf", "watchstate", a, sizeof(a));
    mivf_bookmark_identity_path(WATCHSTATE_ROOT, "sdmc:/mivf/movie.mivf", "movie.mivf", "watchstate", b, sizeof(b));
    CHECK(strcmp(a, b) == 0, "the same source must always resolve to the same watch-state path");
}

static void test_watchstate_and_bookmark_paths_never_collide(void) {
    /* Different roots AND different suffixes -- belt-and-suspenders:
       even if some future root string were accidentally reused, the
       suffix alone still guarantees the two stores can't cross-write. */
    char watch[512], bookmark[512];
    mivf_bookmark_identity_path(WATCHSTATE_ROOT, "sdmc:/mivf/movie.mivf", "movie.mivf", "watchstate", watch, sizeof(watch));
    mivf_bookmark_identity_path(BOOKMARK_ROOT, "sdmc:/mivf/movie.mivf", "movie.mivf", "bookmark", bookmark, sizeof(bookmark));
    CHECK(strcmp(watch, bookmark) != 0, "watch-state and bookmark paths for the same source must never collide");
}

static void test_watchstate_path_reflects_the_real_basename(void) {
    char out[512];
    mivf_bookmark_identity_path(WATCHSTATE_ROOT, "sdmc:/mivf/Show/S01E02.mivf", "S01E02.mivf", "watchstate", out, sizeof(out));
    CHECK(strstr(out, "S01E02.mivf") != NULL, "path should stay human-debuggable via the basename component: got %s", out);
    CHECK(strstr(out, ".watchstate") != NULL, "path should end with the watchstate suffix: got %s", out);
}

int main(void) {
    test_watchstate_paths_disambiguate_shared_basenames();
    test_watchstate_path_is_deterministic();
    test_watchstate_and_bookmark_paths_never_collide();
    test_watchstate_path_reflects_the_real_basename();

    if (g_failures == 0) {
        printf("ALL WATCHSTATE IDENTITY TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
