#include "config.h"
#include "mem_alloc.h"

#include <memory.h>
#include <stdlib.h>
#include <stdio.h>

uint64_t totalCpuMemoryAllocated = 0;

void *Malloc(size_t bytes, const char *reason)
{
	void *ptr = malloc(bytes);
	if (ptr)
	{
		totalCpuMemoryAllocated += bytes; // Currently we don't decrement this, so this only counts up (all allocations are persistent so far, so that's ok for now)
//		printf("Allocated %zd bytes of CPU memory for %s. Total memory allocated: %llu bytes\n", bytes, reason, totalCpuMemoryAllocated);
		return ptr;
	}
	else
	{
		printf("Failed to allocate %zd bytes of memory for %s!\n", bytes, reason);
		exit(1);
	}
}
