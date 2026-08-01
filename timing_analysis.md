## Version i (og- opencv)
===== PROFILING =====
6. Hashing and JSON: 4.9355 ms
3. DFT: 1.96646 ms
5. Peak Detection: 20.9872 ms
4. Spectrogram: 7.68408 ms
2. Mat Conversion: 3.79017 ms
1. Windowing: 13.3534 ms

## Version ii (diamond/manhattan)
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