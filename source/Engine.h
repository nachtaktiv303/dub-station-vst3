#pragma once
// C interface of the Dub Station DSP (dsp/engine.cpp). The engine is the same as the MPC build and runs at a
// fixed 44.1 kHz; the plugin resamples around it when the host runs at another rate.
#include <cstdint>

extern "C" {
typedef struct
{
    void *(*create)(const char *data_dir);
    void (*destroy)(void *inst);
    void (*midi)(void *inst, const uint8_t *msg, int len);
    void (*set_param)(void *inst, const char *key, const char *val);
    int (*get_param)(void *inst, const char *key, char *buf, int buf_len);
    void (*render)(void *inst, int16_t *out_lr, int frames);
    void (*process)(void *inst, const int16_t *in_lr, int16_t *out_lr, int frames);
} mpc_engine_t;

const mpc_engine_t *mpc_engine(void);
void dub_engine_process_float(void *inst, float *l, float *r, int frames);
}

namespace dub
{
constexpr double kEngineRate = 44100.0;
}
