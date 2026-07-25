#include "fingerprint.h"
#include "sha1.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using std::complex;
using std::pair;
using std::string;
using std::vector;

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr int kFanValue = 15;
constexpr int kMinHashTimeDelta = 0;
constexpr int kMaxHashTimeDelta = 200;
constexpr int kPeakNeighborhoodSize = 20;
constexpr float kMinimumAmplitude = 10.0f;
constexpr int kWindowSize = 4096;
constexpr float kOverlapRatio = 0.5f;
constexpr float kSampleRate = 22050.0f;

struct Timer {
    std::unordered_map<string, double> elapsed;
    std::unordered_map<string, std::chrono::high_resolution_clock::time_point> started;

    void begin(const string& name) {
        started[name] = std::chrono::high_resolution_clock::now();
    }

    void end(const string& name) {
        const auto stopped = std::chrono::high_resolution_clock::now();
        elapsed[name] += std::chrono::duration<double, std::milli>(
            stopped - started[name]).count();
    }

    void print() const {
        std::cout << "\n===== PROFILING =====\n";
        for (const auto& item : elapsed) {
            std::cout << item.first << ": " << item.second << " ms\n";
        }
    }
};

Timer profiler;

struct Spectrogram {
    int rows = 0;
    int cols = 0;
    vector<float> values;

    Spectrogram(int rowCount, int columnCount)
        : rows(rowCount), cols(columnCount),
          values(static_cast<size_t>(rowCount) * columnCount, 0.0f) {}

    float& at(int row, int col) {
        return values[static_cast<size_t>(row) * cols + col];
    }

    float at(int row, int col) const {
        return values[static_cast<size_t>(row) * cols + col];
    }
};

vector<vector<float>> make_windows(const float* data, int dataSize,
                                    int windowSize, int overlap) {
    vector<vector<float>> windows;
    if (data == nullptr || dataSize < windowSize || overlap >= windowSize) {
        return windows;
    }

    const int step = windowSize - overlap;
    for (int start = 0; start + windowSize <= dataSize; start += step) {
        windows.emplace_back(data + start, data + start + windowSize);
    }
    return windows;
}

vector<float> make_hann_window(int size) {
    vector<float> window(size);
    if (size == 1) {
        window[0] = 1.0f;
        return window;
    }

    for (int i = 0; i < size; ++i) {
        window[i] = 0.5f - 0.5f * std::cos(2.0f * kPi * i / (size - 1));
    }
    return window;
}

// In-place radix-2 FFT. The project uses a 4096-point window, so the
// power-of-two requirement is satisfied without an FFT library.
void fft(vector<complex<float>>& values) {
    const size_t n = values.size();

    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(values[i], values[j]);
        }
    }

    for (size_t length = 2; length <= n; length <<= 1) {
        const float angle = -2.0f * kPi / static_cast<float>(length);
        const complex<float> step(std::cos(angle), std::sin(angle));

        for (size_t start = 0; start < n; start += length) {
            complex<float> multiplier(1.0f, 0.0f);
            const size_t half = length / 2;

            for (size_t i = 0; i < half; ++i) {
                const complex<float> even = values[start + i];
                const complex<float> odd = multiplier * values[start + i + half];
                values[start + i] = even + odd;
                values[start + i + half] = even - odd;
                multiplier *= step;
            }
        }
    }
}

Spectrogram make_spectrogram(const vector<vector<float>>& windows,
                             const vector<float>& hannWindow) {
    const int frequencyBins = kWindowSize / 2 + 1;
    Spectrogram result(frequencyBins, static_cast<int>(windows.size()));

    float windowEnergy = 0.0f;
    for (float value : hannWindow) {
        windowEnergy += value * value;
    }

    for (int column = 0; column < result.cols; ++column) {
        vector<complex<float>> samples(kWindowSize);
        for (int i = 0; i < kWindowSize; ++i) {
            samples[i] = complex<float>(windows[column][i] * hannWindow[i], 0.0f);
        }

        fft(samples);

        for (int row = 0; row < result.rows; ++row) {
            float power = std::norm(samples[row]);
            if (row > 0 && row < result.rows - 1) {
                power *= 2.0f;
            }
            power /= (kSampleRate * windowEnergy);
            result.at(row, column) = 10.0f * std::log10(std::max(power, 1e-8f));
        }
    }

    return result;
}

