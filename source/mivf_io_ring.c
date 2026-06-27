#include "mivf_io_ring.h"

static inline u32 mivf_ring_free_unsafe(const MivfIoRing *r){
    return r->size - r->fill;
}

static inline u32 mivf_ring_available_unsafe(const MivfIoRing *r){
    return r->fill;
}

static u32 mivf_ring_write_unsafe(MivfIoRing *r, const u8 *src, u32 bytes){
    u32 free_bytes = mivf_ring_free_unsafe(r);
    if(bytes > free_bytes) bytes = free_bytes;

    u32 first = r->size - r->write_pos;
    if(first > bytes) first = bytes;

    memcpy(r->buf + r->write_pos, src, first);

    u32 second = bytes - first;
    if(second){
        memcpy(r->buf, src + first, second);
    }

    r->write_pos = (r->write_pos + bytes) % r->size;
    r->fill += bytes;
    return bytes;
}

static u32 mivf_ring_read_unsafe(MivfIoRing *r, u8 *dst, u32 bytes){
    u32 avail = mivf_ring_available_unsafe(r);
    if(bytes > avail) bytes = avail;

    u32 first = r->size - r->read_pos;
    if(first > bytes) first = bytes;

    memcpy(dst, r->buf + r->read_pos, first);

    u32 second = bytes - first;
    if(second){
        memcpy(dst + first, r->buf, second);
    }

    r->read_pos = (r->read_pos + bytes) % r->size;
    r->fill -= bytes;
    return bytes;
}

static void mivf_reader_thread(void *arg){
    MivfIoRing *r = (MivfIoRing*)arg;
    u8 *tmp = (u8*)malloc(MIVF_IO_READ_CHUNK);

    if(!tmp){
        RecursiveLock_Lock(&r->lock);
        r->error = true;
        RecursiveLock_Unlock(&r->lock);
        LightEvent_Signal(&r->can_consume);
        return;
    }

    while(!r->stop){
        RecursiveLock_Lock(&r->lock);

        while(!r->stop &&
              mivf_ring_free_unsafe(r) < MIVF_IO_READ_CHUNK &&
              !r->eof &&
              !r->error){
            RecursiveLock_Unlock(&r->lock);
            LightEvent_Wait(&r->can_read);
            RecursiveLock_Lock(&r->lock);
        }

        if(r->stop || r->eof || r->error){
            RecursiveLock_Unlock(&r->lock);
            break;
        }

        RecursiveLock_Unlock(&r->lock);

        size_t got = fread(tmp, 1, MIVF_IO_READ_CHUNK, r->file);

        RecursiveLock_Lock(&r->lock);

        if(got > 0){
            u32 written = mivf_ring_write_unsafe(r, tmp, (u32)got);
            if(written != (u32)got){
                r->error = true;
            }
            LightEvent_Signal(&r->can_consume);
        }

        if(got < MIVF_IO_READ_CHUNK){
            if(feof(r->file)){
                r->eof = true;
            }else if(ferror(r->file)){
                r->error = true;
            }
            LightEvent_Signal(&r->can_consume);
        }

        RecursiveLock_Unlock(&r->lock);
    }

    free(tmp);
}

bool mivf_io_ring_init(MivfIoRing *r, FILE *file){
    memset(r, 0, sizeof(*r));

    r->buf = (u8*)malloc(MIVF_IO_RING_SIZE);
    if(!r->buf) return false;

    r->size = MIVF_IO_RING_SIZE;
    r->file = file;

    RecursiveLock_Init(&r->lock);
    LightEvent_Init(&r->can_read, RESET_ONESHOT);
    LightEvent_Init(&r->can_consume, RESET_ONESHOT);

    r->thread = threadCreate(
        mivf_reader_thread,
        r,
        MIVF_READER_STACK_SIZE,
        MIVF_READER_PRIORITY,
        -2,
        false
    );

    if(!r->thread){
        free(r->buf);
        memset(r, 0, sizeof(*r));
        return false;
    }

    return true;
}

void mivf_io_ring_shutdown(MivfIoRing *r){
    RecursiveLock_Lock(&r->lock);
    r->stop = true;
    RecursiveLock_Unlock(&r->lock);

    LightEvent_Signal(&r->can_read);
    LightEvent_Signal(&r->can_consume);

    if(r->thread){
        threadJoin(r->thread, U64_MAX);
        threadFree(r->thread);
        r->thread = NULL;
    }

    free(r->buf);
    r->buf = NULL;
}

bool mivf_io_read_exact(MivfIoRing *r, void *dst, u32 bytes){
    u8 *out = (u8*)dst;
    u32 done = 0;

    while(done < bytes){
        RecursiveLock_Lock(&r->lock);

        while(r->fill == 0 && !r->eof && !r->error && !r->stop){
            RecursiveLock_Unlock(&r->lock);
            LightEvent_Wait(&r->can_consume);
            RecursiveLock_Lock(&r->lock);
        }

        if(r->error || r->stop || (r->fill == 0 && r->eof)){
            RecursiveLock_Unlock(&r->lock);
            return false;
        }

        u32 want = bytes - done;
        u32 got = mivf_ring_read_unsafe(r, out + done, want);

        RecursiveLock_Unlock(&r->lock);
        LightEvent_Signal(&r->can_read);

        done += got;
    }

    return true;
}
