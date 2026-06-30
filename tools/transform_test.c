/*
 * transform_test.c - bit-exact verification of the shared transform codec.
 *
 * Proves that mivf_t_make() (encoder side) and mivf_t_decode() (player side)
 * reconstruct IDENTICAL pixels from the same token, for every combination of
 * { dense, sparse } x { intra, motion }, over many random blocks. If this
 * passes, the encoder and player cannot disagree on the transform format, so a
 * format change (e.g. raising MIVF_T_NKEEP) is safe to deploy.
 *
 * Build: gcc -O2 -o tools/transform_test.exe tools/transform_test.c
 * Usage: tools/transform_test.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../source/mivf_transform.h"

/* Mode byte values (must mirror the encoder/decoder enums; only used so the
   token parser knows whether mx/my are present). */
#define MODE_TRANSFORM     8
#define MODE_MVTRANSFORM   9
#define MODE_TRANSFORMZ   12
#define MODE_MVTRANSFORMZ 13

static unsigned long rng = 0x12345678u;
static int rnd(void) { rng = rng * 1103515245u + 12345u; return (int)((rng >> 16) & 0xFF); }

/* Extract an 8x8 block from a plane into block[64]. */
static void grab(const uint8_t *plane, int w, int x0, int y0, uint8_t block[64]) {
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            block[y * 8 + x] = plane[(y0 + y) * w + (x0 + x)];
}

int main(void) {
    const int W = 32, H = 32;
    uint8_t *prev = malloc(W * H);
    uint8_t *dst = malloc(W * H);
    uint8_t cur[64], pred[64], recon_enc[64];
    int total_fails = 0;

    int keeps[] = { 4, 8, 16 };
    for (int ki = 0; ki < 3; ki++) {
        int nkeep = keeps[ki];
        int nslot = nkeep * 4;
        int fails = 0, tests = 0;

        for (int iter = 0; iter < 20000; iter++) {
            int sparse = iter & 1;
            int mv_mode = (iter >> 1) & 1;
            int qp = 1 + (rnd() % 51);

            for (int i = 0; i < W * H; i++) prev[i] = (uint8_t)rnd();
            for (int i = 0; i < 64; i++) cur[i] = (uint8_t)rnd();

            int bx = 1, by = 1;
            int mx = mv_mode ? ((rnd() % 9) - 4) : 0;
            int my = mv_mode ? ((rnd() % 9) - 4) : 0;
            int sx = bx * 8 + mx, sy = by * 8 + my;
            if (sx < 0 || sy < 0 || sx + 8 > W || sy + 8 > H) { mx = my = 0; sx = 8; sy = 8; }

            grab(prev, W, sx, sy, pred);

            uint8_t mode_byte = mv_mode
                ? (sparse ? MODE_MVTRANSFORMZ : MODE_MVTRANSFORM)
                : (sparse ? MODE_TRANSFORMZ : MODE_TRANSFORM);

            uint8_t token[128];
            int tok_len = 0;
            int sad = mivf_t_make(cur, pred, qp, mv_mode, mx, my, sparse,
                                  mode_byte, token, (int)sizeof(token),
                                  recon_enc, &tok_len, nkeep);
            if (sad < 0) { printf("make failed nkeep=%d iter=%d\n", nkeep, iter); fails++; continue; }

            size_t off = 1;
            if (mv_mode) off += 2;
            off += 1;

            int8_t coeffs[MIVF_T_MAX_NSLOT];
            if (sparse) {
                if (mivf_t_read_sparse(token, (size_t)tok_len, &off, coeffs, nslot) < 0) {
                    printf("read_sparse failed nkeep=%d iter=%d\n", nkeep, iter); fails++; continue;
                }
            } else {
                memset(coeffs, 0, sizeof(coeffs));
                for (int i = 0; i < nslot; i++) coeffs[i] = (int8_t)token[off++];
            }

            memcpy(dst, prev, W * H);
            mivf_t_decode(dst, prev, W, H, bx, by, mx, my, qp, coeffs, nkeep);

            uint8_t recon_dec[64];
            grab(dst, W, bx * 8, by * 8, recon_dec);

            tests++;
            if (memcmp(recon_enc, recon_dec, 64) != 0) {
                fails++;
                if (fails <= 3) {
                    printf("MISMATCH nkeep=%d iter=%d sparse=%d mv=%d qp=%d\n",
                           nkeep, iter, sparse, mv_mode, qp);
                }
            }
        }

        printf("nkeep=%2d nslot=%2d  tests=%d fails=%d  %s\n",
               nkeep, nslot, tests, fails, fails == 0 ? "PASS" : "FAIL");
        total_fails += fails;
    }

    printf("RESULT: %s\n", total_fails == 0 ? "ALL PASS" : "FAIL");
    free(prev);
    free(dst);
    return total_fails ? 1 : 0;
}
