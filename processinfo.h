#ifndef PROCESSINFO_H_
#define PROCESSINFO_H_

#include "param.h"

// Size of the process info array is 772 bytes
typedef struct {
	int num_processes;
	int pids[NPROC];
	int times_scheduled[NPROC];		// times_scheduled = number of times process has been scheduled
	int tickets[NPROC];				// tickets = number of tickets set by settickets()
} processes_info;

#endif /* PROCESSINFO_H_ */