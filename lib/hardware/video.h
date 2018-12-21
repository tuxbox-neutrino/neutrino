#if HAVE_COOL_HARDWARE
#include <cs_api.h>
#include <video_cs.h>
#elif USE_STB_HAL
#include <init_td.h>
#include <video_hal.h>
#else
#error neither HAVE_COOL_HARDWARE nor USE_STB_HAL defined.
#error do you need to include config.h?
#endif
