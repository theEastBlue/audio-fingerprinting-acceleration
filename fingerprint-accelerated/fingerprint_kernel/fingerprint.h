#ifndef FINGERPRINT_H
#define FINGERPRINT_H

#include "ap_fixed.h"

typedef ap_fixed<16, 8> DTYPE;

constexpr int DEFAULT_WINDOW_SIZE = 4096;
// for cosim: constexpr int DEFAULT_WINDOW_SIZE = 1024;
// for cosim: pls make sure you've turned off trace-enable in cosim
constexpr float DEFAULT_OVERLAP_RATIO = 0.5f;

constexpr int WINDOW_SIZE = DEFAULT_WINDOW_SIZE;
constexpr int OVERLAP = (int)(DEFAULT_WINDOW_SIZE * DEFAULT_OVERLAP_RATIO);
constexpr int HOP = WINDOW_SIZE - OVERLAP;

constexpr int MAX_SAMPLES = 200000;
constexpr int MAX_WINDOWS = 100;
constexpr int MAX_FREQ = WINDOW_SIZE / 2 + 1;
constexpr int MAX_PEAKS = 20000 + 1;

constexpr int DEFAULT_FAN_VALUE = 15;
constexpr int MIN_HASH_TIME_DELTA = 0;
constexpr int MAX_HASH_TIME_DELTA = 200;
constexpr int PEAK_NEIGHBORHOOD_SIZE = 20;
constexpr float DEFAULT_AMP_MIN = 10.0f;

constexpr float FS = 22050.0f;

struct Peak {
	int freq;
	int time;
};

struct PeakList {
	Peak peaks[MAX_PEAKS];
	int count;
};

struct Spectrogram {
	DTYPE power[MAX_FREQ][MAX_WINDOWS];
	int num_freq;
	int num_windows;
};

void build_windows(const float* data, int data_size,
float windows[MAX_WINDOWS][WINDOW_SIZE], int& num_windows);

void build_hann_window(float hann[WINDOW_SIZE]);

void apply_hann(float windows[MAX_WINDOWS][WINDOW_SIZE],
const float hann[WINDOW_SIZE], int num_windows);

void compute_spectrogram(const float windows[MAX_WINDOWS][WINDOW_SIZE], int num_windows,
const float hann[WINDOW_SIZE], Spectrogram& spec);

void detect_peaks(
	const DTYPE spec[MAX_FREQ][MAX_WINDOWS],
	int num_windows,
	int peak_freq[MAX_PEAKS],
	int peak_time[MAX_PEAKS]
);

void preprocessing(const float* data, int data_size, Spectrogram& spec, int& num_windows);

#endif