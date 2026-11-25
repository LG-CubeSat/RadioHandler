#include "rtl_capture.h"
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

struct rtl_capture_runtime {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    bool running;
    bool stop_requested;
    bool finished;
    bool needs_reopen;
};

typedef struct {
    rtl_capture_state_t *state;
    rtl_capture_callback_t callback;
    void *context;
    rtl_capture_stats_t stats;
    bool invalid_input;
} rtl_capture_session_t;

static bool rtl_capture_should_stop(struct rtl_capture_runtime *runtime){
    pthread_mutex_lock(&runtime->mutex);
    bool stop_requested = runtime->stop_requested;
    pthread_mutex_unlock(&runtime->mutex);
    return stop_requested;
}

static void *rtl_capture_cancel_worker(void *context){
    rtl_capture_session_t *session = context;
    struct rtl_capture_runtime *runtime = session->state->runtime;
    const struct timespec pause = {.tv_sec = 0, .tv_nsec = 1000000};

    for (;;){
        pthread_mutex_lock(&runtime->mutex);
        while (!runtime->stop_requested && !runtime->finished){
            pthread_cond_wait(&runtime->condition, &runtime->mutex);
        }
        bool finished = runtime->finished;
        pthread_mutex_unlock(&runtime->mutex);

        if (finished) return NULL;
        rtlsdr_cancel_async(session->state->device);
        nanosleep(&pause, NULL);
    }
}

static void rtl_capture_handle_samples(unsigned char *input_bytes,
                                       uint32_t input_byte_count,
                                       void *context){
    rtl_capture_session_t *session = context;
    if (rtl_capture_should_stop(session->state->runtime)) return;
    if (input_byte_count == 0) return;

    if (input_bytes == NULL){
        session->invalid_input = true;
        rtl_capture_request_stop(session->state);
        return;
    }

    session->stats.blocks_received++;
    session->stats.bytes_received += input_byte_count;
    session->stats.callback_status = session->callback(input_bytes, input_byte_count, session->context);

    if (session->stats.callback_status != RTL_CAPTURE_OK){
        rtl_capture_request_stop(session->state);
    }
}

