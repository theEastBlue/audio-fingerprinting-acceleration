# Where this branch fits in the real project

This is a note added on top of the original branch — nothing above this file
was changed. It exists to connect this exploratory work to the project the
team actually shipped (COMP4601, "FPGA-Accelerated Audio Fingerprinting using
HLS on the Kria KV260" — see `COMP4601 Final Presentation.pdf` and
`hw-profiling-times.md` on `main`), and to record which of this branch's
numbers are independently reproducible and which aren't.

## Relevance to the shipped project

This branch's whole point — replacing the FFT butterfly's loop-carried
twiddle recurrence with a precomputed LUT to unblock pipelining — targets
exactly the bottleneck the final report's "Future Work" section names:
FFT+Spectrogram at **983.60 ms of ~1010 ms total runtime (97.4%)** on the
real board, after peak detection was already accelerated 4,071x
(`hw-profiling-times.md` on `main`). This is real, targeted follow-on work,
not a random experiment.

## Independently verified (reproduced exactly)

Compiling each kernel variant directly with `g++ -std=c++17` (no HLS
toolchain needed — none of these files use HLS-only headers) against the
real, committed `test.wav`:

**Original kernel** (`compute_spectrogram_kernel.cpp`) and **no-pragma
baseline** (`compute_spectrogram_kernel_no_pragma.cpp`):
```
max_err = 0.000000 dB   failures = 0 / 137283 bins
CSIM PASS: kernel matches reference for all bins
```

**LUT kernel** (`compute_spectrogram_kernel_lut.cpp`), compiled against the
same testbench in place of the original:
```
max_err = 10.934242 dB   failures = 24 / 137283 bins
CSIM FAIL: 24 mismatches
```
This matches `compute_spectrogram_lut_results.md`'s claimed
`max_err = 10.93 dB, failures = 24 / 137283 bins` almost exactly. The 24
mismatches are in low-power bins skipped by the reference's noise-floor
cutoff, consistent with floating-point rounding rather than a correctness
bug — the doc's own explanation for this holds up.

## Not independently verifiable from this repo

No Vitis HLS csynth report (`.rpt`/`.xml`) is committed on this branch, so
the specific II/Fmax/DSP/LUT/BRAM numbers in `compute_spectrogram_results.md`
and `compute_spectrogram_lut_results.md` (e.g. butterfly II 21→2, Fmax
137MHz, DSP 64→40) could not be cross-checked against a report file the way
the csim numbers above could. `netik-fused-kernel` shows what committing
that evidence looks like (`fused_prj/solution1/syn/report/*.rpt`,
1,300+ lines) — that's the bar for a synthesis claim to be independently
checkable later. Re-running `vitis_hls -f kernel_run_compare.tcl` /
`kernel_run_lut.tcl` and committing the resulting report files would close
this gap.

## Target device note

Both TCL scripts target `xc7a35tcsg324-1` (Artix-7) as a placeholder — the
real board is a Kria KV260 (Zynq UltraScale+ K26 SOM, part
`xck26-sfvc784-2LV-c`). See `netik-fused-kernel`'s `fused_kernel_results.md`
for the documented attempt to synthesize this same LUT-FFT logic against the
real KV260 part (blocked by a missing Zynq UltraScale+ device pack, not by
unfinished design work).
