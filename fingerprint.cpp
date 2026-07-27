#define _USE_MATH_DEFINES
#include "fingerprint.h"

#include <iostream>
#include <algorithm>
#include <vector>
#include <limits>
#include <iterator>
#include <cmath>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "sha1.hpp"
#include <fstream>
#include <sstream>
#include <unordered_map>

using boost::property_tree::ptree;
using namespace std;

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

int DEFAULT_FAN_VALUE = 5;
int MIN_HASH_TIME_DELTA = 0;
int MAX_HASH_TIME_DELTA = 200;
// FINGERPRINT_REDUCTION removed — was declared but never used
int PEAK_NEIGHBORHOOD_SIZE = 6;   // was 20 — shrunk for short clip + smaller HLS kernel
float DEFAULT_AMP_MIN = 10;
int DEFAULT_WINDOW_SIZE = 512;
float DEFAULT_OVERLAP_RATIO = 0.25;
float FS = 11025.0;

std::vector<std::vector<float>> stride_windows(const std::vector<float>& data,
                                               size_t blocksize,
                                               size_t overlap) {
  std::vector<std::vector<float>> res;
  size_t minlen = (data.size() - overlap) / (blocksize - overlap);
  auto start = data.begin();

  for (size_t i = 0; i < blocksize; ++i) {
    res.emplace_back(std::vector<float>());
    auto it = start++;

    for (size_t j = 0; j < minlen; ++j) {
      res.back().push_back(*it);
      std::advance(it, (blocksize - overlap));
    }
  }
  return res;
}

std::vector<float> create_window(int wsize) {
  std::vector<float> res;
  res.reserve(wsize);

  for (int i = 0; i < wsize; i++) {
    float multiplier =
        0.5 - 0.5 * cos(2.0 * M_PI * i / (wsize - 1));
    res.push_back(multiplier);
  }
  return res;
}

void apply_window(std::vector<float>& hann_window,
                  std::vector<std::vector<float>>& data) {
  size_t nocols = data[0].size();
  size_t norows = data.size();

  for (size_t i = 0; i < nocols; ++i)
    for (size_t j = 0; j < norows; ++j)
      data[j][i] *= hann_window[j];
}

std::string get_sha1(const std::string& input) {
  SHA1 checksum;
  checksum.processBytes(input.data(), input.size());
  return checksum.getHash();
}

std::string generate_hashes(vector<pair<int, int>>& v_in) {
  sort(v_in.begin(), v_in.end(),
       [](auto& a, auto& b) {
         if (a.second == b.second)
           return a.first < b.first;
         return a.second < b.second;
       });

  std::ostringstream buf;
  buf << "[";

  bool first = true;

  for (int i = 0; i < (int)v_in.size(); i++) {
    for (int j = 1; j < DEFAULT_FAN_VALUE; j++) {
      if (i + j < (int)v_in.size()) {

        int freq1 = v_in[i].first;
        int freq2 = v_in[i + j].first;
        int time1 = v_in[i].second;
        int time2 = v_in[i + j].second;

        int t_delta = time2 - time1;

        if (t_delta >= MIN_HASH_TIME_DELTA &&
            t_delta <= MAX_HASH_TIME_DELTA) {

          char buffer[100];
          snprintf(buffer, sizeof(buffer),
                   "%d|%d|%d", freq1, freq2, t_delta);

          string to_be_hashed(buffer);

          string hash_result = get_sha1(to_be_hashed);

          ptree pt;
          pt.put("hash", hash_result);
          pt.put("offset", time1);

          if (!first) buf << ",";
          first = false;

          write_json(buf, pt, false);
        }
      }
    }
  }

  buf << "]";
  return buf.str();
}

