# kernel_run_lut.tcl
# Runs csim_design + csynth_design for the LUT-based FFT kernel.
#
# The twiddle recurrence (II=21) is replaced by a direct lookup into the
# precomputed W_real/W_imag tables from coefficients.h, targeting II=1
# on the butterfly loop.
#
# Run from the repository root:
#   C:\Xilinx\Vitis_HLS\2024.1\bin\vitis_hls.bat -f fingerprint/fingerprint_kernel/kernel_run_lut.tcl
#
# By Netik Maheshwar

set KDIR "fingerprint/fingerprint_kernel"
set PART  "xc7a35tcsg324-1"

open_project -reset lut_prj
set_top compute_spectrogram_kernel
add_files $KDIR/compute_spectrogram_kernel_lut.cpp -cflags "-I $KDIR"
add_files -tb $KDIR/compute_spectrogram_tb.cpp     -cflags "-I $KDIR"
add_files -tb $KDIR/preprocessing.cpp              -cflags "-I $KDIR"

open_solution -reset "solution1"
set_part $PART
create_clock -period 10 -name default

file mkdir lut_prj/solution1/csim/build
file copy -force $KDIR/test.wav lut_prj/solution1/csim/build/test.wav
catch { csim_design }
csynth_design

exit
