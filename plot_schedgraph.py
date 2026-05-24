#!/usr/bin/env python3
"""
Generate the lottery scheduler proportionality plot from schedgraph runs.

Output: schedgraph.png with two subplots:
  (a) expected vs observed CPU share of the high-ticket child
  (b) expected vs observed ratio of CPU time

Source data: three independent runs of `schedgraph` inside xv6 with
CPUS=1 and a 200-tick (~20s) observation window per experiment. Each
experiment forks two CPU-bound children with tickets (10, 10*R) and
samples per-process ticks via getpinfo().
"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

# (T_A, T_B, ticks_A, ticks_B) per run
runs = {
    "run A": [
        (10,  10,  94, 106),
        (10,  20,  84, 116),
        (10,  30,  51, 149),
        (10,  50,  25, 175),
        (10, 100,  20, 180),
        (10, 200,   9, 191),
    ],
    "run B": [
        (10,  10, 101,  99),
        (10,  20,  69, 131),
        (10,  30,  46, 154),
        (10,  50,  39, 162),
        (10, 100,  18, 182),
        (10, 200,  15, 185),
    ],
    "run C": [
        (10,  10, 101,  99),
        (10,  20,  66, 134),
        (10,  30,  51, 149),
        (10,  50,  25, 176),
        (10, 100,  27, 173),
        (10, 200,   5, 195),
    ],
}

ratios = [1, 2, 3, 5, 10, 20]
expected_share_B = [tb/(ta+tb) for ta, tb in [(10, 10*r) for r in ratios]]

# Aggregate
obs_share_B_runs = []
obs_ratio_runs = []
for name, rows in runs.items():
    shares = []
    ratios_obs = []
    for (ta_t, tb_t, ta_k, tb_k) in rows:
        shares.append(tb_k / (ta_k + tb_k))
        ratios_obs.append(tb_k / max(ta_k, 1))
    obs_share_B_runs.append(shares)
    obs_ratio_runs.append(ratios_obs)

obs_share_B_arr = np.array(obs_share_B_runs)  # (3 runs, 6 ratios)
mean_share = obs_share_B_arr.mean(axis=0)
std_share  = obs_share_B_arr.std(axis=0)

obs_ratio_arr = np.array(obs_ratio_runs)
mean_ratio = obs_ratio_arr.mean(axis=0)
std_ratio  = obs_ratio_arr.std(axis=0)

# --- Plot ---
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 4.5))

# (a) share of B (%)
xs = np.array(expected_share_B) * 100
ax1.plot([0, 100], [0, 100], "--", color="gray", label="ideal y=x")
for name, shares in zip(runs.keys(), obs_share_B_runs):
    ax1.scatter(xs, np.array(shares)*100, alpha=0.5, s=40, label=name)
ax1.errorbar(xs, mean_share*100, yerr=std_share*100,
             fmt="o-", color="black", capsize=4, linewidth=2,
             markersize=6, label="média ± dp")
ax1.set_xlabel("Compartilhamento esperado de B  (%)")
ax1.set_ylabel("Compartilhamento observado de B  (%)")
ax1.set_title("(a) CPU share vs proporção de tickets")
ax1.set_xlim(45, 100)
ax1.set_ylim(45, 100)
ax1.grid(True, alpha=0.3)
ax1.legend(loc="lower right", fontsize=9)

# (b) ratio ticks_B / ticks_A
ax2.plot([0, 22], [0, 22], "--", color="gray", label="ideal y=x")
for name, ratios_obs in zip(runs.keys(), obs_ratio_runs):
    ax2.scatter(ratios, ratios_obs, alpha=0.5, s=40, label=name)
ax2.errorbar(ratios, mean_ratio, yerr=std_ratio,
             fmt="o-", color="black", capsize=4, linewidth=2,
             markersize=6, label="média ± dp")
ax2.set_xlabel("Razão de tickets  $T_B / T_A$")
ax2.set_ylabel("Razão observada de ticks  $t_B / t_A$")
ax2.set_title("(b) Razão observada vs razão de tickets")
ax2.set_xlim(0, 22)
ax2.set_ylim(0, 30)  # observed ratio can overshoot for small denominators
ax2.grid(True, alpha=0.3)
ax2.legend(loc="upper left", fontsize=9)

plt.suptitle("Lottery scheduler no xv6-riscv — proporcionalidade entre tickets e CPU\n"
             "(3 runs independentes, 6 experimentos por run, janela de ~20s, CPUS=1)",
             fontsize=11)
plt.tight_layout()
plt.savefig("schedgraph.png", dpi=140, bbox_inches="tight")
print("wrote schedgraph.png")

# Also print the aggregated table for the report
print("\nAggregated results (mean of 3 runs):")
print(f"{'R_tkt':>5} {'exp%':>5} {'obs%':>5} {'±sd':>5}")
for r, e, m, s in zip(ratios, expected_share_B, mean_share, std_share):
    print(f"{r:5d} {e*100:5.1f} {m*100:5.1f} {s*100:5.1f}")