#pragma once

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define MIVF_IO_RING_SIZE       (2 * 1024 * 1024)
#define MIVF_IO_READ_CHUNK      (32 * 1024)
#define MIVF_READER_STACK_SIZE  (32 * 1024)
#define MIVF_READER_PRIORITY    0x30

typedef struct MivfIoRing {
    u8   *buf;
    u32   size;

    volatile u32 read_pos;
    volatile u32 write_pos;
    volatile u32 fill;

    volatile bool eof;
    volatile bool error;
    volatile bool stop;

    FILE *file;

    LightEvent can_read;
    LightEvent can_consume;

    Thread thread;
    RecursiveLock lock;
} MivfIoRing;

bool mivf_io_ring_init(MivfIoRing *r, FILE *file);
void mivf_io_ring_shutdown(MivfIoRing *r);
bool mivf_io_read_exact(MivfIoRing *r, void *dst, u32 bytes);