int32_t rtl_capture_open(rtl_capture_state_t *state, const rtl_capture_config_t *config){
    if (state == NULL || config == NULL) return RTL_CAPTURE_ERROR_NULL;
    if (state->device != NULL || state->runtime != NULL) return RTL_CAPTURE_ERROR_ALREADY_OPEN;

    uint32_t buffer_count = config->usb_buffer_count == 0
        ? RTL_CAPTURE_DEFAULT_BUFFER_COUNT : config->usb_buffer_count;
    uint32_t buffer_size = config->usb_buffer_size_bytes == 0
        ? RTL_CAPTURE_DEFAULT_BUFFER_SIZE_BYTES : config->usb_buffer_size_bytes;

    if (config->center_frequency_hz == 0 ||
        config->frequency_correction_ppm < -488 ||
        config->frequency_correction_ppm > 488 ||
        config->sample_rate_hz <= 225000 ||
        config->sample_rate_hz > 3200000 ||
        (config->sample_rate_hz > 300000 && config->sample_rate_hz <= 900000) ||
        buffer_count > RTL_CAPTURE_MAX_BUFFER_COUNT ||
        buffer_size > RTL_CAPTURE_MAX_BUFFER_SIZE_BYTES ||
        buffer_size % 512 != 0){
        return RTL_CAPTURE_ERROR_RANGE;
    }

    rtlsdr_dev_t *device = NULL;
    struct rtl_capture_runtime *runtime = NULL;
    int *gains = NULL;
    bool mutex_initialized = false;
    bool condition_initialized = false;
    int32_t status = RTL_CAPTURE_ERROR_CONFIG;

    if (rtlsdr_open(&device, config->device_index) != 0 || device == NULL){
        status = RTL_CAPTURE_ERROR_OPEN;
        goto cleanup;
    }

    if (rtlsdr_set_sample_rate(device, config->sample_rate_hz) != 0) goto cleanup;

    if (rtlsdr_get_freq_correction(device) != config->frequency_correction_ppm){
        if (rtlsdr_set_freq_correction(device, config->frequency_correction_ppm) != 0) goto cleanup;
    }

    if (rtlsdr_set_center_freq(device, config->center_frequency_hz) != 0) goto cleanup;
    if (rtlsdr_set_agc_mode(device, 0) != 0) goto cleanup;
    if (rtlsdr_set_tuner_gain_mode(device, config->automatic_gain ? 0 : 1) != 0) goto cleanup;

    if (!config->automatic_gain){
        int gain_count = rtlsdr_get_tuner_gains(device, NULL);
        if (gain_count <= 0 || (size_t)gain_count > SIZE_MAX / sizeof(*gains)) goto cleanup;

        gains = malloc((size_t)gain_count * sizeof(*gains));
        if (gains == NULL){
            status = RTL_CAPTURE_ERROR_MEMORY;
            goto cleanup;
        }
        if (rtlsdr_get_tuner_gains(device, gains) != gain_count) goto cleanup;

        bool supported_gain = false;
        for (int i = 0; i < gain_count; i++){
            if (gains[i] == config->gain_tenths_db){
                supported_gain = true;
                break;
            }
        }

        if (!supported_gain){
            status = RTL_CAPTURE_ERROR_RANGE;
            goto cleanup;
        }

        if (rtlsdr_set_tuner_gain(device, config->gain_tenths_db) != 0) goto cleanup;
    }

    uint32_t actual_sample_rate_hz = rtlsdr_get_sample_rate(device);
    uint32_t actual_center_frequency_hz = rtlsdr_get_center_freq(device);
    if (actual_sample_rate_hz == 0) goto cleanup;
    if (actual_center_frequency_hz == 0) goto cleanup;
    if (rtlsdr_reset_buffer(device) != 0) goto cleanup;

    runtime = calloc(1, sizeof(*runtime));
    if (runtime == NULL){
        status = RTL_CAPTURE_ERROR_MEMORY;
        goto cleanup;
    }
    if (pthread_mutex_init(&runtime->mutex, NULL) != 0){
        status = RTL_CAPTURE_ERROR_THREAD;
        goto cleanup;
    }
    mutex_initialized = true;
    if (pthread_cond_init(&runtime->condition, NULL) != 0){
        status = RTL_CAPTURE_ERROR_THREAD;
        goto cleanup;
    }
    condition_initialized = true;

    rtl_capture_state_t prepared = {
        .device = device,
        .actual_sample_rate_hz = actual_sample_rate_hz,
        .actual_center_frequency_hz = actual_center_frequency_hz,
        .config = *config,
        .runtime = runtime
    };
    prepared.config.usb_buffer_count = buffer_count;
    prepared.config.usb_buffer_size_bytes = buffer_size;

    free(gains);
    *state = prepared;
    return RTL_CAPTURE_OK;

cleanup:
    free(gains);
    if (condition_initialized) pthread_cond_destroy(&runtime->condition);
    if (mutex_initialized) pthread_mutex_destroy(&runtime->mutex);
    free(runtime);
    if (device != NULL) rtlsdr_close(device);
    return status;
}

int32_t rtl_capture_run(rtl_capture_state_t *state, rtl_capture_callback_t callback, void *context){
    if (state == NULL || callback == NULL) return RTL_CAPTURE_ERROR_NULL;
    if (state->device == NULL || state->runtime == NULL) return RTL_CAPTURE_ERROR_NOT_OPEN;

    struct rtl_capture_runtime *runtime = state->runtime;
    pthread_mutex_lock(&runtime->mutex);
    if (runtime->running){
        pthread_mutex_unlock(&runtime->mutex);
        return RTL_CAPTURE_ERROR_BUSY;
    }
    if (runtime->needs_reopen){
        pthread_mutex_unlock(&runtime->mutex);
        return RTL_CAPTURE_ERROR_STATE;
    }
    runtime->running = true;
    runtime->stop_requested = false;
    runtime->finished = false;
    pthread_mutex_unlock(&runtime->mutex);

    rtl_capture_session_t session = {
        .state = state,
        .callback = callback,
        .context = context
    };
    pthread_t cancel_worker;
    bool worker_started = false;
    bool read_started = false;
    bool needs_reopen = false;
    int32_t status = RTL_CAPTURE_OK;

    session.stats.driver_status = rtlsdr_reset_buffer(state->device);
    if (session.stats.driver_status != 0){
        status = RTL_CAPTURE_ERROR_CONFIG;
        needs_reopen = true;
        goto finish;
    }

    if (pthread_create(&cancel_worker, NULL, rtl_capture_cancel_worker, &session) != 0){
        status = RTL_CAPTURE_ERROR_THREAD;
        goto finish;
    }
    worker_started = true;

    if (!rtl_capture_should_stop(runtime)){
        read_started = true;
        session.stats.driver_status = rtlsdr_read_async(
            state->device, rtl_capture_handle_samples, &session,
            state->config.usb_buffer_count, state->config.usb_buffer_size_bytes);
    }

finish:
    pthread_mutex_lock(&runtime->mutex);
    bool stop_requested = runtime->stop_requested;
    runtime->finished = true;
    pthread_cond_broadcast(&runtime->condition);
    pthread_mutex_unlock(&runtime->mutex);

    if (worker_started) pthread_join(cancel_worker, NULL);

    if (session.invalid_input || session.stats.driver_status != 0 ||
        (read_started && !stop_requested)){
        needs_reopen = true;
        if (status == RTL_CAPTURE_OK) status = RTL_CAPTURE_ERROR_READ;
    }
    if (session.stats.callback_status != RTL_CAPTURE_OK &&
        session.stats.callback_status != RTL_CAPTURE_STOP){
        status = RTL_CAPTURE_ERROR_CALLBACK;
    }

    state->stats = session.stats;
    pthread_mutex_lock(&runtime->mutex);
    runtime->needs_reopen = needs_reopen;
    runtime->running = false;
    pthread_mutex_unlock(&runtime->mutex);
    return status;
}

