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

#define CS_PIO_LOW                     0
#define CS_PIO_HIGH                    1
int cs_gpio_drive(int gpio, int value);
#endif

#include "tsrouter.h"

typedef struct _scart_status {
	bool	widescreen;
	bool	function;
	bool	fastblank;
} scart_status_t;

/* ioctls */
#define CS_CONTROL_MAGIC		0xDE
#define IOC_CONTROL_WIDESCREEN		_IOW(CS_CONTROL_MAGIC, 20, unsigned int)
#define IOC_CONTROL_TVAV		_IOW(CS_CONTROL_MAGIC, 21, unsigned int)
#define IOC_CONTROL_RGB			_IOW(CS_CONTROL_MAGIC, 22, unsigned int)
#define IOC_CONTROL_SCART_STATUS	_IOR(CS_CONTROL_MAGIC, 23, scart_status_t *)
#define IOC_CONTROL_HDDPOWER		_IOW(CS_CONTROL_MAGIC, 25, unsigned int)

/* ioctl for getting board serial and revision */
#define IOC_CONTROL_BOARD_SERIAL_LOW	_IOR(CS_CONTROL_MAGIC, 26, unsigned int *)
#define IOC_CONTROL_BOARD_SERIAL_HIGH	_IOR(CS_CONTROL_MAGIC, 27, unsigned int *)
#define IOC_CONTROL_BOARD_REV		_IOR(CS_CONTROL_MAGIC, 28, unsigned int *)

/* ioctl for setting TS routing */
#define IOC_CONTROL_TSROUTE_GET_HSDP_CONFIG	_IOR(CS_CONTROL_MAGIC, 29, tsrouter_hsdp_config_t *)
#define IOC_CONTROL_TSROUTE_SET_HSDP_CONFIG	_IOW(CS_CONTROL_MAGIC, 30, tsrouter_tsp_config_t *)
#define IOC_CONTROL_TSROUTE_GET_TSP_CONFIG	_IOR(CS_CONTROL_MAGIC, 31, tsrouter_hsdp_config_t *)
#define IOC_CONTROL_TSROUTE_SET_TSP_CONFIG	_IOW(CS_CONTROL_MAGIC, 32, tsrouter_tsp_config_t *)
/* Gets the current TS port frequency of the CI */
#define IOC_CONTROL_TSROUTE_GET_CI_SPEED	_IOR(CS_CONTROL_MAGIC, 33, unsigned int *)
/* Sets the current TS port frequency of the CI in Hz (max=12Mhz) */
#define IOC_CONTROL_TSROUTE_SET_CI_SPEED	_IOW(CS_CONTROL_MAGIC, 34, unsigned int)
/* Gets the current TS port base PLL of the CI */
#define IOC_CONTROL_TSROUTE_GET_CI_PLL		_IOR(CS_CONTROL_MAGIC, 35, unsigned int *)
/* Sets the current TS port base PLL of the CI */
#define IOC_CONTROL_TSROUTE_SET_CI_PLL		_IOW(CS_CONTROL_MAGIC, 36, unsigned int)

#endif /* __CONTROL_H */
