/*
 * determine the capabilities of the hardware.
 * part of libstb-hal
 *
 * (C) 2010-2012,2016 Stefan Seyfried
 *
 * License: GPL v2 or later
 */
#ifndef __HARDWARE_CAPS_H__
#define __HARDWARE_CAPS_H__

typedef enum
{
	HW_DISPLAY_NONE,
	HW_DISPLAY_LED_ONLY,
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
	int can_cpufreq;
	int can_shutdown;
	int can_cec;
	int can_ar_14_9;	/* video drivers have 14:9 aspect ratio mode */
	int can_ps_14_9;	/* video drivers have 14:9 panscan mode */
	int force_tuner_2G;	/* force DVB-S2 even though driver may not advertise it */
	display_type_t display_type;
	int display_xres;	/* x resolution or chars per line */
	int display_yres;
	int display_can_deepstandby;
	int display_can_set_brightness;
	int display_has_statusline;
	int display_has_colon;
	int has_button_timer;
	int has_button_vformat;
	char boxvendor[64];
	char boxname[64];
	char boxarch[64];
} hw_caps_t;

hw_caps_t *get_hwcaps(void);
#endif
