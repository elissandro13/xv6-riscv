// schedtest: spawn CPU-bound children with different ticket counts and
// observe how the lottery scheduler allocates CPU time.
//
// Usage: schedtest [t0 t1 ... tN]
//        schedtest             (defaults: 10 20 30)
//
// Methodology: each child sets its ticket count, then busy-loops
// indefinitely. The parent waits a fixed wall-clock interval, snapshots
// the proc table via getpinfo(), then kills the children. Because the
// workload is time-bounded (not iteration-bounded), the ticks each
// child accumulated track its share of CPU time -- and therefore its
// share of tickets.
//
// Run xv6 with CPUS=1 (e.g. `make qemu CPUS=1`) to force contention on
// a single CPU; otherwise children can run in parallel on separate
// cores and the proportionality disappears.

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/pstat.h"
#include "user/user.h"

#define MAXCHILD     8
#define RUN_TICKS    100   // wall-clock observation window (10 ticks/sec -> ~10s)
#define SNAP_TICKS   20    // progress snapshots every ~2s

static int default_tickets[3] = {10, 20, 30};

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

int
main(int argc, char *argv[])
{
  int n;
  int tickets[MAXCHILD];
  int pids[MAXCHILD];

  if(argc >= 2) {
    n = argc - 1;
    if(n > MAXCHILD) {
      fprintf(2, "schedtest: at most %d children supported\n", MAXCHILD);
      exit(1);
    }
    for(int i = 0; i < n; i++)
      tickets[i] = atoi(argv[i+1]);
  } else {
    n = 3;
    for(int i = 0; i < n; i++)
      tickets[i] = default_tickets[i];
  }

  // Parent grabs many tickets so getpinfo()/printf calls are not
  // starved while it polls. Pause()s release the CPU anyway.
  if(settickets(100) < 0) {
    fprintf(2, "schedtest: parent settickets failed\n");
    exit(1);
  }

  printf("schedtest: spawning %d CPU-bound children\n", n);
  for(int i = 0; i < n; i++) {
    int pid = fork();
    if(pid < 0) {
      fprintf(2, "schedtest: fork failed\n");
      exit(1);
    }
    if(pid == 0) {
      if(settickets(tickets[i]) < 0) {
        fprintf(2, "schedtest: child settickets(%d) failed\n", tickets[i]);
        exit(1);
      }
      busywork_forever();
      exit(0);  // unreachable
    }
    pids[i] = pid;
    printf("  child %d: pid=%d tickets=%d\n", i, pid, tickets[i]);
  }

  // Observe progress at regular intervals.
  struct pstat ps;
  int snapshots = RUN_TICKS / SNAP_TICKS;
  for(int round = 0; round < snapshots; round++) {
    pause(SNAP_TICKS);
    if(getpinfo(&ps) < 0) {
      fprintf(2, "schedtest: getpinfo failed\n");
      goto cleanup;
    }
    printf("\n--- snapshot %d (~%ds elapsed) ---\n",
           round, ((round + 1) * SNAP_TICKS) / 10);
    int total_ticks = 0;
    for(int i = 0; i < n; i++) {
      int idx = find_proc(&ps, pids[i]);
      if(idx < 0) {
        printf("  pid=%d  (gone)\n", pids[i]);
      } else {
        printf("  pid=%d  tickets=%d  ticks=%d\n",
               ps.pid[idx], ps.tickets[idx], ps.ticks[idx]);
        total_ticks += ps.ticks[idx];
      }
    }
    if(total_ticks > 0) {
      int ttotal = 0;
      for(int i = 0; i < n; i++)
        ttotal += tickets[i];
      printf("  observed share | expected share\n");
      for(int i = 0; i < n; i++) {
        int idx = find_proc(&ps, pids[i]);
        int t = (idx < 0) ? 0 : ps.ticks[idx];
        printf("    pid=%d: %d%% | %d%%\n",
               pids[i], (t * 100) / total_ticks, (tickets[i] * 100) / ttotal);
      }
    }
  }

cleanup:
  // Stop the spinning children and reap them.
  for(int i = 0; i < n; i++)
    kill(pids[i]);
  for(int i = 0; i < n; i++)
    wait(0);

  printf("\nschedtest: done\n");
  exit(0);
}
