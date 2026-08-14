// Main outer loop flushes spectrogram through line buffer
slide_window: for (int f = 0; f < MAX_FREQ + R; f++) {
      read_row: burst_read_DDR_to_local_row(new_row); // AXI Burst Read

      shift_window: for (int w = 0; w < num_windows + R; w++) {
          #pragma HLS PIPELINE II=1
          
          // 1. Shift 41x41 register window left by 1 column
          //    (ideally in 1 clock cycle)
          shift_register_grid_left();
          
          // 2. Feed new BRAM line-buffer column into right edge of window
          load_new_column_from_line_buf();

          // 3. Parallel Balanced Tree Reduction over 841 points
          DTYPE max_val = find_diamond_max(window);
          
          // 4. Store peak in local BRAM to prevent AXI stall
          if (window[20][20] == max_val && window[20][20] > AMP_MIN) {
              local_peak_buf[pc++] = {f_center, w_center};
          }
      }
  }
  write_axi: burst_write_local_peaks_to_DDR(); // Sequential AXI Burst Output


#include <ap_fixed.h>
#include <cstdint>
#include "fingerprint.h"

// --------------------------------------------------------
// DATATYPES & CONSTANTS
// --------------------------------------------------------
constexpr int R = 20;
constexpr int MASK_SIZE = 2 * R + 1;          // 41
constexpr int NUM_LINE_BUFS = MASK_SIZE - 1;  // 40
constexpr int NUM_DIAMOND_PTS = 2 * R * (R + 1) + 1; // 841 points for R=20
const DTYPE AMP_MIN_FIXED = 10.0;

// diamond mask generated at compile time
constexpr bool is_in_diamond(int i, int j, int r) {
    return ((i > r ? i - r : r - i) + (j > r ? j - r : r - j)) <= r;
}

struct DiamondMask {
    bool data[MASK_SIZE][MASK_SIZE];
    constexpr DiamondMask() : data() {
        for (int i = 0; i < MASK_SIZE; i++) {
            for (int j = 0; j < MASK_SIZE; j++) {
                data[i][j] = is_in_diamond(i, j, R);
            }
        }
    }
};

constexpr DiamondMask DIAMOND = DiamondMask();

// BALANCED TREE REDUCTION FOR PEAK SEARCH
constexpr int TREE_SIZE = 1024;
inline DTYPE find_diamond_max(const DTYPE window[MASK_SIZE][MASK_SIZE]) {
    #pragma HLS INLINE
    DTYPE tree[TREE_SIZE];
    #pragma HLS ARRAY_PARTITION variable = tree complete dim = 1

    int idx = 0;
    flatten_i: for (int i = 0; i < MASK_SIZE; i++) {
    #pragma HLS UNROLL
        flatten_j: for (int j = 0; j < MASK_SIZE; j++) {
    #pragma HLS UNROLL
            if (DIAMOND.data[i][j]) {
                tree[idx++] = window[i][j];
            }
        }
    }

    pad_tree: for (int k = 841; k < TREE_SIZE; k++) {
        #pragma HLS UNROLL
        tree[k] = (DTYPE)-32.0;
    }

    reduce_s0: for (int i = 0; i < 512; i++) { 
        #pragma HLS UNROLL 
        tree[i] = (tree[i*2] > tree[i*2+1]) ? tree[i*2] : tree[i*2+1]; 
    }
    reduce_s1: for (int i = 0; i < 256; i++) { 
        #pragma HLS UNROLL 
        tree[i] = (tree[i*2] > tree[i*2+1]) ? tree[i*2] : tree[i*2+1]; 
    }
    reduce_s2: for (int i = 0; i < 128; i++) { 
        #pragma HLS UNROLL 
        tree[i] = (tree[i*2] > tree[i*2+1]) ? tree[i*2] : tree[i*2+1]; 
    }
    reduce_s3: for (int i = 0; i < 64;  i++) { 
        #pragma HLS UNROLL 
        tree[i] = (tree[i*2] > tree[i*2+1]) ? tree[i*2] : tree[i*2+1]; 
    }
    reduce_s4: for (int i = 0; i < 32;  i++) { 
        #pragma HLS UNROLL 
        tree[i] = (tree[i*2] > tree[i*2+1]) ? tree[i*2] : tree[i*2+1]; 
    }
    reduce_s5: for (int i = 0; i < 16;  i++) { 
        #pragma HLS UNROLL 
        tree[i] = (tree[i*2] > tree[i*2+1]) ? tree[i*2] : tree[i*2+1]; 
    }
    reduce_s6: for (int i = 0; i < 8;   i++) { 
        #pragma HLS UNROLL 
        tree[i] = (tree[i*2] > tree[i*2+1]) ? tree[i*2] : tree[i*2+1]; 
    }
    reduce_s7: for (int i = 0; i < 4;   i++) { 
        #pragma HLS UNROLL 
        tree[i] = (tree[i*2] > tree[i*2+1]) ? tree[i*2] : tree[i*2+1]; 
    }
    reduce_s8: for (int i = 0; i < 2;   i++) { 
        #pragma HLS UNROLL 
        tree[i] = (tree[i*2] > tree[i*2+1]) ? tree[i*2] : tree[i*2+1]; 
    }
    reduce_s9: { tree[0] = (tree[0] > tree[1]) ? tree[0] : tree[1]; }

    return tree[0];
}

