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

#define CS_CHIP_APOLLO			0x8490
#define CS_CHIP_SHINER			0x8470
#define CS_CHIP_KRONOS_S		0x7540
#define CS_CHIP_KRONOS_C		0x7550
#define CS_CHIP_KRONOS_IP		0x7530
#define CS_CHIP_NEVIS			0x0000	/* workaround for nonexistant nevis chiptype */

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

/* Dummy for compatibility with HD2 */
#define cs_new_auto_videosystem();

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
int cs_set_ts_config(unsigned int port, tsrouter_hsdp_config_t *hsdp_config);
int cs_get_ts_config(unsigned int port, tsrouter_hsdp_config_t *hsdp_config);
int cs_set_tsp_config(unsigned int port, tsrouter_tsp_config_t *tsp_config);
int cs_get_tsp_config(unsigned int port, tsrouter_tsp_config_t *tsp_config);

// Serial nr and revision accessors
unsigned long long cs_get_serial(void);
unsigned int cs_get_revision(void);
/* Dummy function for compatibility with hd2 */
unsigned int cs_get_chip_type(void);


// library version functions
typedef struct cs_libversion_t
{
    int    vMajor;
    int    vMinor;
    int    vPatch;
    char   vStr[16];
    char   vGit[41];
    char   vGitDescribe[64];
    time_t vGitTime;
} cs_libversion_struct_t;

void cs_get_lib_version(cs_libversion_t *ver);

/* return value:
   -------------
   1 Library version newer than given version
   0 library version equals given version
  -1 Library version older than given version */
int  cs_compare_lib_versions(int Major, int Minor, int Patch);

#endif //__CS_API_H_
