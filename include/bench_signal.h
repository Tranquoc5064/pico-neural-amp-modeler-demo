// Deterministic test signal + run parameters shared by the host reference tool
// and the on-device benchmark, so both feed the engine the exact same input.
// Keep this header free of platform headers (host and bare-metal both include it).
#pragma once

#include <math.h>
#include <stdint.h>

// 48 kHz block of 48 frames == the Daisy reference condition (142,350 cycles).
#define NAM_SR        48000
#define BLOCK_FRAMES  48

// Warmup frames pushed through the model before the measured/compared block, so
// the dilated-conv ring buffers are fully primed (A2-Lite receptive field is a few
// thousand samples). Must be a multiple of BLOCK_FRAMES.
#define WARMUP_FRAMES (48 * 64)   // 3072

// Deterministic input sample at absolute index i. A mix of two sines keeps the
// signal in the model's active range without needing any external data.
static inline float bench_input(uint32_t i) {
    const float t = (float)i / (float)NAM_SR;
    return 0.5f * sinf(2.0f * 3.14159265358979f * 220.0f * t)
         + 0.25f * sinf(2.0f * 3.14159265358979f * 880.0f * t);
}
