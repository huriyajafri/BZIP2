# BZip2 Compression — Implementation

A simplified BZip2 compression algorithm implemented in C, covering all major
pipeline stages: RLE-1, BWT, MTF, RLE-2, and Canonical Huffman coding.

---

## Team Members

| Name | Student ID | Contribution |
|------|-----------|--------------|
| Member 1 | xxxxxxx | RLE-1, BWT, Block management |
| Member 2 | xxxxxxx | MTF, RLE-2, Config parser |
| Member 3 | xxxxxxx | Huffman coding, Makefile, Benchmarking |

---

## Repository Structure

```
project-bzip2/
├── src/
│   ├── main.c          # Entry point, pipeline orchestration
│   ├── block.c         # Block division & reassembly
│   ├── rle.c           # RLE-1 and RLE-2 encode/decode
│   ├── bwt.c           # Burrows-Wheeler Transform
│   ├── mtf.c           # Move-to-Front transform
│   ├── huffman.c       # Canonical Huffman coding
│   └── config.c        # config.ini parser
├── include/
│   ├── block.h
│   ├── rle.h
│   ├── bwt.h
│   ├── mtf.h
│   ├── huffman.h
│   └── config.h
├── benchmarks/         # Place Canterbury/Calgary/Silesia corpus files here
├── results/            # results.csv and generated graphs written here
├── Makefile
├── config.ini
├── benchmark.sh        # Automated benchmark runner
├── plot_results.py     # Graph generator (requires matplotlib, pandas)
└── README.md
```

---

## Feature Description

### Stage 1 — Block Division + RLE-1 + BWT

**Block Division** (`block.c`)  
Reads the input file and splits it into fixed-size blocks (configurable via
`config.ini`, default 500 KB). Supports arbitrarily large files. Each block is
processed independently through the full pipeline.

**RLE-1** (`rle.c` — `rle1_encode` / `rle1_decode`)  
bzip2-style run-length encoding. Only encodes runs of **4 or more** identical
bytes — shorter runs are copied literally so inputs without long runs pass
through completely unchanged.

- Encoded run format: `B B B B COUNT` where `COUNT` is extra repetitions beyond 4.
- Maximum run per token: 4 + 255 = 259 bytes.
- Example: `"aaaaaa"` → `'a' 'a' 'a' 'a' 2` (6 bytes → 5 bytes).

**BWT** (`bwt.c` — `bwt_encode` / `bwt_decode`)  
Matrix-based Burrows-Wheeler Transform. Creates all cyclic rotations of the
input, sorts them lexicographically, and outputs the last column together with
the primary index needed for decoding.

### Stage 2 — MTF + RLE-2

**MTF** (`mtf.c` — `mtf_encode` / `mtf_decode`)  
Move-to-Front transform. Maintains a list of all 256 symbols initialised to
`[0, 1, 2, …, 255]`. For each input byte, outputs its current index in the list
then moves it to position 0. BWT output clusters identical bytes together, so
MTF output is dominated by small indices (especially 0), which compresses well.

**RLE-2** (`rle.c` — `rle2_encode` / `rle2_decode`)  
Zero-run encoding targeting the zero-heavy MTF output. Uses an escape scheme:
a run of N zeros is stored as `0x00 (N-1)`. Non-zero bytes are passed through
unchanged. This is unambiguous because every `0x00` in the encoded stream is
always followed by exactly one count byte.

### Stage 3 — Canonical Huffman Coding

**Huffman** (`huffman.c`)  
Canonical Huffman coding — stores only per-symbol code lengths (256 bytes) in
the header rather than the full tree, making the header compact and decoder
reconstruction deterministic.

- Builds a min-heap-based Huffman tree from symbol frequencies.
- Extracts code lengths, then generates canonical codes by sorting symbols by
  (length, symbol) and assigning codes left-to-right.
- For blocks too small to benefit from Huffman (≤ 264 bytes), the data is
  stored raw with a `0x00` flag byte to avoid expansion.

---

## Build Instructions

### Linux

```bash
# Build (release)
make

# Build with debug symbols
make debug

# Run
make run

# Clean
make clean
```

### Windows (native — requires MinGW or MSYS2)

Open **MSYS2 MinGW 64-bit** or **Git Bash** and run exactly the same commands:

```bash
make
make run
make clean
```

### Cross-compile Windows .exe from Linux

```bash
# Install the cross-compiler (one time)
sudo apt install gcc-mingw-w64-x86-64

# Build a Windows executable
make windows
# Output: program_win.exe
```

---

## Usage Examples

```bash
# 1. Edit config.ini to set your input file
#    input_file = my_document.txt
#    block_size = 500000

# 2. Compress
./program

# 3. View output
#    Compressed file: output.bz2  (path from config.ini)
#    Compression ratio printed to stdout
```

**Changing the input file without editing config.ini:**

```bash
sed -i 's|^input_file.*|input_file = path/to/myfile.bin|' config.ini
./program
```

---

## Benchmarking

### Run all benchmark files

```bash
# Place any test files (Canterbury/Calgary/Silesia corpus) in benchmarks/
# Then run:
bash benchmark.sh                    # default block size 500000
bash benchmark.sh --block 900000     # larger blocks
bash benchmark.sh --dir my_files     # custom directory
```

`benchmark.sh` automatically handles all file formats — `.txt`, `.bin`,
`.bmp`, `.jpg`, `.pdf`, `.html`, etc. — by treating every file as raw bytes.

Results are written to `results/results.csv`:

```
File,Extension,Size_bytes,BlockSize,OurRatio,OurTime_s,OurMemory_KB,Bzip2Ratio,Bzip2Time_s
cp.txt,txt,24603,500000,0.456123,0.012345,4096,0.421000,0.008000
...
```

### Generate graphs

```bash
pip install matplotlib pandas
python plot_results.py
```

Graphs saved to `results/`:

| Graph | Description |
|-------|-------------|
| `compression_ratio.png` | Our ratio vs bzip2 per file |
| `throughput.png` | Speed (MB/s) per file |
| `ratio_by_type.png` | Average ratio grouped by extension |
| `size_vs_ratio.png` | Scatter: file size vs ratio |
| `memory_usage.png` | Peak memory per file |
| `performance_score.png` | SRS formula: w1·(Cref/C) + w2·(S/Sref) |

---

## Implementation Details per Stage

### RLE-1 design decision
We chose the bzip2 threshold of 4: only runs of 4+ bytes are encoded. This
means the worst-case expansion for any input is 1 extra byte per 4-byte run
(the count byte), instead of the 2× expansion that naive (count, byte) schemes
produce on non-repeating data like `banana$`.

### BWT primary index
We use the standard matrix rotation approach. The primary index is the row in
the sorted rotation matrix that begins with the original string. It is stored
as a 4-byte little-endian integer in the packed block header.

### Huffman bypass for small blocks
The Huffman header is always 264 bytes (8 bytes for original length + 256 bytes
for code lengths). For blocks smaller than this threshold the data is stored raw
with a 1-byte flag, avoiding harmful expansion on tiny inputs.

### Canonical codes
Canonical codes are regenerated on the decoder side from the 256 code-length
bytes alone — no tree is transmitted. This is identical to how the real bzip2
and DEFLATE formats work.