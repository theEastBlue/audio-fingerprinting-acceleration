# compute_spectrogram HLS Kernel — Results

**By Netik Maheshwar**  
Target device: Artix-7 xc7a35tcsg324-1 | Clock: 100 MHz (10 ns)  
Audio: test.wav — 140,561 samples @ 22,050 Hz mono → 67 windows of 4,096 samples

---

## Files

| File | Purpose |
|---|---|
| `compute_spectrogram_kernel.h` | Kernel declaration |
| `compute_spectrogram_kernel.cpp` | Kernel **with** HLS directives (PIPELINE, ARRAY_PARTITION, m_axi) |
| `compute_spectrogram_kernel_no_pragma.cpp` | Kernel **without** HLS directives (baseline, same logic) |
| `compute_spectrogram_tb.cpp` | Testbench — loads test.wav, compares kernel vs reference |
| `kernel_run_compare.tcl` | TCL script — runs csim + csynth for both versions |

---

## C Simulation (csim) — PASS (both versions)

```
Loaded 140561 samples from test.wav
Windows: 67   hann_energy: 1535.62

max_err = 0.000000 dB   failures = 0 / 137283 bins
CSIM PASS: kernel matches reference for all bins
```

Both versions are bit-exact against the reference `compute_spectrogram()` in `preprocessing.cpp`
across all 137,283 (freq × window) bins.

---

## C Synthesis (csynth) — Comparison

### Timing

| | With Directives | Without Directives |
|---|---|---|
| Target clock | 10.00 ns | 10.00 ns |
| Estimated clock | **7.30 ns** | **7.29 ns** |
| Est. Fmax | ~137 MHz | ~137 MHz |

### Resource Utilization

| Resource | With Directives | Without Directives | Delta |
|---|---|---|---|
| LUT | 13,248 / 20,800 **(63%)** | 10,140 / 20,800 **(48%)** | +3,108 |
| FF | 9,988 / 41,600 **(24%)** | 7,563 / 41,600 **(18%)** | +2,425 |
| DSP | 64 / 90 **(71%)** | 64 / 90 **(71%)** | — |
| BRAM_18K | 20 / 100 **(20%)** | 16 / 100 **(16%)** | +4 |

### Loop Performance (per window)

| Loop | With Directives | Without Directives |
|---|---|---|
| LOAD (4,096 samples) | **4,099 cycles, II=1** | 4,098 cycles, II=1 |
| BIT_REVERSE | ~20,480 cycles | ~20,480 cycles |
| FFT BUTTERFLY (12 stages) | **516,132 cycles, II=21** | 516,132 cycles, II=21 |
| PSD (2,049 bins) | **2,103 cycles, II=1** | 2,098 cycles, II=1 |
| **Total / window** | **~543,000 cycles = 5.43 ms** | **~543,000 cycles = 5.43 ms** |

---

## Acceleration vs CPU Software

Software profiling (same audio, Intel x86 CPU):

```
FFT + Spectrogram: 3,837 ms  (for 67 windows)
```

HLS kernel estimate (67 windows @ 100 MHz, compute only):

```
With directives : ~364 ms
Without directives: ~364 ms
```

| | Speedup |
|---|---|
| With HLS directives vs software | **~10.5×** |
| Without HLS directives vs software | **~10.5×** |
| With vs Without (pragma benefit on latency) | **~1.0×** |

---

## Key Finding

**The HLS directives do not reduce latency in this kernel.**

The FFT butterfly accounts for **95% of total cycles** in both versions.
The bottleneck is the **loop-carried twiddle recurrence**:

```cpp
float nwr = cwr * wr - cwi * wi;   // depends on previous cwr, cwi
float nwi = cwr * wi + cwi * wr;
cwr = nwr; cwi = nwi;              // next iteration needs these
```

This dependency forces the butterfly's achieved II to **21 cycles** regardless
of whether `#pragma HLS PIPELINE` is present. HLS automatically pipelines the
loop body in both cases because it detects the simple arithmetic structure.

**What the directives DO provide (critical for board deployment):**

| Directive | Effect |
|---|---|
| `m_axi` with burst | AXI4 burst transfers from DDR — without this, random-access DDR latency would eliminate the 10.5× gain on-board |
| `ARRAY_PARTITION cyclic factor=4` | 4 parallel BRAM banks for re/im — removes port-conflict stalls during butterfly reads/writes |
| `s_axilite` | Proper start/done/interrupt handshake for host control |

---

## Next Optimisation Target

Replace the iterative twiddle recurrence with a **precomputed twiddle LUT**:

```cpp
// Instead of: cwr = cwr*wr - cwi*wi (loop-carried dep)
// Use:        twiddle = cos_table[k], sin_table[k]  (no dependency)
```

This would break the loop-carried dependency and allow II=1 on the butterfly,
reducing FFT from **516,132 cycles to ~25,000 cycles per window** — a further
**~20× speedup** on top of the current 10.5×, for a combined **~210× speedup**
over software.
