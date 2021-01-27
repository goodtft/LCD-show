#pragma once

#include <sys/types.h>
#include <inttypes.h>

extern uint64_t totalCpuMemoryAllocated;

void *Malloc(size_t bytes, const char *reason);
