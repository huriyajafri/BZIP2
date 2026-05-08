#!/usr/bin/env python3
"""
plot_results.py — Performance graph generator  (SRS Section 7.3 / 9.3)

Reads  results/results.csv  and produces the following graphs in results/:
  1. compression_ratio.png  — Our ratio vs bzip2 ratio per file
  2. throughput.png          — Compression speed (MB/s) per file
  3. ratio_by_type.png       — Average ratio grouped by file extension
  4. size_vs_ratio.png       — Scatter: file size vs compression ratio
  5. memory_usage.png        — Memory usage per file (if available)

Usage:
    python plot_results.py                    # reads results/results.csv
    python plot_results.py --csv my.csv       # custom CSV path
    python plot_results.py --out graphs/      # custom output directory
"""

import argparse
import os
import sys

import pandas as pd
import matplotlib
matplotlib.use("Agg")          # no display needed — saves to files
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import numpy as np

# --------------------------------------------------------------------------- #
# Config                                                                       #
# --------------------------------------------------------------------------- #

COLORS = {
    "ours":  "#2196F3",   # blue
    "bzip2": "#FF5722",   # orange
    "bar":   "#4CAF50",   # green
    "mem":   "#9C27B0",   # purple
}

# --------------------------------------------------------------------------- #
# Argument parsing                                                             #
# --------------------------------------------------------------------------- #

def parse_args():
    p = argparse.ArgumentParser(description="Generate performance graphs from results.csv")
    p.add_argument("--csv", default="results/results.csv",
                   help="Path to results CSV (default: results/results.csv)")
    p.add_argument("--out", default="results",
                   help="Output directory for PNG files (default: results/)")
    return p.parse_args()

# --------------------------------------------------------------------------- #
# Load & clean data                                                            #
# --------------------------------------------------------------------------- #

def load(csv_path: str) -> pd.DataFrame:
    if not os.path.exists(csv_path):
        print(f"ERROR: '{csv_path}' not found.  Run benchmark.sh first.")
        sys.exit(1)

    df = pd.read_csv(csv_path)
    df.columns = df.columns.str.strip()

    # Coerce numeric columns
    for col in ["Size_bytes", "BlockSize", "OurRatio", "OurTime_s",
                "OurMemory_KB", "Bzip2Ratio", "Bzip2Time_s"]:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")

    # Derived columns
    df["Size_MB"]        = df["Size_bytes"] / (1024 * 1024)
    df["OurSpeed_MBps"]  = df["Size_MB"] / df["OurTime_s"].replace(0, float("nan"))
    df["Bzip2Speed_MBps"]= df["Size_MB"] / df["Bzip2Time_s"].replace(0, float("nan"))

    # Short file label for axis ticks
    df["Label"] = df["File"].str[:20]

    return df

# --------------------------------------------------------------------------- #
# Helpers                                                                      #
# --------------------------------------------------------------------------- #

def save(fig, out_dir: str, name: str):
    path = os.path.join(out_dir, name)
    fig.savefig(path, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"  Saved: {path}")

def style_ax(ax, title, xlabel, ylabel):
    ax.set_title(title, fontsize=13, fontweight="bold", pad=10)
    ax.set_xlabel(xlabel, fontsize=10)
    ax.set_ylabel(ylabel, fontsize=10)
    ax.grid(axis="y", alpha=0.3, linestyle="--")
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

# --------------------------------------------------------------------------- #
# Graph 1 — Compression ratio per file (ours vs bzip2)                        #
# --------------------------------------------------------------------------- #

