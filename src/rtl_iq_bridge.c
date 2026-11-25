#include "rtl_iq_bridge.h"
#include <math.h>
#include <stdlib.h>

static int32_t rtl_iq_bridge_handle_bytes(const uint8_t *input_bytes,
                                         size_t input_byte_count,
                                         void *context){
    rtl_iq_bridge_state_t *state = context;
    if (state == NULL) return RTL_IQ_BRIDGE_ERROR_NULL;
    if (!state->running || state->output_samples == NULL || state->sink == NULL){
        return RTL_IQ_BRIDGE_ERROR_STATE;
    }
    if (input_byte_count > state->max_input_byte_count) return RTL_IQ_BRIDGE_ERROR_RANGE;

    size_t samples_written = 0;
    state->last_pipeline_status = iq_pipeline_process(
        &state->pipeline, input_bytes, input_byte_count,
        state->output_samples, state->output_capacity,
        &samples_written, state->frequency_offset_hz);

    if (state->last_pipeline_status != IQ_PIPELINE_OK) return RTL_IQ_BRIDGE_ERROR_PIPELINE;
    if (samples_written == 0) return RTL_CAPTURE_OK;

    state->last_sink_status = state->sink(
        state->output_samples, samples_written, state->sink_context);

    if (state->last_sink_status == RTL_IQ_BRIDGE_STOP) return RTL_CAPTURE_STOP;
    if (state->last_sink_status != RTL_IQ_BRIDGE_OK) return RTL_IQ_BRIDGE_ERROR_SINK;
    return RTL_CAPTURE_OK;
}

int32_t rtl_iq_bridge_init(rtl_iq_bridge_state_t *state,
                           const rtl_capture_state_t *capture,
                           const rtl_iq_bridge_config_t *config,
                           rtl_iq_sink_t sink, void *sink_context){
    if (state == NULL || capture == NULL || config == NULL || sink == NULL){
        return RTL_IQ_BRIDGE_ERROR_NULL;
    }
    if (state->running) return RTL_IQ_BRIDGE_ERROR_BUSY;
    if (state->output_samples != NULL) return RTL_IQ_BRIDGE_ERROR_ALREADY_INITIALIZED;
    if (capture->device == NULL || capture->runtime == NULL){
        return RTL_IQ_BRIDGE_ERROR_CAPTURE;
    }

    size_t max_input_byte_count = capture->config.usb_buffer_size_bytes;
    if (capture->actual_sample_rate_hz == 0 ||
        max_input_byte_count == 0 ||
        max_input_byte_count > RTL_CAPTURE_MAX_BUFFER_SIZE_BYTES ||
        max_input_byte_count % 512 != 0 ||
        !isfinite(config->frequency_offset_hz)){
        return RTL_IQ_BRIDGE_ERROR_RANGE;
    }

    size_t output_capacity = max_input_byte_count / 2 + max_input_byte_count % 2;
    if (output_capacity > SIZE_MAX / sizeof(float complex)) return RTL_IQ_BRIDGE_ERROR_RANGE;

    rtl_iq_bridge_state_t prepared = {0};
    prepared.last_pipeline_status = iq_pipeline_init(
        &prepared.pipeline, capture->actual_sample_rate_hz,
        config->bandwidth_hz, config->tap_count,
        config->cutoff_frequency_hz, config->dsp_enabled);
    if (prepared.last_pipeline_status != IQ_PIPELINE_OK) return RTL_IQ_BRIDGE_ERROR_PIPELINE;

    prepared.output_samples = malloc(output_capacity * sizeof(*prepared.output_samples));
    if (prepared.output_samples == NULL) return RTL_IQ_BRIDGE_ERROR_MEMORY;

    prepared.output_capacity = output_capacity;
    prepared.max_input_byte_count = max_input_byte_count;
    prepared.frequency_offset_hz = config->frequency_offset_hz;
    prepared.sink = sink;
    prepared.sink_context = sink_context;
    *state = prepared;
    return RTL_IQ_BRIDGE_OK;
}

