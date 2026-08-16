# Where this branch fits in the real project

This is a note added on top of the original branch — nothing above this file
was changed. `fused_kernel_results.md` on this branch is already an unusually
rigorous, self-critical writeup (it documents its own device-target
limitation and what's verified vs. not) — this file just adds the explicit
link to the shipped project and an independent re-verification.

## Direct match to the final report's own "Future Work"

From `COMP4601 Final Presentation.pdf` (page 8, Future Work), verbatim:

> "Fused kernel/host interface: host sends raw windowed audio in -> single
> kernel computes fft + spectrogram and finds peaks -> returns peak
> coordinates."

That is exactly what `fused_kernel.cpp` implements — `compute_spectrogram`
(LUT-FFT, II=2) and `detect_peaks` merged into one top-level
`fingerprint_kernel` with a single `s_axilite` control bundle, motivated by
the same number the report cites as the new bottleneck: FFT+Spectrogram at
983.60 ms of ~1010 ms total runtime (97.4%) after peak detection was
accelerated to 4.00 ms (`hw-profiling-times.md` on `main`).

## Independent re-verification

The fused kernel's csim has no HLS-only headers, so it was recompiled
directly:

```
$ g++ -O2 -std=c++17 -I. fused_kernel_tb.cpp fused_kernel.cpp preprocessing.cpp -o tb
$ ./tb
Loaded 140561 samples from test.wav
Windows: 67  hann_energy: 1535.62
Found 43 peaks.
RMSE(Freq) RMSE(Time) = 0.000000000000000 0.000000000000000
gold peaks: 43   fused kernel peaks: 43
CSIM PASS: fused kernel matches golden peaks
```

Matches `fused_kernel_results.md`'s claimed "43/43 peaks, RMSE 0" exactly.

## What's still outstanding (per the branch's own results doc)

`fused_kernel_results.md` already documents this precisely, so it's not
repeated in full here: the real target part (`xck26-sfvc784-2LV-c`, KRIA
KV260) failed to synthesize in the environment it was tried in because the
Zynq UltraScale+ device pack wasn't installed — all committed resource
numbers (BRAM 53%, DSP 50%, FF 27%, LUT 62%) are against the `xc7a35t`
placeholder part, not the real board. `kernel_run_fused_kv260.tcl` is
already written and ready to run once that device pack is available.
