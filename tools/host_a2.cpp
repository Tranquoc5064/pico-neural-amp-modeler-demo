// Host verifier for the A2 fast path: load a generated A2-Lite .nam via the
// engine's JSON loader, confirm is_a2_shape matches (so a2_fast engages), then
// run the shared warmup+block and print a checksum to compare against the
// device. Build: scripts/build_host_a2.sh
#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>

#include "json.hpp"
#include <NAM/dsp.h>
#include <NAM/get_dsp.h>
#include <NAM/wavenet/a2_fast.h>

#include "../include/bench_signal.h"

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: host_a2 <model.nam>\n"); return 2; }

    std::ifstream f(argv[1]);
    nlohmann::json j; f >> j;

    int ch = 0;
    const bool a2 = nam::wavenet::a2_fast::is_a2_shape(j["config"], &ch);
    printf("is_a2_shape = %s (channels=%d)\n", a2 ? "TRUE -> a2_fast ENGAGES" : "false (generic path)", ch);

    nam::DspLoadOptions opt; opt.prewarm = false;
    std::unique_ptr<nam::DSP> dsp = nam::get_dsp(j, opt);
    if (!dsp) { fprintf(stderr, "load failed\n"); return 1; }
    dsp->Reset(NAM_SR, BLOCK_FRAMES);

    float in[BLOCK_FRAMES], out[BLOCK_FRAMES];
    float* inp[1] = { in };
    float* outp[1] = { out };

    uint32_t idx = 0;
    for (int b = 0; b < WARMUP_FRAMES / BLOCK_FRAMES; ++b) {
        for (int k = 0; k < BLOCK_FRAMES; ++k) in[k] = bench_input(idx++);
        dsp->process(inp, outp, BLOCK_FRAMES);
    }
    for (int k = 0; k < BLOCK_FRAMES; ++k) in[k] = bench_input(idx++);
    dsp->process(inp, outp, BLOCK_FRAMES);

    double sum = 0.0;
    for (int k = 0; k < BLOCK_FRAMES; ++k) sum += out[k];
    printf("REF checksum=%.6f\n", sum);
    for (int k = 0; k < 8; ++k) printf("REF[%02d]=%.6f\n", k, out[k]);
    return 0;
}
