#ifndef COMPUTE_SPECTROGRAM_KERNEL_H
#define COMPUTE_SPECTROGRAM_KERNEL_H

#include "fingerprint.h"

void compute_spectrogram_kernel(
    float windows[MAX_WINDOWS][WINDOW_SIZE],
    int   num_windows,
    float hann_energy,
    float spec_power[MAX_FREQ][MAX_WINDOWS]
);

#endif
