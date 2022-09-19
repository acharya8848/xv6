#include "types.h"
#include "stat.h"
#include "user.h"

// Quality of life
#define WRITE 1
#define READ  0

int
main(int argc, char *argv[])
{
  int fd[2];
  if (pipe(fd) != 0) {
    printf(1, "pipe failed\r\n");
    exit();
  }
  // Reset the writecount
  setwritecount(0);
  // Write to the pipe
  write(fd[WRITE], "hello world!", 12);
  if (writecount() == 1) {
    printf(1, "Single write succeeded %d\r\n", 1);
  } else {
    printf(1, "Single write failed %d\r\n", writecount());
  }
  // Set write count
  if ((setwritecount(5) != 0) || (writecount() != 5)){
    printf(1, "setwritecount failed\r\n");
  }
  // Write after setwritecount
  write(fd[WRITE], "another one!", 12);
  if (writecount() == 6) {
    printf(1, "Double write succeeded %d\r\n", 6);
  } else {
    printf(1, "Double write failed %d \r\n", writecount());
  }

  // Close the pipe
  close(fd[WRITE]);
  close(fd[READ]);

  // exit
  exit();
}
