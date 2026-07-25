# Audio Fingerprinting — Dependency-Free C++ Implementation

**By Netik Maheshwar**

A fully dependency-free C++ port of the Shazam-like audio fingerprinting algorithm, based on the original work at [theEastBlue/audio-fingerprinting-acceleration](https://github.com/theEastBlue/audio-fingerprinting-acceleration).

---

## What This Branch Does

The original `theEastBlue` implementation required three external libraries:

| Library | Used for |
|---|---|
| `OpenCV` | 2D morphological dilation for spectral peak detection |
| `Boost.PropertyTree` | JSON output serialization |
| `Boost.UUID.SHA1` | SHA-1 hash generation |

This branch (`main`) **removes all three dependencies** and replaces them with standard C++ only:

| Replaced with | How |
|---|---|
| OpenCV peak detection | Custom cross-shaped neighbourhood search (mathematically equivalent to OpenCV dilation) |
| Boost JSON | `std::ostringstream` |
| Boost SHA1 | Standalone header-only `sha1.hpp` (custom implementation) |

The algorithm — windowing, FFT, spectrogram, peak detection, hashing — is **identical**. Only the libraries used to implement it have changed.

---

## Files

| File | Description |
|---|---|
| `fingerprint-no-opencv-boost.cpp` | Main implementation — no external library dependencies |
| `fingerprint.h` | Function declaration for `fingerprint()` |
| `sha1.hpp` | Standalone header-only SHA-1 implementation |
| `run.tcl` | Vitis HLS TCL script for C simulation |

---

## How to Run (Vitis HLS C Simulation)

**Requirements:** Xilinx Vitis HLS 2024.1 installed.

No `ffmpeg` or audio file needed — the `main()` function generates a synthetic multi-tone signal (440 Hz + 660 Hz + 880 Hz, 5 seconds at 22050 Hz sample rate) and passes it directly into `fingerprint()`.

```
cd C:\Users\mahes\fingerprint_hls
vitis_hls -f run.tcl
```

---

## Results

Obtained from Vitis HLS 2024.1 C Simulation (`csim_design`), 0 errors.

### Profiling

| Stage | Time | Notes |
|---|---|---|
| Windowing | 3.66 ms | Splits 5s audio into 4096-sample overlapping frames |
| FFT + Spectrogram | 3837 ms | Custom radix-2 FFT — the bottleneck (99.7% of total time) |
| Peak Detection | 5.64 ms | Cross-shaped neighbourhood local maxima search |
| Hashing + JSON | 0.29 ms | SHA1 + JSON serialisation |
| **Total** | **~3847 ms** | For 5 seconds of audio input |

> The FFT + Spectrogram stage accounts for 99.7% of total runtime — making it the
> primary target for FPGA acceleration via Vitis HLS synthesis.

### Fingerprint Output

83 hashes generated (full 40-character SHA-1, no truncation):

```json
[{"hash":"4efc35615b169a2192920c539a3c5cc38498f774","offset":0},
 {"hash":"5cfcc92ef9b2c69da70eefa2955f8fb8765b1c35","offset":0},
 {"hash":"14df3cd584ed6f3b11b91d10f6e4e803aab760c6","offset":0},
 {"hash":"f50f68f4c20a0690ffabcbc78a5a2b8e52b142a5","offset":0},
 {"hash":"1458a8948a9f7b0b61225696739e63159ab83563","offset":0},
 {"hash":"0b91e0428df03e2eb1384827f28b54e9d6994138","offset":0},
 {"hash":"671c6f248f2e5b26501fc15ea94bdddef5ff7698","offset":0},
 {"hash":"0629b209afd9d67b14d7054be58dae51afdb0402","offset":0},
 {"hash":"8ca912c0d3956683669191949b9a1c24976b16cc","offset":0},
 {"hash":"3ed9af72c43755456fb58cbfa79f53366ebd5110","offset":0},
 {"hash":"c69fc7d4ce745d0409b1610a99b2a2eae601b270","offset":0},
 {"hash":"27ffe884e55a72cc1b98c870bd0dadd1685081be","offset":0},
 {"hash":"4522a0d12952a456ea897cfb9a8f59a619f587fd","offset":5},
 {"hash":"35527adffab1059d862f4fc2b7f680049d52213a","offset":5},
 {"hash":"5f148ac105546f2c1bb247f458cc5a1083004866","offset":5},
 {"hash":"dad0b4635b19b23d14901cfaa828cb74bded87c7","offset":5},
 {"hash":"4fcf6f1fed98561a733973acf2a1b8f0d724b34e","offset":5},
 {"hash":"c633e9245dc2071f45ec7c6ae5009deac1b3cd31","offset":5},
 {"hash":"8cf016487fee01f20228f29f8038810c4579dabc","offset":5},
 {"hash":"0b91e0428df03e2eb1384827f28b54e9d6994138","offset":5},
 {"hash":"d4adf656ffc76a63c44ff288671fd060a7fdd38c","offset":5},
 {"hash":"8b22e9710d157aa92d820dfb7343d597a6c1dbe8","offset":5},
 {"hash":"35f82f97bbe465ea64bb483a4746b02bbb052d24","offset":5},
 {"hash":"5cfcc92ef9b2c69da70eefa2955f8fb8765b1c35","offset":7},
 {"hash":"ec4374dde7be8e4524d72e4da4c43c3aae6fa140","offset":7},
 {"hash":"14df3cd584ed6f3b11b91d10f6e4e803aab760c6","offset":7},
 {"hash":"1458a8948a9f7b0b61225696739e63159ab83563","offset":7},
 {"hash":"4fcf6f1fed98561a733973acf2a1b8f0d724b34e","offset":7},
 {"hash":"a99ecbbf07a3859e9723a464d5c2e89f70804f72","offset":7},
 {"hash":"84345261e207b7b8087a203f6b183be8c2efc316","offset":7},
 {"hash":"671c6f248f2e5b26501fc15ea94bdddef5ff7698","offset":7},
 {"hash":"d4adf656ffc76a63c44ff288671fd060a7fdd38c","offset":7},
 {"hash":"22df818b4ec0334275054b7615031d2ddc4778f9","offset":7},
 {"hash":"a008ec61b6045f9de9ebe66e1b913097eb926604","offset":14},
 {"hash":"5cfcc92ef9b2c69da70eefa2955f8fb8765b1c35","offset":14},
 {"hash":"14df3cd584ed6f3b11b91d10f6e4e803aab760c6","offset":14},
 {"hash":"dad0b4635b19b23d14901cfaa828cb74bded87c7","offset":14},
 {"hash":"316334edf18a7064af8f54bda987466b31f381b8","offset":14},
 {"hash":"abe363c7472ea130603f162ebafe764d96a931cc","offset":14},
 {"hash":"4fcf6f1fed98561a733973acf2a1b8f0d724b34e","offset":14},
 {"hash":"c633e9245dc2071f45ec7c6ae5009deac1b3cd31","offset":14},
 {"hash":"3ed9af72c43755456fb58cbfa79f53366ebd5110","offset":14},
 {"hash":"2c1f835e9017ea59e121f2fa4b1af2bb810d0e0a","offset":20},
 {"hash":"967d212a979bc228b425c4fe53ea392f575a5d84","offset":20},
 {"hash":"f28a1eeb7a2cbeaae198457a68bcf93bac390f61","offset":20},
 {"hash":"f078f928b5cfdb2e91e2a70da5b12f41ec00724c","offset":20},
 {"hash":"2898c121a5ce9508d141643f22766a244f425863","offset":20},
 {"hash":"feb7ede005b8314cda02b6229eb5ca5ea66528dc","offset":20},
 {"hash":"b91095ce67d9fed035bcfcabdf2a7cc13a41e96b","offset":20},
 {"hash":"0ce5771db5469f3301a4b9d8b70a8086659b20d7","offset":20},
 {"hash":"5cfcc92ef9b2c69da70eefa2955f8fb8765b1c35","offset":21},
 {"hash":"35527adffab1059d862f4fc2b7f680049d52213a","offset":21},
 {"hash":"e8cc94d6ff3fe9b60b9679316c9df3d08f9a8226","offset":21},
 {"hash":"f431a87ce41db602cf45a8ffdcd15c65dbcf9329","offset":21},
 {"hash":"dad0b4635b19b23d14901cfaa828cb74bded87c7","offset":21},
 {"hash":"2ef78e231f543876bfa29be8acabc8eff894a807","offset":21},
 {"hash":"671c6f248f2e5b26501fc15ea94bdddef5ff7698","offset":21},
 {"hash":"4522a0d12952a456ea897cfb9a8f59a619f587fd","offset":28},
 {"hash":"ee8158f123b1e2323a67bafdac1a33a1e06fe653","offset":28},
 {"hash":"4efc35615b169a2192920c539a3c5cc38498f774","offset":28},
 {"hash":"35527adffab1059d862f4fc2b7f680049d52213a","offset":28},
 {"hash":"4b91165d7ecfded3a7925f0efb2bc3c96c881f48","offset":28},
 {"hash":"4fcf6f1fed98561a733973acf2a1b8f0d724b34e","offset":28},
 {"hash":"5028233c8a2136f283727cf00c3893d816548168","offset":30},
 {"hash":"252c9b74bcbf0475432597e5d1aee6d37925c59b","offset":30},
 {"hash":"5cfcc92ef9b2c69da70eefa2955f8fb8765b1c35","offset":30},
 {"hash":"35527adffab1059d862f4fc2b7f680049d52213a","offset":30},
 {"hash":"1458a8948a9f7b0b61225696739e63159ab83563","offset":30},
 {"hash":"f279067072908efc342058ecd5202b733ab9f979","offset":30},
 {"hash":"106bf80c3043af72803a854f22a753146e429bef","offset":30},
 {"hash":"d4d8bfb2bfe1ff5f1165286d0ba04f95339b1297","offset":30},
 {"hash":"a7c8c20cade4714b0f4db05491663822ed801662","offset":30},
 {"hash":"55bcdf6683c7f058a5f1fc61986d9d0e986ace2d","offset":33},
 {"hash":"e3d98ad5eedbd7cb74c2b48504244dae142e1f53","offset":33},
 {"hash":"2ef78e231f543876bfa29be8acabc8eff894a807","offset":33},
 {"hash":"4522a0d12952a456ea897cfb9a8f59a619f587fd","offset":37},
 {"hash":"14df3cd584ed6f3b11b91d10f6e4e803aab760c6","offset":37},
 {"hash":"f431a87ce41db602cf45a8ffdcd15c65dbcf9329","offset":39}]
```

---

## What Is Excluded from Timing

- **ffmpeg MP3 decoding** — replaced by a synthetic sine wave generator in `main()`.
  In a real deployment, ffmpeg decodes the audio before it reaches `fingerprint()`.
  That step runs on the host CPU and is not part of the core algorithm being accelerated.

---

## Branch Comparison

| Branch | Hash length | Dependencies | Peak detection |
|---|---|---|---|
| `main` (this branch) | 40 chars (full SHA-1) | None — standard C++ only | Custom neighbourhood search |
| `theEastBlue` | 20 chars (truncated) | Equivalent to OpenCV + Boost output | Same algorithm |

See the `theEastBlue` branch for the version whose output matches the original
`theEastBlue/audio-fingerprinting-acceleration` implementation exactly.

---

## Reference

- Original implementation: [theEastBlue/audio-fingerprinting-acceleration](https://github.com/theEastBlue/audio-fingerprinting-acceleration)
- Algorithm based on: [Dejavu audio fingerprinting](https://github.com/worldveil/dejavu)
- Platform: Xilinx Vitis HLS 2024.1, Windows 11