// Simplified: removed background/eroded_background branch. After the dB floor
// in fingerprint() (v clamped to >= 1e-8 before log10), no cell is ever exactly
// 0.0f again, so (data == 0) was always all-false, making eroded_background
// always all-false and detected_peaks == local_max anyway. This is behaviorally
// identical to the original but does one dilate instead of dilate+erode.
vector<pair<int, int>> get_2D_peaks(cv::Mat data) {
  cv::Mat tmpkernel =
      cv::getStructuringElement(cv::MORPH_CROSS,
                                cv::Size(3, 3),
                                cv::Point(-1, -1));

  cv::Mat kernel =
      cv::Mat(PEAK_NEIGHBORHOOD_SIZE * 2 + 1,
              PEAK_NEIGHBORHOOD_SIZE * 2 + 1,
              CV_8U, uint8_t(0));

  kernel.at<uint8_t>(PEAK_NEIGHBORHOOD_SIZE,
                      PEAK_NEIGHBORHOOD_SIZE) = 1;

  cv::dilate(kernel, kernel, tmpkernel,
             cv::Point(-1, -1),
             PEAK_NEIGHBORHOOD_SIZE, 1, 1);

  cv::Mat d1;
  cv::dilate(data, d1, kernel);

  vector<pair<int, int>> out;

  for (int i = 0; i < data.rows; i++) {
    for (int j = 0; j < data.cols; j++) {
      if (data.at<float>(i, j) == d1.at<float>(i, j) &&
          data.at<float>(i, j) > DEFAULT_AMP_MIN) {
        out.push_back({i, j});
      }
    }
  }

  return out;
}

std::string fingerprint(float* data, int data_size) {
  profiler.begin("1. Windowing");
  vector<float> vec(data, data + data_size);
  int max_freq = (DEFAULT_WINDOW_SIZE % 2 == 0) ? (DEFAULT_WINDOW_SIZE / 2 + 1) : ((DEFAULT_WINDOW_SIZE + 1) / 2);
  auto blocks = stride_windows(vec, DEFAULT_WINDOW_SIZE, DEFAULT_WINDOW_SIZE * DEFAULT_OVERLAP_RATIO);
  auto hann_window = create_window(DEFAULT_WINDOW_SIZE);
  apply_window(hann_window, blocks);
  profiler.end("1. Windowing");

  profiler.begin("2. Mat Conversion");
  cv::Mat dst(blocks[0].size(), blocks.size(), CV_32F);
  for (int i = 0; i < dst.rows; i++)
    for (int j = 0; j < dst.cols; j++)
      dst.at<float>(i, j) = blocks[j][i];
  profiler.end("2. Mat Conversion");

  profiler.begin("3. DFT");
  cv::dft(dst, dst, cv::DFT_COMPLEX_OUTPUT | cv::DFT_ROWS);
  profiler.end("3. DFT");

  profiler.begin("4. Spectrogram");
  cv::mulSpectrums(dst, dst, dst, 0, true);
  cv::Mat dst2(max_freq, blocks[0].size(), CV_32F);
  
  for (int i = 0; i < max_freq; i++)
    for (int j = 0; j < dst2.cols; j++)
      dst2.at<float>(i, j) = dst.ptr<float>(j)[2 * i];

  for (int i = 1; i < dst2.rows - 1; i++)
    for (int j = 0; j < dst2.cols; j++)
      dst2.at<float>(i, j) *= 2;

  dst2 *= (1.0 / FS);

  float sum = 0;
  for (auto& v : hann_window)
    sum += v * v;

  dst2 *= (1.0 / sum);

  // The local variable declaration that fixes the cv::threshold collision
  float threshold = 1e-8;

  for (int i = 0; i < dst2.rows; i++) {
    for (int j = 0; j < dst2.cols; j++) {
      float& v = dst2.at<float>(i, j);
      if (v < threshold) v = threshold;
      v = 10 * log10(v);
    }
  }
  profiler.end("4. Spectrogram");

  profiler.begin("5. Peak Detection");
  auto peaks = get_2D_peaks(dst2);
  profiler.end("5. Peak Detection");

  // ---- added: inspect peaks before hashing ----
  cout << "peak count: " << peaks.size() << endl;
  for (auto& p : peaks) {
    cout << "(" << p.first << ", " << p.second << ")\n";
  }
  // -----------------------------------------------

  profiler.begin("6. Hashing and JSON");
  std::string result = generate_hashes(peaks);
  profiler.end("6. Hashing and JSON");

  profiler.print(); 

  return result;
}
std::vector<float> load_audio(const std::string& path) {
  std::string cmd = "ffmpeg -hide_banner -loglevel panic -i " + path +
                     " -f s16le -acodec pcm_s16le -ss 0 -ac 1 -ar 11025 - > raw_data";
  system(cmd.c_str());

  std::ifstream f_in("raw_data", std::ios::binary);
  std::vector<float> data;
  short speech;
  while (f_in.read((char*)&speech, 2)) data.push_back(speech);
  return data;
}
