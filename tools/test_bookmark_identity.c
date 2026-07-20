/*
    Host-compilable test harness for source/mivf_bookmark_io.h.

    Lives in tools/, not source/, for the same reason as
    test_hfix56_gain.c: the 3DS Makefile globs every .c file under
    source/ for the player build, and a second main() there would
    collide with main.c's real main().

    Covers the two defects the bookmark-hardening pass fixes:
      1. basename-only identity collisions (two different full paths
         sharing a basename used to resolve to the same bookmark file);
      2. non-atomic writes (a crash/interruption mid-write could leave a
         truncated or missing bookmark, destroying the *previous*
         successfully-saved position, not just the new one).

    Build (plain gcc, no devkitARM/libctru needed):
        gcc -O2 -std=c99 -Wall -Wextra -Werror -o test_bookmark_identity tools/test_bookmark_identity.c
        ./test_bookmark_identity
*/
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "../source/mivf_bookmark_io.h"

static int g_failures = 0;

#define CHECK(cond, fmt, ...) \
    do { \
        if (!(cond)) { \
            printf("FAIL: " fmt "\n", ##__VA_ARGS__); \
            g_failures++; \
        } \
    } while (0)

/* --- 1. Hash determinism and distribution --------------------------- */

static void test_hash_determinism(void) {
    uint32_t a = mivf_fnv1a32("sdmc:/mivf/ShowA/ep01.mivf");
    uint32_t b = mivf_fnv1a32("sdmc:/mivf/ShowA/ep01.mivf");
    CHECK(a == b, "hash must be deterministic for identical input");

    CHECK(mivf_fnv1a32("") == 2166136261u, "empty string must hash to the FNV-1a offset basis");
    CHECK(mivf_fnv1a32(NULL) == 2166136261u, "NULL must be handled safely, same as empty string");
}

static void test_hash_disambiguates_shared_basenames(void) {
    /* The exact collision scenario the previous basename-only scheme
       could not distinguish. */
    static const char *paths[] = {
        "sdmc:/mivf/ShowA/ep01.mivf",
        "sdmc:/mivf/ShowB/ep01.mivf",
        "sdmc:/mivf/ShowC/Season1/ep01.mivf",
        "sdmc:/mivf/ep01.mivf",
    };
    size_t i, j;
    int distinct_pairs = 0;

    for (i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        for (j = i + 1; j < sizeof(paths) / sizeof(paths[0]); j++) {
            if (mivf_fnv1a32(paths[i]) != mivf_fnv1a32(paths[j])) {
                distinct_pairs++;
            } else {
                printf("FAIL: hash collision between \"%s\" and \"%s\"\n", paths[i], paths[j]);
                g_failures++;
            }
        }
    }

    CHECK(distinct_pairs == 6, "all 4 same-basename-family paths must hash pairwise-distinct (got %d/6)", distinct_pairs);
}

/* --- 2. Identity path construction ------------------------------------ */

static void test_identity_path_disambiguates(void) {
    char path_a[256];
    char path_b[256];

    mivf_bookmark_identity_path("sdmc:/appdata/bookmarks", "sdmc:/mivf/ShowA/ep01.mivf", "ep01.mivf", "bookmark",
                                 path_a, sizeof(path_a));
    mivf_bookmark_identity_path("sdmc:/appdata/bookmarks", "sdmc:/mivf/ShowB/ep01.mivf", "ep01.mivf", "bookmark",
                                 path_b, sizeof(path_b));

    CHECK(path_a[0] != 0 && path_b[0] != 0, "both identity paths must be non-empty");
    CHECK(strcmp(path_a, path_b) != 0,
          "two different shows' same-named episode must resolve to DIFFERENT bookmark files (this is the regression test for the original bug): a=%s b=%s",
          path_a, path_b);
    /* Both must still contain the human-readable basename for debuggability. */
    CHECK(strstr(path_a, "ep01.mivf") != NULL, "identity path must retain the basename for on-SD-card debuggability");
}

static void test_identity_path_deterministic(void) {
    char first[256];
    char second[256];

    mivf_bookmark_identity_path("sdmc:/appdata/bookmarks", "sdmc:/mivf/movie.mivf", "movie.mivf", "bookmark",
                                 first, sizeof(first));
    mivf_bookmark_identity_path("sdmc:/appdata/bookmarks", "sdmc:/mivf/movie.mivf", "movie.mivf", "bookmark",
                                 second, sizeof(second));

    CHECK(strcmp(first, second) == 0, "identical inputs must always produce the identical identity path (required for load/save round-trip)");
}

static void test_identity_path_null_safety(void) {
    char out[64];
    out[0] = 'X';
    mivf_bookmark_identity_path(NULL, "sdmc:/mivf/movie.mivf", "movie.mivf", "bookmark", out, sizeof(out));
    CHECK(out[0] == 0, "NULL root must produce an empty string, not a crash");

    out[0] = 'X';
    mivf_bookmark_identity_path("root", NULL, "movie.mivf", "bookmark", out, sizeof(out));
    CHECK(out[0] == 0, "NULL full_path must produce an empty string, not a crash");

    mivf_bookmark_identity_path("root", "path", "base", "suffix", NULL, 0);
    /* must not crash */
}

/* --- 3. Atomic write: round-trip, overwrite, and interruption safety - */

static const char *SCRATCH_DIR = "bookmark_test_scratch";

static void ensure_scratch_dir(void) {
#if defined(_WIN32)
    mkdir(SCRATCH_DIR);
#else
    mkdir(SCRATCH_DIR, 0777);
#endif
}

static int read_whole_file(const char *path, char *out, size_t out_sz) {
    FILE *fp = fopen(path, "rb");
    size_t n;
    if (!fp) {
        return 0;
    }
    n = fread(out, 1, out_sz - 1, fp);
    out[n] = 0;
    fclose(fp);
    return 1;
}

static int file_exists(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (fp) {
        fclose(fp);
        return 1;
    }
    return 0;
}

static void test_atomic_write_round_trip(void) {
    char path[256];
    char readback[256];
    snprintf(path, sizeof(path), "%s/roundtrip.bookmark", SCRATCH_DIR);
    remove(path);

    CHECK(mivf_atomic_write_text(path, "sdmc:/mivf/movie.mivf\n1234\n") == true, "atomic write must report success");
    CHECK(read_whole_file(path, readback, sizeof(readback)) == 1, "written file must be readable");
    CHECK(strcmp(readback, "sdmc:/mivf/movie.mivf\n1234\n") == 0, "readback content must exactly match what was written, got: %s", readback);

    remove(path);
}

static void test_atomic_write_leaves_no_stray_tmp(void) {
    char path[256];
    char tmp_path[300];
    snprintf(path, sizeof(path), "%s/notemp.bookmark", SCRATCH_DIR);
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    remove(path);
    remove(tmp_path);

    mivf_atomic_write_text(path, "content\n");
    CHECK(!file_exists(tmp_path), "a successful atomic write must not leave a .tmp file behind");

    remove(path);
}

static void test_atomic_write_overwrite_semantics(void) {
    char path[256];
    char readback[256];
    snprintf(path, sizeof(path), "%s/overwrite.bookmark", SCRATCH_DIR);
    remove(path);

    mivf_atomic_write_text(path, "first\n");
    mivf_atomic_write_text(path, "second\n");

    CHECK(read_whole_file(path, readback, sizeof(readback)) == 1, "file must exist after two writes");
    CHECK(strcmp(readback, "second\n") == 0, "second atomic write must fully replace the first (rename-over-existing), got: %s", readback);

    remove(path);
}

/*
    This is the core interruption-safety property: an INCOMPLETE write
    (simulated by leaving a stray/partial .tmp file behind without ever
    calling rename -- exactly what a crash between fclose() and rename()
    on the real device would leave) must never disturb the previously
    committed, fully-written file. The old fopen(path,"wb") design had
    no such guarantee: an interruption during the actual write call
    would corrupt `path` directly.
*/
static void test_atomic_write_interruption_preserves_previous_content(void) {
    char path[256];
    char tmp_path[300];
    char readback[256];
    FILE *fp;

    snprintf(path, sizeof(path), "%s/interrupt.bookmark", SCRATCH_DIR);
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    remove(path);
    remove(tmp_path);

    /* Step 1: a real, successful save (this is what a prior clean save
       already left on disk). */
    CHECK(mivf_atomic_write_text(path, "sdmc:/mivf/original.mivf\n500\n") == true, "initial save must succeed");

    /* Step 2: simulate a write that was interrupted before rename() --
       a partially-written temp file exists, but rename() never ran. */
    fp = fopen(tmp_path, "wb");
    CHECK(fp != NULL, "must be able to create a simulated partial temp file");
    if (fp) {
        fputs("sdmc:/mivf/original.mivf\n9", fp); /* deliberately truncated, no trailing newline/rename */
        fclose(fp);
    }

    /* Step 3: the real file must be completely unaffected by the
       abandoned temp file. */
    CHECK(read_whole_file(path, readback, sizeof(readback)) == 1, "original file must still exist after a simulated interrupted write");
    CHECK(strcmp(readback, "sdmc:/mivf/original.mivf\n500\n") == 0,
          "an interrupted write must NOT corrupt the previously committed bookmark; expected original content, got: %s", readback);

    /* Step 4: a subsequent real save must still succeed and fully
       replace both the stale temp file and the old content (proving
       the stray temp file left behind by the simulated crash doesn't
       wedge future saves). */
    CHECK(mivf_atomic_write_text(path, "sdmc:/mivf/original.mivf\n999\n") == true, "a save after a simulated interruption must still succeed");
    CHECK(read_whole_file(path, readback, sizeof(readback)) == 1, "file must be readable after recovery save");
    CHECK(strcmp(readback, "sdmc:/mivf/original.mivf\n999\n") == 0, "recovery save must take effect, got: %s", readback);

    remove(path);
    remove(tmp_path);
}

/*
    Simulates a long periodic-checkpoint session: many sequential
    overwrites of the same destination (a 90-minute movie at the
    player's 20-second interval is ~270 writes; 50 here is a
    representative stress pass, not an attempt at the real count).
    Verifies: every write lands correctly (no corruption accumulating
    across repeated open/rename cycles), no .tmp file survives between
    writes, and no other stray file accumulates in the scratch dir.
*/
static void test_atomic_write_repeated_checkpoint_session(void) {
    char path[256];
    char tmp_path[300];
    char readback[256];
    char expected[256];
    int i;
    int all_correct = 1;

    snprintf(path, sizeof(path), "%s/periodic_session.bookmark", SCRATCH_DIR);
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    remove(path);
    remove(tmp_path);

    for (i = 0; i < 50; i++) {
        snprintf(expected, sizeof(expected), "sdmc:/mivf/session.mivf\n%d\n", i * 20 * 30 /* ~20s at 30fps */);
        if (!mivf_atomic_write_text(path, expected)) {
            all_correct = 0;
            break;
        }
        if (!read_whole_file(path, readback, sizeof(readback)) || strcmp(readback, expected) != 0) {
            all_correct = 0;
            break;
        }
        if (file_exists(tmp_path)) {
            all_correct = 0;
            printf("FAIL: stray .tmp file present after checkpoint %d\n", i);
            g_failures++;
        }
    }

    CHECK(all_correct, "50 sequential checkpoint-style writes must each land correctly with no accumulating corruption");

    /* Final content must be exactly the last write, not some earlier one. */
    snprintf(expected, sizeof(expected), "sdmc:/mivf/session.mivf\n%d\n", 49 * 20 * 30);
    CHECK(read_whole_file(path, readback, sizeof(readback)) == 1 && strcmp(readback, expected) == 0,
          "after 50 checkpoints, content must reflect only the final write, got: %s", readback);

    remove(path);
    remove(tmp_path);
}

static void test_atomic_write_null_safety(void) {
    CHECK(mivf_atomic_write_text(NULL, "text") == false, "NULL path must fail safely, not crash");
    CHECK(mivf_atomic_write_text("", "text") == false, "empty path must fail safely, not crash");

    {
        char path[256];
        snprintf(path, sizeof(path), "%s/nulltext.bookmark", SCRATCH_DIR);
        remove(path);
        CHECK(mivf_atomic_write_text(path, NULL) == false, "NULL text must fail safely, not crash");
        CHECK(!file_exists(path), "a failed NULL-text write must not create a file");
    }
}

int main(void) {
    ensure_scratch_dir();

    test_hash_determinism();
    test_hash_disambiguates_shared_basenames();
    test_identity_path_disambiguates();
    test_identity_path_deterministic();
    test_identity_path_null_safety();
    test_atomic_write_round_trip();
    test_atomic_write_leaves_no_stray_tmp();
    test_atomic_write_overwrite_semantics();
    test_atomic_write_interruption_preserves_previous_content();
    test_atomic_write_repeated_checkpoint_session();
    test_atomic_write_null_safety();

    if (g_failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
