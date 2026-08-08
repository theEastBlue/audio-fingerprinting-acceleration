// fused_kernel.cpp
// Fused Vitis HLS kernel: Hann-pre-windowed LUT-FFT + PSD, feeding directly
// into 41x41 diamond peak detection, as ONE top-level function.
//
// This replaces two separate top-level kernels (compute_spectrogram_kernel_lut
// and detect_peaks in fingerprint_kernel.cpp), each with their own m_axi/
// s_axilite interface. Fusing them means:
//   - Only ONE kernel launch from the host (one XRT enqueue instead of two).
//   - spec_power is a device-side DDR scratch buffer (its own m_axi bundle),
//     allocated once and never sync'd to/from the host. The full spectrogram
//     (2049 x 100 floats, ~800KB) does not fit in the ~100 BRAM18K blocks
//     available on xc7a35t as an on-chip buffer -- see fused_kernel_results.md
//     for why (checked both frequency-major and time-major buffering; neither
//     fits at float precision). Keeping it in device DDR instead of host-
//     visible DDR still removes what actually mattered: the host never syncs
//     this buffer and there's no second kernel launch.
//
// The FFT stage reuses the LUT-based approach from
// compute_spectrogram_kernel_lut.cpp (precomputed twiddle factors +
// DEPENDENCE false, achieving butterfly II=2) rather than the original
// iterative-recurrence version (II=21) that was never actually accelerated.
//
// By Netik Maheshwar

#include "fingerprint.h"
#include "fused_kernel.h"
#include "coefficients.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------------
// Stage 1: 4096-point radix-2 FFT using precomputed twiddle LUT.
// Identical to compute_spectrogram_kernel_lut.cpp's fft_lut(), just no
// longer a standalone top-level kernel -- it's an internal helper now.
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Stage 1 top: FFT + PSD over all windows, writing into an on-chip buffer.
// Same body as compute_spectrogram_kernel_lut.cpp's top-level function,
// minus the interface pragmas (spec_power is now a local array, not a
// DDR-backed argument).
// ---------------------------------------------------------------------------
static void compute_spectrogram_stage(
    float windows[MAX_WINDOWS][WINDOW_SIZE],
    int   num_windows,
    float hann_energy,
    float spec_power[MAX_FREQ][MAX_WINDOWS]
) {
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

// ---------------------------------------------------------------------------
// Stage 2: 41x41 diamond-mask peak detection. Same algorithm as
// detect_peaks() in fingerprint_kernel.cpp, minus the interface pragmas --
// spec is now a local on-chip array, not a DDR-backed argument.
// ---------------------------------------------------------------------------
static constexpr int R = 20;
static constexpr int MASK_SIZE = 2 * R + 1; // 41

static void detect_peaks_stage(
    const float spec[MAX_FREQ][MAX_WINDOWS],
    int num_windows,
    int peak_freq[MAX_PEAKS],
    int peak_time[MAX_PEAKS],
    int* peak_count
) {
    float line_buf[MASK_SIZE][MAX_WINDOWS];
    float window[MASK_SIZE][MASK_SIZE];
    float new_row[MAX_WINDOWS];

    for (int i = 0; i < MASK_SIZE; i++) {
        for (int j = 0; j < MAX_WINDOWS; j++) {
            line_buf[i][j] = 0.0f;
        }
    }
    for (int i = 0; i < MASK_SIZE; i++) {
        for (int j = 0; j < MASK_SIZE; j++) {
            window[i][j] = 0.0f;
        }
    }

    int pc = 0;

    for (int f = 0; f < MAX_FREQ + R; f++) {

        if (f < MAX_FREQ) {
            for (int w = 0; w < num_windows; w++) {
                new_row[w] = spec[f][w];
            }
        } else {
            for (int w = 0; w < num_windows; w++) {
                new_row[w] = 0.0f;
            }
        }

        for (int w = 0; w < num_windows + R; w++) {

            for (int i = 0; i < MASK_SIZE; i++) {
                for (int j = 0; j < MASK_SIZE - 1; j++) {
                    window[i][j] = window[i][j+1];
                }
            }

            if (w < num_windows) {
                line_buf[f % MASK_SIZE][w] = new_row[w];
                for (int i = 0; i < MASK_SIZE; i++) {
                    int row_idx = (f + 1 + i) % MASK_SIZE;
                    window[i][MASK_SIZE - 1] = line_buf[row_idx][w];
                }
            } else {
                for (int i = 0; i < MASK_SIZE; i++) {
                    window[i][MASK_SIZE - 1] = 0.0f;
                }
            }

            int f_center = f - R;
            int w_center = w - R;

            if (f_center >= 0 && f_center < MAX_FREQ && w_center >= 0 && w_center < num_windows) {
                float center_val = window[R][R];
                float max_val = center_val;
                bool all_bg = true;

                for (int i = 0; i < MASK_SIZE; i++) {
                    for (int j = 0; j < MASK_SIZE; j++) {
                        int dist = std::abs(i - R) + std::abs(j - R);
                        if (dist <= R) {
                            float val = window[i][j];
                            if (val > max_val) max_val = val;
                            if (val > 0.0f) all_bg = false;
                        }
                    }
                }

                if (center_val == max_val && !all_bg && center_val > DEFAULT_AMP_MIN) {
                    if (pc < MAX_PEAKS) {
                        peak_freq[pc] = f_center;
                        peak_time[pc] = w_center;
                        pc++;
                    }
                }
            }
        }
    }

    *peak_count = pc;
}

// ---------------------------------------------------------------------------
// Top-level fused kernel. Single m_axi/s_axilite interface: audio windows in,
// peak coordinates out. spec_power is a local static on-chip array shared
// between the two stages -- it never appears on the argument list, so it
// never crosses the AXI boundary.
// ---------------------------------------------------------------------------
void fingerprint_kernel(
    float windows[MAX_WINDOWS][WINDOW_SIZE],
    int   num_windows,
    float hann_energy,
    float spec_power[MAX_FREQ][MAX_WINDOWS],
    int   peak_freq[MAX_PEAKS],
    int   peak_time[MAX_PEAKS],
    int*  peak_count
) {
#pragma HLS INTERFACE m_axi port=windows    depth=409600 offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=spec_power depth=204900 offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=peak_freq  depth=20000  offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=peak_time  depth=20000  offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=peak_count depth=1       offset=slave bundle=gmem1

#pragma HLS INTERFACE s_axilite port=num_windows bundle=control
#pragma HLS INTERFACE s_axilite port=hann_energy bundle=control
#pragma HLS INTERFACE s_axilite port=return      bundle=control

    compute_spectrogram_stage(windows, num_windows, hann_energy, spec_power);
    detect_peaks_stage(spec_power, num_windows, peak_freq, peak_time, peak_count);
}
