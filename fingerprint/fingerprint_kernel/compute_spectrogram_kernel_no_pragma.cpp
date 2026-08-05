// compute_spectrogram_kernel_no_pragma.cpp
// Baseline version of the kernel with ALL HLS directives removed.
// Used to run csynth without any pragmas so the two synthesis reports
// can be compared directly (with vs without HLS optimisation directives).
//
// LOOP_TRIPCOUNT hints are kept on the FFT loops so that HLS can produce
// a concrete cycle estimate instead of reporting '?' for every loop.
//
// By Netik Maheshwar

#include "fingerprint.h"
#include "compute_spectrogram_kernel.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void fft_baseline(float re[WINDOW_SIZE], float im[WINDOW_SIZE]) {
    // Bit-reversal
    VITIS_LOOP_BIT_REV: for (int i = 1, j = 0; i < WINDOW_SIZE; i++) {
#pragma HLS LOOP_TRIPCOUNT min=4096 max=4096 avg=4096
        int bit = WINDOW_SIZE >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float tmp;
            tmp = re[i]; re[i] = re[j]; re[j] = tmp;
            tmp = im[i]; im[i] = im[j]; im[j] = tmp;
        }
    }

    // Butterfly (no PIPELINE pragma -- HLS schedules at its discretion)
    FFT_STAGE: for (int len = 2; len <= WINDOW_SIZE; len <<= 1) {
#pragma HLS LOOP_TRIPCOUNT min=12 max=12 avg=12
        float ang = -2.0f * (float)M_PI / len;
        float wr  = cosf(ang);
        float wi  = sinf(ang);

        FFT_BLOCK: for (int i = 0; i < WINDOW_SIZE; i += len) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=2048 avg=341
            float cwr = 1.0f, cwi = 0.0f;

            BUTTERFLY: for (int k = 0; k < len / 2; k++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=2048 avg=341
                float ur = re[i + k];
                float ui = im[i + k];
                float vr = re[i + k + len/2] * cwr - im[i + k + len/2] * cwi;
                float vi = re[i + k + len/2] * cwi + im[i + k + len/2] * cwr;
                re[i + k]         = ur + vr;
                im[i + k]         = ui + vi;
                re[i + k + len/2] = ur - vr;
                im[i + k + len/2] = ui - vi;
                float nwr = cwr * wr - cwi * wi;
                float nwi = cwr * wi + cwi * wr;
                cwr = nwr;
                cwi = nwi;
            }
        }
    }
}

void compute_spectrogram_kernel(
    float windows[MAX_WINDOWS][WINDOW_SIZE],
    int   num_windows,
    float hann_energy,
    float spec_power[MAX_FREQ][MAX_WINDOWS]
) {
    // No INTERFACE, no ARRAY_PARTITION, no PIPELINE pragmas.
    // HLS uses default ap_memory interfaces and its own scheduler.

    static float re[WINDOW_SIZE];
    static float im[WINDOW_SIZE];

    WINDOW_LOOP: for (int w = 0; w < num_windows; w++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=100 avg=67

        for (int s = 0; s < WINDOW_SIZE; s++) {
            re[s] = windows[w][s];
            im[s] = 0.0f;
        }

        fft_baseline(re, im);

        for (int f = 0; f < MAX_FREQ; f++) {
            float p = re[f] * re[f] + im[f] * im[f];
            if (f > 0 && f < MAX_FREQ - 1) p *= 2.0f;
            p *= (1.0f / FS);
            p *= (1.0f / hann_energy);
            if (p < 1e-8f) p = 1e-8f;
            spec_power[f][w] = 10.0f * log10f(p);
        }
    }
}
