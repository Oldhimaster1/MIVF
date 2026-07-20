#pragma once

/*
    Pure, platform-independent bookmark persistence primitives.

    No <3ds.h> dependency by design -- kept host-testable, see
    tools/test_bookmark_identity.c. source/mivf_settings.c is the only
    caller; it supplies the real MIVF_BOOKMARK_PATH-style roots.
*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
    FNV-1a 32-bit over the full path string. Used to disambiguate
    same-named files that live in different folders: keying a bookmark
    off basename alone (e.g. "ep01.mivf") let two different shows'
    same-named episodes silently overwrite one another's resume position
    -- whichever was saved most recently "won" the shared file, and the
    other title's bookmark reads back as a mismatched identity and is
    silently discarded by the caller.
*/
static inline uint32_t mivf_fnv1a32(const char *s) {
    uint32_t h = 2166136261u;
    const unsigned char *p = (const unsigned char *)s;

    if (!p) {
        return h;
    }

    while (*p) {
        h ^= *p++;
        h *= 16777619u;
    }

    return h;
}

/*
    Build a collision-resistant identity path: "<root>.<basename>.<hash8>.<suffix>".
    `full_path` is the source of truth for identity (hashed in full);
    `basename` is included only for human-readable debuggability when
    browsing the SD card, and never used to disambiguate by itself.
*/
static inline void mivf_bookmark_identity_path(const char *root, const char *full_path,
                                                const char *basename, const char *suffix,
                                                char *out, size_t out_sz) {
    if (!out || out_sz == 0) {
        return;
    }

    out[0] = 0;
    if (!root || !full_path || !basename || !suffix) {
        return;
    }

    snprintf(out, out_sz, "%s.%s.%08lx.%s", root, basename,
             (unsigned long)mivf_fnv1a32(full_path), suffix);
}

/*
    Atomically replace `path`'s contents with `text`: write to a
    same-directory ".tmp" file, close it, then rename() over the
    destination. A crash or power loss mid-write corrupts only the
    abandoned temp file -- the previous contents of `path` (if any)
    survive untouched until the rename, which is the filesystem's one
    atomic step. Returns false, and leaves `path` untouched, on any
    failure (including a failed rename, which is retried once via
    remove-then-rename for filesystem layers that refuse to replace an
    existing file directly).
*/
static inline bool mivf_atomic_write_text(const char *path, const char *text) {
    char tmp_path[560];
    FILE *fp;

    if (!path || !*path || !text) {
        return false;
    }

    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    fp = fopen(tmp_path, "wb");
    if (!fp) {
        return false;
    }

    if (fputs(text, fp) == EOF) {
        fclose(fp);
        remove(tmp_path);
        return false;
    }

    if (fclose(fp) != 0) {
        remove(tmp_path);
        return false;
    }

    if (rename(tmp_path, path) != 0) {
        remove(path);
        if (rename(tmp_path, path) != 0) {
            remove(tmp_path);
            return false;
        }
    }

    return true;
}
