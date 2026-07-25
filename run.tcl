open_project -reset fingerprint_prj
set_top fingerprint
add_files fingerprint-no-opencv-boost.cpp
add_files -tb fingerprint-no-opencv-boost.cpp

open_solution -reset "solution1"
set_part {xc7a35tcsg324-1}
create_clock -period 10 -name default

csim_design
exit
