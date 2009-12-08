#ifndef HELPERS_H_
#define HELPERS_H_

#include <inttypes.h>

uint32_t getbits(const uint8_t* buf, uint32_t offset, uint8_t len);
void hexdump(uint8_t* buf);

#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

#define MIN(a,b) (((a)<(b))?(a):(b))

#endif
