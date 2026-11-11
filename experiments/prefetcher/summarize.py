#!/usr/bin/env python3
"""Compare L2 prefetchers. Miss counts use LOAD (demand) lines, because TOTAL includes prefetches.

usage: python3 summarize.py <trace_dir_name> [results_dir]
"""
import re
import sys
from pathlib import Path

trace = sys.argv[1] if len(sys.argv) > 1 else "605.mcf_s-665B"
results = Path(__file__).parent / (sys.argv[2] if len(sys.argv) > 2 else "results") / trace
PREFETCHERS = ["no", "next_line", "ip_stride", "va_ampm_lite", "spp_dev", "best_offset", "sandbox"]


def load_misses(text, cache):
    m = re.search(rf"cpu0->{cache} LOAD\s+ACCESS:\s+\d+\s+HIT:\s+\d+\s+MISS:\s+(\d+)", text)
    return int(m.group(1))


rows = []
for name in PREFETCHERS:
    f = results / f"{name}.txt"
    text = f.read_text() if f.exists() else ""
    ipc = re.search(r"CPU 0 cumulative IPC:\s+[\d.]+\s+instructions:\s+(\d+)\s+cycles:\s+(\d+)", text)
    if not ipc:
        print(f"{name:>13}  (missing or incomplete)")
        continue
    instrs, cycles = map(int, ipc.groups())
    br = re.search(r"Branch Prediction Accuracy:\s+([\d.]+)%\s+MPKI:\s+([\d.]+)", text)
    res = re.search(r"Mispredict Resolve Latency: average ([\d.]+) cycles over (\d+) mispredicts.*fetch stalled on mispredicts: ([\d.]+)%", text)
    hist = re.search(r"Mispredict Resolve Latency histogram \(cycles\):(.*)", text)
    buckets = [int(n) for n in re.findall(r":\s+(\d+)\s+\(", hist.group(1))]
    pf = re.search(r"cpu0->cpu0_L2C PREFETCH REQUESTED:\s+(\d+)\s+ISSUED:\s+(\d+)\s+USEFUL:\s+(\d+)\s+USELESS:\s+(\d+)", text)
    rob = re.search(r"Average ROB Occupancy \(per cycle\):\s+([\d.]+)", text)
    mshr = re.search(r"^cpu0_L2C MSHR OCCUPANCY: average ([\d.]+) \| cycles with outstanding misses: ([\d.]+)% \| MLP \(avg outstanding misses when >= 1\): ([\d.]+)", text, re.M)
    lat = re.search(r"cpu0->cpu0_L1D AVERAGE MISS LATENCY: ([\d.]+) cycles", text)
    rows.append(dict(name=name, ipc=instrs / cycles, cycles=cycles, instrs=instrs, br_mpki=float(br.group(2)),
                     l1d=1000 * load_misses(text, "cpu0_L1D") / instrs, l2=1000 * load_misses(text, "cpu0_L2C") / instrs,
                     llc=1000 * load_misses(text, "LLC") / instrs, l1lat=float(lat.group(1)),
                     resolve=float(res.group(1)), mispredicts=int(res.group(2)), stall=float(res.group(3)), slow=buckets[-1],
                     issued=int(pf.group(2)), useful=int(pf.group(3)), useless=int(pf.group(4)),
                     rob=float(rob.group(1)), mshr_busy=float(mshr.group(2)), mlp=float(mshr.group(3))))

base = next((r for r in rows if r["name"] == "no"), None)
if not base:
    sys.exit("no baseline (no.txt)")

print(f"{'prefetcher':>13} | {'IPC':>7} {'vs none':>8} | {'issued':>10} {'useful':>10} {'useless':>10} {'accuracy':>8} | {'L1D ld MPKI':>11} {'L2 ld MPKI':>10} {'LLC ld MPKI':>11} {'L1D miss lat':>12}")
for r in rows:
    judged = r["useful"] + r["useless"]
    acc = f"{100 * r['useful'] / judged:.1f}%" if judged else "-"
    print(f"{r['name']:>13} | {r['ipc']:>7.4f} {100 * (r['ipc'] / base['ipc'] - 1):>+7.2f}% | {r['issued']:>10,} {r['useful']:>10,} {r['useless']:>10,} {acc:>8} | "
          f"{r['l1d']:>11.2f} {r['l2']:>10.2f} {r['llc']:>11.2f} {r['l1lat']:>12.1f}")

print()
print(f"{'prefetcher':>13} | {'br MPKI':>7} {'avg resolve':>11} {'>=200 count':>11} {'stalled':>8} {'stall cycles':>13} {'stall removed':>13} {'cycles saved':>13} | {'ROB/cycle':>9} {'L2 busy':>8} {'L2 MLP*':>7}")
for r in rows:
    stall = r["resolve"] * r["mispredicts"]
    removed = base["resolve"] * base["mispredicts"] - stall
    print(f"{r['name']:>13} | {r['br_mpki']:>7.2f} {r['resolve']:>11.1f} {r['slow']:>11,} {r['stall']:>7.1f}% {stall:>13,.0f} {removed:>13,.0f} {base['cycles'] - r['cycles']:>13,} | "
          f"{r['rob']:>9.1f} {r['mshr_busy']:>7.1f}% {r['mlp']:>7.3f}")
print("* the L2 MSHR counts include prefetches, so MLP is not demand-only when a prefetcher is on")
