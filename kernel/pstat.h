#ifndef _PSTAT_H_
#define _PSTAT_H_

#include "param.h"

// Process status snapshot returned by getpinfo().
// One slot per entry in the kernel proc table.
struct pstat {
  int inuse[NPROC];    // 1 if this slot is in use, 0 otherwise
  int tickets[NPROC];  // number of lottery tickets held by the process
  int pid[NPROC];      // PID of the process
  int ticks[NPROC];    // timer ticks the process has consumed
};

#endif // _PSTAT_H_
