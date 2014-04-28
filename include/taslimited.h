#ifndef _TAS_LIMITED_H_
#define _TAS_LIMITED_H_

#include <sys/resource.h>
#include <unistd.h>

#include "time.h"
#include "cpu.h"
#include "scheduler.h"
#include "force.h"

int InitTAS(cpu_id_t cpu, cpu_speed_t *cpu_speed);

#endif // _TAS_LIMITED_H_
