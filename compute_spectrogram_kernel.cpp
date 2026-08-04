#include "compute_spectrogram_kernel.h"
#include <cmath>

// By Netik Maheshwar

// Radix-2 Cooley-Tukey FFT operating on separate real/imag arrays (HLS-friendly).
// Bit-reversal + butterfly in-place on arrays of length WINDOW_SIZE.
static void fft_hls(float re[WINDOW_SIZE], float im[WINDOW_SIZE]) {
#pragma HLS INLINE

    // Bit-reversal permutation
    BIT_REVERSE: for (int i = 1, j = 0; i < WINDOW_SIZE; i++) {
#pragma HLS LOOP_TRIPCOUNT min=4096 max=4096
        int bit = WINDOW_SIZE >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float tmp;
            tmp = re[i]; re[i] = re[j]; re[j] = tmp;
            tmp = im[i]; im[i] = im[j]; im[j] = tmp;
        }
    }

    // Butterfly stages: log2(4096) = 12 stages
    STAGE: for (int len = 2; len <= WINDOW_SIZE; len <<= 1) {
#pragma HLS LOOP_TRIPCOUNT min=12 max=12
        float ang = -2.0f * PI_F / (float)len;
        float wr  = cosf(ang);
        float wi  = sinf(ang);

        BLOCK: for (int i = 0; i < WINDOW_SIZE; i += len) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=2048
            float cwr = 1.0f, cwi = 0.0f;

            BUTTERFLY: for (int k = 0; k < len / 2; k++) {
#pragma HLS PIPELINE
                float ur = re[i + k];
                float ui = im[i + k];
                float vr = re[i + k + len/2] * cwr - im[i + k + len/2] * cwi;
                float vi = re[i + k + len/2] * cwi + im[i + k + len/2] * cwr;
                re[i + k]          = ur + vr;
                im[i + k]          = ui + vi;
                re[i + k + len/2]  = ur - vr;
                im[i + k + len/2]  = ui - vi;
                // update twiddle factor
                float nwr = cwr * wr - cwi * wi;
                float nwi = cwr * wi + cwi * wr;
                cwr = nwr;
                cwi = nwi;
            }
        }
    }
}

// Top-level HLS kernel: applies Hann window, runs FFT, produces power spectrum (dB).
// Processes one frame (WINDOW_SIZE = 4096 samples) at a time.
void compute_spectrogram_kernel(
    float samples_in[WINDOW_SIZE],   // raw PCM samples for one frame
    float hann[WINDOW_SIZE],         // Hann window coefficients
    float hann_energy,               // precomputed: sum of hann[i]^2
    float spec_out[MAX_FREQ]         // output: MAX_FREQ = 2049 dB values
) {
#pragma HLS INTERFACE m_axi port=samples_in  depth=4096  offset=slave bundle=BUS_A
#pragma HLS INTERFACE m_axi port=hann        depth=4096  offset=slave bundle=BUS_B
#pragma HLS INTERFACE m_axi port=spec_out    depth=2049  offset=slave bundle=BUS_C
#pragma HLS INTERFACE s_axilite port=hann_energy
#pragma HLS INTERFACE s_axilite port=return

    static float re[WINDOW_SIZE];
    static float im[WINDOW_SIZE];
#pragma HLS ARRAY_PARTITION variable=re cyclic factor=4
#pragma HLS ARRAY_PARTITION variable=im cyclic factor=4

    // Apply Hann window and initialise imaginary part to zero
    APPLY_HANN: for (int s = 0; s < WINDOW_SIZE; s++) {
#pragma HLS PIPELINE II=1
        re[s] = samples_in[s] * hann[s];
        im[s] = 0.0f;
    }

    fft_hls(re, im);

    // Compute power spectral density in dB (matches make_spectrogram in reference)
    POWER_LOOP: for (int f = 0; f < MAX_FREQ; f++) {
#pragma HLS PIPELINE II=1
        float p = re[f] * re[f] + im[f] * im[f];
        if (f > 0 && f < MAX_FREQ - 1) p *= 2.0f;
        p /= (FS_K * hann_energy);
        if (p < 1e-8f) p = 1e-8f;
        spec_out[f] = 10.0f * log10f(p);
    }
}
