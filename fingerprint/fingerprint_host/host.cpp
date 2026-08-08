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
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include "sha1.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

// Overloaded function to pass XRT device and kernel objects.
//
// Calls the FUSED kernel (fingerprint_kernel: spectrogram + peak detection
// in one launch) instead of the old two-kernel sequence (a CPU-side
// compute_spectrogram followed by a detect_peaks kernel launch). See
// fingerprint/fingerprint_kernel/fused_kernel_results.md for why spec_power
// is a device-side scratch buffer here: it's allocated on the device and
// never sync'd to/from the host -- the kernel writes and reads it
// internally, which is what actually removes the second kernel launch and
// the spectrogram's host round trip that existed before.
std::string fingerprint(float* data, int data_size, xrt::device& device, xrt::kernel& krnl) {
    static float windows[MAX_WINDOWS][WINDOW_SIZE];
    static float hann[WINDOW_SIZE];
    static PeakList peaks;
    int num_windows = 0;

    // Step 1: Windowing (CPU). The FFT/spectrogram itself now runs on the
    // FPGA as part of the fused kernel, not here.
    profiler.begin("1. Windowing");
    build_windows(data, data_size, windows, num_windows);
    build_hann_window(hann);
    apply_hann(windows, hann, num_windows);
    profiler.end("1. Windowing");

    float hann_energy = 0.0f;
    for (int i = 0; i < WINDOW_SIZE; i++) hann_energy += hann[i] * hann[i];

    // Step 2: Hardware Acceleration (XRT) -- ONE kernel launch.
    profiler.begin("2. Fused Kernel (XRT Transfer & Kernel Exec)");

    // Allocate Buffer Objects (BOs) in Global Memory.
    // Argument order matches fingerprint_kernel's signature:
    //   windows(0), num_windows(1), hann_energy(2), spec_power(3),
    //   peak_freq(4), peak_time(5), peak_count(6)
    auto bo_windows    = xrt::bo(device, MAX_WINDOWS * WINDOW_SIZE * sizeof(float), krnl.group_id(0));
    auto bo_spec_power = xrt::bo(device, MAX_FREQ * MAX_WINDOWS * sizeof(float), krnl.group_id(3));
    auto bo_peak_freq  = xrt::bo(device, MAX_PEAKS * sizeof(int), krnl.group_id(4));
    auto bo_peak_time  = xrt::bo(device, MAX_PEAKS * sizeof(int), krnl.group_id(5));
    auto bo_peak_count = xrt::bo(device, sizeof(int), krnl.group_id(6));

    // Map device buffers to host pointers.
    auto map_windows    = bo_windows.map<float*>();
    auto map_peak_freq  = bo_peak_freq.map<int*>();
    auto map_peak_time  = bo_peak_time.map<int*>();
    auto map_peak_count = bo_peak_count.map<int*>();

    // Copy windowed audio to the mapped host pointer, then sync to device.
    std::memcpy(map_windows, windows, MAX_WINDOWS * WINDOW_SIZE * sizeof(float));
    bo_windows.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    // bo_spec_power is intentionally NEVER sync'd here -- it's scratch DRAM
    // the kernel allocates work in internally (spectrogram computation
    // writes it, peak detection reads it back), all within this single
    // kernel invocation. The host never needs its contents.
    auto run = krnl(bo_windows, num_windows, hann_energy, bo_spec_power,
                     bo_peak_freq, bo_peak_time, bo_peak_count);

    // Wait for the FPGA to finish processing.
    run.wait();

    // Sync output arrays from the device back to host memory.
    bo_peak_freq.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    bo_peak_time.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    bo_peak_count.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    int peak_count = map_peak_count[0];

    // Repack flattened hardware arrays back into CPU structs.
    peaks.count = peak_count;
    for (int i = 0; i < peak_count; i++) {
        peaks.peaks[i].freq = map_peak_freq[i];
        peaks.peaks[i].time = map_peak_time[i];
    }
    profiler.end("2. Fused Kernel (XRT Transfer & Kernel Exec)");

    std::cout << "Found " << peaks.count << " peaks.\n";

    // Step 3: Post-processing (CPU)
    profiler.begin("3. Hashing and JSON");
    std::string result = generate_hashes(peaks);
    profiler.end("3. Hashing and JSON");

    profiler.print();

    return result;
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
    auto krnl = xrt::kernel(device, uuid, "fingerprint_kernel");

    // Read Audio Data
    // system("ffmpeg -hide_banner -loglevel panic -i test.mp3 -acodec pcm_s16le -ac 1 -ar 22050 test.wav");

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

    // Call fingerprint function and pass the initialized XRT objects
    std::cout << fingerprint(data, i, device, krnl) << std::endl;
    
    return EXIT_SUCCESS;
}