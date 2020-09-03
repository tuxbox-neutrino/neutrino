#if HAVE_CST_HARDWARE
#include <record_cs.h>
#elif USE_STB_HAL
#include <record_hal.h>
#else
#error neither HAVE_CST_HARDWARE nor USE_STB_HAL defined.
#error do you need to include config.h?
#endif
