// Equivalence check for the partitioned-inference API: process_partitioned()
// (the layers split into segments at arbitrary boundaries, each segment on its
// own handle) must be bit-identical to a single model's process(), for any
// partitioning. This mirrors what the device does across two cores.
#include <cstdio>
#include <cmath>
#include <fstream>
#include <memory>
#include <vector>

#include "json.hpp"
#include <NAM/dsp.h>
#include <NAM/get_dsp.h>
#include <NAM/wavenet/a2_fast.h>

#include "../include/bench_signal.h"

namespace a2 = nam::wavenet::a2_fast;
static constexpr int C = 3;

// Run `ref` (single instance, process()) and a partitioned run over `boundaries`
// for `totalBlocks` blocks of the shared signal; return the max abs diff on the
// last block.
static double run(const nlohmann::json& j, const std::vector<float>& weights,
                  const std::vector<int>& boundaries, int totalBlocks) {
    nam::DspLoadOptions opt; opt.prewarm = false;
    auto ref = nam::get_dsp(j, opt);
    ref->Reset(NAM_SR, BLOCK_FRAMES);

    std::vector<void*> segs;
    for (size_t i = 0; i < boundaries.size() + 1; ++i)
        segs.push_back(a2::partition_create(weights, NAM_SR, BLOCK_FRAMES));

    float in[BLOCK_FRAMES], rout[BLOCK_FRAMES], pout[BLOCK_FRAMES];
    float* inp[1] = { in };
    float* routp[1] = { rout };

    double maxerr = 0.0;
    uint32_t idx = 0;
    for (int b = 0; b < totalBlocks; ++b) {
        for (int k = 0; k < BLOCK_FRAMES; ++k) in[k] = bench_input(idx++);
        ref->process(inp, routp, BLOCK_FRAMES);
        a2::process_partitioned(segs, boundaries, in, pout, BLOCK_FRAMES);
        if (b == totalBlocks - 1)
            for (int k = 0; k < BLOCK_FRAMES; ++k)
                maxerr = std::fmax(maxerr, std::fabs(rout[k] - pout[k]));
    }
    for (void* s : segs) a2::partition_destroy(s);
    return maxerr;
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: host_split <model.nam>\n"); return 2; }
    std::ifstream f(argv[1]);
    nlohmann::json j; f >> j;
    std::vector<float> weights = j["weights"].get<std::vector<float>>();

    const int blocks = WARMUP_FRAMES / BLOCK_FRAMES + 1;
    struct { const char* name; std::vector<int> b; } cases[] = {
        {"split {12}", {12}},
        {"split {14}", {14}},
        {"split {8,16}", {8, 16}},
        {"split {6,12,18}", {6, 12, 18}},
    };
    int rc = 0;
    for (auto& c : cases) {
        const double e = run(j, weights, c.b, blocks);
        const bool ok = e < 1e-4;
        printf("%-16s max|err| vs process() = %.3e -> %s\n", c.name, e, ok ? "MATCH" : "MISMATCH");
        if (!ok) rc = 1;
    }
    return rc;
}
