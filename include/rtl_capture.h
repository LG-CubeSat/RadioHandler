#ifndef RTL_CAPTURE_H
#define RTL_CAPTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <rtl-sdr.h>

#define RTL_CAPTURE_DEFAULT_BUFFER_COUNT 15U
#define RTL_CAPTURE_DEFAULT_BUFFER_SIZE_BYTES 262144U
#define RTL_CAPTURE_MAX_BUFFER_COUNT 64U
#define RTL_CAPTURE_MAX_BUFFER_SIZE_BYTES 1048576U

typedef struct {

    uint32_t device_index;
    uint32_t center_frequency_hz;
    uint32_t sample_rate_hz;
    int32_t frequency_correction_ppm;
    bool automatic_gain;
    int32_t gain_tenths_db;
    uint32_t usb_buffer_count;
    uint32_t usb_buffer_size_bytes;

} rtl_capture_config_t;

typedef struct {
    uint64_t blocks_received;
    uint64_t bytes_received;
    int32_t driver_status;
    int32_t callback_status;
} rtl_capture_stats_t;

struct rtl_capture_runtime;

typedef struct {

    rtlsdr_dev_t *device;
    uint32_t actual_sample_rate_hz;
    uint32_t actual_center_frequency_hz;
    rtl_capture_config_t config;
    rtl_capture_stats_t stats;
    struct rtl_capture_runtime *runtime;

} rtl_capture_state_t;

typedef enum {

    RTL_CAPTURE_OK = 0,
    RTL_CAPTURE_STOP = 1,
    RTL_CAPTURE_ERROR_NULL = -1,
    RTL_CAPTURE_ERROR_RANGE = -2,
    RTL_CAPTURE_ERROR_ALREADY_OPEN = -3,
    RTL_CAPTURE_ERROR_OPEN = -4,
    RTL_CAPTURE_ERROR_CONFIG = -5,
    RTL_CAPTURE_ERROR_NOT_OPEN = -6,
    RTL_CAPTURE_ERROR_BUSY = -7,
    RTL_CAPTURE_ERROR_MEMORY = -8,
    RTL_CAPTURE_ERROR_THREAD = -9,
    RTL_CAPTURE_ERROR_READ = -10,
    RTL_CAPTURE_ERROR_CALLBACK = -11,
    RTL_CAPTURE_ERROR_CLOSE = -12,
    RTL_CAPTURE_ERROR_STATE = -13
    
} rtl_capture_status_t;

typedef int32_t (*rtl_capture_callback_t)(const uint8_t *input_bytes,
                                         size_t input_byte_count,
                                         void *context);

int32_t rtl_capture_open(rtl_capture_state_t *state, const rtl_capture_config_t *config);
int32_t rtl_capture_run(rtl_capture_state_t *state, rtl_capture_callback_t callback, void *context);
int32_t rtl_capture_request_stop(rtl_capture_state_t *state);
int32_t rtl_capture_close(rtl_capture_state_t *state);
const char *rtl_capture_status_string(int32_t status);

#endif
