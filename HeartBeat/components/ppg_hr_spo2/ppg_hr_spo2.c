// Heart rate (BPM) and SpO2 from raw IR/RED samples. See ppg_hr_spo2.h for how to use it.
#include "ppg_hr_spo2.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define EXTRA_BEFORE   (PPG_MA_SIZE / 2)               // samples before the window, needed by the moving average
#define FIRST_OUTPUT   (PPG_BUF_LEN + EXTRA_BEFORE)    // first result after 114 samples (about 11.4 s)

void ppg_init(ppg_state_t *s)
{
    memset(s, 0, sizeof(*s));
}

static double mean(const float *x, int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; i++) sum += x[i];
    return sum / n;
}

static double rms(const float *x, int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; i++) sum += (double)x[i] * x[i];
    return sqrt(sum / n);
}

static double std_dev(const float *x, int n)
{
    double m = mean(x, n);
    double sum = 0.0;
    for (int i = 0; i < n; i++) sum += (x[i] - m) * (x[i] - m);
    return sqrt(sum / n);
}

// Pulse signal = raw value minus the average of the 10 samples around it.
// This removes the slow baseline and leaves only the heartbeat wiggle.
static void remove_baseline(const float *raw, float *pulse)
{
    for (int k = 0; k < PPG_WINDOW_SAMPLES; k++) {
        int first = k;                                    // the 10 samples around window sample k
        double avg = 0.0;
        for (int j = first; j < first + PPG_MA_SIZE; j++) avg += raw[j];
        avg /= PPG_MA_SIZE;
        pulse[k] = (float)(raw[EXTRA_BEFORE + k] - avg);
    }
}

// How much the peak at p rises above the higher of the two valleys on either side of it
static float prominence(const float *x, int n, int p)
{
    float left_min = x[p], right_min = x[p];
    for (int j = p; j >= 0 && x[j] <= x[p]; j--) if (x[j] < left_min) left_min = x[j];
    for (int j = p; j < n && x[j] <= x[p]; j++)  if (x[j] < right_min) right_min = x[j];
    return x[p] - fmaxf(left_min, right_min);
}

// Finds the heartbeats (peaks) in the pulse signal. Returns how many were found; beats[] holds their positions.
static int find_beats(const float *x, int n, int *beats)
{
    // 1) every local maximum is a candidate
    int cand[PPG_MAX_BEATS];
    bool alive[PPG_MAX_BEATS];
    int nc = 0;
    for (int i = 1; i < n - 1; i++) {
        if (x[i - 1] < x[i] && x[i] >= x[i + 1]) {
            alive[nc] = true;
            cand[nc++] = i;
        }
    }

    // 2) take the tallest candidate, remove the ones closer than the minimum gap, and repeat
    bool kept[PPG_MAX_BEATS] = { false };
    while (true) {
        int tallest = -1;
        for (int k = 0; k < nc; k++) {
            if (alive[k] && (tallest < 0 || x[cand[k]] > x[cand[tallest]])) tallest = k;
        }
        if (tallest < 0) break;
        kept[tallest] = true;
        for (int k = 0; k < nc; k++) {
            if (abs(cand[k] - cand[tallest]) < PPG_MIN_PEAK_GAP) alive[k] = false;
        }
    }

    // 3) keep only the peaks that stand out enough from the valleys around them
    float min_prominence = PPG_PROMINENCE * (float)std_dev(x, n);
    int count = 0;
    for (int k = 0; k < nc; k++) {
        if (kept[k] && prominence(x, n, cand[k]) >= min_prominence) beats[count++] = cand[k];
    }
    return count;
}

// BPM = 60 / average time between beats
static float calc_bpm(const int *beats, int count)
{
    double avg_gap_s = (double)(beats[count - 1] - beats[0]) / (count - 1) / PPG_SAMPLE_RATE_HZ;
    return (float)(60.0 / avg_gap_s);
}

// SpO2 from the ratio of ratios. AC = size of the pulse (RMS), DC = average level of the raw signal.
static float calc_spo2(const float *ir, const float *red, const float *pulse_ir, const float *pulse_red)
{
    double ac_ir = rms(pulse_ir, PPG_WINDOW_SAMPLES);
    double ac_red = rms(pulse_red, PPG_WINDOW_SAMPLES);
    double dc_ir = mean(&ir[EXTRA_BEFORE], PPG_WINDOW_SAMPLES);
    double dc_red = mean(&red[EXTRA_BEFORE], PPG_WINDOW_SAMPLES);
    if (ac_ir == 0.0) return 0.0f;
    double R = (ac_red / dc_red) / (ac_ir / dc_ir);
    return (float)(PPG_SPO2_A - PPG_SPO2_B * R);
}

bool ppg_push(ppg_state_t *s, uint32_t ir, uint32_t red, ppg_result_t *out)
{
    // slide the buffer by one sample and add the new one
    memmove(&s->ir[0], &s->ir[1], (PPG_BUF_LEN - 1) * sizeof(float));
    memmove(&s->red[0], &s->red[1], (PPG_BUF_LEN - 1) * sizeof(float));
    s->ir[PPG_BUF_LEN - 1] = (float)ir;
    s->red[PPG_BUF_LEN - 1] = (float)red;
    s->count++;

    // wait until the buffer is full, then give one result per second
    if (s->count < FIRST_OUTPUT || (s->count - FIRST_OUTPUT) % PPG_SAMPLE_RATE_HZ != 0) {
        return false;
    }

    float pulse_ir[PPG_WINDOW_SAMPLES], pulse_red[PPG_WINDOW_SAMPLES];
    remove_baseline(s->ir, pulse_ir);
    remove_baseline(s->red, pulse_red);

    out->spo2 = calc_spo2(s->ir, s->red, pulse_ir, pulse_red);

    int beats[PPG_MAX_BEATS];
    int count = find_beats(pulse_ir, PPG_WINDOW_SAMPLES, beats);
    out->bpm_valid = (count >= 3);
    out->bpm = out->bpm_valid ? calc_bpm(beats, count) : 0.0f;
    return true;
}
