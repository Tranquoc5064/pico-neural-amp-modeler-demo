// NAM A2-Lite benchmark on RP2350 — dual-core a2_fast layer pipeline.
//
// core1 runs the front half (layers [0,KSPLIT)); core0 runs the back half
// (layers [KSPLIT,23) + head) and owns the output. They pipeline at 48-frame
// block granularity via a double-buffered handoff + SIO FIFO, so block t's back
// overlaps block t+1's front (+1 block latency, ~2x throughput), bit-exact vs a
// single core. a2_fast is data-independent, so the per-sample cycle count holds
// for any A2-Lite regardless of the weight values.
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"

#include <NAM/wavenet/a2_fast.h>

#include "dwt.h"
#include "bench_signal.h"
#include "nam_model.h"

#ifndef TARGET_SYS_KHZ
#define TARGET_SYS_KHZ 300000
#endif
#ifndef KSPLIT
#define KSPLIT 14          // optimal split (front = layers [0,KSPLIT); back = [KSPLIT,23))
#endif
#define C 3

// Double-buffered handoff (same address space, shared between cores).
static float g_in[2][BLOCK_FRAMES];
static float g_mid[2][C * BLOCK_FRAMES];
static float g_hs[2][C * BLOCK_FRAMES];
static float g_cond[2][BLOCK_FRAMES];
static void* g_front_model = nullptr;

static void gen_input(uint32_t block, float* buf) {
    const uint32_t base = block * BLOCK_FRAMES;
    for (int k = 0; k < BLOCK_FRAMES; ++k) buf[k] = bench_input(base + k);
}

// Core1: front-half worker. Pops a block index, fronts it, pushes it back.
static void core1_main() {
    for (;;) {
        const uint32_t i = multicore_fifo_pop_blocking();
        const int s = i & 1;
        nam::wavenet::a2_fast::partition_input(g_front_model, g_in[s], BLOCK_FRAMES, g_mid[s], g_cond[s]);
        nam::wavenet::a2_fast::partition_layers(g_front_model, 0, KSPLIT, BLOCK_FRAMES, g_cond[s], g_mid[s],
                                                g_hs[s]);
        multicore_fifo_push_blocking(i);
    }
}

static void overclock(uint32_t khz) {
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(khz, false);
}

