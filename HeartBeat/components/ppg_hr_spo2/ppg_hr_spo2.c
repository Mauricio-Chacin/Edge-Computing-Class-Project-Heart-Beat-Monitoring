// Heart rate (BPM) and SpO2 from raw IR/RED samples. See ppg_hr_spo2.h for how to use it.
#include "ppg_hr_spo2.h"

#include <math.h>
#include <string.h>

// First result after 114 samples (about 11.4 s): the window plus the extra samples the moving average needs
#define PPG_FIRST_OUTPUT   (PPG_BUF_LEN + PPG_MA_SIZE / 2)
#define PPG_OUTPUT_EVERY   PPG_SAMPLE_RATE_HZ      // one result per second

void ppg_init(ppg_state_t *s)
{
    memset(s, 0, sizeof(*s));
}

// Finds the beats in x (the baseline-removed IR) and converts the gaps between them to BPM.
// Same steps as scipy find_peaks(distance=4, prominence=0.3*std). Returns the number of beats found;
// *bpm is only set if there are at least 3.
static int find_beats(ppg_state_t *s, const float *x, int n, float x_std, float *bpm)
{
    int *pos = s->pos;
    int np = 0;

    // 1) local maxima
    for (int i = 1; i < n - 1 && np < PPG_MAX_PEAKS; i++) {
        if (x[i - 1] < x[i] && x[i] >= x[i + 1]) {
            pos[np++] = i;
        }
    }
    if (np == 0) {
        return 0;
    }

    // 2) minimum distance: going from the highest peak down, drop neighbours that are too close
    for (int k = 0; k < np; k++) {
        s->keep[k] = true;
        s->order[k] = k;
    }
    for (int a = 1; a < np; a++) {                       // sort by height, highest first
        int v = s->order[a];
        int b = a - 1;
        while (b >= 0 && x[pos[s->order[b]]] < x[pos[v]]) {
            s->order[b + 1] = s->order[b];
            b--;
        }
        s->order[b + 1] = v;
    }
    for (int a = 0; a < np; a++) {
        int idx = s->order[a];
        if (!s->keep[idx]) {
            continue;
        }
        for (int k = idx - 1; k >= 0 && pos[idx] - pos[k] < PPG_MIN_PEAK_GAP; k--) {
            s->keep[k] = false;
        }
        for (int k = idx + 1; k < np && pos[k] - pos[idx] < PPG_MIN_PEAK_GAP; k++) {
            s->keep[k] = false;
        }
    }

    // 3) prominence: drop peaks that do not stand out enough from the valleys around them
    int final_pos[PPG_MAX_PEAKS];
    int nf = 0;
    const float min_prom = PPG_PROMINENCE * x_std;
    for (int k = 0; k < np; k++) {
        if (!s->keep[k]) {
            continue;
        }
        int p = pos[k];
        float left_min = x[p];
        for (int j = p; j >= 0 && x[j] <= x[p]; j--) {
            if (x[j] < left_min) left_min = x[j];
        }
        float right_min = x[p];
        for (int j = p; j < n && x[j] <= x[p]; j++) {
            if (x[j] < right_min) right_min = x[j];
        }
        float prominence = x[p] - fmaxf(left_min, right_min);
        if (prominence >= min_prom) {
            final_pos[nf++] = p;
        }
    }
    if (nf < 3) {
        return nf;
    }

    // BPM = 60 / average gap between beats
    double sum = 0.0;
    for (int j = 0; j < nf - 1; j++) {
        sum += (double)(final_pos[j + 1] - final_pos[j]) / PPG_SAMPLE_RATE_HZ;   // gap in seconds
    }
    double mean = sum / (nf - 1);
    *bpm = (float)(60.0 / mean);
    return nf;
}

bool ppg_push(ppg_state_t *s, uint32_t ir, uint32_t red, ppg_result_t *out)
{
    // slide the buffer by one sample and add the new one
    memmove(&s->ir[0], &s->ir[1], (PPG_BUF_LEN - 1) * sizeof(float));
    memmove(&s->red[0], &s->red[1], (PPG_BUF_LEN - 1) * sizeof(float));
    s->ir[PPG_BUF_LEN - 1] = (float)ir;
    s->red[PPG_BUF_LEN - 1] = (float)red;
    s->count++;

    if (s->count < PPG_FIRST_OUTPUT || (s->count - PPG_FIRST_OUTPUT) % PPG_OUTPUT_EVERY != 0) {
        return false;
    }

    // the window is buffer[5..104]; the other samples are only used for the moving average
    const int base = PPG_MA_SIZE / 2;
    double sum_ir = 0.0, sum_red = 0.0, sq_ir = 0.0, sq_red = 0.0;
    for (int k = 0; k < PPG_WINDOW_SAMPLES; k++) {
        int i = base + k;
        double ma_ir = 0.0, ma_red = 0.0;
        for (int j = i - base; j < i - base + PPG_MA_SIZE; j++) {      // moving average
            ma_ir += s->ir[j];
            ma_red += s->red[j];
        }
        ma_ir /= PPG_MA_SIZE;
        ma_red /= PPG_MA_SIZE;
        double ac_ir = s->ir[i] - ma_ir;
        double ac_red = s->red[i] - ma_red;
        s->ac_ir[k] = (float)ac_ir;
        sq_ir += ac_ir * ac_ir;
        sq_red += ac_red * ac_red;
        sum_ir += s->ir[i];
        sum_red += s->red[i];
    }
    double mean_ir = sum_ir / PPG_WINDOW_SAMPLES;
    double mean_red = sum_red / PPG_WINDOW_SAMPLES;

    memset(out, 0, sizeof(*out));

    // SpO2 from the ratio of ratios
    double rms_ir = sqrt(sq_ir / PPG_WINDOW_SAMPLES);
    double rms_red = sqrt(sq_red / PPG_WINDOW_SAMPLES);
    if (rms_ir > 0.0) {
        double R = (rms_red / mean_red) / (rms_ir / mean_ir);
        out->spo2 = (float)(PPG_SPO2_A - PPG_SPO2_B * R);
    }

    // standard deviation of the pulse signal, used for the prominence limit
    double mean_x = 0.0;
    for (int k = 0; k < PPG_WINDOW_SAMPLES; k++) mean_x += s->ac_ir[k];
    mean_x /= PPG_WINDOW_SAMPLES;
    double var_x = 0.0;
    for (int k = 0; k < PPG_WINDOW_SAMPLES; k++) {
        double d = s->ac_ir[k] - mean_x;
        var_x += d * d;
    }
    float x_std = (float)sqrt(var_x / PPG_WINDOW_SAMPLES);

    float bpm = 0.0f;
    if (find_beats(s, s->ac_ir, PPG_WINDOW_SAMPLES, x_std, &bpm) >= 3) {
        out->bpm = bpm;
        out->bpm_valid = true;
    }
    return true;
}
