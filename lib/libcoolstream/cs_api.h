#ifndef __CS_API_H_
#define __CS_API_H_

typedef void (*cs_messenger) (unsigned int msg, unsigned int data);

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

// Initialization
void cs_api_init(void);
void cs_api_exit(void);

// Memory helpers
void *cs_malloc_uncached(size_t size);
void cs_free_uncached(void *ptr);
void *cs_phys_addr(void *ptr);

// Callback function helpers
void cs_register_messenger(cs_messenger messenger);
void cs_deregister_messenger(void);
cs_messenger cs_get_messenger(void);

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
unsigned int cs_get_revision(void);

#endif //__CS_API_H_
