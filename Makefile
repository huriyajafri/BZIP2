# =============================================================================
# BZip2 Implementation — Cross-Platform Makefile
# Section 6.1 of SRS
#
# Targets:
#   all        — compile the complete project  (default)
#   debug      — compile with debug symbols, no optimisation
#   run        — build then run
#   windows    — cross-compile .exe from Linux (mingw-w64)
#   benchmark  — run all files in benchmarks/ → results/results.csv
#   clean      — remove all build artefacts
#   help       — print this target list
# =============================================================================

# --------------------------------------------------------------------------- #
# Platform detection                                                           #
# --------------------------------------------------------------------------- #
ifeq ($(OS),Windows_NT)
    PLATFORM  := windows
    TARGET    := program.exe
    NULL      := nul
    MKDIR     := if not exist obj mkdir obj
    DEL_BIN   := if exist program.exe   del /Q program.exe
    DEL_WIN   := if exist program_win.exe del /Q program_win.exe
    DEL_OBJ   := if exist obj rmdir /S /Q obj
    RUN_CMD   := program.exe
else
    PLATFORM  := linux
    TARGET    := program
    NULL      := /dev/null
    MKDIR     := mkdir -p obj
    DEL_BIN   := rm -f program
    DEL_WIN   := rm -f program_win.exe
    DEL_OBJ   := rm -rf obj
    RUN_CMD   := ./program
endif

# --------------------------------------------------------------------------- #
# Compilers                                                                    #
# --------------------------------------------------------------------------- #
CC         := gcc
CC_WIN     := x86_64-w64-mingw32-gcc      # mingw cross-compiler
TARGET_WIN := program_win.exe

# --------------------------------------------------------------------------- #
# Flags                                                                        #
# --------------------------------------------------------------------------- #
INCS        := -Iinclude
CFLAGS      := $(INCS) -Wall -Wextra -O2
CFLAGS_DBG  := $(INCS) -Wall -Wextra -O0 -g -DDEBUG
CFLAGS_WIN  := $(INCS) -Wall -Wextra -O2 -static

# --------------------------------------------------------------------------- #
# Sources / objects                                                            #
# --------------------------------------------------------------------------- #
SRC_DIR := src
OBJ_DIR := obj

SOURCES := $(SRC_DIR)/main.c    \
           $(SRC_DIR)/block.c   \
           $(SRC_DIR)/rle.c     \
           $(SRC_DIR)/bwt.c     \
           $(SRC_DIR)/mtf.c     \
           $(SRC_DIR)/huffman.c \
           $(SRC_DIR)/config.c

OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))

# --------------------------------------------------------------------------- #
# Benchmark settings (can override on command line)                           #
# e.g.  make benchmark BLOCK_SIZE=900000                                      #
# --------------------------------------------------------------------------- #
BENCH_DIR  := benchmarks
RESULT_DIR := results
RESULT_CSV := $(RESULT_DIR)/results.csv
BLOCK_SIZE := 500000

# =============================================================================
.PHONY: all debug run windows benchmark plot clean help

# --------------------------------------------------------------------------- #
# all — default build                                                          #
# --------------------------------------------------------------------------- #
all: $(OBJ_DIR) $(TARGET)
	@echo ""
	@echo "  Build OK  ->  $(TARGET)   [$(PLATFORM)]"
	@echo ""

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(CFLAGS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Create obj/ directory
$(OBJ_DIR):
	$(MKDIR)

# --------------------------------------------------------------------------- #
# debug                                                                        #
# --------------------------------------------------------------------------- #
debug: $(OBJ_DIR)
	$(CC) $(SOURCES) $(CFLAGS_DBG) -o $(TARGET)
	@echo "  Debug build -> $(TARGET)"

# --------------------------------------------------------------------------- #
# run                                                                          #
# --------------------------------------------------------------------------- #
run: all
	$(RUN_CMD)

# --------------------------------------------------------------------------- #
# windows — cross-compile on Linux → .exe                                     #
# Install cross-compiler:  sudo apt install gcc-mingw-w64-x86-64              #
# --------------------------------------------------------------------------- #
windows:
	@echo "Cross-compiling for Windows..."
ifeq ($(PLATFORM),linux)
	@which $(CC_WIN) > $(NULL) 2>&1 || \
	  (echo "ERROR: $(CC_WIN) not found." && \
	   echo "  Install: sudo apt install gcc-mingw-w64-x86-64" && exit 1)
	$(CC_WIN) $(SOURCES) $(CFLAGS_WIN) -o $(TARGET_WIN)
	@echo "  Windows build -> $(TARGET_WIN)"
else
	@echo "  Already on Windows — use 'make all' with MSYS2/MinGW or native gcc."
endif

# --------------------------------------------------------------------------- #
# benchmark — run every file in benchmarks/ and write results.csv             #
# Cross-platform Python benchmark runner                                      #
# --------------------------------------------------------------------------- #
benchmark: all
	python scripts/benchmark.py --block-size $(BLOCK_SIZE) --bench-dir $(BENCH_DIR) --results-dir $(RESULT_DIR)

# --------------------------------------------------------------------------- #
# plot — generate graphs from results.csv                                     #
# --------------------------------------------------------------------------- #
plot:
	python scripts/plot_results.py --csv $(RESULT_CSV) --out $(RESULT_DIR)

benchmark-all: benchmark plot

# --------------------------------------------------------------------------- #
# clean                                                                        #
# --------------------------------------------------------------------------- #
clean:
	$(DEL_BIN)
	$(DEL_WIN)
	$(DEL_OBJ)
	@echo "  Clean done."

# --------------------------------------------------------------------------- #
# help                                                                         #
# --------------------------------------------------------------------------- #
help:
	@echo ""
	@echo "  BZip2 Implementation — Makefile targets"
	@echo "  ─────────────────────────────────────────────────────────────"
	@echo "  make                Build (auto-detects Linux / Windows)"
	@echo "  make debug          Debug build (-g, no optimisation)"
	@echo "  make run            Build and run"
	@echo "  make windows        Cross-compile Windows .exe (Linux only)"
	@echo "                      Needs: sudo apt install gcc-mingw-w64-x86-64"
	@echo "  make benchmark      Compress all benchmarks/ files → results.csv"
	@echo "  make benchmark BLOCK_SIZE=900000   Override block size"
	@echo "  make clean          Remove all build artefacts"
	@echo "  make help           This message"
	@echo ""
	@echo "  Detected platform : $(PLATFORM)"
	@echo "  Compiler          : $(CC)"
	@echo "  Binary            : $(TARGET)"
	@echo ""