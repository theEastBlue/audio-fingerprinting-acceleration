# kernel_run.tcl
# Vitis HLS script: csim + csynth for compute_spectrogram_kernel
# Run from fingerprint_hls root:
#   C:\Xilinx\Vitis_HLS\2024.1\bin\vitis_hls.bat -f fingerprint/fingerprint_kernel/kernel_run.tcl
#
# By Netik Maheshwar

open_project -reset compute_spectrogram_hls_prj
set_top compute_spectrogram_kernel

# Kernel source
add_files fingerprint/fingerprint_kernel/compute_spectrogram_kernel.cpp \
    -cflags "-I fingerprint/fingerprint_kernel"

# Testbench + reference implementation (preprocessing.cpp provides the reference)
add_files -tb fingerprint/fingerprint_kernel/compute_spectrogram_tb.cpp \
    -cflags "-I fingerprint/fingerprint_kernel"
add_files -tb fingerprint/fingerprint_kernel/preprocessing.cpp \
    -cflags "-I fingerprint/fingerprint_kernel"

open_solution -reset "solution1"
set_part {xc7a35tcsg324-1}
create_clock -period 10 -name default

# Copy test.wav into the csim build directory
file mkdir compute_spectrogram_hls_prj/solution1/csim/build
file copy -force fingerprint/fingerprint_kernel/test.wav \
    compute_spectrogram_hls_prj/solution1/csim/build/test.wav

csim_design
csynth_design
exit
