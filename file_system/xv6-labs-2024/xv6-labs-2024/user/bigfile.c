#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

int
main()
{
  char buf[BSIZE];
  int fd, i;
  int blocks;

  printf("bigfile running\n");

  fd = open("bigfile", O_CREATE | O_WRONLY);
  if(fd < 0){
    printf("bigfile: cannot open bigfile for writing\n");
    exit(1);
  }

  blocks = 0;
  while(1){
    *(int*)buf = blocks;
    int n = write(fd, buf, sizeof(buf));
    if(n <= 0)
      break;
    blocks++;
    if(blocks % 1000 == 0)
      printf("w");
  }

  printf("\nwrote %d blocks\n", blocks);

  close(fd);
  fd = open("bigfile", O_RDONLY);
  if(fd < 0){
    printf("bigfile: cannot re-open bigfile for reading\n");
    exit(1);
  }

  for(i = 0; i < blocks; i++){
    int n = read(fd, buf, sizeof(buf));
    if(n <= 0){
      printf("bigfile: error reading block %d\n", i);
      exit(1);
    }
    if(*(int*)buf != i){
      printf("bigfile: read the wrong data, expected %d\n", i);
      exit(1);
    }
  }

  close(fd);
  printf("done; ok\n");

  exit(0);
}
