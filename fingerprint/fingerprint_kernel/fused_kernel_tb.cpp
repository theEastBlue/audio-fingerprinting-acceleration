// fused_kernel_tb.cpp
// Testbench for the fused fingerprint_kernel (spectrogram + peak detection
// in one kernel call). Loads test.wav, runs the CPU-side preprocessing
// (build_windows / build_hann_window / apply_hann -- identical to what the
// host does before a real kernel launch), calls the fused kernel once, and
// checks the resulting peaks against peaks.gold.dat using the same RMSE
// pass criteria as test.cpp (RMSE(freq) <= 1.0, RMSE(time) <= 1.0, and
// matching peak counts).
//
// By Netik Maheshwar

#include "fingerprint.h"
#include "fused_kernel.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <algorithm>

struct Rmse {
    int num_sq = 0;
    float sum_sq = 0;
    float error = 0;

    float add_value(float d_n) {
        num_sq++;
        sum_sq += (d_n * d_n);
        error = sqrtf(sum_sq / num_sq);
        return error;
    }
};

int main() {
    std::ifstream wav("test.wav", std::ios::binary);
    if (!wav.is_open()) {
        std::cerr << "ERROR: cannot open test.wav\n";
        return 1;
    }
    wav.seekg(44, std::ios::beg);

    static float data[MAX_SAMPLES];
    int n = 0;
    short s;
    while (n < MAX_SAMPLES && wav.read(reinterpret_cast<char*>(&s), 2))
        data[n++] = static_cast<float>(s);
    wav.close();
    std::cout << "Loaded " << n << " samples from test.wav\n";

    static float windows[MAX_WINDOWS][WINDOW_SIZE];
    static float hann[WINDOW_SIZE];
    int num_windows = 0;

    build_windows(data, n, windows, num_windows);
    build_hann_window(hann);
    apply_hann(windows, hann, num_windows);

    float hann_energy = 0.0f;
    for (int i = 0; i < WINDOW_SIZE; i++) hann_energy += hann[i] * hann[i];

    std::cout << "Windows: " << num_windows
              << "  hann_energy: " << hann_energy << "\n";

    // Single fused kernel call: spectrogram computation + peak detection.
    static int peak_freq[MAX_PEAKS];
    static int peak_time[MAX_PEAKS];
    int peak_count = 0;

    fingerprint_kernel(windows, num_windows, hann_energy,
                        peak_freq, peak_time, &peak_count);

    std::cout << "Found " << peak_count << " peaks.\n";

    // Load gold peaks (same file/format used by test.cpp).
    PeakList gold_peaks;
    gold_peaks.count = 0;
    FILE *fp = fopen("peaks.gold.dat", "r");
    if (!fp) {
        std::cerr << "ERROR: Could not open 'peaks.gold.dat'." << std::endl;
        return 1;
    }
    int index;
    float gold_freq, gold_time;
    while (fscanf(fp, "%d %f %f", &index, &gold_freq, &gold_time) == 3) {
        if (gold_peaks.count < MAX_PEAKS) {
            gold_peaks.peaks[gold_peaks.count].freq = (int)gold_freq;
            gold_peaks.peaks[gold_peaks.count].time = (int)gold_time;
            gold_peaks.count++;
        }
    }
    fclose(fp);

    Rmse rmse_freq, rmse_time;
    int max_compare = std::min(peak_count, gold_peaks.count);

    for (int i = 0; i < max_compare; i++) {
        rmse_freq.add_value((float)peak_freq[i] - (float)gold_peaks.peaks[i].freq);
        rmse_time.add_value((float)peak_time[i] - (float)gold_peaks.peaks[i].time);
    }
    for (int i = max_compare; i < peak_count; i++) {
        rmse_freq.add_value((float)peak_freq[i]);
        rmse_time.add_value((float)peak_time[i]);
    }
    for (int i = max_compare; i < gold_peaks.count; i++) {
        rmse_freq.add_value((float)gold_peaks.peaks[i].freq);
        rmse_time.add_value((float)gold_peaks.peaks[i].time);
    }

    printf("----------------------------------------------\n");
    printf("   RMSE(Freq)          RMSE(Time)\n");
    printf("%0.15f %0.15f\n", rmse_freq.error, rmse_time.error);
    printf("gold peaks: %d   fused kernel peaks: %d\n", gold_peaks.count, peak_count);
    printf("----------------------------------------------\n");

    if (rmse_freq.error > 1.0f || rmse_time.error > 1.0f || peak_count != gold_peaks.count) {
        printf("*******************************************\n");
        printf("CSIM FAIL: Peak coordinates DO NOT match the golden output\n");
        printf("*******************************************\n");
        return 1;
    }

    printf("*******************************************\n");
    printf("CSIM PASS: fused kernel matches golden peaks\n");
    printf("*******************************************\n");
    return 0;
}
