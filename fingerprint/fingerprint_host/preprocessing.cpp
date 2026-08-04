// preprocessing.cpp
#include "fingerprint.h"
#include "timer.h"
#include <cmath>      // cosf, sinf, log10f
#include <algorithm>  // std::swap
#include <iostream>   // std::cerr

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Timer profiler1;

void build_windows(const float* data, int data_size,
                    float windows[MAX_WINDOWS][WINDOW_SIZE], int& num_windows) {
    int minlen = (data_size - OVERLAP) / HOP;
    if (minlen < 0) minlen = 0;
    if (minlen > MAX_WINDOWS) {
        std::cerr << "[fingerprint] warning: clamping " << minlen
                   << " windows down to MAX_WINDOWS=" << MAX_WINDOWS << "\n";
        minlen = MAX_WINDOWS;
    }
    num_windows = minlen;

    for (int w = 0; w < num_windows; w++) {
        int base = w * HOP;
        for (int s = 0; s < WINDOW_SIZE; s++) {
            windows[w][s] = data[base + s];
        }
    }
}

void build_hann_window(float hann[WINDOW_SIZE]) {
    for (int i = 0; i < WINDOW_SIZE; i++) {
        hann[i] = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * i / (WINDOW_SIZE - 1));
    }
}

void apply_hann(float windows[MAX_WINDOWS][WINDOW_SIZE],
                 const float hann[WINDOW_SIZE], int num_windows) {
    for (int w = 0; w < num_windows; w++)
        for (int s = 0; s < WINDOW_SIZE; s++)
            windows[w][s] *= hann[s];
}

void fft_radix2(float* re, float* im, int n) {
    // Bit-reversal permutation.
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }

    for (int len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * (float)M_PI / len;
        float wr = cosf(ang), wi = sinf(ang);
        for (int i = 0; i < n; i += len) {
            float cwr = 1.0f, cwi = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                float ur = re[i + k],           ui = im[i + k];
                float vr = re[i + k + len / 2] * cwr - im[i + k + len / 2] * cwi;
                float vi = re[i + k + len / 2] * cwi + im[i + k + len / 2] * cwr;

                re[i + k]           = ur + vr;
                im[i + k]           = ui + vi;
                re[i + k + len / 2] = ur - vr;
                im[i + k + len / 2] = ui - vi;

                float nwr = cwr * wr - cwi * wi;
                float nwi = cwr * wi + cwi * wr;
                cwr = nwr; cwi = nwi;
            }
        }
    }
}

void compute_spectrogram(const float windows[MAX_WINDOWS][WINDOW_SIZE], int num_windows,
                          const float hann[WINDOW_SIZE], Spectrogram& spec) {

    static float re[WINDOW_SIZE];
    static float im[WINDOW_SIZE];

    float hann_energy = 0.0f;
    for (int i = 0; i < WINDOW_SIZE; i++) hann_energy += hann[i] * hann[i];

    spec.num_freq = MAX_FREQ;
    spec.num_windows = num_windows;

    for (int w = 0; w < num_windows; w++) {
        for (int s = 0; s < WINDOW_SIZE; s++) {
            re[s] = windows[w][s];
            im[s] = 0.0f;
        }

        fft_radix2(re, im, WINDOW_SIZE);

        for (int f = 0; f < MAX_FREQ; f++) {
            float p = re[f] * re[f] + im[f] * im[f];

            if (f > 0 && f < MAX_FREQ - 1) p *= 2.0f;

            p *= (1.0f / FS);
            p *= (1.0f / hann_energy);

            if (p < 1e-8f) p = 1e-8f;

            spec.power[f][w] = 10.0f * log10f(p);
        }
    }
}


void preprocessing(const float* data, int data_size, Spectrogram& spec, int& num_windows) {
    static float windows[MAX_WINDOWS][WINDOW_SIZE];
    static float hann[WINDOW_SIZE];

    profiler1.begin("1. Windowing");
    build_windows(data, data_size, windows, num_windows);
    build_hann_window(hann);
    apply_hann(windows, hann, num_windows);
    profiler1.end("1. Windowing");

    profiler1.begin("2. FFT + Spectrogram");
    compute_spectrogram(windows, num_windows, hann, spec);
    profiler1.end("2. FFT + Spectrogram");

    profiler1.print();
}