// fingerprint.cpp
// ============================================================================
// REWRITE FOR KRIA KV260 / VITIS HLS
// ============================================================================
// What changed vs. the original and why (read this before touching the code):
//
// 1. REMOVED OpenCV entirely.
// - cv::dft(...) is replaced with a radix-2 FFT (fft_radix2).
// WINDOW_SIZE=4096 is a power of two so radix-2 works cleanly.
// UPDATE: the original hand-rolled iterative version was
// replaced with the loop structure from the course lecture notes
//  — a separate bit_reverse() pass
// followed by stage_loop -> butterfly_loop -> dft_loop, matching
// the reference implementation rather than a from-scratch version.
// Host-side FFT+spectrogram latency dropped from
// 53ms to 20ms after this change, but that's a CPU timing number,
// not a synthesized result and not yet validated against
// Vitis HLS latency/II either. before trusting the "faster" claim on
// hardware, check post-synthesis reports once available (see
// caveats in the report re: cos/sin call frequency and the
// loop-carried dependency in butterfly_loop).
//
// 2. REMOVED Boost (property_tree / json_parser).
// - generate_hashes() now builds the JSON array with a plain
// std::ostringstream, matching the old compact (non-pretty) output
// shape: [{"hash":"...","offset":N}, ...].
//
// 3. REMOVED std::vector everywhere in the hot path.
// - Windows, the spectrogram, and the peak list are now fixed-capacity
// static arrays/structs (see the "SIZING" block below). This is the
// main thing that makes this synthesizable/reasonable for Vitis HLS —
// HLS does not like std::vector (heap allocation, no fixed bounds for
// pragma-driven unrolling/pipelining).
// - Fixed-size buffers mean there are now real upper bounds. If your
// actual audio is longer than MAX_SAMPLES, or produces more windows
// than MAX_WINDOWS, or more peaks than MAX_PEAKS, this code clamps /
// silently drops rather than growing. I left comments at each cap —
// you will want to size these to your real KV260 memory budget
// (BRAM/URAM) rather than my placeholder numbers.
//
// 4. ELIMINATED two of the original's transposes (this was your ask):
// - Old flow: stride_windows() built a [sample][window] "blocks" array,
// then a manual loop transposed it into a [window][sample] cv::Mat
// before the DFT. New flow: build_windows() writes directly into the
// final [window][sample] row-major layout, so that transpose is gone.
// - Old flow: after cv::dft + cv::mulSpectrums, a second manual loop
// transposed the complex output into a [freq][window] dst2 matrix.
// New flow: compute_spectrogram() runs the FFT per window and writes
// the power value straight into spec.power[f][w] (freq-major) in the
// same pass — no separate transpose step.
// - Net effect: everything is row-major from the moment samples are
// copied in, and freq-major from the moment the FFT comes out, with
// no intermediate matrix laid out the "wrong" way for what comes next.
//
// 5. Globals that were mutable `int`/`float` (DEFAULT_FAN_VALUE, etc.) are
// now `constexpr`. They were never actually changed at runtime, and
// HLS strongly prefers compile-time constants for array sizing and
// pragma unroll/pipeline factors.
//
// 6. Kept std::string / std::ostringstream / SHA1 / Timer as host-side-only.
// generate_hashes() (variable-length string + SHA1 hashing) and the
// Timer profiler are NOT things you'd put inside an HLS kernel anyway —
// variable-length output and std::string aren't synthesizable, and the
// profiler is a debug aid, not part of the algorithm. The candidate for
// the actual Vitis HLS top-level function is the block that's now fully
// array/struct based: build_windows -> apply_hann -> compute_spectrogram
// -> detect_peaks. I left those four as free functions taking
// fixed-size arrays/structs by reference so you can lift them into an
// HLS top function with minimal reshaping.
//
// 7. Added a bounds check in main()'s read loop. The original wrote into
// `float data[200000]` with no check that the file didn't have more
// samples than that — a silent buffer overflow on a long enough input.
// Now guarded against MAX_SAMPLES.
//
// Things I did NOT change and you should look at separately:
// - The Timer/profiler class (still uses std::unordered_map<std::string,...>).
// It's diagnostic-only and never runs on the FPGA fabric, so I left it.
// - PEAK_NEIGHBORHOOD_SIZE=20 makes detect_peaks() the expensive part:
// per pixel it scans ~2*R^2+2*R+1 = 841 neighbors, over up to
// 2049 x MAX_WINDOWS pixels. That's on the order of 10^8 comparisons
// per call. Fine on the ARM host for now, but this is exactly the
// kind of loop you'll want a sliding-window/incremental-max version
// of before you push it into HLS with pragmas — flagging it rather
// than prematurely optimizing it here.
// ============================================================================

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

