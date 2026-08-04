// Testbench for compute_spectrogram_kernel
// By Netik Maheshwar
//
// Loads raw_data (decoded test.mp3 at 22050 Hz mono, int16 PCM),
// builds the first window, runs the HLS kernel and the reference C++
// implementation, then checks that every output bin matches within 0.1 dB.

#include "compute_spectrogram_kernel.h"

#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

static const int kWindowSize = WINDOW_SIZE;
static const float kPi      = 3.14159265358979323846f;
static const float kSampleRate = FS_K;
static const int kOverlap   = kWindowSize / 2;

// Reference Hann window
static void make_hann(float* w, int n) {
    for (int i = 0; i < n; i++)
        w[i] = 0.5f - 0.5f * cosf(2.0f * kPi * i / (n - 1));
}

// Reference FFT using std::complex (same as fingerprint-no-opencv-boost.cpp)
static void fft_ref(std::vector<std::complex<float>>& v) {
    int n = (int)v.size();
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(v[i], v[j]);
    }
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * kPi / (float)len;
        std::complex<float> step(cosf(ang), sinf(ang));
        for (int start = 0; start < n; start += len) {
            std::complex<float> mult(1.0f, 0.0f);
            for (int i = 0; i < len / 2; i++) {
                auto u = v[start + i];
                auto t = mult * v[start + i + len/2];
                v[start + i]         = u + t;
                v[start + i + len/2] = u - t;
                mult *= step;
            }
        }
    }
}

// Reference spectrogram for one frame
static void spec_ref(const float* samples, const float* hann_w,
                     float hann_energy, float* out) {
    std::vector<std::complex<float>> buf(kWindowSize);
    for (int i = 0; i < kWindowSize; i++)
        buf[i] = std::complex<float>(samples[i] * hann_w[i], 0.0f);
    fft_ref(buf);
    for (int f = 0; f < MAX_FREQ; f++) {
        float p = std::norm(buf[f]);
        if (f > 0 && f < MAX_FREQ - 1) p *= 2.0f;
        p /= (kSampleRate * hann_energy);
        if (p < 1e-8f) p = 1e-8f;
        out[f] = 10.0f * log10f(p);
    }
}

int main() {
    // Load raw_data (int16 PCM)
    std::ifstream fp("raw_data", std::ios::binary);
    if (!fp) {
        std::cerr << "ERROR: cannot open raw_data\n";
        return 1;
    }
    std::vector<float> pcm;
    short s = 0;
    while (fp.read(reinterpret_cast<char*>(&s), sizeof(s)))
        pcm.push_back((float)s);
    fp.close();

    if ((int)pcm.size() < kWindowSize) {
        std::cerr << "ERROR: raw_data too short (" << pcm.size() << " samples)\n";
        return 1;
    }
    std::cout << "Loaded " << pcm.size() << " samples\n";

    // Build Hann window and energy
    float hann_w[WINDOW_SIZE];
    make_hann(hann_w, kWindowSize);
    float hann_energy = 0.0f;
    for (int i = 0; i < kWindowSize; i++) hann_energy += hann_w[i] * hann_w[i];

    // Choose three windows to test: first, middle, last
    int num_windows = ((int)pcm.size() - kWindowSize) / kOverlap + 1;
    int test_wins[3] = {0, num_windows / 2, num_windows - 1};
    int fail_total = 0;

    for (int t = 0; t < 3; t++) {
        int win_idx = test_wins[t];
        int offset  = win_idx * kOverlap;

        float samples_in[WINDOW_SIZE];
        for (int i = 0; i < kWindowSize; i++)
            samples_in[i] = pcm[offset + i];

        // Reference output
        float ref_out[MAX_FREQ];
        spec_ref(samples_in, hann_w, hann_energy, ref_out);

        // HLS kernel output
        float kern_out[MAX_FREQ];
        compute_spectrogram_kernel(samples_in, hann_w, hann_energy, kern_out);

        // Compare
        float max_err = 0.0f;
        int failures = 0;
        for (int f = 0; f < MAX_FREQ; f++) {
            float err = fabsf(kern_out[f] - ref_out[f]);
            if (err > max_err) max_err = err;
            if (err > 0.1f) {
                failures++;
                if (failures <= 3)
                    printf("  bin %d: kernel=%.4f ref=%.4f diff=%.4f\n",
                           f, kern_out[f], ref_out[f], err);
            }
        }

        printf("Window %d (offset=%d): max_err=%.6f dB, failures=%d / %d bins\n",
               win_idx, offset, max_err, failures, MAX_FREQ);
        fail_total += failures;
    }

    if (fail_total == 0) {
        std::cout << "\nCSIM PASS: kernel output matches reference for all tested windows\n";
        return 0;
    } else {
        std::cout << "\nCSIM FAIL: " << fail_total << " mismatches detected\n";
        return 1;
    }
}
