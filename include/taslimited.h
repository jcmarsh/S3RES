#ifndef _TAS_LIMITED_H_
#define _TAS_LIMITED_H_

#include "../include/cpu.h"
#include "../include/time.h"

int InitTAS(cpu_id_t cpu, int prio_offset);
void OptOutRT(void);
void EveryTAS(void);

#endif // _TAS_LIMITED_H_
