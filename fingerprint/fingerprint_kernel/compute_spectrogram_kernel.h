#ifndef COMPUTE_SPECTROGRAM_KERNEL_H
#define COMPUTE_SPECTROGRAM_KERNEL_H

#include "fingerprint.h"

// HLS kernel: FFT + power spectral density over all frames.
// Input windows must already have the Hann window applied (via apply_hann).
void compute_spectrogram_kernel(
    float windows[MAX_WINDOWS][WINDOW_SIZE],   // pre-Hann-windowed frames
    int   num_windows,
    float hann_energy,                          // precomputed: sum of hann[i]^2
    float spec_power[MAX_FREQ][MAX_WINDOWS]    // output dB PSD
);

#endif
