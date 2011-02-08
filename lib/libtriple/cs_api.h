/* compatibility header for tripledragon. I'm lazy, so I just left it
   as "cs_api.h" so that I don't need too many ifdefs in the code */

#ifndef __CS_API_H_
#define __CS_API_H_

#include "init_td.h"
typedef void (*cs_messenger) (unsigned int msg, unsigned int data);

#if 0
enum CS_LOG_MODULE {
	CS_LOG_CI	= 0,
	CS_LOG_HDMI_CEC,
	CS_LOG_HDMI,
	CS_LOG_VIDEO,
	CS_LOG_VIDEO_DRM,
	CS_LOG_AUDIO,
	CS_LOG_DEMUX,
	CS_LOG_DENC,
	CS_LOG_PVR_RECORD,
	CS_LOG_PVR_PLAY,
	CS_LOG_POWER_CTRL,
	CS_LOG_POWER_CLK,
	CS_LOG_MEM,
	CS_LOG_API,
};
#endif

inline void cs_api_init()
{
	init_td_api();
};

inline void cs_api_exit()
{
	shutdown_td_api();
};

#define cs_malloc_uncached	malloc
#define cs_free_uncached	free

// Callback function helpers
static inline void cs_register_messenger(cs_messenger) { return; };
static inline void cs_deregister_messenger(void) { return; };
//cs_messenger cs_get_messenger(void);

#if 0
// Logging functions
void cs_log_enable(void);
void cs_log_disable(void);
void cs_log_message(const char *prefix, const char *fmt, ...);
void cs_log_module_enable(enum CS_LOG_MODULE module);
void cs_log_module_disable(enum CS_LOG_MODULE module);
void cs_log_module_message(enum CS_LOG_MODULE module, const char *fmt, ...);

// TS Routing
unsigned int cs_get_ts_output(void);
int cs_set_ts_output(unsigned int port);

// Serial nr and revision accessors
unsigned long long cs_get_serial(void);
#endif
/* compat... HD1 seems to be version 6. everything newer ist > 6... */
static inline unsigned int cs_get_revision(void) { return 1; };
extern int cnxt_debug;
#endif //__CS_API_H_
