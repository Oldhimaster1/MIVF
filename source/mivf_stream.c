#include "mivf_stream.h"

static inline u16 mivf_le16(const u8 *p){
    return (u16)p[0] | ((u16)p[1] << 8);
}

static inline u32 mivf_le32(const u8 *p){
    return (u32)p[0] |
           ((u32)p[1] << 8) |
           ((u32)p[2] << 16) |
           ((u32)p[3] << 24);
}

bool mivf_stream_open(MivfStream *s, FILE *f){
    memset(s, 0, sizeof(*s));
    return mivf_io_ring_init(&s->ring, f);
}

void mivf_stream_close(MivfStream *s){
    mivf_io_ring_shutdown(&s->ring);

    free(s->page_buf);
    s->page_buf = NULL;
    s->page_cap = 0;
}

/* HFIX61: fast in-place seek. Keeps the reader thread + ring buffer alive and
   just repositions the file, instead of close()+open() per seek. */
bool mivf_stream_reseek(MivfStream *s, long offset){
    if(!s){
        return false;
    }

    if(!mivf_io_ring_reseek(&s->ring, offset)){
        return false;
    }

    s->eof = false;
    s->error = false;
    return true;
}

bool mivf_stream_next_page(MivfStream *s, MivfPageView *out){
    memset(out, 0, sizeof(*out));

    u8 hdr[MIVF_PAGE_HEADER_SIZE];

    if(!mivf_io_read_exact(&s->ring, hdr, MIVF_PAGE_HEADER_SIZE)){
        s->eof = true;
        return false;
    }

    if(hdr[0] != 'M' || hdr[1] != 'P'){
        s->error = true;
        return false;
    }

    MivfPage pg;
    pg.flags   = hdr[3];
    pg.no      = mivf_le32(hdr + 4);
    pg.payload = mivf_le32(hdr + 0x10);
    pg.packets = mivf_le16(hdr + 0x14);
    pg.crc     = mivf_le32(hdr + 0x18);

    if(pg.payload == 0 || pg.payload > MIVF_MAX_PAGE_PAYLOAD){
        s->error = true;
        return false;
    }

    if(pg.payload > s->page_cap){
        u8 *n = (u8*)realloc(s->page_buf, pg.payload);
        if(!n){
            s->error = true;
            return false;
        }

        s->page_buf = n;
        s->page_cap = pg.payload;
    }

    if(!mivf_io_read_exact(&s->ring, s->page_buf, pg.payload)){
        s->error = true;
        return false;
    }

    out->pg = pg;
    out->data = s->page_buf;
    out->size = pg.payload;

    return true;
}

void mivf_stream_release_page(MivfStream *s, MivfPageView *page){
    (void)s;
    (void)page;
}
