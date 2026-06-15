#pragma once
// Embedded NAM model interface. The definitions live in the build-generated
// nam_model.c (see nam_set_model() in CMakeLists.txt + scripts/nam_to_c.py).
#ifdef __cplusplus
extern "C" {
#endif

// Inference data: A2-Lite weight vector (a2_fast rebuilds the net from the fixed
// A2 shape + these weights — no on-device JSON parsing).
extern const float    nam_model_weights[];
extern const unsigned nam_model_weights_len;

// Report metadata (computed at embed time from the .nam).
extern const char nam_model_name[];
extern const char nam_model_arch[];
extern const int  nam_model_channels;
extern const int  nam_model_layers;
extern const int  nam_model_kernel_min;
extern const int  nam_model_kernel_max;
extern const int  nam_model_receptive_field; // samples

#ifdef __cplusplus
}
#endif