vector<pair<int, int>> get_2d_peaks(const Spectrogram& data) {
    vector<pair<int, int>> peaks;
    const int radius = kPeakNeighborhoodSize;

    for (int row = 0; row < data.rows; ++row) {
        for (int col = 0; col < data.cols; ++col) {
            const float value = data.at(row, col);
            if (value <= kMinimumAmplitude) {
                continue;
            }

            bool isMaximum = true;
            bool hasNonZeroNeighbour = false;

            for (int dr = -radius; dr <= radius && isMaximum; ++dr) {
                const int remaining = radius - std::abs(dr);
                for (int dc = -remaining; dc <= remaining; ++dc) {
                    const int neighbourRow = row + dr;
                    const int neighbourCol = col + dc;
                    if (neighbourRow < 0 || neighbourRow >= data.rows ||
                        neighbourCol < 0 || neighbourCol >= data.cols) {
                        continue;
                    }

                    const float neighbour = data.at(neighbourRow, neighbourCol);
                    if (neighbour > 0.0f) {
                        hasNonZeroNeighbour = true;
                    }
                    if (neighbour > value) {
                        isMaximum = false;
                        break;
                    }
                }
            }

            if (isMaximum && hasNonZeroNeighbour) {
                peaks.emplace_back(row, col);
            }
        }
    }
    return peaks;
}

string sha1(const string& input) {
    SHA1 checksum;
    checksum.processBytes(input.data(), input.size());
    return checksum.getHash();
}

string generate_hashes(vector<pair<int, int>> peaks) {
    std::sort(peaks.begin(), peaks.end(), [](const auto& left, const auto& right) {
        return left.second == right.second
            ? left.first < right.first
            : left.second < right.second;
    });

    std::ostringstream output;
    output << '[';
    bool first = true;

    for (int i = 0; i < static_cast<int>(peaks.size()); ++i) {
        for (int j = 1; j < kFanValue && i + j < static_cast<int>(peaks.size()); ++j) {
            const int delta = peaks[i + j].second - peaks[i].second;
            if (delta < kMinHashTimeDelta || delta > kMaxHashTimeDelta) {
                continue;
            }

            std::ostringstream input;
            input << peaks[i].first << '|'
                  << peaks[i + j].first << '|'
                  << delta;

            if (!first) {
                output << ',';
            }
            first = false;
            output << "{\"hash\":\"" << sha1(input.str())
                   << "\",\"offset\":" << peaks[i].second << '}';
        }
    }

    output << ']';
    return output.str();
}

} // namespace

std::string fingerprint(float* data, int data_size) {
    profiler.begin("1. Windowing");
    const int overlap = static_cast<int>(kWindowSize * kOverlapRatio);
    const auto windows = make_windows(data, data_size, kWindowSize, overlap);
    const auto hannWindow = make_hann_window(kWindowSize);
    profiler.end("1. Windowing");

    if (windows.empty()) {
        return "[]";
    }

    profiler.begin("2. FFT and Spectrogram");
    const auto spectrogram = make_spectrogram(windows, hannWindow);
    profiler.end("2. FFT and Spectrogram");

    profiler.begin("3. Peak Detection");
    const auto peaks = get_2d_peaks(spectrogram);
    profiler.end("3. Peak Detection");

    profiler.begin("4. Hashing and JSON");
    const std::string result = generate_hashes(peaks);
    profiler.end("4. Hashing and JSON");

    profiler.print();
    return result;
}

int main() {
    const float sampleRate = 22050.0f;
    const int numSamples = static_cast<int>(sampleRate * 5.0f); // 5 seconds
    const float pi = 3.14159265358979323846f;

    vector<float> samples(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        // Multi-tone signal: 440 Hz (A4) + 660 Hz + 880 Hz (A5)
        samples[i] = 16384.0f * std::sin(2.0f * pi * 440.0f * i / sampleRate)
                   +  8192.0f * std::sin(2.0f * pi * 660.0f * i / sampleRate)
                   +  4096.0f * std::sin(2.0f * pi * 880.0f * i / sampleRate);
    }

    const std::string result = fingerprint(samples.data(), numSamples);
    std::cout << result << '\n';
    return 0;
}
