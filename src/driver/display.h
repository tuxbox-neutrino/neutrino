/* helper for different display CVFD implementations */
#if HAVE_COOL_HARDWARE
#include <driver/vfd.h>
#endif
#if HAVE_TRIPLEDRAGON || HAVE_SPARK_HARDWARE || HAVE_AZBOX_HARDWARE || HAVE_GENERIC_HARDWARE
#include <driver/lcdd.h>
#endif
