# Where this branch fits in the real project

This is a note added on top of the original branch — nothing above this file
was changed. It exists to connect this exploratory work to the project the
team actually shipped (COMP4601, "FPGA-Accelerated Audio Fingerprinting using
HLS on the Kria KV260" — see `COMP4601 Final Presentation.pdf` and
`hw-profiling-times.md` on `main`).

## What this branch actually is

A self-contained `compute_spectrogram` HLS kernel (Hann window + 4096-pt
radix-2 FFT + PSD) with a testbench that reads the real, committed
`test.wav` — no missing inputs, unlike the earlier draft on
`netik-compute-spectrogram-hls`. Directly targets the bottleneck the team's
final report calls out as **Future Work**: after accelerating peak detection
(4,071x speedup, down to 4.00 ms), FFT+Spectrogram became the new dominant
cost at **983.60 ms of ~1010 ms total runtime (97.4%)** on the real board
(`hw-profiling-times.md`, "SW + HW (accelerated)" on `main`). This kernel is
exactly the next thing the report says needs offloading.

## Independent verification

```
$ g++ -O2 -std=c++17 -I. fingerprint/fingerprint_kernel/compute_spectrogram_tb.cpp \
      fingerprint/fingerprint_kernel/compute_spectrogram_kernel.cpp \
      fingerprint/fingerprint_kernel/preprocessing.cpp -o tb
$ ./tb
Loaded 140561 samples from test.wav
Windows: 67   hann_energy: 1535.62
max_err = 0.000000 dB   failures = 0 / 137283 bins
CSIM PASS: kernel matches reference for all bins
```

Reproduced exactly, no HLS toolchain required (the kernel code has no
HLS-only headers, so this checks the algorithm's correctness directly).

## Target device note

`kernel_run.tcl` targets `xc7a35tcsg324-1` (Artix-7) as a placeholder — the
real board is a Kria KV260 (Zynq UltraScale+ K26 SOM, part
`xck26-sfvc784-2LV-c`). See `netik-fused-kernel`'s `fused_kernel_results.md`
for the documented attempt to synthesize this same accelerated-FFT logic
against the real KV260 part, and why that step is still outstanding (missing
Zynq UltraScale+ device pack in the environment it was tried in) rather than
unfinished design work.
