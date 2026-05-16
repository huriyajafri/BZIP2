#!/usr/bin/env python3
import argparse
import csv
import os
import shutil
import subprocess
import time
from pathlib import Path


def parse_args():
    p = argparse.ArgumentParser(description="Run project benchmark and generate results.csv")
    p.add_argument("--config", default="config.ini", help="Path to config.ini")
    p.add_argument("--bench-dir", default="benchmarks", help="Directory containing benchmark files")
    p.add_argument("--results-dir", default="results", help="Directory for outputs and CSV")
    p.add_argument("--block-size", type=int, default=500000, help="Block size in bytes")
    p.add_argument("--limit", type=int, default=0, help="Limit number of files for quick testing")
    return p.parse_args()


def parse_config(path):
    values = {}
    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.split("#", 1)[0].strip()
            if not line or line.startswith("["):
                continue
            if "=" not in line:
                continue
            k, v = line.split("=", 1)
            values[k.strip()] = v.strip()
    return values


def write_config(path, values):
    lines = []
    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            before_comment = raw.split("#", 1)[0]
            stripped = before_comment.strip()
            if "=" in stripped and not stripped.startswith("["):
                k = stripped.split("=", 1)[0].strip()
                if k in values:
                    indent = raw[: len(raw) - len(raw.lstrip())]
                    raw = f"{indent}{k} = {values[k]}\n"
            lines.append(raw)
    with open(path, "w", encoding="utf-8") as f:
        f.writelines(lines)


def collect_files(bench_dir):
    out = []
    for root, _, files in os.walk(bench_dir):
        for name in files:
            p = os.path.join(root, name)
            if os.path.isfile(p) and os.path.getsize(p) > 0:
                out.append(p)
    out.sort()
    return out


def run_cmd(command):
    return subprocess.run(
        command,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )


def main():
    args = parse_args()
    config_path = Path(args.config)
    bench_dir = Path(args.bench_dir)
    results_dir = Path(args.results_dir)
    results_dir.mkdir(parents=True, exist_ok=True)

    exe = Path("program.exe" if os.name == "nt" else "program")
    if not exe.exists():
        raise SystemExit("Build executable first (make).")
    if not config_path.exists():
        raise SystemExit("config.ini not found.")
    if not bench_dir.exists():
        raise SystemExit("benchmarks directory not found.")

    files = collect_files(str(bench_dir))
    if args.limit > 0:
        files = files[: args.limit]
    if not files:
        raise SystemExit("No benchmark files found.")

    csv_path = results_dir / "results.csv"
    original_cfg = parse_config(str(config_path))
    bzip2_path = shutil.which("bzip2")

    rows = []
    try:
        for bench_file in files:
            src = Path(bench_file)
            out_file = results_dir / f"{src.name}.bz2impl"
            cfg_updates = {
                "input_file": src.as_posix(),
                "output_file": out_file.as_posix(),
                "block_size": str(max(100000, min(900000, args.block_size))),
                "benchmark_mode": "true",
                "output_metrics": "false",
            }
            write_config(str(config_path), cfg_updates)

            t0 = time.perf_counter()
            proc = run_cmd([str(exe)])
            t1 = time.perf_counter()

            src_size = src.stat().st_size
            comp_size = out_file.stat().st_size if out_file.exists() else 0
            ok = proc.returncode == 0 and comp_size > 0
            ratio = (comp_size / src_size) if (src_size > 0 and ok) else 0.0
            status = "OK" if ok else "FAIL"

            bz2_ratio = ""
            bz2_time = ""
            if bzip2_path:
                bz2_out = results_dir / f"{src.name}.bz2ref"
                tb0 = time.perf_counter()
                run_cmd([bzip2_path, "-k", "-f", "-c", src.as_posix()])
                tb1 = time.perf_counter()
                bz2_time = f"{(tb1 - tb0):.6f}"
                try:
                    bz2_comp = subprocess.run(
                        [bzip2_path, "-k", "-f", "-c", src.as_posix()],
                        stdout=subprocess.PIPE,
                        stderr=subprocess.DEVNULL,
                        check=False,
                    ).stdout
                    with open(bz2_out, "wb") as f:
                        f.write(bz2_comp)
                    if bz2_out.exists() and src_size > 0:
                        bz2_ratio = f"{(bz2_out.stat().st_size / src_size):.6f}"
                    bz2_out.unlink(missing_ok=True)
                except Exception:
                    bz2_ratio = ""
                    bz2_time = ""

            rows.append(
                {
                    "File": src.name,
                    "Size": str(src_size),
                    "BlockSize": str(args.block_size),
                    "CompressionRatio": f"{ratio:.6f}",
                    "Time": f"{(t1 - t0):.6f}",
                    "Memory": "",
                    "Status": status,
                    "Bzip2Ratio": bz2_ratio,
                    "Bzip2Time": bz2_time,
                }
            )
            print(f"{src.name}: {status} ratio={ratio:.6f} time={(t1 - t0):.4f}s")
    finally:
        write_config(str(config_path), original_cfg)

    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=[
                "File",
                "Size",
                "BlockSize",
                "CompressionRatio",
                "Time",
                "Memory",
                "Status",
                "Bzip2Ratio",
                "Bzip2Time",
            ],
        )
        writer.writeheader()
        writer.writerows(rows)

    print(f"\nWrote: {csv_path}")


if __name__ == "__main__":
    main()