// ----------------------------------------------------------------------------
// Timer / profiler — unchanged, host-side diagnostic only (see note 6 above).
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// Constants — now constexpr (see note 5). These drive every fixed-size
// array below, so they're the first thing to look at / tune for your board.
// ----------------------------------------------------------------------------
constexpr int DEFAULT_FAN_VALUE = 15;
constexpr int MIN_HASH_TIME_DELTA = 0;
constexpr int MAX_HASH_TIME_DELTA = 200;
constexpr int PEAK_NEIGHBORHOOD_SIZE = 20;
constexpr float DEFAULT_AMP_MIN = 10.0f;
constexpr int DEFAULT_WINDOW_SIZE = 4096;
constexpr float DEFAULT_OVERLAP_RATIO = 0.5f;
constexpr float FS = 22050.0f;

// ----------------------------------------------------------------------------
// SIZING — fixed upper bounds for every static buffer below. These are the
// numbers to revisit for your real input lengths / on-chip memory budget.
// ----------------------------------------------------------------------------
constexpr int WINDOW_SIZE = DEFAULT_WINDOW_SIZE; // 4096, must stay power-of-two for fft_radix2
constexpr int OVERLAP = (int)(DEFAULT_WINDOW_SIZE * DEFAULT_OVERLAP_RATIO); // 2048
constexpr int HOP = WINDOW_SIZE - OVERLAP; // 2048

constexpr int MAX_SAMPLES = 200000; // matches the fixed `data[200000]` buffer main() has always used
// Upper bound on number of windows a clip of MAX_SAMPLES can produce, plus
// a little headroom. If you feed in longer clips than MAX_SAMPLES, raise
// both constants together.
constexpr int MAX_WINDOWS = (MAX_SAMPLES - OVERLAP) / HOP + 4;

// WINDOW_SIZE is even, so this is always WINDOW_SIZE/2 + 1 — the old code
// computed this at runtime to handle an odd window size that never
// actually occurs here; folded into a compile-time constant instead.
constexpr int MAX_FREQ = WINDOW_SIZE / 2 + 1; // 2049

// Fixed cap on how many spectral peaks we'll record per clip. Peaks beyond
// this are silently dropped (see detect_peaks) rather than growing a
// container — tune this against real peak counts on your test corpus.
constexpr int MAX_PEAKS = 20000;

// ----------------------------------------------------------------------------
// Fixed-size structs (replaces std::vector<std::pair<int,int>> peaks, and
// the various std::vector<std::vector<float>> matrices).
// ----------------------------------------------------------------------------
struct Peak {
int freq;
int time;
};

struct PeakList {
Peak peaks[MAX_PEAKS];
int count; // we dont need that really
};

struct Spectrogram {
// Row-major, freq-major: power[f][w] is the dB power at frequency bin f,
// time window w. This is the layout the old code only reached after a
// second manual transpose (dst2); here it's written directly (note 4).
float power[MAX_FREQ][MAX_WINDOWS];
int num_freq; // again not needed
int num_windows; // not needed
};

// ----------------------------------------------------------------------------
// Windowing
// ----------------------------------------------------------------------------

// Builds windows directly in [window][sample] row-major layout.
// windows[w][s] = data[w*HOP + s]
// (Old code built a [sample][window] "blocks" array via stride_windows(),
// then transposed it into this same layout by hand afterwards — that
// transpose is gone, this writes the target layout straight away.)
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

