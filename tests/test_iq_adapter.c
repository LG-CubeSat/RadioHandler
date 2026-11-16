#include "iq_adapter.h"
#include <math.h>
#include <stdio.h>
#include <inttypes.h>

int main(void){

    iq_adapter_state_t iq_pair_preservation_test = {0};

    float complex one_sample_output[] = {0};
    size_t samples_written = 0;
    uint8_t first_input[] = {255};
    uint8_t second_input[] = {0};

    int32_t byte_255_status = iq_adapter_convert(&iq_pair_preservation_test, first_input, 1, one_sample_output, 1, &samples_written);

    if (byte_255_status != IQ_ADAPTER_OK) {
        printf("Byte 255 conversion failed with error code: %" PRId32 "\n", byte_255_status);
        return -1;
    }

    if (samples_written != 0) {
        printf("Byte 255 conversion wrote samples with count: %zu\n", samples_written);
        return -1;
    }

    if (iq_pair_preservation_test.has_pending_i != true) {
        printf("IQ adapter state did not preserve pending i across calls\n");
        return -3;
    }

    if (iq_pair_preservation_test.pending_i != 255) {
        printf("IQ adapter state did not preserve pending i across calls\n");
        return -4;
    }

    int32_t byte_0_status = iq_adapter_convert(&iq_pair_preservation_test, second_input, 1, one_sample_output, 1, &samples_written);

    if (byte_0_status != IQ_ADAPTER_OK) {
        printf("Byte 0 conversion failed with error code: %" PRId32 "\n", byte_0_status);
        return -5;
    }

    if (samples_written != 1) {
        printf("Byte 0 conversion wrote samples with count: %zu\n", samples_written);
        return -6;
    }

    if (iq_pair_preservation_test.has_pending_i != false) {
        printf("Pending i flag was not cleared\n");
        return -7;
    }

    float one_sample_diff = cabsf(one_sample_output[0] - (0.99609375f - 0.99609375f * _Complex_I));

    if (!isfinite(one_sample_diff)){
        printf("One sample difference is not finite!\n");
        return -8;
    }

    if (one_sample_diff > 1e-6f){
        printf("One sample difference is too big!\n");
        return -9;
    }

    return 0;
}