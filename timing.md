## Version i (og- opencv)
===== PROFILING =====
6. Hashing and JSON: 4.9355 ms
3. DFT: 1.96646 ms
5. Peak Detection: 20.9872 ms
4. Spectrogram: 7.68408 ms
2. Mat Conversion: 3.79017 ms
1. Windowing: 13.3534 ms

## Version ii (diamond/Manhattan peak detection)
===== PROFILING =====
4. Hashing and JSON: 0.811666 ms
3. Peak Detection: 362.57 ms
2. FFT + Spectrogram: 16.255 ms
1. Windowing: 0.674041 ms

## Version iii (chebyshev i.e square dilation/erosion results)
===== PROFILING =====
4. Hashing and JSON: 0.799 ms
3. Peak Detection: 70.6609 ms
2. FFT + Spectrogram: 16.8506 ms
1. Windowing: 0.660458 ms

random re-run gave me this time...idk why:
===== PROFILING =====
4. Hashing and JSON: 0.831416 ms
3. Peak Detection: 81.6206 ms
2. FFT + Spectrogram: 35.7025 ms
1. Windowing: 1.92033 ms

with -O3 flag:
===== PROFILING =====
4. Hashing and JSON: 0.91525 ms
3. Peak Detection: 13.3812 ms
2. FFT + Spectrogram: 12.1743 ms
1. Windowing: 0.445 ms


## Version iv (line buffers for peak detection)
===== PROFILING =====
2. FFT + Spectrogram: 18.6549 ms
1. Windowing: 0.669709 ms

===== PROFILING =====
3. Peak Detection: 1024.96 ms
Found 43 peaks.

## CSIM Profiling
===== PROFILING =====
2. FFT + Spectrogram: 4712 ms
1. Windowing: 3.2366 ms

===== PROFILING =====
3. Peak Detection: 1515.63 ms
Found 43 peaks.

## Board results
===== PROFILING =====
2. FFT + Spectrogram: 272.839 ms
1. Windowing: 10.5635 ms
Found 99 peaks.
===== PROFILING =====
4. Hashing and JSON: 19.8557 ms
3. Peak Detection (XRT Transfer & Kernel Exec): 3615.7 ms
