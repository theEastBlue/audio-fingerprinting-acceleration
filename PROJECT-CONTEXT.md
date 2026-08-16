# Where this branch fits in the real project

This is a note added on top of the original branch — nothing above this file
was changed. It exists to connect this exploratory work to the project the
team actually shipped (COMP4601, "FPGA-Accelerated Audio Fingerprinting using
HLS on the Kria KV260" — see `COMP4601 Final Presentation.pdf` and
`hw-profiling-times.md` on `main`).

## What this branch actually is

Same standalone port as `netik-no-opencv-boost`, with one line changed
(`sha1().substr(0, 20)`) to match `theEastBlue`'s original 20-char truncated
output exactly. Useful as a reference for output-format compatibility, not
part of the deployed `main` pipeline.

## Independent verification

```
$ g++ -O2 -std=c++17 -I. fingerprint-no-opencv-boost.cpp -o fp
$ ./fp
```

produces **21** truncated (20-char) hashes for the built-in 5-second
synthetic 3-tone signal — same real, deterministic count as the untruncated
version on `netik-no-opencv-boost`, just shorter strings. The README's
"83 hashes" figure has the same origin as on that branch: its JSON list
splices these 21 real (truncated) hashes together with roughly 60 additional
strings that don't come from running this code as committed.

This isn't a correction of the original README (untouched, per instruction)
— just a documented, reproducible cross-check for anyone picking this branch
up later.
