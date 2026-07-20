#ifndef MIVF_SUBTITLES_H
#define MIVF_SUBTITLES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MIVF_SUBTITLE_MAX_TEXT 192
#define MIVF_SUBTITLE_MAX_PATH 512

typedef struct MivfSubtitleEntry {
    uint32_t start_ms;
    uint32_t end_ms;
    char text[MIVF_SUBTITLE_MAX_TEXT];
} MivfSubtitleEntry;

typedef struct MivfSubtitles {
    MivfSubtitleEntry *entries;
    uint32_t count;
    uint32_t capacity;
    uint32_t cursor;
    bool loaded;
} MivfSubtitles;

void MIVF_SubtitlesInit(MivfSubtitles *subs);
void MIVF_SubtitlesFree(MivfSubtitles *subs);

bool MIVF_SubtitlesLoadSrt(MivfSubtitles *subs, const char *srt_path);
bool MIVF_SubtitlesLoadForVideo(MivfSubtitles *subs, const char *video_path);

const char *MIVF_SubtitlesTextAtMs(MivfSubtitles *subs, uint32_t now_ms);

bool MIVF_SubtitlesMakeSidecarPath(
    const char *video_path,
    char *out,
    size_t out_sz
);

#endif /* MIVF_SUBTITLES_H */
