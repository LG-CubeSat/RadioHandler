#include "iq_pipeline.h"
#include <math.h>

int32_t iq_pipeline_init(iq_pipeline_state_t *state, double sample_rate_hz, double bandwidth_hz, size_t tap_count, double cutoff_frequency_hz, bool dsp_enabled){

    if (state == NULL) return IQ_PIPELINE_ERROR_NULL;
    iq_pipeline_state_t prepared = {0};

    int32_t dsp_init_status = dsp_init(&prepared.dsp, sample_rate_hz, bandwidth_hz, tap_count, cutoff_frequency_hz);

    if (dsp_init_status != DSP_OK) return IQ_PIPELINE_ERROR_DSP;

    prepared.dsp_enabled = dsp_enabled;
    *state = prepared;

    return IQ_PIPELINE_OK;

}

int32_t iq_pipeline_process(iq_pipeline_state_t *state, const uint8_t *input_bytes, size_t input_byte_count, float complex *output_samples, size_t output_capacity, size_t *samples_written, float frequency_offset_hz){

    if (samples_written == NULL) return IQ_PIPELINE_ERROR_NULL;
    *samples_written = 0;
    if (state == NULL) return IQ_PIPELINE_ERROR_NULL;

    if (state->dsp_enabled == true && !isfinite(frequency_offset_hz)) return IQ_PIPELINE_ERROR_DSP;

    int32_t iq_adapter_status = iq_adapter_convert(&state->adapter, input_bytes, input_byte_count, output_samples, output_capacity, samples_written);

    if (iq_adapter_status != IQ_ADAPTER_OK) return IQ_PIPELINE_ERROR_ADAPTER;
    if (state->dsp_enabled == false || *samples_written == 0) return IQ_PIPELINE_OK;

    int32_t dsp_shift_status = dsp_shift(&state->dsp, output_samples, *samples_written, frequency_offset_hz);

    if (dsp_shift_status != DSP_OK) return IQ_PIPELINE_ERROR_DSP;

    int32_t dsp_process_status = dsp_process(&state->dsp, output_samples, *samples_written);

    if (dsp_process_status != DSP_OK) return IQ_PIPELINE_ERROR_DSP;

    return IQ_PIPELINE_OK;
}