int main() {
    overclock(TARGET_SYS_KHZ);
    stdio_init_all();
    dwt_init();
    sleep_ms(2000);

    const uint32_t sys_hz = clock_get_hz(clk_sys);
    const uint32_t budget = sys_hz / NAM_SR;
    const unsigned mhz_i = (unsigned)(sys_hz / 1000000), mhz_f = (unsigned)((sys_hz / 1000) % 1000);

    // Embedded model (NAM_MODEL): weights drive inference, the constants drive the
    // report — both track whatever .nam is compiled in.
    std::vector<float> weights(nam_model_weights, nam_model_weights + nam_model_weights_len);
    const char* net_arch = nam_model_arch;
    const char* net_name = nam_model_name;
    const int net_ch = nam_model_channels;
    const int net_layers = nam_model_layers;
    const int net_kmin = nam_model_kernel_min;
    const int net_kmax = nam_model_kernel_max;
    const int net_rf = nam_model_receptive_field;
    const size_t net_params = weights.size();
    const unsigned net_kb_i = (unsigned)(net_params * 4 / 1024);
    const unsigned net_kb_f = (unsigned)((net_params * 4 * 10 / 1024) % 10);
    const unsigned rf_us = (unsigned)((uint64_t)net_rf * 1000000ull / NAM_SR);

    printf("\n=== NAM dual-core a2_fast benchmark — RP2350 ===\n");
    printf("platform : RP2350  2x Cortex-M33 @ %u.%03u MHz (FPU, single-precision)  | cores used: 2\n", mhz_i, mhz_f);
    printf("network  : %s \"%s\"  %d ch  %d conv layers (k=%d..%d)  %u params, %u.%01u KB f32  | LeakyReLU\n",
           net_arch, net_name, net_ch, net_layers, net_kmin, net_kmax,
           (unsigned)net_params, net_kb_i, net_kb_f);
    printf("           receptive field %d samples (%u.%03u ms @ %d Hz)\n",
           net_rf, rf_us / 1000, rf_us % 1000, NAM_SR);
    printf("audio    : %d Hz  block %d frames  budget %u cyc/sample  | pipeline KSPLIT=%d (core1 front 0..%d / core0 back %d..%d + head)\n",
           NAM_SR, BLOCK_FRAMES, (unsigned)budget, KSPLIT, KSPLIT - 1, KSPLIT, net_layers - 1);

    g_front_model = nam::wavenet::a2_fast::partition_create(weights, NAM_SR, BLOCK_FRAMES);
    void* back_model = nam::wavenet::a2_fast::partition_create(weights, NAM_SR, BLOCK_FRAMES);
    if (!g_front_model || !back_model) { printf("ERROR: model create failed\n"); while (true) tight_loop_contents(); }

    multicore_launch_core1(core1_main);

    float out[BLOCK_FRAMES];
    const int WARMUP_BLOCKS = WARMUP_FRAMES / BLOCK_FRAMES;
    const int MEAS_BLOCKS = 200;

    // Pipeline: front(block i) on core1 overlaps back(block i-1) on core0.
    // Prime with block 0.
    uint32_t blk = 0;
    gen_input(blk, g_in[blk & 1]);
    multicore_fifo_push_blocking(blk);

    double meas_checksum = 0.0;
    uint32_t best_block = 0xFFFFFFFFu;
    uint64_t acc = 0;

    const int total = WARMUP_BLOCKS + MEAS_BLOCKS;
    for (int n = 1; n <= total; ++n) {
        const uint32_t i = (uint32_t)n;
        const int s = i & 1;
        gen_input(i, g_in[s]);
        const bool timed = (n > WARMUP_BLOCKS);

        const uint32_t t0 = dwt_cyccnt();
        multicore_fifo_push_blocking(i);          // core1: front block i
        multicore_fifo_pop_blocking();            // wait core1 finished front(i-1)
        const int ps = (i - 1) & 1;
        nam::wavenet::a2_fast::partition_output(back_model, KSPLIT, BLOCK_FRAMES, g_cond[ps], g_mid[ps], g_hs[ps],
                                                out);   // back block i-1
        const uint32_t dt = dwt_cyccnt() - t0;

        if (timed) {
            if (dt < best_block) best_block = dt;
            acc += dt;
        }
        if (n == WARMUP_BLOCKS + 1) {             // checksum of first post-warmup output block
            for (int k = 0; k < BLOCK_FRAMES; ++k) meas_checksum += out[k];
        }
    }

    const float cps_min = (float)best_block / BLOCK_FRAMES;
    const float cps_avg = (float)(acc / MEAS_BLOCKS) / BLOCK_FRAMES;
    const float cpu_min = 100.0f * cps_min / (float)budget;

    printf("checksum=%.6f (dual-core output sum; bit-exact vs single core)\n", meas_checksum);
    printf("pipeline block min=%u avg=%u cyc | per-sample min %u.%02u | CPU %u.%01u%% of %u-cyc budget\n",
           (unsigned)best_block, (unsigned)(acc / MEAS_BLOCKS),
           (unsigned)cps_min, (unsigned)((int)(cps_min * 100) % 100),
           (unsigned)cpu_min, (unsigned)((int)(cpu_min * 10) % 10), (unsigned)budget);

    while (true) {
        sleep_ms(5000);
        printf("[idle] RP2350 2x Cortex-M33 @ %u.%03u MHz, cores=2 | %s %dch/%dL/%up RF=%dsmp | "
               "per-sample=%u.%02u cyc CPU=%u.%01u%% chk=%.6f\n",
               mhz_i, mhz_f, net_arch, net_ch, net_layers, (unsigned)net_params, net_rf,
               (unsigned)cps_min, (unsigned)((int)(cps_min * 100) % 100),
               (unsigned)cpu_min, (unsigned)((int)(cpu_min * 10) % 10), meas_checksum);
    }
}