// windows are already [window][sample], so applying the per-sample Hann
// coefficient is just a contiguous row scan (old apply_window() had to
// index [sample][window] against the pre-transpose "blocks" layout).
void apply_hann(float windows[MAX_WINDOWS][WINDOW_SIZE],
const float hann[WINDOW_SIZE], int num_windows) {
for (int w = 0; w < num_windows; w++)
for (int s = 0; s < WINDOW_SIZE; s++)
windows[w][s] *= hann[s];
}

// ----------------------------------------------------------------------------
// FFT — replaces cv::dft(). Iterative radix-2 decimation-in-time, in place.
// NOT validated against cv::dft's numeric output — see note 1 at the top.
// ----------------------------------------------------------------------------
constexpr int FFT_STAGES = 12; // M = log2(WINDOW_SIZE) = log2(4096)

unsigned int reverse_bits(unsigned int input, int M) {
    unsigned int rev = 0;
    for (int i = 0; i < M; i++) {
        rev = (rev << 1) | (input & 1);
        input >>= 1;
    }
    return rev;
}

void bit_reverse(float* re, float* im, int n, int M) {
    for (unsigned int i = 0; i < (unsigned int)n; i++) {
        unsigned int reversed = reverse_bits(i, M);
        if (i <= reversed) {
            std::swap(re[i], re[reversed]);
            std::swap(im[i], im[reversed]);
        }
    }
}

