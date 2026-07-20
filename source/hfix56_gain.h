#pragma once

/*
    P.A1: pure, freestanding amplitude processing for one PCM16 sample.

    No I/O, no globals, no allocation, no 3DS SDK dependency (stdint.h
    only) -- included by BOTH the real player (main.c) and a
    host-compilable test harness (source/test_hfix56_gain.c) so there is
    exactly one implementation, never two that could silently diverge.

    Design (per approved P.A1 decisions):
      - one combined master*channel gain, computed with a wide (int64_t)
        intermediate, one division, one rounding step -- not two
        sequential roundings through separate helpers;
      - the existing soft limiter is applied ONCE, after the combined
        gain, exactly matching where it already sits in the master-only
        path (hfix56_apply_gain_one, main.c) -- so channel==100 reproduces
        that function's output bit-for-bit (verified by
        test_hfix56_gain.c, not merely asserted);
      - channel_percent is attenuation only: clamped to [0,100] inside
        this function, defensively, in addition to whatever clamping the
        caller already did on the persisted setting -- it can never
        amplify, regardless of caller mistakes;
      - master_percent's own existing range/behavior (0..300, boost above
        100 handled by the same limiter) is unchanged by this header --
        this file does not introduce or narrow that bound.
*/

#include <stdint.h>

static inline int16_t hfix56_gain_clamp_s16(int32_t v) {
    if (v < -32768) {
        return -32768;
    }
    if (v > 32767) {
        return 32767;
    }
    return (int16_t)v;
}

/*
    sample: one decoded PCM16 value for one channel.
    master_percent: the existing master-volume setting's live value
        (0..300 today; this function does not itself enforce that bound --
        the caller's existing clamp remains authoritative).
    channel_percent: NEW P.A1 per-channel attenuation, clamped here to
        [0,100] -- never amplifies.
    limiter_enabled: existing soft-limiter toggle; applied once, after the
        combined gain, identical knee/shift to the master-only path.
*/
static inline int16_t hfix56_apply_gain_channel(int16_t sample, int master_percent,
                                                 int channel_percent, int limiter_enabled) {
    int32_t v;

    if (channel_percent < 0) {
        channel_percent = 0;
    }
    if (channel_percent > 100) {
        channel_percent = 100;
    }

    /* sample in [-32768,32767], master up to a few hundred, channel up to
       100 -- the product fits comfortably in int64_t with enormous
       headroom; no overflow risk at any realistic setting. */
    {
        int64_t wide = (int64_t)sample * (int64_t)master_percent * (int64_t)channel_percent;
        v = (int32_t)(wide / 10000); /* /100 for master_percent, /100 for channel_percent */
    }

    if (limiter_enabled) {
        /* Identical to hfix56_apply_gain_one's existing soft-limiter knee
           (main.c) -- copied, not reinvented, so behavior at the boundary
           matches the already-proven master-only path exactly. */
        const int32_t knee = 28000;

        if (v > knee) {
            v = knee + ((v - knee) >> 2);
        } else if (v < -knee) {
            v = -knee + ((v + knee) >> 2);
        }
    }

    return hfix56_gain_clamp_s16(v);
}
