# audio-fingerprinting-acceleration
Hardware acceleration of the  dejavu-cpp-port

> **NOTE: Week 05 Project Brief Presentation: https://canva.link/0kp52c2cmpuakbc**

## Overview

This project investigates hardware acceleration opportunities for an audio fingerprinting algorithm based on the Dejavu fingerprinting pipeline. The objective is to identify computational bottlenecks, implement suitable stages using High-Level Synthesis (HLS), and evaluate the resulting performance improvements on FPGA hardware.

The original software implementation generates compact audio fingerprints by transforming an audio signal into a spectrogram, detecting salient spectral peaks, and constructing hash values that uniquely represent the audio content.

---

## Algorithm Pipeline

The fingerprinting process consists of the following stages:

1. **Audio Preprocessing**

   * Load raw PCM audio samples.
   * Divide the signal into overlapping windows.
   * Apply a Hann window to each frame.

2. **Frequency Analysis**

   * Compute the Fast Fourier Transform (FFT) for each window.
   * Calculate the power spectrum.
   * Convert the spectrum to a logarithmic (dB) scale.

3. **Peak Detection**

   * Apply a local maximum filter.
   * Identify significant spectral peaks above a configurable threshold.

4. **Fingerprint Generation**

   * Pair neighbouring peaks within a predefined time window.
   * Generate SHA-1 hashes from frequency and time relationships.
   * Output fingerprints as `(hash, offset)` pairs.

---

## Project Objectives

* Profile the software implementation to identify computational hotspots.
* Evaluate which stages are suitable for FPGA acceleration.
* Implement selected kernels using Vitis HLS.
* Compare software and hardware implementations in terms of:

  * Execution time
  * Throughput
  * Resource utilisation
  * Accuracy

---

## Team

| Name | zID | Role |
| ---- | --- | ---- |
| Esha |  z5391046   |    Contributor  |
|   Venus   |  z5483461   |  Contributor    |
|   Rukhsaar   |  z5619382   |   Contributor   |
|   Netik   |  z5636903   |   Contributor   |

---

## Building

### Requirements

* C++14
* OpenCV
* Boost
* FFmpeg (demo only)

### Build

```bash
clang++ fingerprint.cpp \
    -std=c++14 \
    -I/opt/homebrew/opt/boost/include \
    $(pkg-config --cflags --libs opencv4) \
    -o fingerprint
```

### Run

```bash
./fingerprint
```

---

## Future Work

* Hardware acceleration of FFT using Vitis FFT IP
* Streaming window generation
* Hardware peak detection
* End-to-end FPGA integration
* Performance evaluation and benchmarking

---

## References

* [Dejavu Audio Fingerprinting Source Algorithm](https://github.com/salsowelim/dejavu_cpp_port/tree/70b4307111be4a2481e40e7d58b763ac50a2081b)
* [OpenCV](https://opencv.org/home-4/)
* [Xilinx Vitis HLS](https://www.xilinx.com/support/documents/sw_manuals/xilinx2022_2/ug1399-vitis-hls.pdf)

