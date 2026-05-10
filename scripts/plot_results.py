#!/usr/bin/env python3
import argparse
import os
import pandas as pd
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def parse_args():
    p = argparse.ArgumentParser(description="Generate benchmark graphs from results.csv")
    p.add_argument("--csv", default="results/results.csv", help="Input CSV path")
    p.add_argument("--out", default="results", help="Output directory for PNG files")
    return p.parse_args()


def main():
    args = parse_args()
    os.makedirs(args.out, exist_ok=True)
    df = pd.read_csv(args.csv)

    for col in ["Size", "BlockSize", "CompressionRatio", "Time", "Bzip2Ratio", "Bzip2Time"]:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")

    # Compression ratio graph
    plt.figure(figsize=(10, 5))
    plt.bar(df["File"], df["CompressionRatio"], label="Our impl")
    if "Bzip2Ratio" in df.columns and df["Bzip2Ratio"].notna().any():
        plt.plot(df["File"], df["Bzip2Ratio"], color="orange", marker="o", label="bzip2")
    plt.axhline(1.0, linestyle="--", color="gray", linewidth=1)
    plt.xticks(rotation=40, ha="right")
    plt.ylabel("Compression Ratio")
    plt.title("Compression Ratio per File")
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(args.out, "compression_ratio.png"), dpi=150)
    plt.close()

    # Throughput graph
    size_mb = df["Size"] / (1024 * 1024)
    speed = size_mb / df["Time"].replace(0, pd.NA)
    plt.figure(figsize=(10, 5))
    plt.bar(df["File"], speed)
    plt.xticks(rotation=40, ha="right")
    plt.ylabel("MB/s")
    plt.title("Compression Throughput per File")
    plt.tight_layout()
    plt.savefig(os.path.join(args.out, "throughput.png"), dpi=150)
    plt.close()

    # Time vs size scatter
    plt.figure(figsize=(7, 5))
    plt.scatter(df["Size"] / (1024 * 1024), df["Time"])
    plt.xlabel("File Size (MB)")
    plt.ylabel("Compression Time (s)")
    plt.title("Compression Time vs File Size")
    plt.tight_layout()
    plt.savefig(os.path.join(args.out, "time_vs_size.png"), dpi=150)
    plt.close()

    print(f"Graphs written to: {args.out}")


if __name__ == "__main__":
    main()
