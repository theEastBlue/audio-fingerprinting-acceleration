# TIMING ANALYSIS

## SW Only (ARM processor- PS)
===== PROFILING =====
2. FFT + Spectrogram: 392.447 ms
1. Windowing: 14.86 ms

===== PROFILING =====
3. Peak Detection: 16284.9 ms
4. Hashing and JSON: 19.21 ms

## SW + HW (unaccelerated)

===== PROFILING =====
2. FFT + Spectrogram: 389.705 ms
1. Windowing: 14.9801 ms
4. Hashing and JSON: 7.85546 ms
3. Peak Detection (XRT Transfer & Kernel Exec): 5005.44 ms

##  SW + HW (accelerated)
===== PROFILING =====
2. FFT + Spectrogram: 983.713 ms
1. Windowing: 14.8794 ms
4. Hashing and JSON: 0.02669 ms
3. Peak Detection (XRT Transfer & Kernel Exec): 16.5133 ms
