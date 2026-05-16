#!/usr/bin/env python3
import argparse
import os
import pandas as pd
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def parse_args():
    p = argparse.ArgumentParser(description="Generate benchmark graphs from results.csv")
    p.add_argument("--csv", default="results/results.csv", help="Input CSV path")
    p.add_argument("--out", default="results", help="Output directory for PNG files")
    return p.parse_args()


def main():
    args = parse_args()
    os.makedirs(args.out, exist_ok=True)
    df = pd.read_csv(args.csv)
    df.columns = df.columns.str.strip()

    for col in ["Size", "BlockSize", "CompressionRatio", "Time", "Bzip2Ratio", "Bzip2Time"]:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")

    if "Status" not in df.columns:
        df["Status"] = np.where(df["CompressionRatio"] > 0, "OK", "FAIL")

    df = df.sort_values("File").reset_index(drop=True)
    labels = df["File"].astype(str).tolist()
    n = len(df)
    width = max(12, n * 0.55)

    # Compression ratio graph (show all files, including failures)
    fig, ax = plt.subplots(figsize=(width, 6))
    x = np.arange(n)
    our = df["CompressionRatio"].fillna(0.0)
    colors = ["#2196F3" if s == "OK" else "#E53935" for s in df["Status"]]
    ax.bar(x, our, color=colors, alpha=0.9, label="Our impl")
    if "Bzip2Ratio" in df.columns and df["Bzip2Ratio"].notna().any():
        ax.plot(x, df["Bzip2Ratio"], color="#FF5722", marker="o", linewidth=1.5, label="bzip2")
    ax.axhline(1.0, color="gray", linestyle="--", linewidth=1)
    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=45, ha="right", fontsize=9)
    ax.set_ylabel("Compression Ratio")
    ax.set_title("Compression Ratio per File (lower is better)")
    ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(args.out, "compression_ratio.png"), dpi=150)
    plt.close(fig)

    # Throughput graph
    size_mb = df["Size"] / (1024 * 1024)
    speed = size_mb / df["Time"].replace(0, np.nan)
    fig, ax = plt.subplots(figsize=(width, 6))
    ax.bar(x, speed.fillna(0.0), color=colors, alpha=0.9)
    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=45, ha="right", fontsize=9)
    ax.set_ylabel("MB/s")
    ax.set_title("Compression Throughput per File")
    fig.tight_layout()
    fig.savefig(os.path.join(args.out, "throughput.png"), dpi=150)
    plt.close(fig)

    # Time vs size scatter
    fig, ax = plt.subplots(figsize=(8, 6))
    for i, row in df.iterrows():
        c = "#2196F3" if row["Status"] == "OK" else "#E53935"
        ax.scatter(row["Size"] / (1024 * 1024), row["Time"], c=c, s=70)
        ax.annotate(str(row["File"]), (row["Size"] / (1024 * 1024), row["Time"]),
                    textcoords="offset points", xytext=(4, 3), fontsize=7)
    ax.set_xlabel("File Size (MB)")
    ax.set_ylabel("Compression Time (s)")
    ax.set_title("Compression Time vs File Size")
    fig.tight_layout()
    fig.savefig(os.path.join(args.out, "time_vs_size.png"), dpi=150)
    plt.close(fig)

    print(f"Graphs written to: {args.out} ({n} files)")


if __name__ == "__main__":
    main()
