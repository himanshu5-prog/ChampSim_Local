#!/usr/bin/env python3
"""Summarize branch predictor comparison.

usage: python3 summarize.py [trace_dir_name] [results_dir]
  e.g. python3 summarize.py 605.mcf_s-665B results_resolve

Reads <results_dir>/<trace>/<predictor>.txt, the plain-text output of each run. Those logs are
not committed: build each bp_*.json config and run it on the trace first.
"""
import re
import sys
from pathlib import Path

trace = sys.argv[1] if len(sys.argv) > 1 else "605.mcf_s-665B"
results = Path(__file__).parent / (sys.argv[2] if len(sys.argv) > 2 else "results") / trace
PREDICTORS = ["bimodal", "gshare", "perceptron", "hashed_perceptron", "hashed_perceptron_4k"]


def misses(text, cache, kind):
    m = re.search(rf"cpu0->{cache} {kind}\s+ACCESS:\s+(\d+)\s+HIT:\s+(\d+)\s+MISS:\s+(\d+)", text)
    return int(m.group(3)) if m else None


rows = []
for bp in PREDICTORS:
    f = results / f"{bp}.txt"
    text = f.read_text() if f.exists() else ""
    ipc = re.search(r"CPU 0 cumulative IPC:\s+[\d.]+\s+instructions:\s+(\d+)\s+cycles:\s+(\d+)", text)
    br = re.search(r"Branch Prediction Accuracy:\s+([\d.]+)%\s+MPKI:\s+([\d.]+)\s+Average ROB Occupancy at Mispredict:\s+([\d.]+)", text)
    ld = misses(text, "cpu0_L1D", "LOAD")
    llc = misses(text, "LLC", "TOTAL")
    if not (ipc and br and ld is not None and llc is not None):
        print(f"{bp:>20}  (missing or incomplete)")
        continue
    instrs, cycles = map(int, ipc.groups())
    acc, mpki, rob = map(float, br.groups())
    res = re.search(r"Mispredict Resolve Latency: average ([\d.]+) cycles over (\d+) mispredicts.*fetch stalled on mispredicts: ([\d.]+)%", text)
    hist = re.search(r"Mispredict Resolve Latency histogram \(cycles\):(.*)", text)
    buckets = [int(n) for n in re.findall(r":\s+(\d+)\s+\(", hist.group(1))] if hist else []
    rob_cycle = re.search(r"Average ROB Occupancy \(per cycle\):\s+([\d.]+)", text)
    mshr = {}
    for cache in ["cpu0_L1D", "cpu0_L2C", "LLC"]:
        m = re.search(rf"^{cache} MSHR OCCUPANCY: average ([\d.]+) \| cycles with outstanding misses: ([\d.]+)% \| MLP \(avg outstanding misses when >= 1\): ([\d.]+)",
                      text, re.M)
        if m:
            mshr[cache] = tuple(map(float, m.groups()))
    rows.append(dict(bp=bp, ipc=instrs / cycles, cycles=cycles, instrs=instrs, acc=acc, mpki=mpki, rob=rob,
                     ldm=1000 * ld / instrs, llcm=1000 * llc / instrs,
                     avg_lat=float(res.group(1)) if res else None, resolved=int(res.group(2)) if res else None,
                     stall=float(res.group(3)) if res else None, buckets=buckets,
                     rob_cycle=float(rob_cycle.group(1)) if rob_cycle else None, mshr=mshr))

base = next((r for r in rows if r["bp"] == "bimodal"), None)
print(f"{'predictor':>20} | {'IPC':>7} {'vs bimodal':>10} | {'accuracy':>8} {'br MPKI':>7} {'ROB@misp':>8} | {'L1D ld MPKI':>11} {'LLC MPKI':>8}")
for r in rows:
    rel = f"{100 * (r['ipc'] / base['ipc'] - 1):+.2f}%" if base else "-"
    print(f"{r['bp']:>20} | {r['ipc']:>7.4f} {rel:>10} | {r['acc']:>7.2f}% {r['mpki']:>7.2f} {r['rob']:>8.1f} | {r['ldm']:>11.2f} {r['llcm']:>8.2f}")

if any(r["avg_lat"] is not None for r in rows):
    labels = ["<10", "10-24", "25-49", "50-99", "100-199", ">=200"]
    print()
    print(f"{'predictor':>20} | {'avg resolve':>11} {'fetch stalled':>13} {'stall cycles':>13} | " + " ".join(f"{l:>8}" for l in labels) + "   (% of mispredicts)")
    for r in rows:
        if r["avg_lat"] is None:
            continue
        stall_cycles = r["avg_lat"] * r["resolved"]
        pct = [f"{100 * b / r['resolved']:>7.1f}%" for b in r["buckets"]]
        print(f"{r['bp']:>20} | {r['avg_lat']:>11.1f} {r['stall']:>12.1f}% {stall_cycles:>13,.0f} | " + " ".join(pct))
    if base and base["avg_lat"] is not None:
        print()
        print("vs bimodal: mispredicts avoided, stall cycles removed, total cycles saved, and the >=200-cycle bucket")
        for r in rows:
            if r is base or r["avg_lat"] is None:
                continue
            avoided = base["resolved"] - r["resolved"]
            stall_removed = base["avg_lat"] * base["resolved"] - r["avg_lat"] * r["resolved"]
            saved = base["cycles"] - r["cycles"]
            slow_removed = base["buckets"][-1] - r["buckets"][-1]
            print(f"{r['bp']:>20} | avoided {avoided:>9,} | stall removed {stall_removed:>13,.0f} | cycles saved {saved:>12,} | "
                  f">=200-cycle mispredicts removed {slow_removed:>8,} ({100 * slow_removed / avoided if avoided else float('nan'):.0f}% of avoided)")

if any(r["rob_cycle"] is not None for r in rows):
    print()
    print(f"{'predictor':>20} | {'ROB/cycle':>9} {'ROB@misp':>8} | " + " | ".join(f"{c + ' avg / busy% / MLP':>27}" for c in ["L1D", "L2C", "LLC"]))
    for r in rows:
        if r["rob_cycle"] is None:
            continue
        cells = []
        for cache in ["cpu0_L1D", "cpu0_L2C", "LLC"]:
            a = r["mshr"].get(cache)
            cells.append(f"{a[0]:>8.3f} / {a[1]:>5.1f}% / {a[2]:>5.3f}" if a else f"{'-':>27}")
        print(f"{r['bp']:>20} | {r['rob_cycle']:>9.1f} {r['rob']:>8.1f} | " + " | ".join(f"{c:>27}" for c in cells))