def plot_ratio(df: pd.DataFrame, out_dir: str):
    has_bzip2 = df["Bzip2Ratio"].notna().any()
    fig, ax = plt.subplots(figsize=(max(8, len(df) * 0.6 + 2), 5))

    x = np.arange(len(df))
    width = 0.35 if has_bzip2 else 0.6

    ax.bar(x - width/2 if has_bzip2 else x,
           df["OurRatio"], width, label="Our implementation",
           color=COLORS["ours"], alpha=0.85)

    if has_bzip2:
        ax.bar(x + width/2, df["Bzip2Ratio"].fillna(0), width,
               label="System bzip2", color=COLORS["bzip2"], alpha=0.85)

    ax.axhline(1.0, color="gray", linestyle="--", linewidth=0.8,
               label="ratio = 1 (no change)")

    ax.set_xticks(x)
    ax.set_xticklabels(df["Label"], rotation=40, ha="right", fontsize=8)
    style_ax(ax, "Compression Ratio per File  (lower = better)",
             "File", "Compression Ratio  (compressed / original)")
    ax.legend(fontsize=9)
    save(fig, out_dir, "compression_ratio.png")

# --------------------------------------------------------------------------- #
# Graph 2 — Throughput (MB/s) per file                                        #
# --------------------------------------------------------------------------- #

def plot_throughput(df: pd.DataFrame, out_dir: str):
    has_bzip2 = df["Bzip2Speed_MBps"].notna().any()
    fig, ax = plt.subplots(figsize=(max(8, len(df) * 0.6 + 2), 5))

    x = np.arange(len(df))
    width = 0.35 if has_bzip2 else 0.6

    ax.bar(x - width/2 if has_bzip2 else x,
           df["OurSpeed_MBps"].fillna(0), width,
           label="Our implementation", color=COLORS["ours"], alpha=0.85)

    if has_bzip2:
        ax.bar(x + width/2, df["Bzip2Speed_MBps"].fillna(0), width,
               label="System bzip2", color=COLORS["bzip2"], alpha=0.85)

    ax.set_xticks(x)
    ax.set_xticklabels(df["Label"], rotation=40, ha="right", fontsize=8)
    style_ax(ax, "Compression Throughput per File  (higher = better)",
             "File", "Speed  (MB/s)")
    ax.legend(fontsize=9)
    save(fig, out_dir, "throughput.png")

# --------------------------------------------------------------------------- #
# Graph 3 — Average ratio grouped by file extension                           #
# --------------------------------------------------------------------------- #

def plot_by_type(df: pd.DataFrame, out_dir: str):
    if "Extension" not in df.columns:
        return

    grp = (df.groupby("Extension")["OurRatio"]
             .mean()
             .dropna()
             .sort_values())

    if grp.empty:
        return

    fig, ax = plt.subplots(figsize=(max(6, len(grp) * 0.8 + 2), 5))
    bars = ax.bar(grp.index, grp.values, color=COLORS["bar"], alpha=0.85)
    ax.axhline(1.0, color="gray", linestyle="--", linewidth=0.8)

    # Annotate each bar with its value
    for bar, val in zip(bars, grp.values):
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.01,
                f"{val:.3f}", ha="center", va="bottom", fontsize=8)

    style_ax(ax, "Average Compression Ratio by File Type  (lower = better)",
             "File Extension", "Average Ratio")
    save(fig, out_dir, "ratio_by_type.png")

# --------------------------------------------------------------------------- #
# Graph 4 — Scatter: file size vs compression ratio                           #
# --------------------------------------------------------------------------- #

def plot_size_vs_ratio(df: pd.DataFrame, out_dir: str):
    valid = df[df["OurRatio"].notna() & df["Size_MB"].notna()]
    if valid.empty:
        return

    fig, ax = plt.subplots(figsize=(7, 5))
    scatter = ax.scatter(valid["Size_MB"], valid["OurRatio"],
                         c=COLORS["ours"], alpha=0.75, s=60, edgecolors="white")

    # Label each point
    for _, row in valid.iterrows():
        ax.annotate(row["Label"], (row["Size_MB"], row["OurRatio"]),
                    textcoords="offset points", xytext=(5, 3), fontsize=7)

    ax.axhline(1.0, color="gray", linestyle="--", linewidth=0.8)
    style_ax(ax, "File Size vs Compression Ratio",
             "Original File Size  (MB)", "Compression Ratio")
    save(fig, out_dir, "size_vs_ratio.png")

