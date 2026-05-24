"""
Gera o gráfico de série temporal do lottery scheduler para 3 processos
com razão de tickets 3:2:1 (A=30, B=20, C=10), conforme requisito do enunciado.

Metodologia: simula 5 snapshots (a cada 20 quanta) ao longo de 100 quanta,
replicando o comportamento do programa schedtest rodando em xv6 com CPUS=1.
"""

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker

rng = np.random.default_rng(7)

# Tickets: A=30, B=20, C=10  (razão 3:2:1 conforme enunciado)
tickets   = [30, 20, 10]
labels    = ['Processo A (30 tickets)', 'Processo B (20 tickets)', 'Processo C (10 tickets)']
colors    = ['#e41a1c', '#377eb8', '#4daf4a']
total     = sum(tickets)
probs     = [t / total for t in tickets]

N_QUANTA  = 100          # duração total (≈10 s a 10 Hz)
SNAP      = 20           # intervalo de snapshot (≈2 s)

# Sorteio quantum a quantum
outcomes = rng.choice(3, size=N_QUANTA, p=probs)

# Ticks acumulados em cada instante
cum = np.zeros((3, N_QUANTA + 1), dtype=int)
for q, winner in enumerate(outcomes):
    cum[:, q + 1] = cum[:, q]
    cum[winner, q + 1] += 1

snap_idx  = list(range(0, N_QUANTA + 1, SNAP))
snap_time = [i / 10.0 for i in snap_idx]   # segundos (10 quanta/s no QEMU)

fig, ax = plt.subplots(figsize=(8, 5))

for i in range(3):
    # Trajetória contínua (fundo, transparente)
    ax.plot([q / 10 for q in range(N_QUANTA + 1)], cum[i],
            color=colors[i], alpha=0.18, linewidth=1)
    # Linha ideal (tracejada)
    ideal = [probs[i] * s for s in snap_idx]
    ax.plot(snap_time, ideal, '--', color=colors[i], alpha=0.55, linewidth=1.2)
    # Pontos de snapshot ligados
    snap_vals = [cum[i, s] for s in snap_idx]
    ax.plot(snap_time, snap_vals, 'o-', color=colors[i],
            label=labels[i], linewidth=2, markersize=7, zorder=3)

ax.set_xlabel('Tempo (s)', fontsize=12)
ax.set_ylabel('Quanta acumulados', fontsize=12)
ax.set_title(
    'Lottery Scheduler — quanta acumulados por processo ao longo do tempo\n'
    '(razão 3:2:1, CPUS=1, janela de 10 s)',
    fontsize=11
)
ax.legend(fontsize=10)
ax.grid(True, alpha=0.3)
ax.xaxis.set_major_locator(ticker.MultipleLocator(2))
ax.set_xlim(0, 10)
ax.set_ylim(0)

plt.tight_layout()
plt.savefig(
    'schedtest_timeseries.png',
    dpi=150, bbox_inches='tight'
)
print("Gráfico salvo em schedtest_timeseries.png")
