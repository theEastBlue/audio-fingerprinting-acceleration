#include "fingerprint.h"

#include <iostream>
#include <algorithm>
#include <limits>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <string>
#include "sha1.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Timer {
    std::unordered_map<std::string, double> times;
    std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> start;

    void begin(const std::string& name) {
        start[name] = std::chrono::high_resolution_clock::now();
    }

    void end(const std::string& name) {
        auto stop = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(stop - start[name]).count();
        times[name] += ms;
    }

    void print() {
        std::cout << "\n===== PROFILING =====\n";
        for (auto& p : times) {
            std::cout << p.first << ": " << p.second << " ms\n";
        }
    }
};

Timer profiler;

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

void detect_peaks(const Spectrogram& spec, PeakList& peaks) {
    static float   dilA[MAX_FREQ][MAX_WINDOWS], dilB[MAX_FREQ][MAX_WINDOWS];
    static uint8_t bgA[MAX_FREQ][MAX_WINDOWS],  bgB[MAX_FREQ][MAX_WINDOWS];

    int nf = spec.num_freq, nw = spec.num_windows;

    for (int f = 0; f < nf; f++)
        for (int w = 0; w < nw; w++) {
            dilA[f][w] = spec.power[f][w];
            bgA[f][w]  = (spec.power[f][w] == 0.0f) ? 1 : 0;
        }

    float   (*curD)[MAX_WINDOWS] = dilA; float   (*nxtD)[MAX_WINDOWS] = dilB;
    uint8_t (*curB)[MAX_WINDOWS] = bgA;  uint8_t (*nxtB)[MAX_WINDOWS] = bgB;

    for (int i = 0; i < PEAK_NEIGHBORHOOD_SIZE; i++) {
        dilate_cross_pass(curD, nxtD, nf, nw);
        erode_cross_pass_bg(curB, nxtB, nf, nw);
        std::swap(curD, nxtD);
        std::swap(curB, nxtB);
    }

    peaks.count = 0;
    for (int f = 0; f < nf; f++) {
        for (int w = 0; w < nw; w++) {
            float center = spec.power[f][w];
            bool is_peak = (center == curD[f][w]) && !curB[f][w] && (center > DEFAULT_AMP_MIN);
            if (is_peak && peaks.count < MAX_PEAKS) {
                peaks.peaks[peaks.count].freq = f;
                peaks.peaks[peaks.count].time = w;
                peaks.count++;
            }
        }
    }
}

std::string get_sha1(const std::string& input) {
    SHA1 checksum;
    checksum.processBytes(input.data(), input.size());
    return checksum.getHash();
}

std::string generate_hashes(PeakList& peaks) {
    std::sort(peaks.peaks, peaks.peaks + peaks.count,
              [](const Peak& a, const Peak& b) {
                  if (a.time == b.time) return a.freq < b.freq;
                  return a.time < b.time;
              });

    std::ostringstream buf;
    buf << "[";
    bool first = true;

    for (int i = 0; i < peaks.count; i++) {
        for (int j = 1; j < DEFAULT_FAN_VALUE; j++) {
            int k = i + j;

            if (k >= peaks.count) break;

            int freq1 = peaks.peaks[i].freq;
            int freq2 = peaks.peaks[k].freq;
            int time1 = peaks.peaks[i].time;
            int time2 = peaks.peaks[k].time;
            int t_delta = time2 - time1;

            if (t_delta >= MIN_HASH_TIME_DELTA && t_delta <= MAX_HASH_TIME_DELTA) {
                char buffer[100];
                snprintf(buffer, sizeof(buffer), "%d|%d|%d", freq1, freq2, t_delta);

                std::string to_be_hashed(buffer);
                std::string hash_result = get_sha1(to_be_hashed);

                if (!first) buf << ",";
                first = false;

                buf << "{\"hash\":\"" << hash_result << "\",\"offset\":" << time1 << "}";
            }
        }
    }

    buf << "]";
    return buf.str();
}

std::string fingerprint(float* data, int data_size) {

    static float windows[MAX_WINDOWS][WINDOW_SIZE];
    static float hann[WINDOW_SIZE];
    static Spectrogram spec;
    static PeakList peaks;

    profiler.begin("1. Windowing");
    int num_windows = 0;
    build_windows(data, data_size, windows, num_windows);
    build_hann_window(hann);
    apply_hann(windows, hann, num_windows);
    profiler.end("1. Windowing");

    profiler.begin("2. FFT + Spectrogram");
    compute_spectrogram(windows, num_windows, hann, spec);
    profiler.end("2. FFT + Spectrogram");

    profiler.begin("3. Peak Detection");
    detect_peaks(spec, peaks);
    profiler.end("3. Peak Detection");
    std::cout << "Found " << peaks.count << " peaks:\n";
    for (int i = 0; i < peaks.count; i++) {
        std::cout << "Peak " << i
                << "  freq=" << peaks.peaks[i].freq
                << "  time=" << peaks.peaks[i].time
                << '\n';
    }

    profiler.begin("4. Hashing and JSON");
    std::string result = generate_hashes(peaks);
    profiler.end("4. Hashing and JSON");

    profiler.print();

    return result;
}

int main() {
    // wav stuff instead of using mp3 directly
    system("ffmpeg -hide_banner -loglevel panic -i test.mp3 -acodec pcm_s16le -ac 1 -ar 22050 test.wav");
           

    // std::ifstream f_in("raw_data", std::ios::binary);

    std::ifstream f_in("test.wav", std::ios::binary);
    if (!f_in.is_open()) {
        std::cerr << "ERROR: Could not open 'test.wav'." << std::endl;
        return 1;
    }
    
    f_in.seekg(44, std::ios::beg);

    short speech;
    static float data[MAX_SAMPLES];
    int i = 0;

    while (i < MAX_SAMPLES && f_in.read((char*)&speech, 2)) {
        data[i++] = speech;
    }

    f_in.close();

    std::cout << fingerprint(data, i) << std::endl;
    return 0;
}