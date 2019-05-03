/* helper for different display CVFD implementations */
#if HAVE_COOL_HARDWARE
#include <driver/vfd.h>
#endif
#if HAVE_TRIPLEDRAGON
#include <driver/lcdd.h>
#endif
#if HAVE_SPARK_HARDWARE || HAVE_AZBOX_HARDWARE || HAVE_GENERIC_HARDWARE  || HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
#include <driver/simple_display.h>
#endif
