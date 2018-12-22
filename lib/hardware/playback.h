#if HAVE_COOL_HARDWARE
#include <playback_cs.h>
#elif USE_STB_HAL
#include <playback_hal.h>
#else
#error neither HAVE_COOL_HARDWARE nor USE_STB_HAL defined.
#error do you need to include config.h?
#endif
