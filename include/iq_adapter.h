#ifndef IQ_ADAPTER_H
#define IQ_ADAPTER_H

#include <complex.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {

    IQ_ADAPTER_OK = 0,
    IQ_ADAPTER_ERROR_NULL = -1,
    IQ_ADAPTER_ERROR_BUFFER_TOO_SMALL = -2,

} iq_adapter_status_t;

typedef struct {

    uint8_t pending_i;  // saves unmatched i byte
    bool has_pending_i; // indicated whether saved byte exists

} iq_adapter_state_t;

int32_t iq_adapter_convert(iq_adapter_state_t *state, const uint8_t *input_bytes, size_t input_byte_count, float complex *output_samples, size_t output_capacity, size_t *samples_written);


#endif