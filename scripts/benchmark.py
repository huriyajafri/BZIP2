#!/usr/bin/env python3
"""
Run bz.exe on every file in benchmarks/:
  compress:  bz.exe <file> -c results/<name>.bz2
  decompress: bz.exe results/<name>.bz2 -d results/<name>.restored
Optional --save-stages enables hex dumps via config.ini (save_stages=true).
"""
import argparse
import csv
import filecmp
import os
import shutil
import subprocess
import time
from pathlib import Path


def parse_args():
    p = argparse.ArgumentParser(description="Benchmark bz.exe on all files in benchmarks/")
    p.add_argument("--config", default="config.ini", help="Path to config.ini")
    p.add_argument("--bench-dir", default="benchmarks", help="Directory containing benchmark files")
    p.add_argument("--results-dir", default="results", help="Directory for .bz2, restored files, CSV")
    p.add_argument("--stages-dir", default="stages", help="Root for hex stage dumps when --save-stages")
    p.add_argument("--block-size", type=int, default=500000, help="Block size in bytes")
    p.add_argument("--limit", type=int, default=0, help="Limit number of files (quick test)")
    p.add_argument("--only", metavar="NAME", help="Run a single benchmark file by name")
    p.add_argument(
        "--save-stages",
        action="store_true",
        help="Write compress/decompress pipeline stages as hex .txt files",
    )
    p.add_argument("--quiet", action="store_true", help="Hide bz.exe console output")
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


def run_bz(exe, args_list, quiet):
    kw = {"check": False}
    if quiet:
        kw["stdout"] = subprocess.DEVNULL
        kw["stderr"] = subprocess.DEVNULL
    return subprocess.run([str(exe), *args_list], **kw)


def files_equal(a, b):
    if not a.exists() or not b.exists():
        return False
    return filecmp.cmp(a, b, shallow=False)


def main():
    args = parse_args()
    config_path = Path(args.config)
    bench_dir = Path(args.bench_dir)
    results_dir = Path(args.results_dir)
    stages_dir = Path(args.stages_dir)
    results_dir.mkdir(parents=True, exist_ok=True)

    exe = Path("bz.exe" if os.name == "nt" else "bz")
    if not exe.exists():
        raise SystemExit("Build CLI first:  make cli")
    if not config_path.exists():
        raise SystemExit("config.ini not found.")
    if not bench_dir.exists():
        raise SystemExit("benchmarks directory not found.")

    files = collect_files(str(bench_dir))
    if args.only:
        files = [f for f in files if Path(f).name == args.only]
        if not files:
            raise SystemExit(f"No benchmark file named '{args.only}' in {bench_dir}")
    if args.limit > 0:
        files = files[: args.limit]
    if not files:
        raise SystemExit("No benchmark files found.")

    csv_path = results_dir / "results.csv"
    original_cfg = parse_config(str(config_path))
    bzip2_path = shutil.which("bzip2")
    block_size = str(max(100000, min(900000, args.block_size)))

    rows = []
    try:
        for bench_file in files:
            src = Path(bench_file)
            comp_out = results_dir / f"{src.name}.bz2"
            restored = results_dir / f"{src.name}.restored"

            cfg_updates = {
                "block_size": block_size,
                "save_stages": "true" if args.save_stages else "false",
                "stages_directory": stages_dir.as_posix(),
                "benchmark_mode": "true",
                "output_metrics": "false",
            }
            write_config(str(config_path), cfg_updates)

            t0 = time.perf_counter()
            pc = run_bz(exe, [src.as_posix(), "-c", comp_out.as_posix()], args.quiet)
            pd = run_bz(exe, [comp_out.as_posix(), "-d", restored.as_posix()], args.quiet)
            t1 = time.perf_counter()

            src_size = src.stat().st_size
            comp_size = comp_out.stat().st_size if comp_out.exists() else 0
            roundtrip = files_equal(src, restored)
            ok = pc.returncode == 0 and pd.returncode == 0 and comp_size > 0 and roundtrip
            ratio = (comp_size / src_size) if (src_size > 0 and comp_size > 0) else 0.0
            status = "OK" if ok else "FAIL"

            bz2_ratio = ""
            bz2_time = ""
            if bzip2_path:
                tb0 = time.perf_counter()
                try:
                    bz2_comp = subprocess.run(
                        [bzip2_path, "-k", "-f", "-c", src.as_posix()],
                        stdout=subprocess.PIPE,
                        stderr=subprocess.DEVNULL,
                        check=False,
                    ).stdout
                    tb1 = time.perf_counter()
                    bz2_time = f"{(tb1 - tb0):.6f}"
                    if src_size > 0 and bz2_comp:
                        bz2_ratio = f"{(len(bz2_comp) / src_size):.6f}"
                except Exception:
                    bz2_ratio = ""
                    bz2_time = ""

            rows.append(
                {
                    "File": src.name,
                    "Size": str(src_size),
                    "BlockSize": block_size,
                    "CompressionRatio": f"{ratio:.6f}",
                    "Time": f"{(t1 - t0):.6f}",
                    "Memory": "",
                    "Status": status,
                    "Roundtrip": "yes" if roundtrip else "no",
                    "Bzip2Ratio": bz2_ratio,
                    "Bzip2Time": bz2_time,
                }
            )
            stages_note = f"  stages -> {stages_dir / src.stem}" if args.save_stages else ""
            print(
                f"{src.name}: {status} ratio={ratio:.6f} "
                f"time={(t1 - t0):.4f}s roundtrip={'yes' if roundtrip else 'no'}{stages_note}"
            )
    finally:
        write_config(str(config_path), original_cfg)

    fieldnames = [
        "File",
        "Size",
        "BlockSize",
        "CompressionRatio",
        "Time",
        "Memory",
        "Status",
        "Roundtrip",
        "Bzip2Ratio",
        "Bzip2Time",
    ]
    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    print(f"\nWrote: {csv_path}")
    if args.save_stages:
        print(f"Pipeline logs: {stages_dir}/<filename>/compress|decompress/pipeline.txt")


if __name__ == "__main__":
    main()
