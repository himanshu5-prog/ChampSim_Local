# Branch predictor comparison

Compares five ChampSim branch predictors on one core, with everything else held fixed.
The runs report IPC and branch MPKI. They also report how long mispredicts take to resolve,
using counters added on this branch.

| Config | Predictor | Storage |
| --- | --- | --- |
| `bp_bimodal.json` | bimodal | 4 KB |
| `bp_gshare.json` | gshare | 4 KB |
| `bp_perceptron.json` | perceptron | ~3.8 KB |
| `bp_hashed_perceptron.json` | hashed perceptron | 64 KB |
| `bp_hashed_perceptron_4k.json` | hashed perceptron, 256-entry tables (`branch/hashed_perceptron_4k/`) | 4 KB |

The five configs differ only in `branch_predictor` and `executable_name`. The shared settings
are a 4 GHz core with a 352-entry ROB; 48 KB L1D, 512 KB L2 and 2 MB LLC caches; DDR4-3200 memory;
and no prefetchers.

## 1. Get the traces

The runs use SPEC CPU 2017 ChampSim traces from the
[DPC-3 trace collection](https://dpc3.compas.cs.stonybrook.edu/champsim-traces/speccpu/).
Put them in `dir_trace/` at the repository root:

```text
dir_trace/605.mcf_s-665B.champsimtrace.xz
dir_trace/620.omnetpp_s-874B.champsimtrace.xz
```

## 2. Build

Run this from the repository root. It builds one binary per config,
`bin/champsim_bp_<predictor>`.

```bash
./config.sh --join chain experiments/branch_pred/bp_*.json
make -j8
```

Running `config.sh` again replaces the previous build configuration. Build all five configs
together, or rebuild before running a different set.

## 3. Run

Run 20M warmup and 100M measured instructions for each predictor and trace. The summary script
reads the plain-text output. The `--json` file is optional.

```bash
for t in 605.mcf_s-665B 620.omnetpp_s-874B; do
  mkdir -p experiments/branch_pred/results_mlp/$t
  for bp in bimodal gshare perceptron hashed_perceptron hashed_perceptron_4k; do
    bin/champsim_bp_$bp --warmup-instructions 20000000 --simulation-instructions 100000000 \
      --json experiments/branch_pred/results_mlp/$t/$bp.json \
      dir_trace/$t.champsimtrace.xz > experiments/branch_pred/results_mlp/$t/$bp.txt &
  done
done
wait
```

Each simulation is single-threaded. The loop starts all ten at once, so on a machine with fewer
cores they share CPU time. On an 8-core Apple M1, a run took from about 20 minutes to 2 hours,
depending on the trace and how many runs shared the machine. Keep the machine
awake for the whole run, for example with `caffeinate -i` on macOS.

## 4. Summarize

```bash
python3 experiments/branch_pred/summarize.py 605.mcf_s-665B results_mlp
python3 experiments/branch_pred/summarize.py 620.omnetpp_s-874B results_mlp
```

The script prints four tables, each comparing the predictors against bimodal:

1. IPC, prediction accuracy, branch MPKI, L1D and LLC MPKI.
2. Mispredict resolve latency: the average, the share of cycles fetch was stalled, and a histogram.
3. Change from bimodal: mispredicts avoided, stall cycles removed, cycles saved, and the change
   in mispredicts that waited 200 cycles or more.
4. ROB occupancy per cycle, and MSHR occupancy and MLP for L1D, L2 and LLC.

The script expects every run in step 3 and the `bimodal` baseline. It parses lines that only
this branch's counters print, so it fails on output from stock ChampSim.

## Notes

- Results are not committed. The `.txt` logs are large, so regenerate them with steps 2–3.
- The `hashed_perceptron` config uses 16 times more storage than the others. Compare at equal
  storage with `hashed_perceptron_4k`.
- Two traces with one 100M-instruction region each are a small sample. Treat small IPC
  differences with care.
