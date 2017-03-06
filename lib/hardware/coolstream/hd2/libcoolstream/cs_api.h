/*******************************************************************************/
/*                                                                             */
/* libcoolstream/cs_api.h                                                      */
/*   Public header file for CoolStream Public API                              */
/*                                                                             */
/* (C) 2010 CoolStream International                                           */
/*                                                                             */
/* $Id::                                                                     $ */
/*******************************************************************************/
#ifndef __CS_API_H_
#define __CS_API_H_

#include <control.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>

typedef void (*cs_messenger) (unsigned int msg, unsigned int data);

#define CS_REVISION_CHIP_TYPE(a)	(((a) >> 16) & 0xFFF0)
#define CS_REVISION_HW_VERSION_MAJOR(a)	(((a) >>  8)  & 0x00FF)
#define CS_REVISION_HW_HAS_CI(a)	(!(CS_REVISION_HW_VERSION_MAJOR(a) & 0xC0))
#define CS_REVISION_HW_VERSION_MINOR(a)	(((a) >>  0)  & 0x00FF)
#define CS_CHIP_APOLLO			0x8490
#define CS_CHIP_SHINER			0x8470
#define CS_CHIP_KRONOS_S		0x7540
#define CS_CHIP_KRONOS_C		0x7550
#define CS_CHIP_KRONOS_IP		0x7530

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
	CS_LOG_FILEPLAYER,
	CS_LOG_POWER_CTRL,
	CS_LOG_POWER_CLK,
	CS_LOG_MEM,
	CS_LOG_API,
	CS_LOG_CA
};

// Initialization
void cs_api_init(void);
void cs_api_exit(void);
void cs_new_auto_videosystem();

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
int cs_set_ts_ci_clock(unsigned int speed);
int cs_get_ts_ci_clock(unsigned int *speed);
int cs_set_ts_ci_clk_src(unsigned int clk_src);
int cs_get_ts_ci_clk_src(unsigned int *clk_src);
int cs_set_ts_config(unsigned int port, tsrouter_hsdp_config_t *hsdp_config);
int cs_get_ts_config(unsigned int port, tsrouter_hsdp_config_t *hsdp_config);
int cs_set_tsp_config(unsigned int port, tsrouter_tsp_config_t *tsp_config);
int cs_get_tsp_config(unsigned int port, tsrouter_tsp_config_t *tsp_config);

// Serial nr and revision accessors
unsigned long long cs_get_serial(void);
unsigned int cs_get_revision(void);
unsigned int cs_get_chip_type(void);
bool cs_box_has_ci(void);

unsigned int cs_get_chip_id(void);
unsigned int cs_get_chip_rev_id(void);

#endif //__CS_API_H_
