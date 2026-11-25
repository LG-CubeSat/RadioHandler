#ifndef IQ_PIPELINE_H
#define IQ_PIPELINE_H

#include "iq_adapter.h"
#include "dsp.h"
#include <stdbool.h>

typedef struct {

    iq_adapter_state_t adapter;
    dsp_state_t dsp;
    bool dsp_enabled;

} iq_pipeline_state_t;

typedef enum {

    IQ_PIPELINE_OK = 0,
    IQ_PIPELINE_ERROR_NULL = -1,
    IQ_PIPELINE_ERROR_ADAPTER = -2,
    IQ_PIPELINE_ERROR_DSP = -3

} iq_pipeline_status_t;

int32_t iq_pipeline_init(iq_pipeline_state_t *state, double sample_rate_hz, double bandwidth_hz, size_t tap_count, double cutoff_frequency_hz, bool dsp_enabled);
int32_t iq_pipeline_process(iq_pipeline_state_t *state, const uint8_t *input_bytes, size_t input_byte_count, float complex *output_samples, size_t output_capacity, size_t *samples_written, float frequency_offset_hz);
int32_t iq_pipeline_set_enabled(iq_pipeline_state_t *state, bool enabled);

#endif