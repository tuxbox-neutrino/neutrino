/* helper for different display CVFD implementations */
#if HAVE_CST_HARDWARE
#include <driver/vfd.h>
#endif
#if HAVE_SPARK_HARDWARE || HAVE_GENERIC_HARDWARE || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
#include <driver/simple_display.h>
#endif
#ifdef ENABLE_GRAPHLCD
#include <driver/glcd/glcd.h>
#endif
