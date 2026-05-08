#!/usr/bin/env bash
# =============================================================================
# benchmark.sh — Automated benchmark runner  (SRS Section 7)
#
# Runs the bzip2 implementation against every file in benchmarks/ and writes
# results/results.csv with columns:
#   File, Size, BlockSize, CompressionRatio, Time, Memory_KB
#
# Also compares against system bzip2 where available.
#
# Usage:
#   bash benchmark.sh                        # defaults
#   bash benchmark.sh --block 900000         # override block size
#   bash benchmark.sh --dir my_benchmarks    # override directory
#   bash benchmark.sh --block 500000 --dir benchmarks
# =============================================================================

set -euo pipefail

# --------------------------------------------------------------------------- #
# Defaults                                                                     #
# --------------------------------------------------------------------------- #
PROGRAM="./program"
CONFIG="config.ini"
BENCH_DIR="benchmarks"
RESULT_DIR="results"
RESULT_CSV="$RESULT_DIR/results.csv"
BLOCK_SIZE=500000

# --------------------------------------------------------------------------- #
# Argument parsing                                                             #
# --------------------------------------------------------------------------- #
while [[ $# -gt 0 ]]; do
    case $1 in
        --block)  BLOCK_SIZE="$2"; shift 2 ;;
        --dir)    BENCH_DIR="$2";  shift 2 ;;
        -h|--help)
            echo "Usage: $0 [--block N] [--dir PATH]"
            echo "  --block N    Block size in bytes (default: 500000)"
            echo "  --dir PATH   Benchmark file directory (default: benchmarks)"
            exit 0 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# --------------------------------------------------------------------------- #
# Sanity checks                                                                #
# --------------------------------------------------------------------------- #
if [[ ! -f "$PROGRAM" ]]; then
    echo "ERROR: '$PROGRAM' not found.  Run 'make' first."
    exit 1
fi
if [[ ! -d "$BENCH_DIR" ]]; then
    echo "ERROR: Benchmark directory '$BENCH_DIR' not found."
    echo "       Download Canterbury / Calgary / Silesia corpora and place"
    echo "       files (any format) inside benchmarks/"
    exit 1
fi
if [[ ! -f "$CONFIG" ]]; then
    echo "ERROR: '$CONFIG' not found."
    exit 1
fi

mkdir -p "$RESULT_DIR"

# --------------------------------------------------------------------------- #
# Helper: portable in-place sed                                                #
# --------------------------------------------------------------------------- #
config_set() {
    local key="$1" val="$2"
    if sed --version 2>/dev/null | grep -q GNU; then
        sed -i "s|^[[:space:]]*${key}[[:space:]]*=.*|${key} = ${val}|" "$CONFIG"
    else
        # macOS BSD sed
        sed -i '' "s|^[[:space:]]*${key}[[:space:]]*=.*|${key} = ${val}|" "$CONFIG"
    fi
}

config_get() {
    grep -E "^[[:space:]]*$1[[:space:]]*=" "$CONFIG" | head -1 \
        | sed 's/.*=[[:space:]]*//' | sed 's/[[:space:]]*#.*//' | tr -d '\r '
}

# --------------------------------------------------------------------------- #
# Save original config values so we can restore them after                    #
# --------------------------------------------------------------------------- #
ORIG_INPUT=$(config_get "input_file"  || echo "input.txt")
ORIG_OUTPUT=$(config_get "output_file" || echo "output.bz2")
ORIG_BLOCK=$(config_get "block_size"  || echo "500000")

restore_config() {
    config_set "input_file"  "$ORIG_INPUT"
    config_set "output_file" "$ORIG_OUTPUT"
    config_set "block_size"  "$ORIG_BLOCK"
}
trap restore_config EXIT

# --------------------------------------------------------------------------- #
# GNU time available? (for memory measurement)                                #
# --------------------------------------------------------------------------- #
HAS_GNU_TIME=false
if /usr/bin/time --version 2>&1 | grep -q GNU; then
    HAS_GNU_TIME=true
fi

# system bzip2 available?
HAS_BZIP2=false
command -v bzip2 &>/dev/null && HAS_BZIP2=true

# --------------------------------------------------------------------------- #
# Write CSV header                                                             #
# --------------------------------------------------------------------------- #
echo "File,Extension,Size_bytes,BlockSize,OurRatio,OurTime_s,OurMemory_KB,Bzip2Ratio,Bzip2Time_s" \
    > "$RESULT_CSV"

# --------------------------------------------------------------------------- #
# Collect all files from benchmarks/ (any extension, any depth)              #
# --------------------------------------------------------------------------- #
mapfile -t FILES < <(find "$BENCH_DIR" -type f | sort)