int32_t rtl_iq_bridge_run(rtl_iq_bridge_state_t *state, rtl_capture_state_t *capture){
    if (state == NULL || capture == NULL) return RTL_IQ_BRIDGE_ERROR_NULL;
    if (state->running) return RTL_IQ_BRIDGE_ERROR_BUSY;
    if (state->output_samples == NULL) return RTL_IQ_BRIDGE_ERROR_NOT_INITIALIZED;
    if (state->sink == NULL || state->output_capacity == 0){
        return RTL_IQ_BRIDGE_ERROR_STATE;
    }

    state->last_capture_status = RTL_CAPTURE_OK;
    state->last_pipeline_status = IQ_PIPELINE_OK;
    state->last_sink_status = RTL_IQ_BRIDGE_OK;

    if (capture->device == NULL || capture->runtime == NULL){
        state->last_capture_status = RTL_CAPTURE_ERROR_NOT_OPEN;
        return RTL_IQ_BRIDGE_ERROR_CAPTURE;
    }
    if (capture->actual_sample_rate_hz != state->pipeline.dsp.sample_rate_hz ||
        capture->config.usb_buffer_size_bytes == 0 ||
        capture->config.usb_buffer_size_bytes > state->max_input_byte_count ||
        capture->config.usb_buffer_size_bytes % 512 != 0 ||
        !isfinite(state->frequency_offset_hz)){
        return RTL_IQ_BRIDGE_ERROR_RANGE;
    }

    size_t required_capacity = state->max_input_byte_count / 2 + state->max_input_byte_count % 2;
    if (required_capacity > state->output_capacity) return RTL_IQ_BRIDGE_ERROR_STATE;

    state->pipeline.adapter = (iq_adapter_state_t){0};
    for (size_t i = 0; i < DSP_MAX_TAPS; i++){
        state->pipeline.dsp.history[i] = 0.0f;
    }
    state->pipeline.dsp.shift_phase = 0.0;
    state->running = true;
    state->last_capture_status = rtl_capture_run(capture, rtl_iq_bridge_handle_bytes, state);
    state->running = false;

    if (state->last_capture_status == RTL_CAPTURE_OK) return RTL_IQ_BRIDGE_OK;
    if (state->last_capture_status == RTL_CAPTURE_ERROR_CALLBACK){
        switch (capture->stats.callback_status){
            case RTL_IQ_BRIDGE_ERROR_NULL:
            case RTL_IQ_BRIDGE_ERROR_RANGE:
            case RTL_IQ_BRIDGE_ERROR_PIPELINE:
            case RTL_IQ_BRIDGE_ERROR_SINK:
            case RTL_IQ_BRIDGE_ERROR_STATE:
                return capture->stats.callback_status;
            default:
                return RTL_IQ_BRIDGE_ERROR_CAPTURE;
        }
    }
    return RTL_IQ_BRIDGE_ERROR_CAPTURE;
}

int32_t rtl_iq_bridge_close(rtl_iq_bridge_state_t *state){
    if (state == NULL) return RTL_IQ_BRIDGE_ERROR_NULL;
    if (state->running) return RTL_IQ_BRIDGE_ERROR_BUSY;

    free(state->output_samples);
    *state = (rtl_iq_bridge_state_t){0};
    return RTL_IQ_BRIDGE_OK;
}

const char *rtl_iq_bridge_status_string(int32_t status){
    switch (status){
        case RTL_IQ_BRIDGE_OK: return "Success";
        case RTL_IQ_BRIDGE_STOP: return "Sink requested stop";
        case RTL_IQ_BRIDGE_ERROR_NULL: return "Required pointer is NULL";
        case RTL_IQ_BRIDGE_ERROR_RANGE: return "Configuration or capture size is out of range";
        case RTL_IQ_BRIDGE_ERROR_ALREADY_INITIALIZED: return "Bridge is already initialized";
        case RTL_IQ_BRIDGE_ERROR_NOT_INITIALIZED: return "Bridge is not initialized";
        case RTL_IQ_BRIDGE_ERROR_MEMORY: return "Output buffer allocation failed";
        case RTL_IQ_BRIDGE_ERROR_PIPELINE: return "IQ pipeline failed";
        case RTL_IQ_BRIDGE_ERROR_SINK: return "Sample sink failed";
        case RTL_IQ_BRIDGE_ERROR_CAPTURE: return "Receiver is not open or capture failed";
        case RTL_IQ_BRIDGE_ERROR_BUSY: return "Bridge is running";
        case RTL_IQ_BRIDGE_ERROR_STATE: return "Bridge state is invalid";
        default: return "Unknown bridge status";
    }
}
