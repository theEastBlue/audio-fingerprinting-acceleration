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
with a single `s_axilite` control bundle and three `m_axi` bundles: `gmem0`
(windows in), `gmem1` (peaks out), `gmem2` (`spec_power` scratch — see "BRAM
fix" below for why this is a third `m_axi` port rather than an on-chip array).

## CSIM — functional correctness

```
Found 43 peaks.
RMSE(Freq) RMSE(Time) = 0.000000000000000 0.000000000000000
gold peaks: 43   fused kernel peaks: 43
CSIM PASS: fused kernel matches golden peaks
```

Bit-exact match against `peaks.gold.dat` — zero error, exact peak count. The
merge did not change the algorithm's output, only where the intermediate
spectrogram lives, and that held true both before and after the BRAM fix
below (re-verified, not assumed).

## CSYNTH round 1 — the real finding: it doesn't fit the target device

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

**Why a rolling on-chip buffer doesn't work here, either:** the obvious fix is
to keep only a 41-row sliding window on-chip, matching `detect_peaks`'s
original approach, instead of the full spectrogram. That doesn't actually
work for the fused kernel: the FFT naturally produces one full-frequency
*column* per time-window (all 2049 bins for window `w`, at once), while peak
detection needs 41-row-tall *frequency* slices across **all** time-windows
before it can evaluate a single row. Producing row `f=0` requires every
window's FFT to already be done — so *some* buffer has to hold the full
spectrogram somewhere before frequency-major peak detection can start,
regardless of which axis it's sliced on. Sliding on time instead of frequency
was checked too (41 columns x 2049 rows ≈ 328KB at float32, ~164KB even at
int16) — still doesn't fit in the ~72 BRAM18K blocks left after the FFT
stage's own 24 and the AXI interfaces' 4. Quantizing further (int8) would
just about fit but risks silently breaking the bit-exact match just proven
above, for a gain that only helps if the goal is specifically on-chip storage.

## BRAM fix — verified, not just proposed

The actual fix: stop trying to fit the full spectrogram on-chip at all.
`spec_power` is now a **device-side DDR scratch buffer** — its own `m_axi`
bundle (`gmem2`), exactly like `windows` and the peak outputs, but the host
never allocates a *host*-visible copy for it and never issues an
`xrt::bo::sync()` for it. It's DRAM that only the kernel invocation touches.
This still delivers the thing that actually mattered — **one** kernel launch
instead of two, and the spectrogram never round-trips through the host —
without needing it to physically fit in BRAM.

Re-ran `csim_design` + `csynth_design` after the change:

```
CSIM PASS: fused kernel matches golden peaks   (43/43 peaks, RMSE 0.0 — unchanged)
Estimated Fmax: 136.99 MHz                      (unchanged)
```

| Resource | Before fix | After fix | Available |
|---|---|---|---|
| BRAM | 561 (561%) ⚠️ | **53 (53%)** ✅ | 100 |
| DSP | 45 (50%) | 45 (50%) | 90 |
| FF | 10,301 (24%) | 11,527 (27%) | 41,600 |
| LUT | 11,619 (55%) | 12,939 (62%) | 20,800 |

`spec_power` no longer appears in the Storage Report at all (it's an AXI
interface now, not an on-chip RAM); `gmem2` costs 4 BRAM blocks for its AXI
FIFOs, same as `gmem0`/`gmem1`. The small increase in FF/LUT (24%→27%,
55%→62%) is the extra AXI master logic for the third interface — expected,
and still well within budget. **The fused kernel now fits the target device.**

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

Step 2+3 (an 800KB round trip through *host-visible* buffers, plus a second
`xrt::kernel` launch's fixed control/interrupt overhead) is what fusion
removes. With the fused kernel, `spec_power` is device-side DRAM that the
host allocates once (e.g. via `xrt::bo` with a device-only flag) and never
syncs — steps 2 and 3 above simply don't happen from the host's perspective,
and there's one `xrt::kernel` launch instead of two. The DDR traffic for
`spec_power` still happens, just entirely inside the single kernel invocation
rather than crossing back out to host memory between two launches. This is a
real, structural savings; it just can't be turned into a measured millisecond
figure without XRT/board access. `hw_emu` (Vitis hardware emulation) — no
board needed — would be the next step to get a real, simulated transfer-time
number in place of this analytical argument.

## Bottom line

- **Fusion is functionally correct** (CSIM PASS, bit-exact against gold
  peaks, re-verified after the fix) and the accelerated FFT carries over
  unchanged (II=2, 136.99MHz).
- **Fusion now fits the target FPGA**: BRAM 561% → 53%, all other resources
  well within budget (DSP 50%, FF 27%, LUT 62%) — both measured via real
  `csynth_design` runs, not estimated.
- Remaining gaps, in order of what's next: (1) no `cosim`/`hw_emu` run yet,
  so timing is still HLS-estimated rather than RTL-cycle-accurate or
  XRT-measured; (2) `kernel_run_fused.tcl` stops after `csynth_design` — no
  `export_design`, so no `.xclbin`/`finger.bin` has been built; (3) `host.cpp`
  has not been updated to call this fused kernel instead of the two old ones;
  (4) no board run, by design (none available). None of these are blocked on
  anything unresolved anymore — the architectural question (does it fit) is
  answered.
