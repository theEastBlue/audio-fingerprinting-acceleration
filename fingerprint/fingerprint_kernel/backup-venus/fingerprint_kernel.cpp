// fingerprint_kernel.cpp
#include "fingerprint.h"
#include <algorithm>
#include <cstdint>

void dilate_cross_pass(const float src[MAX_FREQ][MAX_WINDOWS],
                        float dst[MAX_FREQ][MAX_WINDOWS],
                        int num_freq, int num_windows) {
    for (int f = 0; f < num_freq; f++) {
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
    for (int f = 0; f < num_freq; f++) {
        for (int w = 0; w < num_windows; w++) {
            uint8_t up    = (f > 0)             ? src[f-1][w] : 1;
            uint8_t down  = (f < num_freq - 1)  ? src[f+1][w] : 1;
            uint8_t left  = (w > 0)             ? src[f][w-1] : 1;
            uint8_t right = (w < num_windows-1) ? src[f][w+1] : 1;
            dst[f][w] = src[f][w] & up & down & left & right;
        }
    }
}

void detect_peaks(
    const float spec[MAX_FREQ][MAX_WINDOWS], 
    int num_windows,
    int peak_freq[MAX_PEAKS], 
    int peak_time[MAX_PEAKS], 
    int* peak_count
) {
    // Map pointers/arrays to AXI4 master ports (Global Memory)
    #pragma HLS INTERFACE m_axi port=spec      offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=peak_freq offset=slave bundle=gmem1
    #pragma HLS INTERFACE m_axi port=peak_time offset=slave bundle=gmem2
    #pragma HLS INTERFACE m_axi port=peak_count offset=slave bundle=gmem3
    
    // Map scalars to AXI4-Lite (Host control)
    #pragma HLS INTERFACE s_axilite port=spec        bundle=control
    #pragma HLS INTERFACE s_axilite port=peak_freq   bundle=control
    #pragma HLS INTERFACE s_axilite port=peak_time   bundle=control
    #pragma HLS INTERFACE s_axilite port=num_windows bundle=control
    #pragma HLS INTERFACE s_axilite port=peak_count  bundle=control
    #pragma HLS INTERFACE s_axilite port=return      bundle=control

    static float   dilA[MAX_FREQ][MAX_WINDOWS], dilB[MAX_FREQ][MAX_WINDOWS];
    static uint8_t bgA[MAX_FREQ][MAX_WINDOWS],  bgB[MAX_FREQ][MAX_WINDOWS];

    int nf = MAX_FREQ, nw = num_windows;

    for (int f = 0; f < nf; f++)
        for (int w = 0; w < nw; w++) {
            dilA[f][w] = spec[f][w];
            bgA[f][w]  = (spec[f][w] == 0.0f) ? 1 : 0;
        }

    float   (*curD)[MAX_WINDOWS] = dilA; float   (*nxtD)[MAX_WINDOWS] = dilB;
    uint8_t (*curB)[MAX_WINDOWS] = bgA;  uint8_t (*nxtB)[MAX_WINDOWS] = bgB;

    for (int i = 0; i < PEAK_NEIGHBORHOOD_SIZE; i++) {
        dilate_cross_pass(curD, nxtD, nf, nw);
        erode_cross_pass_bg(curB, nxtB, nf, nw);
        std::swap(curD, nxtD);
        std::swap(curB, nxtB);
    }

    *peak_count = 0;
    int pc = *peak_count;
 
    for (int f = 0; f < nf; f++) {
        for (int w = 0; w < nw; w++) {
            float center = spec[f][w];
            bool is_peak = (center == curD[f][w]) && !curB[f][w] && (center > DEFAULT_AMP_MIN);

            if (is_peak && pc < MAX_PEAKS) {
                peak_freq[pc] = f;
                peak_time[pc] = w;
                pc++;
            }
        }
    }
    *peak_count = pc;
}