void detect_peaks(const DTYPE spec[MAX_FREQ][MAX_WINDOWS], int num_windows,
                  int peak_freq[MAX_PEAKS], int peak_time[MAX_PEAKS]) {
#pragma HLS INTERFACE m_axi port = spec offset = slave bundle = gmem0 depth = 204900
#pragma HLS INTERFACE m_axi port = peak_freq offset = slave bundle = gmem1 depth = 20001
#pragma HLS INTERFACE m_axi port = peak_time offset = slave bundle = gmem2 depth = 20001

#pragma HLS INTERFACE s_axilite port = spec bundle = control
#pragma HLS INTERFACE s_axilite port = peak_freq bundle = control
#pragma HLS INTERFACE s_axilite port = peak_time bundle = control
#pragma HLS INTERFACE s_axilite port = num_windows bundle = control
#pragma HLS INTERFACE s_axilite port = return bundle = control

    uint16_t local_peak_freq[MAX_PEAKS];
    uint16_t local_peak_time[MAX_PEAKS];
    uint16_t pc = 0;

    // Line buffer: the previous 40 rows
    DTYPE line_buf[NUM_LINE_BUFS][MAX_WINDOWS];
#pragma HLS ARRAY_PARTITION variable = line_buf complete dim = 1

    // Window: 41x41 register grid
    DTYPE window[MASK_SIZE][MASK_SIZE];
#pragma HLS ARRAY_PARTITION variable = window complete dim = 0

    DTYPE new_row[MAX_WINDOWS];

init_buffer:
    for (int i = 0; i < NUM_LINE_BUFS; i++) {
    init_buffer_inner:
        for (int j = 0; j < MAX_WINDOWS; j++) {
    #pragma HLS PIPELINE II = 1
            line_buf[i][j] = 0;
        }
    }
init_mask:
    for (int i = 0; i < MASK_SIZE; i++) {
    init_mask_inner:
        for (int j = 0; j < MASK_SIZE; j++) {
    #pragma HLS UNROLL
            window[i][j] = 0;
        }
    }

slide_window:
    for (int f = 0; f < MAX_FREQ + R; f++) {
        if (f < MAX_FREQ) {
        read_row:
            for (int w = 0; w < num_windows; w++) {
            #pragma HLS PIPELINE II = 1
            #pragma HLS LOOP_TRIPCOUNT min = 1 max = MAX_WINDOWS avg = MAX_WINDOWS
                new_row[w] = spec[f][w];
            }
        } else {
        zero_padding:
            for (int w = 0; w < num_windows; w++) {
                #pragma HLS PIPELINE II = 1
                #pragma HLS LOOP_TRIPCOUNT min = 1 max = MAX_WINDOWS avg = MAX_WINDOWS
                new_row[w] = 0;
            }
        }

        // STEP 2 & 3: Shift window and find peak
        shift_window:
        for (int w = 0; w < num_windows + R; w++) {
            #pragma HLS PIPELINE II = 1
            #pragma HLS LOOP_TRIPCOUNT min = 1 max = MAX_WINDOWS + R avg = MAX_WINDOWS + R

            // 2a. Shift the 2D window left by one column
            shift_window_inner:
            for (int i = 0; i < MASK_SIZE; i++) {
                #pragma HLS UNROLL
                for (int j = 0; j < MASK_SIZE - 1; j++) {
                #pragma HLS UNROLL
                    window[i][j] = window[i][j + 1];
                }
            }

            // 2b. Load the newest column into the right-most edge
            if (w < num_windows) {
                window[0][MASK_SIZE - 1] = line_buf[0][w];
            load_col:
                for (int i = 0; i < NUM_LINE_BUFS - 1; i++) {
                    #pragma HLS UNROLL
                    window[i + 1][MASK_SIZE - 1] =
                        (line_buf[i][w] = line_buf[i + 1][w]);
                }
                window[MASK_SIZE - 1][MASK_SIZE - 1] =
                    (line_buf[NUM_LINE_BUFS - 1][w] = new_row[w]);
            } else {
            pad_window:
                for (int i = 0; i < MASK_SIZE; i++) {
                    #pragma HLS UNROLL
                    window[i][MASK_SIZE - 1] = 0;
                }
            }

            // 2c. Peak search with tree reduction
            int f_center = f - R;
            int w_center = w - R;

            if (f_center >= 0 && f_center < MAX_FREQ && w_center >= 0 &&
                w_center < num_windows) {
                
                DTYPE center_val = window[R][R];
                DTYPE max_val = find_diamond_max(window);

                if (center_val == max_val && center_val > AMP_MIN_FIXED) {
                    if (pc < MAX_PEAKS) {
                        local_peak_freq[pc] = f_center;
                        local_peak_time[pc] = w_center;
                        pc++;
                    }
                }
            }
        }
    }

write_axi:
    for (uint16_t i = 0; i < pc; i++) {
#pragma HLS PIPELINE II = 1
        peak_freq[i] = local_peak_freq[i];
        peak_time[i] = local_peak_time[i];
    }

    if (pc < MAX_PEAKS) {
        peak_time[pc] = -1;  // terminator
    } else {
        peak_time[MAX_PEAKS - 1] = -1;
    }
}