if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "No files found in '$BENCH_DIR'.  Add test files and retry."
    exit 1
fi

echo ""
echo "  BZip2 Benchmark  |  block=$BLOCK_SIZE  |  files=${#FILES[@]}"
echo "  ─────────────────────────────────────────────────────────────────"
printf "  %-30s %10s %8s %8s %10s\n" "File" "Size" "Ratio" "Time(s)" "Mem(KB)"
echo "  ─────────────────────────────────────────────────────────────────"

TOTAL_ORIG=0
TOTAL_COMP=0

for BENCH_FILE in "${FILES[@]}"; do
    FNAME=$(basename "$BENCH_FILE")
    EXT="${FNAME##*.}"
    [[ "$FNAME" == "$EXT" ]] && EXT="(none)"   # files with no extension

    FILE_SIZE=$(wc -c < "$BENCH_FILE" | tr -d ' ')
    [[ "$FILE_SIZE" -eq 0 ]] && continue        # skip empty files

    OUTPUT_FILE="$RESULT_DIR/${FNAME}.bz2impl"

    # Update config for this file
    config_set "input_file"  "$BENCH_FILE"
    config_set "output_file" "$OUTPUT_FILE"
    config_set "block_size"  "$BLOCK_SIZE"

    # ------------------------------------------------------------------- #
    # Run our implementation — timed                                       #
    # ------------------------------------------------------------------- #
    START_NS=$(date +%s%N)

    if $HAS_GNU_TIME; then
        MEM_KB=$( { /usr/bin/time -v "$PROGRAM" > /dev/null; } 2>&1 \
                  | grep "Maximum resident" | awk '{print $NF}' )
    else
        "$PROGRAM" > /dev/null 2>&1 || true
        MEM_KB="N/A"
    fi

    END_NS=$(date +%s%N)
    OUR_TIME=$(echo "scale=6; ($END_NS - $START_NS) / 1000000000" | bc)

    # Compression ratio
    if [[ -f "$OUTPUT_FILE" ]]; then
        COMP_SIZE=$(wc -c < "$OUTPUT_FILE" | tr -d ' ')
        OUR_RATIO=$(echo "scale=6; $COMP_SIZE / $FILE_SIZE" | bc)
        TOTAL_ORIG=$((TOTAL_ORIG + FILE_SIZE))
        TOTAL_COMP=$((TOTAL_COMP + COMP_SIZE))
    else
        COMP_SIZE=0
        OUR_RATIO="ERR"
    fi

    # ------------------------------------------------------------------- #
    # Compare with system bzip2                                            #
    # ------------------------------------------------------------------- #
    BZ2_RATIO="N/A"
    BZ2_TIME="N/A"
    if $HAS_BZIP2; then
        BZ2_OUT="$RESULT_DIR/${FNAME}.bz2ref"
        T0=$(date +%s%N)
        bzip2 -k -f -c "$BENCH_FILE" > "$BZ2_OUT" 2>/dev/null || true
        T1=$(date +%s%N)
        BZ2_TIME=$(echo "scale=6; ($T1 - $T0) / 1000000000" | bc)
        if [[ -f "$BZ2_OUT" ]]; then
            BZ2_SIZE=$(wc -c < "$BZ2_OUT" | tr -d ' ')
            BZ2_RATIO=$(echo "scale=6; $BZ2_SIZE / $FILE_SIZE" | bc)
            rm -f "$BZ2_OUT"
        fi
    fi

    # ------------------------------------------------------------------- #
    # Write CSV row                                                        #
    # ------------------------------------------------------------------- #
    echo "$FNAME,$EXT,$FILE_SIZE,$BLOCK_SIZE,$OUR_RATIO,$OUR_TIME,$MEM_KB,$BZ2_RATIO,$BZ2_TIME" \
        >> "$RESULT_CSV"

    printf "  %-30s %10s %8s %8s %10s\n" \
        "$FNAME" "$FILE_SIZE" "$OUR_RATIO" "$OUR_TIME" "$MEM_KB"
done

echo "  ─────────────────────────────────────────────────────────────────"

# Overall ratio across all files
if [[ $TOTAL_ORIG -gt 0 ]]; then
    OVERALL=$(echo "scale=4; $TOTAL_COMP / $TOTAL_ORIG" | bc)
    echo "  Overall ratio: $OVERALL  ($TOTAL_ORIG → $TOTAL_COMP bytes)"
fi

echo ""
echo "  Results written to: $RESULT_CSV"
echo ""