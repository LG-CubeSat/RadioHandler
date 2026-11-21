#include "iq_pipeline.h"
#include <math.h>
#include <stdio.h>
#include <inttypes.h>

int main(void){

    // DSP Disable Test

    iq_pipeline_state_t dsp_disable_test = {0};

    int32_t dsp_disable_init_status = iq_pipeline_init(&dsp_disable_test, 1000, 100, 5, 150, false);

    if (dsp_disable_init_status != IQ_PIPELINE_OK){
        printf("IQ pipeline failed to init with error code: %" PRId32 "\n", dsp_disable_init_status);
        return -1;
    }

    uint8_t dsp_disable_input_bytes[] = {255, 0, 0, 255};
    float complex dsp_disable_output[2] = {0};
    size_t samples_written = 0;

    int32_t dsp_disable_process_status = iq_pipeline_process(&dsp_disable_test, dsp_disable_input_bytes, 4, dsp_disable_output, 2, &samples_written, 50);

    if (dsp_disable_process_status != IQ_PIPELINE_OK){
        printf("IQ pipeline failed to process with error code: %" PRId32 "\n", dsp_disable_process_status);
        return -2;
    }

    if (samples_written != 2){
        printf("Didn't write 2 samples, wrote %zu samples\n", samples_written);
        return -3;
    }

    float index_1_diff = cabsf(dsp_disable_output[1] - (-0.99609375f + 0.99609375f * _Complex_I));
    float index_0_diff = cabsf(dsp_disable_output[0] - (0.99609375f - 0.99609375f * _Complex_I));

    if (!isfinite(index_1_diff) || !isfinite(index_0_diff)) {
        printf("Differences are not finite\n");
        return -4;
    }

    if (index_0_diff > 1e-6f){
        printf("Output sample at index 0 is incorrect\n");
        return -5;
    }

    if (index_1_diff > 1e-6f){
        printf("Output sample at index 1 is incorrect\n");
        return -6;
    }

    // DSP enabled test

    iq_pipeline_state_t enabled_pipeline = {0};

    int32_t enabled_pipeline_init_status = iq_pipeline_init(&enabled_pipeline, 1000, 100, 5, 150, true);
    if (enabled_pipeline_init_status != IQ_PIPELINE_OK) {
        printf("Enabled pipeline failed to init with code: %" PRId32 "\n", enabled_pipeline_init_status);
        return -7;
    }

    dsp_state_t reference_dsp = {0};

    int32_t reference_dsp_init_status = dsp_init(&reference_dsp, 1000, 100, 5, 150);
    if (reference_dsp_init_status != DSP_OK) {
        printf("Reference DSP failed to init with code: %" PRId32 "\n", reference_dsp_init_status);
        return -8;
    }

    float complex expected[2] = {0.99609375f - 0.99609375f * _Complex_I, -0.99609375f + 0.99609375f * _Complex_I};
    
    int32_t reference_shift_status = dsp_shift(&reference_dsp, expected, 2, 50);
    if (reference_shift_status != DSP_OK) {
        printf("Reference dsp shift failed with code: %" PRId32 "\n", reference_shift_status);
        return -9;
    }

    int32_t reference_process_status = dsp_process(&reference_dsp, expected, 2);
    if (reference_process_status != DSP_OK) {
        printf("Reference dsp process failed with code: %" PRId32 "\n", reference_process_status);
        return -10;
    }

    uint8_t dsp_enable_input_bytes[] = {255, 0, 0, 255};
    float complex dsp_enable_output[2] = {0};
    size_t dsp_enable_samples_written = 0;

    int32_t enabled_pipeline_process_status = iq_pipeline_process(&enabled_pipeline, dsp_enable_input_bytes, 4, dsp_enable_output, 2, &dsp_enable_samples_written, 50);

    if (enabled_pipeline_process_status != IQ_PIPELINE_OK){
        printf("Enabled pipeline process failed with code: %" PRId32 "\n", enabled_pipeline_process_status);
        return -11;
    }
    if (dsp_enable_samples_written != 2){
        printf("Wrote %zu samples instead of 2 during enabled pipeline process\n", dsp_enable_samples_written);
        return -12;
    }

    for (size_t i = 0; i < 2; i++){
        float difference = cabsf(dsp_enable_output[i] - expected[i]);
        if (!isfinite(difference)){
            printf("Difference is not finite at index %zu\n", i);
            return -13;
        }
        if (difference > 1e-6f){
            printf("Actual value is not the same as the expected value at index %zu\n", i);
            return -14;
        }
    }

    // Test safe switch: Disable

    int32_t set_disabled_status = iq_pipeline_set_enabled(&enabled_pipeline, false); // set disabled
    if (set_disabled_status != IQ_PIPELINE_OK){
        printf("Setting to disabled failed with code: %" PRId32 "\n", set_disabled_status);
        return -15;
    }
    if (enabled_pipeline.dsp_enabled){
        printf("DSP disable set DSP to enabled in state\n");
        return -16;
    }
    uint8_t one_byte[] = {255};
    size_t safe_switch_samples_written = 0;

    int32_t safe_switch_process_status = iq_pipeline_process(&enabled_pipeline, one_byte, 1, dsp_enable_output, 2, &safe_switch_samples_written, 50);
    if (safe_switch_process_status != IQ_PIPELINE_OK){
        printf("Safe switch one byte iq process failed with code: %" PRId32 "\n", safe_switch_process_status);
        return -17;
    }
    if (safe_switch_samples_written != 0){
        printf("Wrote %zu samples in safe switch one byte test instead of 0\n", safe_switch_samples_written);
        return -18;
    }
    if (!enabled_pipeline.adapter.has_pending_i){
        printf("Adapter did not save flag pending i in state\n");
        return -19;
    }
    if (enabled_pipeline.adapter.pending_i != 255){
        printf("Adapter did not save pending i in state\n");
        return -20;
    }

    // Test DSP re-enable

    int32_t reenable_dsp_safe_switch_status = iq_pipeline_set_enabled(&enabled_pipeline, true); // set enabled
    if (reenable_dsp_safe_switch_status != IQ_PIPELINE_OK){
        printf("DSP reenable failed with code: %" PRId32 "\n", reenable_dsp_safe_switch_status);
        return -21;
    }
    if (!enabled_pipeline.dsp_enabled){
        printf("Didn't flag dsp as enabled in state!\n");
        return -22;
    }
    if (!enabled_pipeline.adapter.has_pending_i){
        printf("Didn't flag pending i in adapter state!\n");
        return -23;
    }
    if (enabled_pipeline.adapter.pending_i != 255){
        printf("Adapter did not save pending i in state during dsp reenable\n");
        return -24;
    }
    if (enabled_pipeline.dsp.shift_phase != 0.0){
        printf("Shift phase changed in dsp\n");
        return -25;
    }

    for (size_t i = 0; i < DSP_MAX_TAPS; i++){
        if (enabled_pipeline.dsp.history[i] != 0.0f){
            printf("FIR history is not 0 at index %zu\n", i);
            return -26;
        }
    }

    uint8_t byte_0[] = {0};
    size_t byte0_safe_switch_samples_written = 0;


    int32_t safe_switch_byte0_process_status = iq_pipeline_process(&enabled_pipeline, byte_0, 1, dsp_enable_output, 2, &byte0_safe_switch_samples_written, 50);
    if (safe_switch_byte0_process_status != IQ_PIPELINE_OK){
        printf("Byte 0 iq process failed with code %" PRId32 "\n", safe_switch_byte0_process_status);
        return -27;
    }
    if (byte0_safe_switch_samples_written != 1){
        printf("Wrote %zu samples instead of 1", byte0_safe_switch_samples_written);
        return -28;
    }
    if (enabled_pipeline.adapter.has_pending_i){
        printf("Did not flag pending i as gone after byte 0 process\n");
        return -29;
    }

    float safe_switch_diff = cabsf(dsp_enable_output[0] - reference_dsp.fir_coefficients[0] * (0.99609375f - 0.99609375f * _Complex_I));
    if (!isfinite(safe_switch_diff)){
        printf("Safe switch diff is not finite\n");
        return -30;
    }
    if (safe_switch_diff > 1e-6f){
        printf("Safe switch difference is too much\n");
        return -31;
    }

    return 0;

}