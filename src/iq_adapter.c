#include "iq_adapter.h"

static float complex iq_u8_pair_to_complex(uint8_t i_byte, uint8_t q_byte){
    float normalized_i = (i_byte - 127.5f) / 128.0f;
    float normalized_q = (q_byte - 127.5f) / 128.0f;

    return normalized_i + normalized_q * _Complex_I;
}

// Always initizalize the state to 0 before calls: Ex. iq_adapter_state_t adapter = {0};

int32_t iq_adapter_convert(iq_adapter_state_t *state, const uint8_t *input_bytes, size_t input_byte_count, float complex *output_samples, size_t output_capacity, size_t *samples_written){

    if (samples_written == NULL) return IQ_ADAPTER_ERROR_NULL;
    *samples_written = 0;
    if (state == NULL) return IQ_ADAPTER_ERROR_NULL;
    if (input_byte_count == 0) return IQ_ADAPTER_OK;
    if (input_bytes == NULL) return IQ_ADAPTER_ERROR_NULL;

    size_t required_samples = input_byte_count / 2;
    if (state->has_pending_i && (input_byte_count % 2) != 0) required_samples++;

    if (required_samples != 0 && output_samples == NULL) return IQ_ADAPTER_ERROR_NULL;
    if (required_samples > output_capacity) return IQ_ADAPTER_ERROR_BUFFER_TOO_SMALL;

    size_t input_index = 0;
    size_t output_index = 0;

    if (state->has_pending_i){
        output_samples[output_index] = iq_u8_pair_to_complex(state->pending_i, input_bytes[input_index]);     // handles pending i value from previous block
        input_index++;
        output_index++;
        state->has_pending_i = false;
    }

    while (input_byte_count - input_index >= 2){
        output_samples[output_index] = iq_u8_pair_to_complex(input_bytes[input_index], input_bytes[input_index + 1]);
        input_index += 2;
        output_index++;
    }

    if (input_index < input_byte_count){
        state->pending_i = input_bytes[input_index];
        state->has_pending_i = true;
    }

    *samples_written = output_index;

    return IQ_ADAPTER_OK;

}
