// Wrapper around the Edge Impulse model (see ei_anomaly.h).
#include "ei_anomaly.h"
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

// The model must have 3 inputs (bpm, spo2, temp_c) and be a K-means anomaly model
static_assert(EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME == 3, "Expected 3 axes: bpm, spo2, temp_c");
static_assert(EI_CLASSIFIER_HAS_ANOMALY == EI_ANOMALY_TYPE_KMEANS, "Expected a K-means anomaly model");

extern "C" int ei_anomaly_window_rows(void)
{
    return EI_CLASSIFIER_RAW_SAMPLE_COUNT;
}

extern "C" bool ei_anomaly_score(const float *window, float *score)
{
    signal_t signal;
    if (ei::numpy::signal_from_buffer(window, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal) != 0) {
        return false;
    }
    ei_impulse_result_t result = { 0 };
    if (run_classifier(&signal, &result, false) != EI_IMPULSE_OK) {
        return false;
    }
    *score = result.anomaly;
    return true;
}
