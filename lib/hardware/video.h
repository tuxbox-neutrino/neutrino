#if HAVE_CST_HARDWARE
#include <cs_api.h>
#include <video_cs.h>
#elif USE_STB_HAL
#include <init.h>
#include <video_hal.h>
#else
#error neither HAVE_CST_HARDWARE nor USE_STB_HAL defined.
#error do you need to include config.h?
#endif
