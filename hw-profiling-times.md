# TIMING ANALYSIS

## SW Only (ARM processor- PS)
===== PROFILING =====
- FFT + Spectrogram: 392.447 ms
- Windowing: 14.86 ms
- Peak Detection: 16284.9 ms
- Hashing and JSON: 7.6 ms

## SW + HW (unaccelerated)

===== PROFILING =====
- FFT + Spectrogram: 389.705 ms
- Windowing: 14.9801 ms
- Hashing and JSON: 7.85546 ms
- Peak Detection (XRT Transfer & Kernel Exec): 5005.44 ms

##  SW + HW (accelerated)
===== PROFILING =====
- FFT + Spectrogram: 983.713 ms
- Windowing: 14.8794 ms
- Hashing and JSON: 0.02669 ms
- Peak Detection (XRT Transfer & Kernel Exec): 16.5133 ms
