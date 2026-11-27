// Heart rate (BPM) and SpO2 estimate from raw MAX30102 IR/RED samples.
// Call ppg_push() every 100 ms (10 Hz). It returns true once per second when a new result is ready,
// the first one after about 11.4 s. SpO2 uses a generic formula and is not calibrated.
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Settings
#define PPG_SAMPLE_RATE_HZ    10      // one sample every 100 ms
#define PPG_WINDOW_SAMPLES    100     // 10 s window
#define PPG_MA_SIZE           10      // moving average length (1 s)
#define PPG_MIN_PEAK_GAP      4       // beats at least 0.4 s apart
#define PPG_PROMINENCE        0.3f    // a beat must stand out by this fraction of the signal's std
#define PPG_SPO2_A            110.0f  // SpO2 = A - B * R
#define PPG_SPO2_B            25.0f

// The window plus the extra samples the moving average needs (5 before, 4 after)
#define PPG_BUF_LEN           (PPG_WINDOW_SAMPLES + PPG_MA_SIZE - 1)
#define PPG_MAX_BEATS         (PPG_WINDOW_SAMPLES / 2)

typedef struct {
    float bpm;          // heart rate, 0 if fewer than 3 beats were found
    bool  bpm_valid;    // true if at least 3 beats were found in this window
    float spo2;         // SpO2 estimate in %, can go above 100 (not calibrated)
} ppg_result_t;

typedef struct {
    float    ir[PPG_BUF_LEN];     // the most recent raw samples
    float    red[PPG_BUF_LEN];
    uint32_t count;               // samples pushed so far
} ppg_state_t;

void ppg_init(ppg_state_t *s);

// Push one raw sample. Returns true when *out holds a new result.
bool ppg_push(ppg_state_t *s, uint32_t ir, uint32_t red, ppg_result_t *out);

#ifdef __cplusplus
}
#endif
