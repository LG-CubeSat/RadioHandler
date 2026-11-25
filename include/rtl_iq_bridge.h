#ifndef RTL_IQ_BRIDGE_H
#define RTL_IQ_BRIDGE_H

#include "iq_pipeline.h"
#include "rtl_capture.h"

#include <complex.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    double bandwidth_hz;
    size_t tap_count;
    double cutoff_frequency_hz;
    bool dsp_enabled;
    float frequency_offset_hz;
} rtl_iq_bridge_config_t;

typedef int32_t (*rtl_iq_sink_t)(
    const float complex *samples,
    size_t sample_count,
    void *context);

typedef struct {

    iq_pipeline_state_t pipeline;
    float complex *output_samples;
    size_t output_capacity;
    float frequency_offset_hz;
    rtl_iq_sink_t sink;
    void *sink_context;
    int32_t last_pipeline_status;
    int32_t last_sink_status;
    int32_t last_capture_status;
    size_t max_input_byte_count;
    bool running;

} rtl_iq_bridge_state_t;

typedef enum {
    RTL_IQ_BRIDGE_OK = 0,
    RTL_IQ_BRIDGE_STOP = 1,
    RTL_IQ_BRIDGE_ERROR_NULL = -1,
    RTL_IQ_BRIDGE_ERROR_RANGE = -2,
    RTL_IQ_BRIDGE_ERROR_ALREADY_INITIALIZED = -3,
    RTL_IQ_BRIDGE_ERROR_NOT_INITIALIZED = -4,
    RTL_IQ_BRIDGE_ERROR_MEMORY = -5,
    RTL_IQ_BRIDGE_ERROR_PIPELINE = -6,
    RTL_IQ_BRIDGE_ERROR_SINK = -7,
    RTL_IQ_BRIDGE_ERROR_CAPTURE = -8,
    RTL_IQ_BRIDGE_ERROR_BUSY = -9,
    RTL_IQ_BRIDGE_ERROR_STATE = -10
} rtl_iq_bridge_status_t;

int32_t rtl_iq_bridge_init(rtl_iq_bridge_state_t *state,
                           const rtl_capture_state_t *capture,
                           const rtl_iq_bridge_config_t *config,
                           rtl_iq_sink_t sink, void *sink_context);
int32_t rtl_iq_bridge_run(rtl_iq_bridge_state_t *state, rtl_capture_state_t *capture);
int32_t rtl_iq_bridge_close(rtl_iq_bridge_state_t *state);
const char *rtl_iq_bridge_status_string(int32_t status);

#endif
