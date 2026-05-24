// schedgraph: characterize the lottery scheduler's proportionality.
//
// For each ratio R in a hardcoded list, spawn two CPU-bound children
// with tickets (BASE, BASE*R), let them run for RUN_TICKS ticks of
// wall clock, snapshot via getpinfo(), kill them, and print one line
// of summary. The output is meant to be copy-pasted into a plotting
// tool (gnuplot, matplotlib, spreadsheet) to produce the OSTEP-style
// graph of expected vs observed CPU share.
//
// IMPORTANT: run xv6 with CPUS=1 (e.g. `make qemu CPUS=1`). With more
// than one CPU, both children fit on separate cores and the lottery
// never has to choose between them.

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/pstat.h"
#include "user/user.h"

#define BASE_TICKETS 10
#define RUN_TICKS    200  // ~20s observation window per experiment

// Ticket ratios to sweep (B/A). A always has BASE_TICKETS tickets,
// B has BASE_TICKETS*ratio tickets.
static int ratios[] = {1, 2, 3, 5, 10, 20};

static void
busywork_forever(void)
{
  volatile uint64 sink = 0;
  for(;;)
    sink++;
}

static int
find_proc(struct pstat *ps, int pid)
{
  for(int i = 0; i < NPROC; i++)
    if(ps->inuse[i] && ps->pid[i] == pid)
      return i;
  return -1;
}

// Run one experiment. Forks two children with t_a / t_b tickets each,
// busy-loops for RUN_TICKS, snapshots, kills, prints a result line.
static void
run_experiment(int t_a, int t_b)
{
  int pid_a, pid_b;
  struct pstat ps;

  pid_a = fork();
  if(pid_a < 0) {
    fprintf(2, "schedgraph: fork A failed\n");
    return;
  }
  if(pid_a == 0) {
    settickets(t_a);
    busywork_forever();
    exit(0);
  }

  pid_b = fork();
  if(pid_b < 0) {
    fprintf(2, "schedgraph: fork B failed\n");
    kill(pid_a); wait(0);
    return;
  }
  if(pid_b == 0) {
    settickets(t_b);
    busywork_forever();
    exit(0);
  }

  // Let the children run for the observation window.
  pause(RUN_TICKS);

  if(getpinfo(&ps) < 0) {
    fprintf(2, "schedgraph: getpinfo failed\n");
    kill(pid_a); kill(pid_b);
    wait(0); wait(0);
    return;
  }

  int ia = find_proc(&ps, pid_a);
  int ib = find_proc(&ps, pid_b);
  int ticks_a = (ia < 0) ? 0 : ps.ticks[ia];
  int ticks_b = (ib < 0) ? 0 : ps.ticks[ib];
  int total   = ticks_a + ticks_b;

  // Reap children before printing so the line isn't interleaved with
  // anything else.
  kill(pid_a);
  kill(pid_b);
  wait(0);
  wait(0);

  int exp_share_b = (t_b * 100) / (t_a + t_b);
  int obs_share_b = (total > 0) ? (ticks_b * 100) / total : 0;

  // ratio_t = t_b / t_a (integer)
  // ratio_obs_x100 = (ticks_b * 100) / ticks_a (scaled for two decimals)
  int ratio_obs_x100 = (ticks_a > 0) ? (ticks_b * 100) / ticks_a : 0;
  int ratio_int = ratio_obs_x100 / 100;
  int ratio_frac = ratio_obs_x100 % 100;

  printf("T_A=%d\tT_B=%d\tR_tkt=%d\tticks_A=%d\tticks_B=%d\tR_obs=%d.",
         t_a, t_b, t_b / t_a, ticks_a, ticks_b, ratio_int);
  if(ratio_frac < 10)
    printf("0");
  printf("%d\texp_B=%d%%\tobs_B=%d%%\n",
         ratio_frac, exp_share_b, obs_share_b);
}

int
main(int argc, char *argv[])
{
  // Parent gives itself many tickets so its pause()/printf() don't
  // get starved when the children are spinning.
  if(settickets(200) < 0) {
    fprintf(2, "schedgraph: parent settickets failed\n");
    exit(1);
  }

  printf("schedgraph: %d experiments, %d ticks each (~%ds)\n",
         (int)(sizeof(ratios)/sizeof(ratios[0])),
         RUN_TICKS, RUN_TICKS / 10);
  printf("Columns (tab-separated):\n");
  printf("  T_A     = tickets of child A\n");
  printf("  T_B     = tickets of child B\n");
  printf("  R_tkt   = T_B / T_A (expected ratio of CPU time)\n");
  printf("  ticks_A = quanta consumed by child A in the window\n");
  printf("  ticks_B = quanta consumed by child B in the window\n");
  printf("  R_obs   = ticks_B / ticks_A (observed ratio)\n");
  printf("  exp_B   = expected share of B = T_B/(T_A+T_B)\n");
  printf("  obs_B   = observed share of B = ticks_B/(ticks_A+ticks_B)\n");
  printf("\n");

  for(int i = 0; i < (int)(sizeof(ratios)/sizeof(ratios[0])); i++) {
    int r = ratios[i];
    run_experiment(BASE_TICKETS, BASE_TICKETS * r);
  }

  printf("\nschedgraph: done\n");
  exit(0);
}
