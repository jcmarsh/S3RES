#ifndef _TAS_LIMITED_H_
#define _TAS_LIMITED_H_

#include "../include/cpu.h"
#include "../include/time.h"

int sched_set_policy(const pid_t pid, const int priority);
int InitTAS(cpu_id_t cpu, int priority);
void OptOutRT(void);

#endif // _TAS_LIMITED_H_
