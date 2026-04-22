#include "kernel/types.h"
#include "kernel/syscall.h"
#include "user/user.h"

int
main(void)
{
  printf("getpid antes: %d\n", getcnt(SYS_getpid));
  getpid();
  getpid();
  getpid();
  printf("getpid depois: %d\n", getcnt(SYS_getpid));

  printf("write antes: %d\n", getcnt(SYS_write));
  printf("write depois: %d\n", getcnt(SYS_write));

  printf("getcnt invalido (-1): %d\n", getcnt(0));
  printf("getcnt invalido (-1): %d\n", getcnt(99));

  exit(0);
}
