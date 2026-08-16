# Where this branch fits in the real project

This is a note added on top of the original branch — nothing above this file
was changed. It exists to connect this exploratory work to the project the
team actually shipped (COMP4601, "FPGA-Accelerated Audio Fingerprinting using
HLS on the Kria KV260" — see `COMP4601 Final Presentation.pdf` and
`hw-profiling-times.md` on `main`).

## What this branch actually is

`fingerprint-no-opencv-boost.cpp` is a standalone, dependency-free C++ port
of the algorithm — useful as a portable reference (builds with plain
`g++ -std=c++17`, no OpenCV/Boost/Vitis HLS needed) to sanity-check the
algorithm logic independently of the HLS toolchain. It's not itself part of
the deployed pipeline on `main` (which uses `fingerprint-baseline/` and
`fingerprint-accelerated/`), but it's the same underlying algorithm.

## Independent verification

Compiling and running this branch's code directly:

```
$ g++ -O2 -std=c++17 -I. fingerprint-no-opencv-boost.cpp -o fp
$ ./fp
```

produces **21** fingerprint hashes (40-char SHA-1) for the built-in 5-second
synthetic 3-tone test signal defined in `main()`. The README's "83 hashes"
figure and its "FFT + Spectrogram: 3837 ms" timing don't reproduce from the
code as committed — the README's own JSON hash list actually contains this
branch's real 21 hashes interleaved with roughly 60 additional hash strings
that don't come from running this code. Likely explanation: an earlier
version of `main()` read a real audio file (`test.mp3` via `ffmpeg`, per the
commit message: "Synthetic audio generator in main() ... replaces ffmpeg +
test.mp3") which would produce more hashes over a longer/richer signal — but
that number doesn't belong to *this* synthetic-audio code path, and no
`test.mp3`-based run is reproducible from what's committed here.

None of the project's actual FFT+Spectrogram timing numbers (`392.447 ms`,
`389.705 ms`, `983.713 ms` on the real board, or `16.255`/`16.85`/`35.7 ms`
from early Mac runs — see `sw-profiling-times.md` and `hw-profiling-times.md`
on `main`) are 3837 ms either, for reference.

This isn't a correction of the original README (untouched, per instruction)
— just a documented, reproducible cross-check for anyone picking this branch
up later.
