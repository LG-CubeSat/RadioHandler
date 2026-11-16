#ifndef IQ_PIPELINE_H
#define IQ_PIPELINE_H

#include "iq_adapter.h"
#include "dsp.h"

typedef struct {

    iq_adapter_state_t adapter;
    dsp_state_t dsp;
    bool dsp_enabled;

};

#endif