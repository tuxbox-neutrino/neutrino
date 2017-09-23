#include <stdint.h>

#ifndef _ABS_TIME_H_
#define _ABS_TIME_H_
#ifdef __cplusplus
extern "C"
{
#endif
time_t time_monotonic(void);
int64_t time_monotonic_ms(void);
uint64_t time_monotonic_us(void);
#ifdef __cplusplus
}
#endif
#endif
