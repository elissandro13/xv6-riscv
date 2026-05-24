// Simple xorshift32 PRNG used by the lottery scheduler.
// Not cryptographically secure; sufficient for picking lottery winners.
//
// xv6 runs the scheduler on every CPU, so concurrent callers can race
// on the PRNG state. A small spinlock keeps the sequence well-defined.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

static uint32 rand_state = 2463534242U; // fallback seed if r_time() is 0
static struct spinlock rand_lock;

// Initialize the PRNG's spinlock and mix the hardware time counter into
// the seed. Called once from main() during boot.
void
randinit(void)
{
  initlock(&rand_lock, "rand");
  // Use the hardware time-of-boot register to vary the sequence
  // across runs. xorshift requires a non-zero state, so OR in 1
  // to be safe.
  uint64 t = r_time();
  uint32 s = (uint32)(t ^ (t >> 32));
  if(s != 0)
    rand_state = s | 1u;
}

// Advance the xorshift state and return the new 32-bit value.
// Caller must hold rand_lock.
static uint32
xorshift32(void)
{
  uint32 x = rand_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rand_state = x;
  return x;
}

// Return a pseudo-random integer in the inclusive range [1, max].
// Caller guarantees max >= 1.
int
rand_int(int max)
{
  acquire(&rand_lock);
  uint32 r = xorshift32();
  release(&rand_lock);
  return (int)(r % (uint32)max) + 1;
}
