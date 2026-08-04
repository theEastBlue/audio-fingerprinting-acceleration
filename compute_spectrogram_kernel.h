#ifndef COMPUTE_SPECTROGRAM_KERNEL_H
#define COMPUTE_SPECTROGRAM_KERNEL_H

#define WINDOW_SIZE 4096
#define MAX_FREQ    2049    // WINDOW_SIZE/2 + 1
#define PI_F        3.14159265358979323846f
#define FS_K        22050.0f

void compute_spectrogram_kernel(
    float samples_in[WINDOW_SIZE],
    float hann[WINDOW_SIZE],
    float hann_energy,
    float spec_out[MAX_FREQ]
);

#endif
