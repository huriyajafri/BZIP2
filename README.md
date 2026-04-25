Readme · MD
Copy

# BZip2 Compression Project — Stage 1
 
## Overview
This project implements Stage 1 of a simplified BZip2 compression algorithm.
It encodes an input file using RLE-1 followed by BWT, writes the compressed
result to a binary output file, then decodes it and prints the original data
to the terminal to verify correctness.
 
---
 
## Pipeline
 
```
INPUT FILE
    |
    v
[ Block Division ]       — splits file into fixed-size blocks
    |
    v
[ RLE-1 Encode ]         — replaces runs with (count, value) pairs
    |
    v
[ BWT Encode ]           — sorts cyclic rotations, extracts last column
    |
    v
[ Pack: index + data ]   — prepends 4-byte BWT primary index per block
    |
    v
OUTPUT FILE (.bin)       — binary, not human-readable (this is normal)
    |
    v
[ BWT Decode ]           — reconstructs pre-BWT data using LF-mapping
    |
    v
[ RLE-1 Decode ]         — expands (count, value) pairs back to original
    |
    v
TERMINAL OUTPUT          — decoded text printed to screen, not saved
```
 
---
 
## Why the Output File Looks Corrupted
 
The output file (e.g. `output.bin`) is a **binary file** — not plain text.
It contains raw bytes including the 4-byte BWT index header per block, which
are not valid ASCII. This is expected. Do not try to open it in a text editor.
 
---
 
## Configuration — `config.ini`
 
```ini
[General]
block_size   = 10        ; size of each block in bytes (100 – 900000)
rle1_enabled = true      ; enable/disable RLE-1
bwt_type     = matrix    ; BWT implementation (matrix only in Stage 1)
 
[Paths]
input_file   = input.txt
output_file  = output.bin
```
 
---
 
## Project Structure
 
```
BZIP2/
├── src/
│   ├── main.c       — encode + decode pipeline
│   ├── rle.c        — RLE-1 encode and decode
│   ├── bwt.c        — BWT encode and decode
│   ├── block.c      — block division and reassembly
│   └── config.c     — config.ini parser
├── include/
│   ├── rle.h
│   ├── bwt.h
│   ├── block.h
│   └── config.h
├── config.ini
├── input.txt
├── output.bin       — compressed binary output (written by encoder)
└── Makefile
```
 
---
 
## Build & Run
 
```bash
# Build
make
 
# Run
make run
 
# Clean
make clean
```