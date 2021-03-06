#ifndef INC_S_PORT_H_
#define INC_S_PORT_H_

/* Copyright xhawk, MIT license */

/* Timer functions need to be implemented on a new porting. */

/* === Timer functions on nuvoton M051 === */
#include "M051Series.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 1. define a type for clock */
typedef uint32_t my_clock_t;
typedef int32_t my_clock_diff_t;

/* 2. define the clock ticks count for one second */
#define MY_CLOCKS_PER_SEC 1024

#ifdef __cplusplus
}
#endif

#include "s_port_armv6m.h"

#endif

