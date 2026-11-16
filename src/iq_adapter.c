#include "iq_adapter.h"

static float complex iq_u8_pair_to_complex(uint8_t i_byte, uint8_t q_byte){
    float normalized_i = (i_byte - 127.5f) / 128.0f;
    float normalized_q = (q_byte - 127.5f) / 128.0f;

    return normalized_i + normalized_q * _Complex_I;
}