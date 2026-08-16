# Where this branch fits in the real project

This is a note added on top of the original branch — nothing above this file
was changed. It exists to connect this exploratory work to the project the
team actually shipped (COMP4601, "FPGA-Accelerated Audio Fingerprinting using
HLS on the Kria KV260" — see `COMP4601 Final Presentation.pdf` and
`hw-profiling-times.md` on `main`).

## What this branch actually is

Early exploration of a `compute_spectrogram` HLS kernel (Hann window + FFT +
PSD), predating the more complete version on `netik-compute-spectrogram-hls-v2`
and the later, report-backed work on `netik-fused-kernel`. Same overall
target as the PDF's own stated "Future Work": FFT+Spectrogram is the CPU
bottleneck the team identified after accelerating peak detection
(983.60 ms of ~1010 ms total on the real board — see `hw-profiling-times.md`
on `main`, "SW + HW (accelerated)").

## What can and can't be reproduced from this commit

`compute_spectrogram_tb.cpp` reads a `raw_data` file (documented as
"decoded test.mp3 at 22050 Hz mono, int16 PCM") that isn't committed on this
branch — only `decode_mp3.py`, which would generate it from a `test.mp3`
that also isn't committed here. So the csim result and the csynth numbers
originally described for this specific commit can't be independently
reproduced as-is; there's no Vitis HLS report file committed either.

`netik-compute-spectrogram-hls-v2` has the same kernel design but a
self-contained testbench (real `test.wav` committed, no missing inputs) —
that version's csim result (`max_err = 0.000000 dB`, 0/137,283 failures)
was independently reproduced by compiling directly with `g++ -std=c++17`.

## Target device note

Like the other kernel branches in this repo, `kernel_run.tcl` here targets
`xc7a35tcsg324-1` (Artix-7) as a placeholder — the real board is a Kria
KV260 (Zynq UltraScale+ K26 SOM, part `xck26-sfvc784-2LV-c`), which has no
7-series equivalent resource budget. See `netik-fused-kernel`'s
`fused_kernel_results.md` for a documented attempt to synthesize against the
real KV260 part and why it couldn't be completed in that environment
(missing Zynq UltraScale+ device pack).
