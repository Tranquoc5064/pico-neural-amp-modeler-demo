// NAM as a USB-audio effect: dual-core a2_fast A2-Lite pipeline.
// core1 = FRONT half, caller's core0 = BACK half + output. +1 block latency.
//
// Cross-core handshake uses shared volatile flags (NOT the SIO FIFO) so the FIFO
// stays free for multicore_lockout, used to park core1 while core0 reads the
// BOOTSEL button (that read toggles the QSPI CS line and would otherwise corrupt
// a concurrent flash fetch on core1).
#include <cstdint>
#include <cstring>
#include <vector>

#include "pico/multicore.h"
#include "hardware/sync.h"

#include <NAM/dsp.h>
#include <NAM/wavenet/a2_fast.h>

#include "nam_model.h"

#define NFRAMES 48
#ifndef KSPLIT
#define KSPLIT 14
#endif
#define NCH 3
#ifndef NAM_OUTPUT_GAIN
#define NAM_OUTPUT_GAIN 1.0f   // ~unity vs passthrough (model already has makeup gain)
#endif
#define SENTINEL 0xFFFFFFFFu

static float g_in[2][NFRAMES];
static float g_mid[2][NCH * NFRAMES];
static float g_hs[2][NCH * NFRAMES];
static float g_cond[2][NFRAMES];
static float g_out_f[NFRAMES];

static void* g_front = nullptr;
static void* g_back = nullptr;

static volatile uint32_t g_req = SENTINEL;   // block index core0 wants fronted
static volatile uint32_t g_ack = SENTINEL;   // block index core1 has fronted
static uint32_t g_blk = 0;
static bool g_primed = false;

static inline int32_t f32_to_q31(float x) {
    x *= NAM_OUTPUT_GAIN;
    if (x >= 1.0f) return 0x7FFFFFFF;
    if (x <= -1.0f) return (int32_t)0x80000000;
    return (int32_t)(x * 2147483648.0f);
}

// core1: front-half worker. Polls g_req; fronts; publishes g_ack.
static void core1_worker() {
    multicore_lockout_victim_init();   // allow core0 to park us during BOOTSEL reads
    for (;;) {
        const uint32_t r = g_req;
        if (r != g_ack) {
            __dmb();
            const int s = r & 1;
            // front = chain head (rechannel) + layers [0, KSPLIT)
            nam::wavenet::a2_fast::partition_input(g_front, g_in[s], NFRAMES, g_mid[s], g_cond[s]);
            nam::wavenet::a2_fast::partition_layers(g_front, 0, KSPLIT, NFRAMES, g_cond[s], g_mid[s], g_hs[s]);
            __dmb();
            g_ack = r;
        } else {
            tight_loop_contents();
        }
    }
}

extern "C" void nam_fx_init(void) {
    std::vector<float> w(nam_model_weights, nam_model_weights + nam_model_weights_len);
    g_front = nam::wavenet::a2_fast::partition_create(w, 48000.0, NFRAMES);
    g_back = nam::wavenet::a2_fast::partition_create(w, 48000.0, NFRAMES);
    multicore_launch_core1(core1_worker);
}

extern "C" const char* fx_name(void) { return "Pico NAM"; }

// Re-prime the pipeline (call on each bypass->active transition).
extern "C" void nam_fx_reset(void) {
    g_req = SENTINEL;
    g_ack = SENTINEL;
    g_blk = 0;
    g_primed = false;
}

extern "C" void nam_fx_process(int32_t* out, const int32_t* in, int frames) {
    const uint32_t b = g_blk;
    const int s = b & 1;
    for (int k = 0; k < frames; k++)
        g_in[s][k] = (float)in[2 * k] * (1.0f / 2147483648.0f);

    if (!g_primed) {                       // first block: request front, no output yet
        __dmb(); g_req = b;
        g_primed = true; g_blk = b + 1;
        std::memset(out, 0, (size_t)frames * 2 * sizeof(int32_t));
        return;
    }

    // Wait until core1 finished the previous request (b-1), THEN request b — this
    // ordering keeps core1 from skipping a block (flags don't queue like a FIFO).
    while (g_ack != b - 1) tight_loop_contents();
    __dmb(); g_req = b;                     // core1 starts front(b)

    const int ps = (b - 1) & 1;
    // back = layers [KSPLIT, end) + head conv. back(b-1) overlaps front(b).
    nam::wavenet::a2_fast::partition_output(g_back, KSPLIT, NFRAMES, g_cond[ps], g_mid[ps], g_hs[ps], g_out_f);

    for (int k = 0; k < frames; k++) {
        const int32_t q = f32_to_q31(g_out_f[k]);
        out[2 * k] = q;
        out[2 * k + 1] = q;
    }
    g_blk = b + 1;
}
