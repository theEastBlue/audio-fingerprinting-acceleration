# kernel_run_compare.tcl
# Runs csim_design + csynth_design for compute_spectrogram_kernel twice,
# using two separate HLS projects so source files never conflict:
#
#   compare_with_prj  --  WITH full HLS directives
#                         (m_axi interfaces, ARRAY_PARTITION, PIPELINE)
#
#   compare_no_prj    --  WITHOUT any HLS directives (baseline)
#                         (LOOP_TRIPCOUNT hints only, for concrete estimates)
#
# Run from the repository root:
#   C:\Xilinx\Vitis_HLS\2024.1\bin\vitis_hls.bat -f fingerprint/fingerprint_kernel/kernel_run_compare.tcl
#
# By Netik Maheshwar

set KDIR "fingerprint/fingerprint_kernel"
set PART  "xc7a35tcsg324-1"

# -----------------------------------------------------------------------
# WITH HLS directives
# -----------------------------------------------------------------------
open_project -reset compare_with_prj
set_top compute_spectrogram_kernel
add_files $KDIR/compute_spectrogram_kernel.cpp      -cflags "-I $KDIR"
add_files -tb $KDIR/compute_spectrogram_tb.cpp      -cflags "-I $KDIR"
add_files -tb $KDIR/preprocessing.cpp               -cflags "-I $KDIR"

open_solution -reset "solution1"
set_part $PART
create_clock -period 10 -name default

file mkdir compare_with_prj/solution1/csim/build
file copy -force $KDIR/test.wav compare_with_prj/solution1/csim/build/test.wav
csim_design
csynth_design

# -----------------------------------------------------------------------
# WITHOUT HLS directives (baseline)
# -----------------------------------------------------------------------
open_project -reset compare_no_prj
set_top compute_spectrogram_kernel
add_files $KDIR/compute_spectrogram_kernel_no_pragma.cpp -cflags "-I $KDIR"
add_files -tb $KDIR/compute_spectrogram_tb.cpp           -cflags "-I $KDIR"
add_files -tb $KDIR/preprocessing.cpp                    -cflags "-I $KDIR"

open_solution -reset "solution1"
set_part $PART
create_clock -period 10 -name default

file mkdir compare_no_prj/solution1/csim/build
file copy -force $KDIR/test.wav compare_no_prj/solution1/csim/build/test.wav
csim_design
csynth_design

exit
