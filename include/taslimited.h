#ifndef _TAS_LIMITED_H_
#define _TAS_LIMITED_H_

#include "../include/cpu.h"
#include "../include/time.h"

void optOutRT();
int InitTAS(cpu_id_t cpu, cpu_speed_t *cpu_speed, int prio_offset);
int EveryTAS();

#endif // _TAS_LIMITED_H_
