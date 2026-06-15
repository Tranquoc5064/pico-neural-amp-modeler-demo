// Host tool: convert a NAM .nam into the embedded C source the firmware compiles.
//
//   nam2c <in.nam> <out.c>
//
// Does the whole .nam -> C pipeline in one validated step:
//   1. load the .nam (nlohmann/json, bundled with the engine)
//   2. if it is a packed A2 "SlimmableContainer", pull out the A2-Lite (3-ch)
//      sub-model; otherwise accept a single WaveNet
//   3. validate it with the ENGINE'S OWN is_a2_shape — the same detector that
//      runs on-device — so a model that a2_fast would reject fails the build here,
//      and our acceptance can never drift from the engine as A2 evolves
//   4. emit nam_model.c: the weight array + a few report-metadata constants
//
// Built natively (host toolchain) and run at firmware-build time by CMake; see
// nam_set_model() in CMakeLists.txt. Mirrors tools/host_a2.cpp's engine link.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "json.hpp"
#include <NAM/wavenet/a2_fast.h>

using nlohmann::json;

// Unwrap a packed A2 container to its A2-Lite (channels==3) WaveNet, or pass a
// bare WaveNet through. Returns nullptr if neither is present.
static const json* find_a2_lite(const json& nam) {
    const std::string arch = nam.value("architecture", std::string());
    if (arch == "WaveNet") return &nam;
    if (arch == "SlimmableContainer") {
        const json* smallest = nullptr;
        int smallest_ch = INT32_MAX;
        for (const auto& entry : nam["config"]["submodels"]) {
            const json& m = entry["model"];
            int ch = 0;
            try {
                ch = m["config"]["layers"][0]["channels"].get<int>();
            } catch (...) {
                continue;
            }
            if (ch == 3) return &m;            // exact A2-Lite
            if (ch < smallest_ch) { smallest_ch = ch; smallest = &m; }
        }
        return smallest;
    }
    return nullptr;
}

// Emit a safe C string literal (escape "/\, octal-escape anything non-ASCII).
static std::string c_escape(const std::string& s) {
    std::string out;
    char buf[8];
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { out += '\\'; out += (char)c; }
        else if (c >= 0x20 && c < 0x7f) { out += (char)c; }
        else { std::snprintf(buf, sizeof buf, "\\%03o", c); out += buf; }
    }
    return out;
}

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: nam2c <in.nam> <out.c>\n"); return 2; }
    const char* in_path = argv[1];
    const char* out_path = argv[2];

    std::ifstream f(in_path);
    if (!f) { std::fprintf(stderr, "nam2c: cannot open %s\n", in_path); return 1; }
    json nam;
    try { f >> nam; } catch (const std::exception& e) {
        std::fprintf(stderr, "nam2c: %s: JSON parse error: %s\n", in_path, e.what());
        return 1;
    }

    const json* model = find_a2_lite(nam);
    if (!model) {
        std::fprintf(stderr, "nam2c: %s: no A2-Lite / WaveNet sub-model found\n", in_path);
        return 1;
    }

    int channels = 0;
    if (!nam::wavenet::a2_fast::is_a2_shape((*model)["config"], &channels) || channels != 3) {
        std::fprintf(stderr,
            "nam2c: %s: not a 3-ch A2-Lite shape (is_a2_shape failed) — a2_fast would reject it\n",
            in_path);
        return 1;
    }

    const json& cfg = (*model)["config"];
    const json& layer = cfg["layers"][0];
    const auto weights = (*model)["weights"].get<std::vector<float>>();
    const auto kernels = layer["kernel_sizes"].get<std::vector<int>>();
    const auto dilations = layer["dilations"].get<std::vector<int>>();
    const int layers = (int)dilations.size();
    const int kmin = kernels.empty() ? 0 : *std::min_element(kernels.begin(), kernels.end());
    const int kmax = kernels.empty() ? 0 : *std::max_element(kernels.begin(), kernels.end());
    long rf = 1; // receptive field = 1 + Σ (kernel-1)·dilation
    for (size_t i = 0; i < dilations.size() && i < kernels.size(); ++i)
        rf += (long)(kernels[i] - 1) * dilations[i];
    const std::string name = nam.value("metadata", json::object()).value("name", std::string());
    const std::string arch = (*model).value("architecture", std::string("WaveNet"));

    FILE* o = std::fopen(out_path, "w");
    if (!o) { std::fprintf(stderr, "nam2c: cannot write %s\n", out_path); return 1; }
    std::fprintf(o, "// Auto-generated from %s by tools/nam2c — do not edit.\n", in_path);
    std::fprintf(o, "// Regenerate by changing nam_set_model(...) / -DNAM_MODEL= in CMakeLists.txt.\n");
    std::fprintf(o, "#include \"nam_model.h\"\n\n");
    std::fprintf(o, "const float nam_model_weights[] = {\n");
    for (size_t i = 0; i < weights.size(); ++i)
        std::fprintf(o, "%.9gf,%s", weights[i], (i % 8 == 7) ? "\n" : " ");
    std::fprintf(o, "\n};\n");
    std::fprintf(o, "const unsigned nam_model_weights_len = %zu;\n\n", weights.size());
    std::fprintf(o, "const char nam_model_name[] = \"%s\";\n", c_escape(name).c_str());
    std::fprintf(o, "const char nam_model_arch[] = \"%s\";\n", c_escape(arch).c_str());
    std::fprintf(o, "const int nam_model_channels = %d;\n", channels);
    std::fprintf(o, "const int nam_model_layers = %d;\n", layers);
    std::fprintf(o, "const int nam_model_kernel_min = %d;\n", kmin);
    std::fprintf(o, "const int nam_model_kernel_max = %d;\n", kmax);
    std::fprintf(o, "const int nam_model_receptive_field = %ld;\n", rf);
    std::fclose(o);

    std::fprintf(stderr, "nam2c: %s -> %s  (A2-Lite %dch/%dL, %zu params, RF=%ld)\n",
                 name.empty() ? "(unnamed)" : name.c_str(), out_path,
                 channels, layers, weights.size(), rf);
    return 0;
}
