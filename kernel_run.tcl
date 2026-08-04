# Vitis HLS script for compute_spectrogram_kernel
# By Netik Maheshwar

open_project -reset spectrogram_kernel_prj
set_top compute_spectrogram_kernel
add_files compute_spectrogram_kernel.cpp
add_files -tb compute_spectrogram_tb.cpp -cflags "-I."

open_solution -reset "solution1"
set_part {xc7a35tcsg324-1}
create_clock -period 10 -name default

# Copy raw_data into csim build directory
file mkdir spectrogram_kernel_prj/solution1/csim/build
file copy -force raw_data spectrogram_kernel_prj/solution1/csim/build/raw_data

# C Simulation – verifies kernel correctness against reference implementation
csim_design

# High-Level Synthesis – produces timing and resource report
csynth_design

exit
