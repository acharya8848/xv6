#include "types.h"
#include "stat.h"
#include "user.h"

// Quality of life
#define WRITE 1
#define READ  0

int
main(int argc, char *argv[])
{
  printf(1, "Resetting writecount to 0 ... ");
  if ((setwritecount(0) != 0) || (writecount() != 0)){
    printf(1, "failed\r\n");
    exit();
  }
  write(1, "done\r\n", 6);
  // Reset the writecount
  if (writecount() == 1) {
    printf(1, "First write increased the writecount to %d\r\n", writecount());
  } else {
    printf(1, "Single write failed %d\r\n", writecount());
    exit();
  }
  // Set write count
  printf(1, "Setting writecount to 5 ... ");
  if ((setwritecount(5) != 0) || (writecount() != 5)){
    printf(1, "failed\r\n");
    exit();
  }
  // Write after setwritecount
  write(1, "done\r\n", 6);
  if (writecount() == 6) {
    printf(1, "Write after setting writecount to 5 increased it to %d\r\n", writecount());
  } else {
    printf(1, "Double write failed %d \r\n", writecount());
    exit();
  }

  printf(1, "All tests finished successfully. New system calls writecount and setwritecount now functional.\r\n");

  // exit
  exit();
}
