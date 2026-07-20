#pragma once

#include <3ds.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "mivf_io_ring.h"

#define MIVF_PAGE_HEADER_SIZE 32
#define MIVF_MAX_PAGE_PAYLOAD  (512 * 1024)

typedef struct {
    u8 flags;
    u32 no;
    u32 payload;
    u16 packets;
    u32 crc;
} MivfPage;

typedef struct {
    MivfPage pg;
    u8 *data;
    u32 size;
} MivfPageView;

typedef struct {
    MivfIoRing ring;

    u8 *page_buf;
    u32 page_cap;

    u64 read_offset;
    u64 media_end_offset;

    bool eof;
    bool error;
} MivfStream;

bool mivf_stream_open(MivfStream *s, FILE *f);
void mivf_stream_close(MivfStream *s);

bool mivf_stream_reseek(MivfStream *s, long offset);
void mivf_stream_set_media_end_offset(MivfStream *s, u64 media_end_offset);

bool mivf_stream_next_page(MivfStream *s, MivfPageView *out);
void mivf_stream_release_page(MivfStream *s, MivfPageView *page);
