# kernel_run_fused_kv260.tcl
# Same as kernel_run_fused.tcl, but targets the REAL board part instead of
# the xc7a35t placeholder used elsewhere in this repo's kernel_run*.tcl
# scripts. Per meeting-minutes/minutes-27-Jul-26.md and minutes-3-aug-26.md,
# the team's actual hardware is a Kria board (PetaLinux + xmutil in
# board.sh), i.e. a Zynq UltraScale+ K26 SOM -- xc7a35t has no ARM PS and
# cannot run PetaLinux at all, so it was never a real deployment target,
# only a convenient stand-in part for early HLS resource estimates.
#
# Run from the repository root:
#   C:\Xilinx\Vitis_HLS\2024.1\bin\vitis_hls.bat -f fingerprint/fingerprint_kernel/kernel_run_fused_kv260.tcl
#
# By Netik Maheshwar

set KDIR "fingerprint/fingerprint_kernel"
set PART  "xck26-sfvc784-2LV-c"

open_project -reset fused_kv260_prj
set_top fingerprint_kernel
add_files $KDIR/fused_kernel.cpp             -cflags "-I $KDIR"
add_files -tb $KDIR/fused_kernel_tb.cpp      -cflags "-I $KDIR"
add_files -tb $KDIR/preprocessing.cpp        -cflags "-I $KDIR"

open_solution -reset "solution1"
set_part $PART
create_clock -period 10 -name default

file mkdir fused_kv260_prj/solution1/csim/build
file copy -force $KDIR/test.wav        fused_kv260_prj/solution1/csim/build/test.wav
file copy -force $KDIR/peaks.gold.dat  fused_kv260_prj/solution1/csim/build/peaks.gold.dat
catch { csim_design }
csynth_design

exit
