#include "fingerprint.h"
#include "timer.h"
#include "cmdlineparser.h"

// XRT includes
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

#include <iostream>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include "sha1.hpp"

Timer profiler;

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

// Change signature to accept the fake Spectrogram directly
std::string fingerprint(Spectrogram& spec, int num_windows, xrt::device& device, xrt::kernel& krnl) {
    static PeakList peaks;

    profiler.begin("3. Peak Detection (XRT Transfer & Kernel Exec)");

    // Allocate Buffer Objects (BOs) in Global Memory
    auto bo_spec      = xrt::bo(device, MAX_FREQ * MAX_WINDOWS * sizeof(float), krnl.group_id(0));
    auto bo_peak_freq = xrt::bo(device, MAX_PEAKS * sizeof(int), krnl.group_id(2));
    auto bo_peak_time = xrt::bo(device, MAX_PEAKS * sizeof(int), krnl.group_id(3));

    // Map device buffers to host pointers
    auto map_spec      = bo_spec.map<float*>();
    auto map_peak_freq = bo_peak_freq.map<int*>();
    auto map_peak_time = bo_peak_time.map<int*>();

    // Copy spectrogram data to the mapped host pointer
    // FIX: Copy the entire MAX_FREQ * MAX_WINDOWS buffer so 2D layout is preserved
    std::memcpy(map_spec, spec.power, MAX_FREQ * MAX_WINDOWS * sizeof(float));
    
    bo_spec.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    std::cout << "[cosim] Execution of the kernel...\n";
    auto run = krnl(bo_spec, num_windows, bo_peak_freq, bo_peak_time);
    run.wait();

    std::cout << "[cosim] Get the output data from the device...\n";
    bo_peak_freq.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    bo_peak_time.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    peaks.count = 0;
    for (int i = 0; i < MAX_PEAKS; i++) {
        if (map_peak_time[i] == -1) break; 
        peaks.peaks[i].freq = map_peak_freq[i];
        peaks.peaks[i].time = map_peak_time[i];
        peaks.count++;
    }

    profiler.end("3. Peak Detection (XRT Transfer & Kernel Exec)");

    std::cout << "[cosim] Found " << peaks.count << " peaks.\n";

    profiler.begin("4. Hashing and JSON");
    std::string result = generate_hashes(peaks);
    profiler.end("4. Hashing and JSON");

    profiler.print();

    return result;
}

int main(int argc, char** argv) {
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

    std::cout << "Opening device " << device_index << "...\n";
    auto device = xrt::device(device_index);
    
    std::cout << "Loading xclbin: " << binaryFile << "\n";
    auto uuid = device.load_xclbin(binaryFile);
    auto krnl = xrt::kernel(device, uuid, "detect_peaks");

    // -----------------------------------------------------------------
    // BYPASS WAV READING: Build the fake spectrogram exactly like test_mini_cosim.cpp
    // -----------------------------------------------------------------
    Spectrogram fake_spec;
    std::memset(fake_spec.power, 0, sizeof(fake_spec.power));
    
    int mini_num_windows = 4;
    
    struct Expected { int f, w; float val; };
    Expected expected[] = {
        { 5,  1, 50.0f },
        { 15, 2, 60.0f },
        { 25, 3, 40.0f },
    };
    
    for (auto& p : expected) {
        fake_spec.power[p.f][p.w] = p.val;
    }
    // -----------------------------------------------------------------

    // Call fingerprint function with the fake data
    std::cout << fingerprint(fake_spec, mini_num_windows, device, krnl) << std::endl;
    
    return EXIT_SUCCESS;
}