# --------------------------------------------------------------------------- #
# Graph 5 — Memory usage per file                                             #
# --------------------------------------------------------------------------- #

def plot_memory(df: pd.DataFrame, out_dir: str):
    if "OurMemory_KB" not in df.columns:
        return

    valid = df[df["OurMemory_KB"].notna() & (df["OurMemory_KB"] > 0)]
    if valid.empty:
        print("  Memory data not available — skipping memory graph.")
        return

    fig, ax = plt.subplots(figsize=(max(8, len(valid) * 0.6 + 2), 5))
    x = np.arange(len(valid))
    ax.bar(x, valid["OurMemory_KB"] / 1024, color=COLORS["mem"], alpha=0.85)
    ax.set_xticks(x)
    ax.set_xticklabels(valid["Label"], rotation=40, ha="right", fontsize=8)
    style_ax(ax, "Peak Memory Usage per File",
             "File", "Peak Memory  (MB)")
    save(fig, out_dir, "memory_usage.png")

# --------------------------------------------------------------------------- #
# Graph 6 — Performance score  Score = w1*(Cref/C) + w2*(S/Sref)            #
# --------------------------------------------------------------------------- #

def plot_score(df: pd.DataFrame, out_dir: str, w1=0.5, w2=0.5):
    """Score as defined in SRS Section 7.2."""
    valid = df[
        df["OurRatio"].notna() &
        df["Bzip2Ratio"].notna() &
        df["OurSpeed_MBps"].notna() &
        df["Bzip2Speed_MBps"].notna() &
        (df["OurRatio"] > 0) &
        (df["Bzip2Speed_MBps"] > 0)
    ].copy()

    if valid.empty:
        print("  Insufficient data for score graph (need bzip2 reference).")
        return

    # Score = w1*(Cref/C) + w2*(S/Sref)
    valid["Score"] = (
        w1 * (valid["Bzip2Ratio"] / valid["OurRatio"]) +
        w2 * (valid["OurSpeed_MBps"] / valid["Bzip2Speed_MBps"])
    )

    fig, ax = plt.subplots(figsize=(max(8, len(valid) * 0.6 + 2), 5))
    x = np.arange(len(valid))
    bars = ax.bar(x, valid["Score"], color=COLORS["ours"], alpha=0.85)
    ax.axhline(1.0, color=COLORS["bzip2"], linestyle="--", linewidth=1.2,
               label="Score = 1  (matches bzip2)")

    for bar, val in zip(bars, valid["Score"]):
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.01,
                f"{val:.2f}", ha="center", va="bottom", fontsize=8)

    ax.set_xticks(x)
    ax.set_xticklabels(valid["Label"], rotation=40, ha="right", fontsize=8)
    style_ax(ax,
             f"Performance Score  (w1={w1}, w2={w2})  — higher = better",
             "File", "Score  = w1·(Cref/C) + w2·(S/Sref)")
    ax.legend(fontsize=9)
    save(fig, out_dir, "performance_score.png")

# --------------------------------------------------------------------------- #
# Main                                                                         #
# --------------------------------------------------------------------------- #

def main():
    args = parse_args()
    os.makedirs(args.out, exist_ok=True)

    print(f"\n  Reading: {args.csv}")
    df = load(args.csv)
    print(f"  Rows loaded: {len(df)}\n")

    print("  Generating graphs...")
    plot_ratio(df, args.out)
    plot_throughput(df, args.out)
    plot_by_type(df, args.out)
    plot_size_vs_ratio(df, args.out)
    plot_memory(df, args.out)
    plot_score(df, args.out)

    print(f"\n  All graphs saved to: {args.out}/\n")

if __name__ == "__main__":
    main()