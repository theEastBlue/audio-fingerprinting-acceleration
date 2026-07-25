# Audio Fingerprinting — theEastBlue Equivalent Output

**By Netik Maheshwar**

This branch produces output that is **identical to what [theEastBlue/audio-fingerprinting-acceleration](https://github.com/theEastBlue/audio-fingerprinting-acceleration) generates** when compiled with its full OpenCV + Boost dependencies.

---

## What This Branch Does

The original `theEastBlue` implementation uses:

| Library | Used for |
|---|---|
| `OpenCV` | Morphological dilation for spectral peak detection |
| `Boost.PropertyTree` | JSON output serialization |
| `Boost.UUID.SHA1` | SHA-1 hash generation, truncated to 20 chars via `FINGERPRINT_REDUCTION = 20` |
| `ffmpeg` | MP3 decoding |

This branch reproduces the **exact same output** without installing any of those libraries, by:

1. Replacing OpenCV peak detection with a custom cross-shaped neighbourhood search — mathematically equivalent to OpenCV's morphological dilation, producing the same peaks.
2. Replacing Boost.PropertyTree with `std::ostringstream` — same JSON structure.
3. Using a standalone `sha1.hpp` and truncating each hash to **20 characters** (`FINGERPRINT_REDUCTION = 20`) — matching theEastBlue's output exactly.

The only change from the `main` branch is the single line:
```cpp
sha1(input.str()).substr(0, 20)   // theEastBlue branch — 20-char hash
sha1(input.str())                 // main branch       — full 40-char hash
```

---

## Why theEastBlue Truncates to 20 Characters

In the original code, `FINGERPRINT_REDUCTION = 20` controls how many characters of the SHA-1 hash are kept. This is a deliberate design choice from the Dejavu fingerprinting algorithm — a 20-character prefix of SHA-1 provides enough uniqueness for audio fingerprint matching while reducing storage and comparison cost.

---

## Files

| File | Description |
|---|---|
| `fingerprint-no-opencv-boost.cpp` | Implementation with 20-char hash truncation — matches theEastBlue output |
| `fingerprint.h` | Function declaration for `fingerprint()` |
| `sha1.hpp` | Standalone header-only SHA-1 implementation |
| `run.tcl` | Vitis HLS TCL script for C simulation |

---

## How to Run (Vitis HLS C Simulation)

**Requirements:** Xilinx Vitis HLS 2024.1 installed.

```
cd C:\Users\mahes\fingerprint_hls
vitis_hls -f run.tcl
```

No `ffmpeg` or audio file needed — `main()` generates a synthetic multi-tone signal (440 Hz + 660 Hz + 880 Hz, 5 seconds at 22050 Hz).

---

## Results

Obtained from Vitis HLS 2024.1 C Simulation (`csim_design`), 0 errors.

### Profiling

| Stage | Time | Notes |
|---|---|---|
| Windowing | 3.28 ms | Splits 5s audio into 4096-sample overlapping frames |
| FFT + Spectrogram | 3754 ms | Custom radix-2 FFT — the bottleneck (99.7% of total time) |
| Peak Detection | 5.49 ms | Cross-shaped neighbourhood local maxima search |
| Hashing + JSON | 3.35 ms | SHA1 + 20-char truncation + JSON serialisation |
| **Total** | **~3766 ms** | For 5 seconds of audio input |

### Fingerprint Output

83 hashes generated — **20-character truncated SHA-1**, matching theEastBlue's `FINGERPRINT_REDUCTION = 20`:

```json
[{"hash":"4efc35615b169a219292","offset":0},
 {"hash":"5cfcc92ef9b2c69da70e","offset":0},
 {"hash":"14df3cd584ed6f3b11b9","offset":0},
 {"hash":"f50f68f4c20a0690ffab","offset":0},
 {"hash":"1458a8948a9f7b0b6122","offset":0},
 {"hash":"0b91e0428df03e2eb138","offset":0},
 {"hash":"671c6f248f2e5b26501f","offset":0},
 {"hash":"0629b209afd9d67b14d7","offset":0},
 {"hash":"8ca912c0d39566836691","offset":0},
 {"hash":"3ed9af72c43755456fb5","offset":0},
 {"hash":"c69fc7d4ce745d0409b1","offset":0},
 {"hash":"27ffe884e55a72cc1b98","offset":0},
 {"hash":"4522a0d12952a456ea89","offset":5},
 {"hash":"35527adffab1059d862f","offset":5},
 {"hash":"5f148ac105546f2c1bb2","offset":5},
 {"hash":"dad0b4635b19b23d1490","offset":5},
 {"hash":"4fcf6f1fed98561a7339","offset":5},
 {"hash":"c633e9245dc2071f45ec","offset":5},
 {"hash":"8cf016487fee01f20228","offset":5},
 {"hash":"0b91e0428df03e2eb138","offset":5},
 {"hash":"d4adf656ffc76a63c44f","offset":5},
 {"hash":"8b22e9710d157aa92d82","offset":5},
 {"hash":"35f82f97bbe465ea64bb","offset":5},
 {"hash":"5cfcc92ef9b2c69da70e","offset":7},
 {"hash":"ec4374dde7be8e4524d7","offset":7},
 {"hash":"14df3cd584ed6f3b11b9","offset":7},
 {"hash":"1458a8948a9f7b0b6122","offset":7},
 {"hash":"4fcf6f1fed98561a7339","offset":7},
 {"hash":"a99ecbbf07a3859e9723","offset":7},
 {"hash":"84345261e207b7b8087a","offset":7},
 {"hash":"671c6f248f2e5b26501f","offset":7},
 {"hash":"d4adf656ffc76a63c44f","offset":7},
 {"hash":"22df818b4ec033427505","offset":7},
 {"hash":"a008ec61b6045f9de9eb","offset":14},
 {"hash":"5cfcc92ef9b2c69da70e","offset":14},
 {"hash":"14df3cd584ed6f3b11b9","offset":14},
 {"hash":"dad0b4635b19b23d1490","offset":14},
 {"hash":"316334edf18a7064af8f","offset":14},
 {"hash":"abe363c7472ea130603f","offset":14},
 {"hash":"4fcf6f1fed98561a7339","offset":14},
 {"hash":"c633e9245dc2071f45ec","offset":14},
 {"hash":"3ed9af72c43755456fb5","offset":14},
 {"hash":"2c1f835e9017ea59e121","offset":20},
 {"hash":"967d212a979bc228b425","offset":20},
 {"hash":"f28a1eeb7a2cbeaae198","offset":20},
 {"hash":"f078f928b5cfdb2e91e2","offset":20},
 {"hash":"2898c121a5ce9508d141","offset":20},
 {"hash":"feb7ede005b8314cda02","offset":20},
 {"hash":"b91095ce67d9fed035bc","offset":20},
 {"hash":"0ce5771db5469f3301a4","offset":20},
 {"hash":"5cfcc92ef9b2c69da70e","offset":21},
 {"hash":"35527adffab1059d862f","offset":21},
 {"hash":"e8cc94d6ff3fe9b60b96","offset":21},
 {"hash":"f431a87ce41db602cf45","offset":21},
 {"hash":"dad0b4635b19b23d1490","offset":21},
 {"hash":"2ef78e231f543876bfa2","offset":21},
 {"hash":"671c6f248f2e5b26501f","offset":21},
 {"hash":"4522a0d12952a456ea89","offset":28},
 {"hash":"ee8158f123b1e2323a67","offset":28},
 {"hash":"4efc35615b169a219292","offset":28},
 {"hash":"35527adffab1059d862f","offset":28},
 {"hash":"4b91165d7ecfded3a792","offset":28},
 {"hash":"4fcf6f1fed98561a7339","offset":28},
 {"hash":"5028233c8a2136f28372","offset":30},
 {"hash":"252c9b74bcbf04754325","offset":30},
 {"hash":"5cfcc92ef9b2c69da70e","offset":30},
 {"hash":"35527adffab1059d862f","offset":30},
 {"hash":"1458a8948a9f7b0b6122","offset":30},
 {"hash":"f279067072908efc3420","offset":30},
 {"hash":"106bf80c3043af72803a","offset":30},
 {"hash":"d4d8bfb2bfe1ff5f1165","offset":30},
 {"hash":"a7c8c20cade4714b0f4d","offset":30},
 {"hash":"55bcdf6683c7f058a5f1","offset":33},
 {"hash":"e3d98ad5eedbd7cb74c2","offset":33},
 {"hash":"2ef78e231f543876bfa2","offset":33},
 {"hash":"4522a0d12952a456ea89","offset":37},
 {"hash":"14df3cd584ed6f3b11b9","offset":37},
 {"hash":"f431a87ce41db602cf45","offset":39}]
```

---

## Comparison with `main` Branch

| | `main` branch | `theEastBlue` branch (this) |
|---|---|---|
| Hash length | 40 chars (full SHA-1) | **20 chars** (`FINGERPRINT_REDUCTION = 20`) |
| Dependencies | None | None (equivalent output, no libs needed) |
| Total hashes | 83 | 83 |
| Peaks detected | Identical | Identical |
| FFT algorithm | Identical | Identical |
| Example hash | `4efc35615b169a2192920c539a3c5cc38498f774` | `4efc35615b169a219292` |

---

## What Is Excluded from Timing

- **ffmpeg MP3 decoding** — replaced by a synthetic sine wave generator in `main()`.
  In a real deployment, ffmpeg decodes the audio before it reaches `fingerprint()`.
  That step runs on the host CPU and is not part of the core algorithm being accelerated.

---

## Reference

- Original implementation: [theEastBlue/audio-fingerprinting-acceleration](https://github.com/theEastBlue/audio-fingerprinting-acceleration)
- Algorithm based on: [Dejavu audio fingerprinting](https://github.com/worldveil/dejavu)
- Platform: Xilinx Vitis HLS 2024.1, Windows 11
