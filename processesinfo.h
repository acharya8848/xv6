#ifndef PROCESSESINFO_H_
#define PROCESSESINFO_H_

#include "param.h"

// Size of the process info array is 772 bytes
struct processes_info{
	int num_processes;
	int pids[NPROC];
	int times_scheduled[NPROC];		// times_scheduled = number of times process has been scheduled
	int tickets[NPROC];				// tickets = number of tickets set by settickets()
};

#endif /* PROCESSESINFO_H_ */