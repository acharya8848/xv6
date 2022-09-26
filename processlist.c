#include "types.h"
#include "param.h"
#include "processesinfo.h"
#include "user.h"

int main(int argc, char *argv[])
{
    struct processes_info info;
    info.num_processes = -9999;  // to make sure getprocessesinfo() doesn't
                                 // depend on its initial value
    getprocessesinfo(&info);
    if (info.num_processes < 0) {
        printf(1, "ERROR: negative number of processes!\n"
                  "Myabe getprocessesinfo() assumes that num_processes is\n"
                  "always initialized to 0?\n");
    }
    printf(1, "%d running processes\n", info.num_processes);
    printf(1, "PID\tTICKETS\tTIMES-SCHEDULED\n");
    for (int i = 0; i < info.num_processes; ++i) {
        printf(1, "%d\t%d\t%d\n", info.pids[i], info.tickets[i], info.times_scheduled[i]);
    }
    exit();
}
