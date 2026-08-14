#include "cmdlineparser.h"
#include "fingerprint.h"
#include "timer.h"

// XRT includes
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "sha1.hpp"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Timer profiler;
Rmse rmse_freq, rmse_time;

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

            if (t_delta >= MIN_HASH_TIME_DELTA &&
                t_delta <= MAX_HASH_TIME_DELTA) {
                char buffer[100];
                snprintf(buffer, sizeof(buffer), "%d|%d|%d", freq1, freq2,
                         t_delta);

                std::string to_be_hashed(buffer);
                std::string hash_result = get_sha1(to_be_hashed);

                if (!first) buf << ",";
                first = false;

                buf << "{\"hash\":\"" << hash_result
                    << "\",\"offset\":" << time1 << "}";
            }
        }
    }

    buf << "]";
    return buf.str();
}

// Overloaded function to pass XRT device and kernel objects
std::string fingerprint(float* data, int data_size, xrt::device& device,
                        xrt::kernel& krnl) {
    static Spectrogram spec;
    static PeakList peaks;
    int num_windows = 0;

    // Step 1: Preprocessing (CPU)
    preprocessing(data, data_size, spec, num_windows);

    // Step 2: Hardware Acceleration (XRT)
    profiler.begin("3. Peak Detection (XRT Transfer & Kernel Exec)");

    // Allocate Buffer Objects (BOs) in Global Memory
    // void detect_peaks(
    // const float spec[MAX_FREQ][MAX_WINDOWS], arg0
    // int num_windows, // arg1
    // int peak_freq[MAX_PEAKS], // arg2
    // int peak_time[MAX_PEAKS] // arg3
    // );

    profiler.begin("init transfer setup");
    auto bo_spec = xrt::bo(device, MAX_FREQ * MAX_WINDOWS * sizeof(DTYPE),
                           krnl.group_id(0));
    auto bo_peak_freq =
        xrt::bo(device, MAX_PEAKS * sizeof(int), krnl.group_id(2));
    auto bo_peak_time =
        xrt::bo(device, MAX_PEAKS * sizeof(int), krnl.group_id(3));

    // Map device buffers to host pointers
    auto map_spec = bo_spec.map<DTYPE*>();
    auto map_peak_freq = bo_peak_freq.map<int*>();
    auto map_peak_time = bo_peak_time.map<int*>();

    // Copy spectrogram data to the mapped host pointer, then sync it to the
    // device
    std::memset(map_spec, 0, MAX_FREQ * MAX_WINDOWS * sizeof(DTYPE));
    std::memcpy(map_spec, spec.power, MAX_FREQ * MAX_WINDOWS * sizeof(DTYPE));
    std::cout << "===============\n";
    bo_spec.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    profiler.end("init transfer setup");

    // Execute the kernel
    // Arguments match the kernel signature: spec (0), num_windows (1),
    // peak_freq (2), peak_time (3)
    std::cout << "Execution of the kernel\n";
    auto run = krnl(bo_spec, num_windows, bo_peak_freq, bo_peak_time);
    // Wait for the FPGA to finish processing
    run.wait();

    // Sync output arrays from the device back to host memory
    
    std::cout << "Get the output data from the device" << std::endl;
    profiler.begin("sync output");
    bo_peak_freq.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    bo_peak_time.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    profiler.end("sync output");

    // Read the output scalar (peak_count) directly from the s_axilite register
    peaks.count = 0;

    for (int i = 0; i < MAX_PEAKS; i++) {
        if (map_peak_time[i] == -1) break;
        peaks.peaks[i].freq = map_peak_freq[i];
        peaks.peaks[i].time = map_peak_time[i];
        peaks.count++;
    }

    profiler.end("3. Peak Detection (XRT Transfer & Kernel Exec)");

    std::cout << "Found " << peaks.count << " peaks.\n";

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
        return "EXIT FAILURE";
    } else {
        fprintf(stdout, "*******************************************\n");
        fprintf(stdout, "PASS: Peak coordinates match the golden output!\n");
        fprintf(stdout, "*******************************************\n");

        fprintf(stdout, "STARTING HASH GENERATION \n");

        // Step 3: Post-processing (CPU)
        profiler.begin("4. Hashing and JSON");
        std::string result = generate_hashes(peaks);
        profiler.end("4. Hashing and JSON");

        profiler.print();

        return result;
    }
}

int main(int argc, char** argv) {
    // Command Line Parser to grab the .xclbin file
    sda::utils::CmdLineParser parser;
    parser.addSwitch("--xclbin_file", "-x", "input binary file string", "");
    parser.addSwitch("--device_id", "-d", "device index", "0");
    parser.parse(argc, argv);

    std::string binaryFile = parser.value("xclbin_file");
    int device_index = stoi(parser.value("device_id"));

    if (argc < 3) {
        parser.printHelp();
        return EXIT_FAILURE;
    }

    // Initialize XRT Device and Kernel
    std::cout << "Opening device " << device_index << "...\n";
    auto device = xrt::device(device_index);
    std::cout << "Loading xclbin: " << binaryFile << "\n";
    auto uuid = device.load_xclbin(binaryFile);
    auto krnl = xrt::kernel(device, uuid, "detect_peaks");

    // Read Audio Data
    // system("ffmpeg -hide_banner -loglevel panic -i test.mp3 -acodec pcm_s16le
    // -ac 1 -ar 22050 test.wav");

    std::ifstream f_in("test.wav", std::ios::binary);
    if (!f_in.is_open()) {
        std::cerr << "ERROR: Could not open 'test.wav'." << std::endl;
        return EXIT_FAILURE;
    }
    f_in.seekg(44, std::ios::beg);

    short speech;
    static float data[MAX_SAMPLES];
    int i = 0;

    while (i < MAX_SAMPLES && f_in.read((char*)&speech, 2)) {
        data[i++] = speech;
    }

    f_in.close();

    std::cout << fingerprint(data, i, device, krnl) << std::endl;
    return EXIT_SUCCESS;
}