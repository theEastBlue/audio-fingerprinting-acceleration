// fingerprint.h
#ifndef FINGERPRINT_H
#define FINGERPRINT_H

constexpr int   DEFAULT_WINDOW_SIZE    = 4096;
constexpr float DEFAULT_OVERLAP_RATIO  = 0.5f;

constexpr int WINDOW_SIZE = DEFAULT_WINDOW_SIZE;
constexpr int OVERLAP     = (int)(DEFAULT_WINDOW_SIZE * DEFAULT_OVERLAP_RATIO);
constexpr int HOP         = WINDOW_SIZE - OVERLAP;

constexpr int MAX_SAMPLES = 200000;
constexpr int MAX_WINDOWS = (MAX_SAMPLES - OVERLAP) / HOP + 4;
constexpr int MAX_FREQ    = WINDOW_SIZE / 2 + 1; // 2049
constexpr int MAX_PEAKS   = 20000;

constexpr int   DEFAULT_FAN_VALUE      = 15;
constexpr int   MIN_HASH_TIME_DELTA    = 0;
constexpr int   MAX_HASH_TIME_DELTA    = 200;
constexpr int   PEAK_NEIGHBORHOOD_SIZE = 20;
constexpr float DEFAULT_AMP_MIN        = 10.0f;

constexpr float FS                     = 22050.0f;

// could i possibly store my structs in a way that saves this conversion trouble?
struct Peak {
    int freq;
    int time;
};

struct PeakList {
    Peak peaks[MAX_PEAKS];
    int count; 
};

struct Spectrogram {
    float power[MAX_FREQ][MAX_WINDOWS];
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

void detect_peaks_old(const Spectrogram& spec, PeakList& peaks);

void detect_peaks(
    const float spec[MAX_FREQ][MAX_WINDOWS], 
    int num_windows,
    int peak_freq[MAX_PEAKS], 
    int peak_time[MAX_PEAKS], 
    int &peak_count
);

void preprocessing(const float* data, int data_size, Spectrogram& spec, int& num_windows);


#endif