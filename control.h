/********************************************************************
* Description: Control driver for PWM and Scart
* Author: CoolStream Dev. Team
* Created at: Fri Jun 26 08:11:43 CEST 2009
*
* Copyright (c) 2009 CoolStream International Ltd. All rights reserved.
*
********************************************************************/

#ifndef __CONTROL_H
#define __CONTROL_H

#ifndef __KERNEL__
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
#include <sys/ioctl.h>

#else
#include <linux/types.h>
#include <asm/ioctl.h>
#endif

#include "tsrouter.h"

typedef struct _scart_status {
	bool	widescreen;
	bool	function;
	bool	fastblank;
} scart_status_t;

typedef struct _pwm_status {
	u16	pwm_pulse_first;
	u16	pwm_pulse_second;
	u32	pwm_clock;
} pwm_status_t;

typedef struct cs_control_data {
	scart_status_t scart_status;
	pwm_status_t pwm_status;
	bool hdd_power;
	u32 readers;
	u32 sys_serial_high, sys_serial_low;
	u32 sys_revision;
} cs_control_data_t;

/* ioctls */
#define CS_CONTROL_MAGIC		0xDE
#define IOC_CONTROL_WIDESCREEN		_IOW(CS_CONTROL_MAGIC, 8, unsigned int)
#define IOC_CONTROL_TVAV		_IOW(CS_CONTROL_MAGIC, 9, unsigned int)
#define IOC_CONTROL_RGB			_IOW(CS_CONTROL_MAGIC, 10, unsigned int)
#define IOC_CONTROL_SCART_STATUS	_IOR(CS_CONTROL_MAGIC, 11, scart_status_t *)
#define IOC_CONTROL_PWM_SPEED		_IOW(CS_CONTROL_MAGIC, 12, unsigned int)
#define IOC_CONTROL_HDDPOWER		_IOW(CS_CONTROL_MAGIC, 13, unsigned int)

/* ioctl for getting board serial and revision */
#define IOC_CONTROL_BOARD_SERIAL_LOW	_IOR(CS_CONTROL_MAGIC, 14, unsigned int *)
#define IOC_CONTROL_BOARD_SERIAL_HIGH	_IOR(CS_CONTROL_MAGIC, 15, unsigned int *)
#define IOC_CONTROL_BOARD_REV		_IOR(CS_CONTROL_MAGIC, 16, unsigned int *)

/* ioctl for setting TS routing */
#define IOC_CONTROL_TSROUTE_GET_HSDP_CONFIG	_IOR(CS_CONTROL_MAGIC, 17, tsrouter_hsdp_config_t *)
#define IOC_CONTROL_TSROUTE_SET_HSDP_CONFIG	_IOW(CS_CONTROL_MAGIC, 18, tsrouter_tsp_config_t *)
#define IOC_CONTROL_TSROUTE_GET_TSP_CONFIG	_IOR(CS_CONTROL_MAGIC, 19, tsrouter_hsdp_config_t *)
#define IOC_CONTROL_TSROUTE_SET_TSP_CONFIG	_IOW(CS_CONTROL_MAGIC, 20, tsrouter_tsp_config_t *)

#endif /* __CONTROL_H */
