#pragma once

#define ROUND_TO_NEAREST_INT(x) ((int)lround((x)))
#define ROUND_TO_FLOOR_INT(x) ((int)(floor((x))))
#define ROUND_TO_CEIL_INT(x) ((int)(ceil((x))))

#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#define MAX(x, y) ((x) >= (y) ? (x) : (y))

#define ABS(x) ((x) < 0 ? (-(x)) : (x))

#define SWAPU32(x, y) { uint32_t tmp = x; x = y; y = tmp; }

#ifndef ALIGN_DOWN
#define ALIGN_DOWN(ptr, alignment) (((ptr)) & ~((alignment)-1))
#endif

#ifndef ALIGN_UP
#define ALIGN_UP(ptr, alignment) (((ptr) + ((alignment)-1)) & ~((alignment)-1))
#endif

#ifdef KERNEL_MODULE
#define LOG(...) do { printk(KERN_INFO __VA_ARGS__); } while(0)
#define FATAL_ERROR(msg) do { pr_alert(msg "\n"); return -1; } while(0)
#else
#define LOG(...) do { printf(__VA_ARGS__); printf("\n"); } while(0)
#define FATAL_ERROR(msg) do { fprintf(stderr, "%s\n", msg); syslog(LOG_ERR, msg); exit(1); } while(0)
#endif

#ifdef KERNEL_MODULE
#define PRINT_FLAG_2(flag_str, flag, shift) printk(KERN_INFO flag_str ": %x", (reg & flag) >> shift)
#else
#define PRINT_FLAG_2(flag_str, flag, shift) printf(flag_str ": %x\n", (reg & flag) >> shift)
#endif

#define PRINT_FLAG(flag) PRINT_FLAG_2(#flag, flag, flag##_SHIFT)

#ifndef KERNEL_MODULE
#define cpu_relax() asm volatile("yield" ::: "memory")
#endif
