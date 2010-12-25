#if HAVE_COOL_HARDWARE
#include <dmx_cs.h>
#elif HAVE_TRIPLEDRAGON
#include <dmx_td.h>
#else
#error neither HAVE_COOL_HARDWARE nor HAVE_TRIPLEDRAGON defined.
#error do you need to include config.h?
#endif

