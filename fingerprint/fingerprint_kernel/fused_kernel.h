#ifndef FUSED_KERNEL_H
#define FUSED_KERNEL_H

#include "fingerprint.h"

// Fused HLS kernel: spectrogram (Hann-windowed FFT + PSD, LUT-based twiddle
// factors) and peak detection in a single kernel invocation. spec_power is
// kept in on-chip memory and never crosses the m_axi boundary, removing the
// second DDR round-trip that existed when compute_spectrogram and
// detect_peaks were separate top-level kernels.
void fingerprint_kernel(
    float windows[MAX_WINDOWS][WINDOW_SIZE],   // pre-Hann-windowed frames
    int   num_windows,
    float hann_energy,                          // precomputed: sum of hann[i]^2
    int   peak_freq[MAX_PEAKS],
    int   peak_time[MAX_PEAKS],
    int*  peak_count
);

#endif
