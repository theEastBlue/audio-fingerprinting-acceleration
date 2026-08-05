// compute_spectrogram_kernel.cpp
// Vitis HLS kernel: Hann-pre-windowed FFT + power spectral density.
//
// Pipeline map
//   LOAD loop   -- II=1  (moves one frame from AXI into on-chip re/im BRAMs)
//   FFT         -- butterfly II=21 (loop-carried twiddle recurrence is the
//                  bottleneck; 12 stages x 2048 ops x 21 cycles = 516k cyc/frame)
//   PSD loop    -- II=1  (power -> dB, writes result to AXI output bus)
//
// By Netik Maheshwar

#include "fingerprint.h"
#include "compute_spectrogram_kernel.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------------
// 4096-point radix-2 Cooley-Tukey FFT on separate float re/im arrays.
// ---------------------------------------------------------------------------
static void fft_hls(float re[WINDOW_SIZE], float im[WINDOW_SIZE]) {
#pragma HLS INLINE

    // Bit-reversal permutation
    BIT_REV: for (int i = 1, j = 0; i < WINDOW_SIZE; i++) {
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

    // 12 butterfly stages.
    // Total butterfly ops per FFT: 12 stages x 2048 = 24,576.
    // Achieved II = 21 cycles (set by twiddle recurrence cwr/cwi dependency).
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
#pragma HLS PIPELINE
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

// ---------------------------------------------------------------------------
// Top-level kernel.
// ---------------------------------------------------------------------------
void compute_spectrogram_kernel(
    float windows[MAX_WINDOWS][WINDOW_SIZE],
    int   num_windows,
    float hann_energy,
    float spec_power[MAX_FREQ][MAX_WINDOWS]
) {
#pragma HLS INTERFACE m_axi port=windows    depth=409600 offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=spec_power depth=204900 offset=slave bundle=gmem1
#pragma HLS INTERFACE s_axilite port=num_windows  bundle=control
#pragma HLS INTERFACE s_axilite port=hann_energy  bundle=control
#pragma HLS INTERFACE s_axilite port=return       bundle=control

    static float re[WINDOW_SIZE];
    static float im[WINDOW_SIZE];
#pragma HLS ARRAY_PARTITION variable=re cyclic factor=4
#pragma HLS ARRAY_PARTITION variable=im cyclic factor=4

    WINDOW_LOOP: for (int w = 0; w < num_windows; w++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=100 avg=67

        LOAD: for (int s = 0; s < WINDOW_SIZE; s++) {
#pragma HLS PIPELINE II=1
            re[s] = windows[w][s];
            im[s] = 0.0f;
        }

        fft_hls(re, im);

        PSD: for (int f = 0; f < MAX_FREQ; f++) {
#pragma HLS PIPELINE II=1
            float p = re[f] * re[f] + im[f] * im[f];
            if (f > 0 && f < MAX_FREQ - 1) p *= 2.0f;
            p *= (1.0f / FS);
            p *= (1.0f / hann_energy);
            if (p < 1e-8f) p = 1e-8f;
            spec_power[f][w] = 10.0f * log10f(p);
        }
    }
}
