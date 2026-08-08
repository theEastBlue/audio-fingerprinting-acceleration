// compute_spectrogram_kernel_lut.cpp
// Vitis HLS kernel: FFT + power spectral density using precomputed twiddle LUT.
//
// Key change over kernel.cpp:
//   The iterative twiddle recurrence (cwr/cwi loop-carried dependency, II=21)
//   is replaced by a direct table lookup:
//       cwr = W_real[k * (WINDOW_SIZE/len)]
//       cwi = W_imag[k * (WINDOW_SIZE/len)]
//   Each butterfly now reads its twiddle independently — no loop-carried dep.
//   This allows HLS to pipeline the BUTTERFLY loop at II=1.
//
// By Netik Maheshwar

#include "fingerprint.h"
#include "compute_spectrogram_kernel.h"
#include "coefficients.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void fft_lut(float re[WINDOW_SIZE], float im[WINDOW_SIZE]) {
#pragma HLS INLINE

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

    FFT_STAGE: for (int len = 2; len <= WINDOW_SIZE; len <<= 1) {
#pragma HLS LOOP_TRIPCOUNT min=12 max=12 avg=12
        int half = len >> 1;
        int step = WINDOW_SIZE / len;

        FFT_BLOCK: for (int i = 0; i < WINDOW_SIZE; i += len) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=2048 avg=341

            BUTTERFLY: for (int k = 0; k < half; k++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=2048 avg=341
#pragma HLS PIPELINE II=1
#pragma HLS DEPENDENCE variable=re inter false
#pragma HLS DEPENDENCE variable=im inter false
                int idx   = k * step;
                float cwr = W_real[idx];
                float cwi = W_imag[idx];

                int p = i + k;
                int q = p + half;
                float ur = re[p];
                float ui = im[p];
                float vr = re[q] * cwr - im[q] * cwi;
                float vi = re[q] * cwi + im[q] * cwr;
                re[p] = ur + vr;
                im[p] = ui + vi;
                re[q] = ur - vr;
                im[q] = ui - vi;
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

        fft_lut(re, im);

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
