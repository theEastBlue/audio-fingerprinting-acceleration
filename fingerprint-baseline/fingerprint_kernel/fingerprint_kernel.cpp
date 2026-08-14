// fingerprint_kernel.cpp
#include "fingerprint.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

void dilate_cross_pass(const float src[MAX_FREQ][MAX_WINDOWS],
                        float dst[MAX_FREQ][MAX_WINDOWS],
                        int num_freq, int num_windows) {
    dilate_loop: for (int f = 0; f < num_freq; f++) {
        for (int w = 0; w < num_windows; w++) {
            float m = src[f][w];
            if (f > 0)              m = std::max(m, src[f-1][w]);
            if (f < num_freq - 1)   m = std::max(m, src[f+1][w]);
            if (w > 0)              m = std::max(m, src[f][w-1]);
            if (w < num_windows-1)  m = std::max(m, src[f][w+1]);
            dst[f][w] = m;
        }
    }
}

void erode_cross_pass_bg(const uint8_t src[MAX_FREQ][MAX_WINDOWS],
                          uint8_t dst[MAX_FREQ][MAX_WINDOWS],
                          int num_freq, int num_windows) {
    erode_loop: for (int f = 0; f < num_freq; f++) {
        erode_inner_loop: for (int w = 0; w < num_windows; w++) {
            uint8_t up    = (f > 0)             ? src[f-1][w] : 1;
            uint8_t down  = (f < num_freq - 1)  ? src[f+1][w] : 1;
            uint8_t left  = (w > 0)             ? src[f][w-1] : 1;
            uint8_t right = (w < num_windows-1) ? src[f][w+1] : 1;
            dst[f][w] = src[f][w] & up & down & left & right;
        }
    }
}

constexpr int R = 20;
constexpr int MASK_SIZE = 2 * R + 1; // 41

void detect_peaks(
    const float spec[MAX_FREQ][MAX_WINDOWS], 
    int num_windows,
    int peak_freq[MAX_PEAKS], 
    int peak_time[MAX_PEAKS] 
) {
    #pragma HLS INTERFACE m_axi port=spec      offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=peak_freq offset=slave bundle=gmem1
    #pragma HLS INTERFACE m_axi port=peak_time offset=slave bundle=gmem2
    
    #pragma HLS INTERFACE s_axilite port=spec        bundle=control
    #pragma HLS INTERFACE s_axilite port=peak_freq   bundle=control
    #pragma HLS INTERFACE s_axilite port=peak_time   bundle=control
    #pragma HLS INTERFACE s_axilite port=num_windows bundle=control
    #pragma HLS INTERFACE s_axilite port=return      bundle=control

    float line_buf[MASK_SIZE][MAX_WINDOWS];
    // #pragma HLS ARRAY_PARTITION variable=line_buf complete dim=1 

    // Window: A 41x41 register grid. Slides right by 1 pixel per clock cycle.
    // Fully partitioned so we can read all 1681 pixels simultaneously.
    float window[MASK_SIZE][MASK_SIZE];
    // #pragma HLS ARRAY_PARTITION variable=window complete dim=0

    // Small row buffer to burst-read from DDR efficiently
    float new_row[MAX_WINDOWS];

    // Initialize buffers to zero to handle edge padding safely
    init_buffer: for (int i = 0; i < MASK_SIZE; i++) {
        init_buffer_inner: for (int j = 0; j < MAX_WINDOWS; j++) {
            // #pragma HLS PIPELINE II=1
            line_buf[i][j] = 0.0f;
        }
    }
    init_mask: for (int i = 0; i < MASK_SIZE; i++) {
        init_mask_inner: for (int j = 0; j < MASK_SIZE; j++) {
            // #pragma HLS UNROLL
            window[i][j] = 0.0f;
        }
    }

    int pc = 0;

    // f loops up to MAX_FREQ + R to allow the final rows to flush out of the window
    slide_window: for (int f = 0; f < MAX_FREQ + R; f++) {
        
        if (f < MAX_FREQ) {
            read_row: for (int w = 0; w < num_windows; w++) {
                // #pragma HLS PIPELINE II=1
                new_row[w] = spec[f][w];
            }
        } else {
            zero_padding: for (int w = 0; w < num_windows; w++) {
                // #pragma HLS PIPELINE II=1
                new_row[w] = 0.0f;
            }
        }

        // w loops up to num_windows + R to flush the right edge
        shift_window: for (int w = 0; w < num_windows + R; w++) {
            // #pragma HLS PIPELINE II=1
            
            shift_window_inner: for (int i = 0; i < MASK_SIZE; i++) {
                for (int j = 0; j < MASK_SIZE - 1; j++) {
                    window[i][j] = window[i][j+1];
                }
            }

            if (w < num_windows) {
                line_buf[f % MASK_SIZE][w] = new_row[w];
                
                sort_window: for (int i = 0; i < MASK_SIZE; i++) {
                    int row_idx = (f + 1 + i) % MASK_SIZE;
                    window[i][MASK_SIZE - 1] = line_buf[row_idx][w];
                }
            } else {
                pad_window: for (int i = 0; i < MASK_SIZE; i++) {
                    window[i][MASK_SIZE - 1] = 0.0f;
                }
            }

            int f_center = f - R;
            int w_center = w - R;

            if (f_center >= 0 && f_center < MAX_FREQ && w_center >= 0 && w_center < num_windows) {
                float center_val = window[R][R];
                float max_val = center_val;
                bool all_bg = true;

                find_peak: for (int i = 0; i < MASK_SIZE; i++) {
                    find_max: for (int j = 0; j < MASK_SIZE; j++) {
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

    if (pc < MAX_PEAKS) {
        peak_time[pc] = -1;   // terminator
    } else {
        peak_time[MAX_PEAKS-1] = -1;

    }
}