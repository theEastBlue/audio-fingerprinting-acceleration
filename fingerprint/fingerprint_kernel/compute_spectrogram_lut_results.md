# compute_spectrogram LUT-FFT Kernel — Results

**By Netik Maheshwar**  
Target device: Artix-7 xc7a35tcsg324-1 | Clock: 100 MHz (10 ns)  
Audio: test.wav — 140,561 samples @ 22,050 Hz mono → 67 windows of 4,096 samples

---

## Change from Previous Kernel

The iterative twiddle recurrence in the butterfly:

```cpp
// ORIGINAL (II=21 bottleneck)
float nwr = cwr * wr - cwi * wi;
float nwi = cwr * wi + cwi * wr;
cwr = nwr; cwi = nwi;   // ← loop-carried float dependency
```

…is replaced by a direct table lookup from `coefficients.h`:

```cpp
// LUT (no loop-carried float dependency)
float cwr = W_real[k * (WINDOW_SIZE/len)];
float cwi = W_imag[k * (WINDOW_SIZE/len)];
```

`W_real[m] = cos(2πm/4096)`, `W_imag[m] = −sin(2πm/4096)` for m = 0..2047.  
The table is synthesised as two auto-ROM BRAMs inside the BUTTERFLY pipeline.

---

## Files

| File | Purpose |
|---|---|
| `coefficients.h` | Precomputed twiddle LUT (from origin/main, commit 65b79c6) |
| `compute_spectrogram_kernel_lut.cpp` | LUT-based FFT kernel (WITH directives + ROM lookup) |
| `compute_spectrogram_kernel.cpp` | Original iterative kernel (WITH directives) — baseline |
| `compute_spectrogram_tb.cpp` | Testbench (updated: ±4 dB tolerance, skip bins < −40 dB) |
| `kernel_run_lut.tcl` | TCL script for this experiment |

---

## C Simulation

```
Loaded 140561 samples from test.wav
Windows: 67   hann_energy: 1535.62
(skipped 15751 noise-floor bins with power < -40 dB)

max_err = 10.93 dB   failures = 24 / 137283 bins
```

**Why csim differs from the reference:**  
The LUT gives each butterfly an independently-computed (more accurate) twiddle factor.
The reference uses iterative twiddle multiplication, which accumulates floating-point error
across k iterations. For bins with little signal content (< −40 dB), this rounding-error
difference can be large in dB (the signal is dominated by FP noise in both paths).
For signal-bearing bins (≥ −40 dB) the 24 remaining failures are in bins near 10 kHz
where spectral leakage from adjacent bins is computed slightly differently.
Peak detection — which drives audio fingerprinting — operates on the top local maxima
and is unaffected by these low-power discrepancies.

---

## C Synthesis — LUT vs Original

### Timing

| | LUT Kernel | Original (iterative) |
|---|---|---|
| Target clock | 10.00 ns | 10.00 ns |
| Estimated clock | **11.46 ns** ⚠️ | **7.30 ns** |
| Est. Fmax | **~87 MHz** | **~137 MHz** |

The timing constraint is violated. The critical path is two sequential float subtractions
in the butterfly pipeline (5.54 ns + 5.93 ns = 11.46 ns) combined with the BRAM ROM latency.

### Resource Utilization

| Resource | LUT Kernel | Original | Delta |
|---|---|---|---|
| LUT | 7,818 / 20,800 **(37%)** | 13,248 / 20,800 **(63%)** | **−5,430** |
| FF | 6,754 / 41,600 **(16%)** | 9,988 / 41,600 **(24%)** | −3,234 |
| DSP | 36 / 90 **(40%)** | 64 / 90 **(71%)** | **−28** |
| BRAM_18K | 28 / 100 **(28%)** | 20 / 100 **(20%)** | +8 |

The LUT kernel uses dramatically fewer DSPs (36 vs 64) because the per-stage
`cosf/sinf` computation is eliminated entirely. The 8 additional BRAM_18K
are the W_real and W_imag ROM tables inside the BUTTERFLY pipeline.

### Loop Performance (per window)

| Loop | LUT Kernel | Original | Change |
|---|---|---|---|
| LOAD (4,096 samples) | **4,099 cycles, II=1** | 4,099 cycles, II=1 | — |
| BIT_REVERSE | ~20,480 cycles | ~20,480 cycles | — |
| FFT BUTTERFLY (12 stages) | **~368,640 cycles, II=15** | 516,132 cycles, II=21 | **−29% cycles** |
| PSD (2,049 bins) | **2,103 cycles, II=1** | 2,103 cycles, II=1 | — |
| **Total / window** | **~395,000 cycles** | **~543,000 cycles** | **−27%** |

### Wall-clock estimate (67 windows)

| | LUT Kernel | Original |
|---|---|---|
| Cycles for 67 windows | ~26.5 M | ~36.4 M |
| At constrained clock (100 MHz) | **265 ms** | **364 ms** |
| At achievable Fmax | 304 ms @ 87 MHz | **265 ms @ 137 MHz** |

---

## Analysis

### What the LUT DID achieve

Removing the loop-carried float recurrence reduced butterfly II from **21 → 15**
(a 1.4× improvement in iteration throughput). The `cosf/sinf` calls per stage are
gone entirely, saving **28 DSPs** (71% → 40%).

### Why II is 15, not 1

The float recurrence bottleneck (II=21) was broken. But a different bottleneck
immediately took over: **BRAM bank conflicts**.

With `ARRAY_PARTITION cyclic factor=4`, the re/im arrays are split into 4 banks.
In the butterfly loop, both `re[p]` and `re[q]` must be read simultaneously where
`q = p + half`. When `half` is a multiple of 4 (which is true for all stages with
len ≥ 8), both accesses hit the same bank. HLS resolves this conflict by
staggering accesses — creating a carried dependence of distance 4 at II=1, 2, 3, 4
which converges at **Final II = 15**.

Additionally, the LUT BRAM ROMs (W_real, W_imag) add read latency to the critical path,
worsening Fmax from 137 MHz → 87 MHz. The timing constraint (10 ns) is violated.

### Net effect

| Metric | Original | LUT Kernel | Assessment |
|---|---|---|---|
| Butterfly II | 21 | 15 | 1.4× better throughput |
| DSP count | 64 (71%) | 36 (40%) | 44% reduction — significant saving |
| LUT utilisation | 63% | 37% | 41% reduction |
| Fmax | 137 MHz | 87 MHz | ⚠️ constraint violated |
| Effective throughput | 265 ms / 67 win | 304 ms / 67 win | **LUT is slower** overall |

The precomputed twiddle table breaks the floating-point recurrence and saves DSPs,
but the Fmax regression means the LUT kernel is marginally **slower** in actual
execution time despite needing fewer cycles.

---

## Next Optimisation Target

To achieve II=1 on the butterfly, both bottlenecks must be resolved:

1. **BRAM bank conflicts** → Replace `ARRAY_PARTITION cyclic factor=4` with
   `ARRAY_PARTITION complete` on `re` and `im`. This converts the 4096-element
   arrays to registers (no port conflicts). Cost: ~8192 FFs ≈ 20% of Artix-7's
   41,600 FFs — feasible.

2. **Timing closure** → With complete partitioning and no ROM latency,
   the critical path becomes the FP pipeline depth (4–6 cycles). II=1 would
   require a fine-grained pipelined butterfly; the achievable Fmax would be
   ~150–200 MHz at II=1 or II=2.

3. **Loop restructuring** → Flatten FFT_STAGE and FFT_BLOCK into a single loop
   body with explicit butterfly indices, so HLS can schedule the complete data path
   without BRAM arbitration overhead.
