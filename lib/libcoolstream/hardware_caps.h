/*
 * determine the capabilities of the hardware.
 * part of libstb-hal
 *
 * (C) 2010-2012 Stefan Seyfried
 *
 * License: GPL v2 or later
 */
#ifndef __HARDWARE_CAPS_H__
#define __HARDWARE_CAPS_H__

#include "cs_api.h"
#include <string.h>

typedef enum
{
	HW_DISPLAY_NONE,
	HW_DISPLAY_LED_NUM,	/* simple 7 segment LED display */
	HW_DISPLAY_LINE_TEXT,	/* 1 line text display */
	HW_DISPLAY_GFX
} display_type_t;


typedef struct hw_caps
{
	int has_fan;
	int has_HDMI;
	int has_SCART;
	int has_SCART_input;
	int has_YUV_cinch;
	int can_shutdown;
	int can_cec;
	display_type_t display_type;
	int display_xres;	/* x resolution or chars per line */
	int display_yres;
	int can_set_display_brightness;
	char boxvendor[64];
	char boxname[64];
} hw_caps_t;

hw_caps_t *get_hwcaps(void);
#endif
