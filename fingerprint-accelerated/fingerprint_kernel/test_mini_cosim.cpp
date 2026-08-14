// test_mini_cosim.cpp
//
// Minimal smoke test for detect_peaks(). Bypasses preprocessing() entirely
// and hand-builds a tiny spectrogram with a few known peaks planted in it.
//
// compile: clang++ test_mini_cosim.cpp fingerprint_kernel.cpp -o cos -std=c++17 -O0

#include "fingerprint.h"
#include <cstring>
#include <iostream>

int main() {
    // ---- 1. Build a fake spectrogram by hand -------------------------------
    static float spec[MAX_FREQ][MAX_WINDOWS];
    std::memset(spec, 0, sizeof(spec));   // 0.0f reads as "background"

    const int NUM_WINDOWS = 4;

    struct Expected { int f, w; float val; };
    Expected expected[] = {
        { 5,  1, 50.0f },
        { 15, 2, 60.0f },
        { 25, 3, 40.0f },
    };
    const int NUM_EXPECTED = sizeof(expected) / sizeof(expected[0]);

    std::cout << "[mini] MAX_FREQ=" << MAX_FREQ << " MAX_WINDOWS=" << MAX_WINDOWS
              << " num_windows=" << NUM_WINDOWS << "\n";
    std::cout << "[mini] building fake spectrogram: "
              << NUM_WINDOWS << " windows, " << NUM_EXPECTED << " planted peaks\n";

    for (int i = 0; i < NUM_EXPECTED; i++) {
        spec[expected[i].f][expected[i].w] = expected[i].val;
        std::cout << "[mini]   planted peak " << i
                  << " at f=" << expected[i].f
                  << " w=" << expected[i].w
                  << " val=" << expected[i].val << "\n";
    }

    // sanity echo -- indices pulled from expected[], not hardcoded, so this
    // can never drift out of bounds again when you change the peak list
    std::cout << "[mini] spec[0][0]=" << spec[0][0];
    for (int i = 0; i < NUM_EXPECTED; i++) {
        std::cout << " spec[" << expected[i].f << "][" << expected[i].w << "]="
                   << spec[expected[i].f][expected[i].w];
    }
    std::cout << "\n";

    // ---- 2. Output buffers --------------------------------------------------
    static int peak_freq[MAX_PEAKS];
    static int peak_time[MAX_PEAKS];
    for (int i = 0; i < MAX_PEAKS; i++) {
        peak_freq[i] = -999;
        peak_time[i] = -999;
    }

    // ---- 3. Call the kernel --------------------------------------------------
    std::cout << "[mini] calling detect_peaks(num_windows=" << NUM_WINDOWS << ")...\n";
    detect_peaks(spec, NUM_WINDOWS, peak_freq, peak_time);
    std::cout << "[mini] detect_peaks() returned\n";

    // ---- 4. Read back whatever the kernel found -------------------------------
    int found = 0;
    std::cout << "[mini] ---- kernel output ----\n";
    for (int i = 0; i < MAX_PEAKS; i++) {
        if (peak_time[i] == -1) {
            std::cout << "[mini] terminator found at index " << i << "\n";
            break;
        }
        std::cout << "[mini]   peak " << found
                  << "  freq=" << peak_freq[i]
                  << "  time=" << peak_time[i] << "\n";
        found++;
    }
    std::cout << "[mini] total peaks found: " << found << "\n";

    // ---- 5. Compare against what we planted -------------------------------------
    bool pass = (found == NUM_EXPECTED);
    if (!pass) {
        std::cout << "[mini] expected " << NUM_EXPECTED << " peaks, found " << found << "\n";
    }
    for (int i = 0; i < NUM_EXPECTED; i++) {
        bool matched = false;
        for (int j = 0; j < found; j++) {
            if (peak_freq[j] == expected[i].f && peak_time[j] == expected[i].w) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            std::cout << "[mini] MISSING expected peak f=" << expected[i].f
                      << " w=" << expected[i].w << "\n";
            pass = false;
        }
    }

    if (pass) {
        std::cout << "*******************************************\n";
        std::cout << "PASS: mini cosim smoke test matched planted peaks\n";
        std::cout << "*******************************************\n";
        return 0;
    } else {
        std::cout << "*******************************************\n";
        std::cout << "FAIL: mini cosim smoke test did not match\n";
        std::cout << "*******************************************\n";
        return 1;
    }
}