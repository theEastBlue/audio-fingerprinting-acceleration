// compute_spectrogram_tb.cpp
// Testbench for compute_spectrogram_kernel.
//
// Reads test.wav, runs the preprocessing pipeline (build_windows,
// build_hann_window, apply_hann), then calls the HLS kernel and
// verifies its output against the reference compute_spectrogram()
// from preprocessing.cpp bin-by-bin.
//
// By Netik Maheshwar

#include "fingerprint.h"
#include "compute_spectrogram_kernel.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>

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

    // Reference output
    static Spectrogram ref_spec;
    compute_spectrogram(windows, num_windows, hann, ref_spec);

    // Kernel output
    static float kern_power[MAX_FREQ][MAX_WINDOWS];
    compute_spectrogram_kernel(windows, num_windows, hann_energy, kern_power);

    // Compare
    float max_err = 0.0f;
    int   failures = 0;
    for (int f = 0; f < MAX_FREQ; f++) {
        for (int w = 0; w < num_windows; w++) {
            float err = fabsf(kern_power[f][w] - ref_spec.power[f][w]);
            if (err > max_err) max_err = err;
            if (err > 0.1f) {
                failures++;
                if (failures <= 3)
                    printf("  MISMATCH f=%d w=%d  kernel=%.4f  ref=%.4f  diff=%.4f\n",
                           f, w, kern_power[f][w], ref_spec.power[f][w], err);
            }
        }
    }

    printf("\nmax_err = %.6f dB   failures = %d / %d bins\n",
           max_err, failures, MAX_FREQ * num_windows);

    if (failures == 0) {
        std::cout << "CSIM PASS: kernel matches reference for all bins\n";
        return 0;
    }
    std::cout << "CSIM FAIL: " << failures << " mismatches\n";
    return 1;
}
