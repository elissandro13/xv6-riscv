#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 2){
    fprintf(2, "Usage: getcnt <syscall number>\n");
    exit(1);
  }

  int num = atoi(argv[1]);
  int cnt = getcnt(num);

  if(cnt < 0){
    fprintf(2, "getcnt: invalid syscall number %d\n", num);
    exit(1);
  }

  printf("syscall %d has been called %d times\n", num, cnt);
  exit(0);
}
