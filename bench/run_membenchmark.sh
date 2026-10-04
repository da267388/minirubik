#!/bin/bash

# ==========================================
# Configuration
# ==========================================
# Define paths relative to the script location (assumes script is in tools/)
RIPES="../../../Ripes-v2.2.6-106-g5b8a616-linux-x86_64.AppImage"
OUT_DIR="out"

# Ensure output directory exists
mkdir -p "$OUT_DIR"

# Test configurations
# MB_LIST=(0 0.25 0.5 1 2 4)
MB_LIST=(0 4)
PROCS=("RV32_ISS" "RV32_5S")
MEM_INSTRUCTIONS=("sw" "sb")

# ==========================================
# Run Benchmark
# ==========================================
echo "Starting Ripes Memory Benchmark..."

for mb in "${MB_LIST[@]}"; do
    
    # Calculate word count (sw) and byte count (sb) for the given MiB
    # 1 MiB = 1048576 bytes = 262144 words
    words=$(awk "BEGIN {print int($mb * 262144)}")
    bytes=$(awk "BEGIN {print int($mb * 1048576)}")
    
    # Generate temporary assembly files using sed
    sed "s/^\.equ NWORDS.*/.equ NWORDS, $words/" membench_sw.s > membench_sw_run.s
    sed "s/^\.equ NBYTES.*/.equ NBYTES, $bytes/" membench_sb.s > membench_sb_run.s

    for proc in "${PROCS[@]}"; do
        for insn in "${MEM_INSTRUCTIONS[@]}"; do
            
            # Select the correct temporary source file and suffix
            if [ "$insn" == "sw" ]; then
                src_file="membench_sw_run.s"
                size_val=$words
            else
                src_file="membench_sb_run.s"
                size_val=$bytes
            fi
            
            # Define output filenames preserving the requested format
            # Format: <insn>_<proc>_<time/iret>_output_<mb>.txt
            iret_out="${OUT_DIR}/${insn}_${proc}_iret_output_${mb}MiB.txt"
            time_out="${OUT_DIR}/${insn}_${proc}_time_output_${mb}MiB.txt"
            
            echo "Running: $proc | $insn | ${mb} MiB"
            
            # Run the measurement (discarding standard error from AppImage extraction if any, 
            # though /usr/bin/time -v writes to stderr which we capture to time_out)
            /usr/bin/time -v "$RIPES" --mode cli --src "$src_file" -t asm --proc "$proc" --iret > "$iret_out" 2> "$time_out"
            
            # Optional: Short sleep to let OS memory allocator settle
            sleep 0.5
        done
    done
done

# Cleanup temporary files
rm -f membench_sw_run.s membench_sb_run.s

echo "Benchmark complete. Results are in $OUT_DIR/"