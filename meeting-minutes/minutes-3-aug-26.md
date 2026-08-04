# Meeting Minutes
## Meeting Information
**Meeting Date/Time:** 03 Aug 2026/4:00-6:00pm
**Meeting Purpose:** discuss project blockers with Tony
**Meeting Location:** Brass lab
**Note Taker:** Esha  

## Attendees
People who attended:
- Venus
- Esha

## Agenda Items

- Discuss what "future improvements" might look like for our project
- Double check that algorithmic changes are fine
- show acceleration of detect_peaks
- Debug the 99 peaks issue on the board
- Discuss metrics like overheads, timing analysis for our implementations, WNS and strategise for timing violations

## Discussion Notes
- Cosim fails on CSE lab machines due to RAM limits
  - Attempted: truncating audio input to 1 sec; still ran out of RAM
  - Tony's suggestion: reduce additional parameters (window size/overlap) to fit cosim within available memory
  - Follow-up: check for timing violations once cosim runs successfully

## Progress

### Kernel Development (Esha, Venus)
- Dependencies (OpenCV/Boost) and dynamic memory allocations removed from reference implementation; output verified against original via test script; best-performing detect_peaks variant selected (initial implementation used 950 BRAMs, which is well over the 144 BRAM limit, so we had to switch out that implementation); code split into host/kernel for board execution; HLS testbench written; kernel accelerated (passes csim, C synthesis); running on Kria board. FFT (fft_radix) verified by Venus to match lab reference implementation.

### Csim results (Netik, Rukhsaar)
- Ran csim on a separate dependency-free version of the reference implementation (dynamic memory allocation not yet removed). No board-level implementation yet; board execution reported as not functioning.

## Action Items
| Done? | Item | Responsible | Due Date |
| ---- | ---- | ---- | ---- |
|done| profiling attempt at max-filter | Esha | 12 Jul |
|done| profiling attempt at get-2D-peak | Venus | 12 Jul |
|partial| verifying correctness of the algorithmic outputs from the reference implementation (hls test bench written in python?) | Rukhsaar | 12 Jul |
|partial| running fingerprint.cpp on kria PS (fingerprint.cpp has not been run on kria PS) | Netik	| 12 Jul |
| In progress | Verify inputs passed to detect_peaks; debug 99-peaks discrepancy | Esha, Venus | 4 Aug |
| | Fix timing violations, further accelerate detect_peaks (post-verification) | Esha, Venus | 5 Aug |
| | Integrate progress into report and slides | Esha, Venus | 7 Aug |
| | Attempt on the board and sketch FFT + spectrogram kernel fusion plan (architecture/interface diagram, expected overhead saving) for future improvements section | Netik, Rukhsaar | 8 Aug |
| | Draft background / related work section | Rukhsaar | 8 Aug |
