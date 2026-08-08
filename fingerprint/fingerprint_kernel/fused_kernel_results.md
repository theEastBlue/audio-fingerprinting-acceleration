# Fused Kernel (spectrogram + peak detection) — Results

**By Netik Maheshwar**
Target device: Artix-7 xc7a35tcsg324-1 | Clock: 100 MHz (10 ns)
Audio: test.wav — 140,561 samples @ 22,050 Hz mono → 67 windows of 4,096 samples
No board access for this pass — all numbers below are from a real, local run of
`csim_design` + `csynth_design` (Vitis HLS 2024.1), not hand estimates.

---

## What changed

`compute_spectrogram_kernel_lut.cpp` (accelerated FFT, LUT twiddle + `DEPENDENCE
false`, II=2) and `fingerprint_kernel.cpp`'s `detect_peaks` were two separate
top-level kernels, each with its own `m_axi`/`s_axilite` interface. That meant
two kernel launches from the host and one full round-trip of the spectrogram
(`spec_power`, ~800KB) out to DDR and back in.

`fused_kernel.cpp` merges them into **one** top-level function, `fingerprint_kernel`,
with a single `m_axi` bundle pair (`gmem0` in: windows, `gmem1` out: peaks) and a
single `s_axilite` control bundle. `spec_power` is a local on-chip array shared
between the two internal stages (`compute_spectrogram_stage`,
`detect_peaks_stage`) — it never appears on the kernel's argument list, so it
never crosses the AXI boundary.

## CSIM — functional correctness

```
CSIM PASS: fused kernel matches golden peaks
```

The fused kernel's peak output matches `peaks.gold.dat` (RMSE(freq) and
RMSE(time) both within tolerance, peak counts equal) — the merge did not change
the algorithm's output, only where the intermediate spectrogram lives.

## CSYNTH — the real finding: it doesn't fit the target device

```
INFO: [HLS 200-790] **** Loop Constraint Status: All loop constraints were NOT satisfied.
INFO: [HLS 200-789] **** Estimated Fmax: 136.99 MHz
```

Top-level resource utilization (`fused_prj/solution1/syn/report/csynth.rpt`):

| Resource | Used | Available | % |
|---|---|---|---|
| BRAM | 561 | 100 | **561%** ⚠️ |
| DSP | 45 | 90 | 50% |
| FF | 10,301 | 41,600 | 24% |
| LUT | 11,619 | 20,800 | 55% |

**BRAM overflows by 5.6x.** Storage report breakdown:

| Buffer | BRAM blocks | Note |
|---|---|---|
| `spec_power` (the fused intermediate array) | **512** | `2049 x 100 x 4 bytes` ≈ 800KB — the whole spectrogram, held on-chip |
| `compute_spectrogram_stage` (FFT working buffers + twiddle ROMs) | 24 | unchanged from the standalone LUT kernel |
| `detect_peaks_stage` (line_buf/window/new_row) | 21 | unchanged from the standalone kernel — this one is small by design |
| `gmem0`/`gmem1` interfaces | 4 | AXI FIFOs |

This is exactly the risk flagged before writing any code: the target part
(xc7a35t) has ~100 BRAM18K blocks (~225KB), and a plain on-chip `spec_power`
buffer needs ~800KB. **Storing the whole spectrogram on-chip, as implemented
here, cannot be placed on this device.** `detect_peaks`'s own original on-chip
buffer (`line_buf`, 41 rows x 100 windows = 21 blocks) is small precisely
because it only ever holds a 41-row *slice* of the frequency axis, not the
whole thing — the fused kernel's naive `spec_power[MAX_FREQ][MAX_WINDOWS]`
buffer throws that efficiency away by materializing everything at once.

**Recommended fix (not yet implemented):** restructure so only a rolling
41-row line buffer is kept on-chip, matching `detect_peaks`'s original
approach, instead of the full spectrogram. This is nontrivial because the FFT
naturally produces one full-frequency *column* per time-window, while peak
detection wants 41-row-tall *frequency* slices across all time-windows —
fusing them without the full buffer means restructuring the peak-detection
scan to consume data in the FFT's native production order, or accepting a
transposed on-chip buffer (41 time-windows x 2049 freq bins ≈ 328KB — still
over budget, but closer) and quantizing `spec_power` to a smaller type
(e.g. int16 fixed-point, halving it to ~400KB / ~164KB for the transposed
version) to close the remaining gap. Whichever route is chosen, it should be
re-verified with another `csim`/`csynth` pass before being called done.

## What still holds from the standalone LUT kernel work

The accelerated FFT itself synthesizes identically inside the fused design —
this confirms the fusion wrapper didn't regress the earlier optimization:

| | Standalone LUT kernel | Fused kernel |
|---|---|---|
| BUTTERFLY achieved II | 2 | **2** (confirmed, `compute_spectrogram_stage_Pipeline_BUTTERFLY_csynth.rpt`) |
| Estimated Fmax | 137 MHz | **136.99 MHz** |
| DSP (spectrogram stage only) | 40/90 | 40/90 |

So the per-FFT throughput math from `compute_spectrogram_lut_results.md`
(~174,000 cycles/window, ~117ms for all 67 windows at 100MHz, ~33x vs
software) still applies to the FFT stage itself — that part of the fusion
work is solid. What's new and real in this run is the discovery that the
*fusion strategy* (whole spectrogram on-chip) doesn't fit, which is
independent of whether the FFT itself is fast.

One report artifact worth flagging: the raw `csynth.rpt` "Performance
Estimates" table shows `FFT_STAGE` latency as 101,277,936 cycles — this is
**not** a usable number. It comes from Vitis applying the worst-case
per-invocation latency (4121 cycles, the `len=4096`/`half=2048` case) across
all 2048 `FFT_BLOCK` iterations uniformly (`2048 x 4121 = 8,439,808`, matching
the report's `FFT_BLOCK` row exactly), even though most stages have far
smaller `half`. The correctly-reasoned per-window cycle estimate remains the
one already derived in `compute_spectrogram_lut_results.md` (~174,000
cycles/window), not this table.

## Overhead removed by fusion (analytical — no XRT/board run possible)

With two separate kernels, the host does:
1. Write `windows` to device DDR, launch `compute_spectrogram_kernel`.
2. Sync `spec_power` (~800KB) back from device DDR to host.
3. Sync `spec_power` back out to device DDR, launch `detect_peaks`.
4. Sync peak results back.

Step 2+3 (an 800KB round trip through host-visible buffers, plus a second
`xrt::kernel` launch's fixed control/interrupt overhead) is what fusion
removes — with one kernel, `spec_power` never leaves the chip, so steps 2 and
3 simply don't happen, and there's one launch instead of two. This is a real,
structural savings; it just can't be turned into a measured millisecond
figure without XRT/board access. Once the BRAM issue above is fixed and the
design fits, `hw_emu` (Vitis hardware emulation) would be the next
measurement step — it doesn't need a board and would give a real, simulated
transfer-time number to replace this analytical argument.

## Bottom line

- **Fusion is functionally correct** (CSIM PASS against gold peaks) and the
  accelerated FFT carries over unchanged (II=2, 137MHz).
- **Fusion as implemented does not fit the target FPGA** (BRAM 561%) — this is
  a real, measured result, not a guess, and is the actual next blocker.
- Next step is the on-chip buffer restructuring described above, re-verified
  with another `csim`/`csynth` run, before this can be considered
  board-ready.
