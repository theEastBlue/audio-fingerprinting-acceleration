// test.cpp
// compile cmd so i dont forget: clang++ test.cpp preprocessing.cpp fingerprint_kernel.cpp -o test_csim -std=c++14 -O3
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <math.h>
#include <algorithm>
#include <vector>

#include "fingerprint.h"
#include "timer.h"

struct Rmse {
    int num_sq;
    float sum_sq;
    float error;

    Rmse() { num_sq = 0; sum_sq = 0; error = 0; }

    float add_value(float d_n) {
        num_sq++;
        sum_sq += (d_n * d_n);
        error = sqrtf(sum_sq / num_sq);
        return error;
    }
};

Rmse rmse_freq, rmse_time;
Timer profiler2;

int main() {
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

    static Spectrogram spec;
    static PeakList peaks;
    int num_windows = 0;

    static int peak_freq[MAX_PEAKS];
    static int peak_time[MAX_PEAKS];
    int peak_count = 0;

    preprocessing(data, i, spec, num_windows);

    profiler2.begin("3. Peak Detection");
    detect_peaks(spec.power, num_windows, peak_freq, peak_time, &peak_count);
    profiler2.end("3. Peak Detection");
    profiler2.print();
    
    peaks.count = peak_count;
    for (int i = 0; i < peak_count; i++) {
        peaks.peaks[i].freq = peak_freq[i];
        peaks.peaks[i].time = peak_time[i];
    }
    

    std::cout << "Found " << peaks.count << " peaks.\n";
    for (int i = 0; i < peaks.count; i++) {
        std::cout << "Peak " << i
                << "  freq=" << peaks.peaks[i].freq
                << "  time=" << peaks.peaks[i].time
                << '\n';
    }

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

    // Calculate RMSE
    int max_compare = std::min(peaks.count, gold_peaks.count);

    for (int i = 0; i < max_compare; i++) {
        rmse_freq.add_value((float)peaks.peaks[i].freq - (float)gold_peaks.peaks[i].freq);
        rmse_time.add_value((float)peaks.peaks[i].time - (float)gold_peaks.peaks[i].time);
    }

    for (int i = max_compare; i < peaks.count; i++) {
        rmse_freq.add_value((float)peaks.peaks[i].freq);
        rmse_time.add_value((float)peaks.peaks[i].time);
    }
    for (int i = max_compare; i < gold_peaks.count; i++) {
        rmse_freq.add_value((float)gold_peaks.peaks[i].freq);
        rmse_time.add_value((float)gold_peaks.peaks[i].time);
    }

    printf("----------------------------------------------\n");
    printf("   RMSE(Freq)          RMSE(Time)\n");
    printf("%0.15f %0.15f\n", rmse_freq.error, rmse_time.error);
    printf("----------------------------------------------\n");

    if (rmse_freq.error > 1.0 || rmse_time.error > 1.0 || peaks.count != gold_peaks.count) {
        fprintf(stdout, "*******************************************\n");
        fprintf(stdout, "FAIL: Peak coordinates DO NOT match the golden output\n");
        fprintf(stdout, "*******************************************\n");
        return 1;
    } else {
        fprintf(stdout, "*******************************************\n");
        fprintf(stdout, "PASS: Peak coordinates match the golden output!\n");
        fprintf(stdout, "*******************************************\n");

        // should i call the generate_hashes function here?? or let it be???
        return 0;
    }
}