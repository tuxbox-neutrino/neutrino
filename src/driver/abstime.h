#include <stdint.h>

#ifndef _ABS_TIME_H_
#define _ABS_TIME_H_
#ifdef __cplusplus
extern "C"
{
#endif
extern time_t time_monotonic_ms(void);
extern time_t time_monotonic(void);
uint64_t time_monotonic_us(void);
#ifdef __cplusplus
}
#endif
#endif
