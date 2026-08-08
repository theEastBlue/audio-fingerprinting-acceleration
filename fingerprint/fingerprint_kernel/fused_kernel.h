#ifndef FUSED_KERNEL_H
#define FUSED_KERNEL_H

#include "fingerprint.h"

// Fused HLS kernel: spectrogram (Hann-windowed FFT + PSD, LUT-based twiddle
// factors) and peak detection in a single kernel invocation.
//
// spec_power is a device-side DDR scratch buffer (its own m_axi bundle),
// not on-chip BRAM: the full spectrogram (2049 x 100 floats, ~800KB) does
// not fit in the ~100 BRAM18K blocks available on xc7a35t no matter which
// axis it's buffered on (see fused_kernel_results.md for the analysis).
// Crucially, the HOST never touches this buffer -- it's allocated once as
// scratch device memory and never sync'd to/from the host, so this still
// removes the thing that mattered: only ONE kernel launch (one XRT enqueue)
// instead of two, and no host-visible round trip for the spectrogram. The
// DDR traffic that remains is internal to the single kernel invocation.
void fingerprint_kernel(
    float windows[MAX_WINDOWS][WINDOW_SIZE],   // pre-Hann-windowed frames
    int   num_windows,
    float hann_energy,                          // precomputed: sum of hann[i]^2
    float spec_power[MAX_FREQ][MAX_WINDOWS],    // device-side scratch, host never touches
    int   peak_freq[MAX_PEAKS],
    int   peak_time[MAX_PEAKS],
    int*  peak_count
);

#endif