int32_t rtl_capture_request_stop(rtl_capture_state_t *state){
    if (state == NULL) return RTL_CAPTURE_ERROR_NULL;
    if (state->device == NULL || state->runtime == NULL) return RTL_CAPTURE_ERROR_NOT_OPEN;

    struct rtl_capture_runtime *runtime = state->runtime;
    pthread_mutex_lock(&runtime->mutex);
    if (runtime->running){
        runtime->stop_requested = true;
        pthread_cond_signal(&runtime->condition);
    }
    pthread_mutex_unlock(&runtime->mutex);
    return RTL_CAPTURE_OK;
}

int32_t rtl_capture_close(rtl_capture_state_t *state){
    if (state == NULL) return RTL_CAPTURE_ERROR_NULL;
    if (state->device == NULL && state->runtime == NULL) return RTL_CAPTURE_OK;
    if (state->device == NULL || state->runtime == NULL) return RTL_CAPTURE_ERROR_STATE;

    struct rtl_capture_runtime *runtime = state->runtime;
    pthread_mutex_lock(&runtime->mutex);
    bool running = runtime->running;
    pthread_mutex_unlock(&runtime->mutex);
    if (running) return RTL_CAPTURE_ERROR_BUSY;

    int driver_status = rtlsdr_close(state->device);
    pthread_cond_destroy(&runtime->condition);
    pthread_mutex_destroy(&runtime->mutex);
    free(runtime);
    *state = (rtl_capture_state_t){0};

    return driver_status == 0 ? RTL_CAPTURE_OK : RTL_CAPTURE_ERROR_CLOSE;
}

const char *rtl_capture_status_string(int32_t status){
    switch (status){
        case RTL_CAPTURE_OK: return "Success";
        case RTL_CAPTURE_STOP: return "Callback requested stop";
        case RTL_CAPTURE_ERROR_NULL: return "Required pointer is NULL";
        case RTL_CAPTURE_ERROR_RANGE: return "Configuration value is out of range";
        case RTL_CAPTURE_ERROR_ALREADY_OPEN: return "Receiver is already open";
        case RTL_CAPTURE_ERROR_OPEN: return "Could not open receiver";
        case RTL_CAPTURE_ERROR_CONFIG: return "Receiver configuration failed";
        case RTL_CAPTURE_ERROR_NOT_OPEN: return "Receiver is not open";
        case RTL_CAPTURE_ERROR_BUSY: return "Capture is running";
        case RTL_CAPTURE_ERROR_MEMORY: return "Memory allocation failed";
        case RTL_CAPTURE_ERROR_THREAD: return "Thread initialization failed";
        case RTL_CAPTURE_ERROR_READ: return "Capture failed or ended unexpectedly";
        case RTL_CAPTURE_ERROR_CALLBACK: return "Sample callback failed";
        case RTL_CAPTURE_ERROR_CLOSE: return "Could not close receiver";
        case RTL_CAPTURE_ERROR_STATE: return "Invalid state; close and reopen receiver";
        default: return "Unknown capture status";
    }
}
