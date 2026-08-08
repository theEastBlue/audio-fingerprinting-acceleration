# kernel_run_fused.tcl
# Runs csim_design + csynth_design for the FUSED kernel: spectrogram
# (LUT-FFT, II=2) + peak detection as a single top-level function with a
# single m_axi/s_axilite interface.
#
# Run from the repository root:
#   C:\Xilinx\Vitis_HLS\2024.1\bin\vitis_hls.bat -f fingerprint/fingerprint_kernel/kernel_run_fused.tcl
#
# By Netik Maheshwar

set KDIR "fingerprint/fingerprint_kernel"
set PART  "xc7a35tcsg324-1"

open_project -reset fused_prj
set_top fingerprint_kernel
add_files $KDIR/fused_kernel.cpp             -cflags "-I $KDIR"
add_files -tb $KDIR/fused_kernel_tb.cpp      -cflags "-I $KDIR"
add_files -tb $KDIR/preprocessing.cpp        -cflags "-I $KDIR"

open_solution -reset "solution1"
set_part $PART
create_clock -period 10 -name default

file mkdir fused_prj/solution1/csim/build
file copy -force $KDIR/test.wav        fused_prj/solution1/csim/build/test.wav
file copy -force $KDIR/peaks.gold.dat  fused_prj/solution1/csim/build/peaks.gold.dat
catch { csim_design }
csynth_design

exit
