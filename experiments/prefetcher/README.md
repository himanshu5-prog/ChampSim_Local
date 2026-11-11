# L2 prefetcher comparison

Compares L2 prefetchers on one core, with everything else held fixed. The prefetchers are the
stock ChampSim ones plus new ones added on this branch. The runs report IPC, prefetch accuracy and
demand misses per level. They also report how prefetching changes mispredict resolve latency,
using counters from the `mispredict-latency-stats` branch.

| Config | L2 prefetcher | Source |
| --- | --- | --- |
| `../branch_pred/bp_gshare.json` | none (baseline, results saved as `no`) | — |
| `pf_next_line.json` | next_line | stock |
| `pf_ip_stride.json` | ip_stride | stock |
| `pf_va_ampm_lite.json` | va_ampm_lite | stock |
| `pf_spp_dev.json` | spp_dev | stock |
| `pf_best_offset.json` | best_offset | `prefetcher/best_offset/`, [Michaud, HPCA 2016](https://inria.hal.science/hal-01254863) |
| `pf_sandbox.json` | sandbox | `prefetcher/sandbox/`, [Pugsley et al., HPCA 2014](https://doi.org/10.1109/HPCA.2014.6835971) |

Every config is identical to `bp_gshare.json` except `L2C.prefetcher` and `executable_name`.
The shared settings are a 4 GHz core with a 352-entry ROB and the gshare branch predictor;
48 KB L1D, 512 KB L2 and 2 MB LLC caches; DDR4-3200 memory with 1 channel; and 4 KB pages.
Only the L2 has a prefetcher.

## 1. Get the traces

The runs use SPEC CPU 2017 ChampSim traces from the
[DPC-3 trace collection](https://dpc3.compas.cs.stonybrook.edu/champsim-traces/speccpu/).
Put them in `dir_trace/` at the repository root:

```text
dir_trace/605.mcf_s-665B.champsimtrace.xz
dir_trace/620.omnetpp_s-874B.champsimtrace.xz
```

## 2. Build

Run this from the repository root. It builds the baseline as `bin/champsim_bp_gshare` and one
binary per prefetcher, `bin/champsim_pf_<prefetcher>`.

```bash
./config.sh --join chain experiments/branch_pred/bp_gshare.json experiments/prefetcher/pf_*.json
make -j8
```

Running `config.sh` again replaces the previous build configuration. Build everything together,
or rebuild before running a different set.

## 3. Run

Run 20M warmup and 100M measured instructions for each prefetcher and trace. The summary script
reads the plain-text output. The `--json` file is optional.

```bash
R=experiments/prefetcher/results
for t in 605.mcf_s-665B 620.omnetpp_s-874B; do
  mkdir -p $R/$t
  bin/champsim_bp_gshare --warmup-instructions 20000000 --simulation-instructions 100000000 \
    --json $R/$t/no.json dir_trace/$t.champsimtrace.xz > $R/$t/no.txt &
  for pf in next_line ip_stride va_ampm_lite spp_dev best_offset sandbox; do
    bin/champsim_pf_$pf --warmup-instructions 20000000 --simulation-instructions 100000000 \
      --json $R/$t/$pf.json dir_trace/$t.champsimtrace.xz > $R/$t/$pf.txt &
  done
done
wait
```

Each simulation is single-threaded. The loop starts all fourteen at once, so on a machine with
fewer cores they share CPU time. On an 8-core Apple M1, a run alone took about 20–25 minutes.
With eight running at once, the batch took about 50 minutes. Keep the machine awake for the
whole run, for example with `caffeinate -i -s` on macOS.

## 4. Summarize

```bash
python3 experiments/prefetcher/summarize.py 605.mcf_s-665B
python3 experiments/prefetcher/summarize.py 620.omnetpp_s-874B
```

The script prints two tables, each comparing the prefetchers against `no`:

1. IPC and its change, L2 prefetches issued, useful and useless, accuracy, demand LOAD MPKI
   at L1D, L2 and LLC, and L1D average miss latency.
2. Branch MPKI, average mispredict resolve latency, mispredicts that waited 200 cycles or more,
   the share of cycles fetch was stalled, stall cycles removed, total cycles saved, ROB occupancy,
   and L2 MSHR busy % and MLP.

Best-Offset also prints its own statistics at the end of each run, in the `.txt` file. They show
learning phases, how many phases ended with prefetching off, prefetches dropped at page
boundaries or because the queue was full, and how often each offset was chosen:

```bash
grep "Best-Offset" experiments/prefetcher/results/*/best_offset.txt
```

Sandbox prints the same kind of counters, plus each candidate offset's final score and how many
prefetches each offset issued:

```bash
grep "Sandbox" experiments/prefetcher/results/*/sandbox.txt
```

## Notes

- Both the `.txt` logs and the `--json` files are committed, so `summarize.py` runs straight
  after cloning. Rerun step 3 only to change the setup.
- Miss counts use LOAD lines, because TOTAL lines include prefetch requests.
- Accuracy is useful / (useful + useless) at the L2. It is not comparable for `spp_dev`, which
  sends low-confidence prefetches to the LLC instead.
- The L2 MSHR busy % and MLP counters include prefetches, so they are not demand-only when a
  prefetcher is on.
- Best-Offset follows the paper's default parameters. It keeps only the 26 offsets up to 63,
  out of the paper's 52, because larger offsets cannot stay inside a 4 KB page.
- Sandbox follows the paper's Table 1 parameters, with one exception. The paper limits each
  core's prefetch degree, from 1 to 8 per direction, by its share of spare memory bandwidth, but
  gives no formula. This version always allows 8, so it has no bandwidth throttle. On `mcf` that
  floods DRAM: it issues many inaccurate prefetches and L2 miss latency rises.
- Two traces with one 100M-instruction region each are a small sample. Treat small IPC
  differences with care.

## Adding a prefetcher

1. Add the module under `prefetcher/<name>/`, in its own commit.
2. Copy `pf_next_line.json` to `pf_<name>.json`. Change `L2C.prefetcher` to `<name>` and
   `executable_name` to `champsim_pf_<name>`.
3. Add `<name>` to `PREFETCHERS` in `summarize.py`.
4. Build, run, and commit the config, script change and results JSONs together.