void fft_radix2(float* re, float* im, int n) {
    bit_reverse(re, im, n, FFT_STAGES);

    for (int stage = 1; stage <= FFT_STAGES; stage++) {
        int DFTpts = 1 << stage;
        int numBF  = DFTpts / 2;
        float e = -6.283185307178f / DFTpts;
        float a = 0.0f;

        for (int j = 0; j < numBF; j++) {
            float c = cosf(a);
            float s = sinf(a);
            a += e;

            for (int i = j; i < n; i += DFTpts) {
                int i_lower = i + numBF;
                float temp_R = re[i_lower] * c - im[i_lower] * s;
                float temp_I = im[i_lower] * c + re[i_lower] * s;
                re[i_lower] = re[i] - temp_R;
                im[i_lower] = im[i] - temp_I;
                re[i]       = re[i] + temp_R;
                im[i]       = im[i] + temp_I;
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Spectrogram — replaces cv::dft + cv::mulSpectrums + the dst2 transpose
// loop + the dB conversion loop, all fused into one pass per window.
// ----------------------------------------------------------------------------
void compute_spectrogram(const float windows[MAX_WINDOWS][WINDOW_SIZE], int num_windows,
const float hann[WINDOW_SIZE], Spectrogram& spec) {
// static: these are per-call scratch buffers reused across the window
// loop below. Declared static (rather than a plain local array) so
// they aren't repeatedly stack-allocated — matters both for a big
// WINDOW_SIZE and for how HLS wants to map this to BRAM.
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
float p = re[f] * re[f] + im[f] * im[f]; // |X[f]|^2, replaces mulSpectrums(dst,dst,dst,0,true)

if (f > 0 && f < MAX_FREQ - 1) p *= 2.0f; // one-sided spectrum: double all bins except DC and Nyquist

p *= (1.0f / FS);
p *= (1.0f / hann_energy);

if (p < 1e-8f) p = 1e-8f; // same floor as the original's `threshold = 1e-8`

// Writing straight into [freq][window] here is what replaces the
// old dst2 transpose loop (`dst2.at(i,j) = dst.ptr<float>(j)[2*i]`).
spec.power[f][w] = 10.0f * log10f(p);
}
}
}

// ----------------------------------------------------------------------------
// Peak detection — replaces get_2D_peaks() (cv::dilate/cv::erode based).
// ----------------------------------------------------------------------------
void detect_peaks(const Spectrogram& spec, PeakList& peaks) {
peaks.count = 0;
const int R = PEAK_NEIGHBORHOOD_SIZE;

for (int f = 0; f < spec.num_freq; f++) {
for (int w = 0; w < spec.num_windows; w++) {
float center = spec.power[f][w];
float max_val = center;
bool all_background = true; // becomes false as soon as any in-range neighbor is nonzero

// Diamond (Manhattan-distance <= R) neighborhood — equivalent to
// the original's cross structuring element dilated R times (see
// note 1). Out-of-range neighbors are simply skipped, which is
// the practical stand-in for OpenCV's border-constant handling
// in dilate/erode (max-value border for erode, min for dilate —
// both amount to "don't let the border falsely win/lose").
for (int df = -R; df <= R; df++) {
int nf = f + df;
if (nf < 0 || nf >= spec.num_freq) continue;
int max_dw = R - std::abs(df);
for (int dw = -max_dw; dw <= max_dw; dw++) {
int nw = w + dw;
if (nw < 0 || nw >= spec.num_windows) continue;
float v = spec.power[nf][nw];
if (v > max_val) max_val = v;
if (v != 0.0f) all_background = false;
}
}

bool is_local_max = (center == max_val);
bool is_peak = is_local_max && !all_background && (center > DEFAULT_AMP_MIN);

if (is_peak) {
if (peaks.count < MAX_PEAKS) {
peaks.peaks[peaks.count].freq = f;
peaks.peaks[peaks.count].time = w;
peaks.count++;
}
// else: dropped — see MAX_PEAKS sizing note above. Consider
// logging/counting drops if you ever hit this in practice.
}
}
}
}

// ----------------------------------------------------------------------------
// Hashing + JSON — host-side only (variable-length string + SHA1, see note 6).
// Boost property_tree/json_parser replaced with a plain ostringstream build
// that reproduces the same compact array-of-objects shape.
// ----------------------------------------------------------------------------
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
// Peaks are sorted ascending by time, so once k runs past the
// end it will for every larger j too — `break` instead of the
// original's per-iteration `if (i+j < count)` guard. Same
// result, skips redundant checks.
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

// ----------------------------------------------------------------------------
// Top level
// ----------------------------------------------------------------------------
std::string fingerprint(float* data, int data_size) {
// static: these are the big buffers (windows alone is ~1.5MB at
// WINDOW_SIZE=4096 x MAX_WINDOWS floats) — file-scope/static rather
// than function-local avoids putting megabytes on the stack, and maps
// more naturally onto BRAM/URAM-backed arrays once this is lifted into
// an HLS top function.
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

// Old code had separate "2. Mat Conversion" and "3. DFT" stages because
// it needed a transpose before cv::dft could run on rows. Both the
// transpose and the DFT/power-spectrum step are now one fused stage.
profiler.begin("2. FFT + Spectrogram");
compute_spectrogram(windows, num_windows, hann, spec);
profiler.end("2. FFT + Spectrogram");

profiler.begin("3. Peak Detection");
detect_peaks(spec, peaks);
profiler.end("3. Peak Detection");
std::cout << "Found " << peaks.count << " peaks:\n";
for (int i = 0; i < peaks.count; i++) {
std::cout << "Peak " << i
<< " freq=" << peaks.peaks[i].freq
<< " time=" << peaks.peaks[i].time
<< '\n';
}


profiler.begin("4. Hashing and JSON");
std::string result = generate_hashes(peaks);
profiler.end("4. Hashing and JSON");

profiler.print();

return result;
}

int main() {
system("ffmpeg -hide_banner -loglevel panic -i test.mp3 "
"-f s16le -acodec pcm_s16le -ss 0 -ac 1 -ar 22050 - > raw_data");

std::ifstream f_in("raw_data", std::ios::binary);

short speech;
static float data[MAX_SAMPLES]; // static: 200000 floats (~800KB) is too big to want on the stack
int i = 0;

// Added a bounds check here — the original wrote into a fixed-size
// array with no guard, which is a silent buffer overflow if the input
// audio decodes to more than MAX_SAMPLES samples.
while (i < MAX_SAMPLES && f_in.read((char*)&speech, 2)) {
data[i++] = speech;
}

f_in.close();

std::cout << fingerprint(data, i) << std::endl;
return 0;
}
