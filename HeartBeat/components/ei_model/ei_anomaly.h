// C interface to the Edge Impulse anomaly model, so main.c can stay in C.
// A window is ei_anomaly_window_rows() rows of 3 values, oldest row first: bpm, spo2, temp_c.
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Number of rows (one per second) the model needs
int ei_anomaly_window_rows(void);

// Runs the model on one window. Returns false if it failed.
// *score is the anomaly score: higher = further from normal, below 0 = inside a normal cluster.
bool ei_anomaly_score(const float *window, float *score);

#ifdef __cplusplus
}
#endif
