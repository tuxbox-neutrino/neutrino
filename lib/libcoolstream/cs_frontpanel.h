/*
 *  Driver for the Coolstream Frontpanel - public definititons.
 *
 *  Copyright (C) 2008 - 2012 Coolstream International Limited
 */

#ifndef __CS_FRONTPANEL_H__
#define __CS_FRONTPANEL_H__

#define CS_FP_DISPLAY_DEVICE_NAME	"cs_display"
#define CS_FP_DISPLAY_DEVICE_MAJOR	238

typedef enum {
	/* for all frontpanels with */
	FP_ICON_NONE		= 0x00000000,
	FP_ICON_BAR8		= 0x00000004,
	FP_ICON_BAR7		= 0x00000008,
	FP_ICON_BAR6		= 0x00000010,
	FP_ICON_BAR5		= 0x00000020,
	FP_ICON_BAR4		= 0x00000040,
	FP_ICON_BAR3		= 0x00000080,
	FP_ICON_BAR2		= 0x00000100,
	FP_ICON_BAR1		= 0x00000200,
	FP_ICON_FRAME		= 0x00000400,
	FP_ICON_MUTE		= 0x00001000,
	FP_ICON_DOLBY		= 0x00002000,
	FP_ICON_TV		= 0x00020000,
	FP_ICON_RADIO		= 0x00040000,
	FP_ICON_HD		= 0x01000001,
	FP_ICON_1080P		= 0x02000001,
	FP_ICON_1080I		= 0x03000001,
	FP_ICON_720P		= 0x04000001,
	FP_ICON_480P		= 0x05000001,
	FP_ICON_480I		= 0x06000001,
	FP_ICON_MP3		= 0x08000001,
	FP_ICON_PLAY		= 0x09000001,
	FP_ICON_PAUSE		= 0x0A000001,
	FP_ICON_CAM1		= 0x0B000001, /* record */
	/* only for frontpanels with VFD */
	FP_ICON_HDD		= 0x00000800,
	FP_ICON_POWER		= 0x00004000,
	FP_ICON_TIMESHIFT	= 0x00008000,
	FP_ICON_SIGNAL		= 0x00010000,
	FP_ICON_USB		= 0x07000001,
	FP_ICON_COL1		= 0x09000002,
	FP_ICON_COL2		= 0x0B000002,
	FP_ICON_CAM2		= 0x0C000001,
	/* only for frontpanels with OLED */
	FP_ICON_SD		= 0x01000002,
	FP_ICON_576P		= 0x02000002,
	FP_ICON_576I		= 0x03000002,
	FP_ICON_MP2		= 0x07000002,
	FP_ICON_DTS		= 0x08000002
} fp_icon;

typedef enum {
	FP_FLAG_NONE			= 0x00,
	FP_FLAG_SCROLL_ON		= 0x01,	/* switch scrolling on */
	FP_FLAG_SCROLL_LTR		= 0x02,	/* scroll from left to right instead of default right to left direction (i.e. for arabic text) */
	FP_FLAG_SCROLL_SIO		= 0x04,	/* start/stop scrolling with empty screen (scroll in/out) */
	FP_FLAG_SCROLL_DELAY		= 0x08,	/* delayed scroll start */
	FP_FLAG_ALIGN_LEFT		= 0x10,	/* align the text in display from the left (default) */
	FP_FLAG_ALIGN_RIGHT		= 0x20,	/* align the text in display from the right (arabic) */
	FP_FLAG_UPDATE_SCROLL_POS	= 0x40	/* update the current position for scrolling */
} fp_flag;

typedef struct {
	unsigned char		brightness;
	unsigned char		flags;
	unsigned char		current_hour;
	unsigned char		current_minute;
	unsigned char		timer_minutes_hi;
	unsigned char		timer_minutes_lo;
} fp_standby_data_t;

typedef enum {
	FP_LED_1_ON		= 0x81,
	FP_LED_2_ON		= 0x82,
	FP_LED_3_ON		= 0x83,
	FP_LED_1_OFF		= 0x01,
	FP_LED_2_OFF		= 0x02,
	FP_LED_3_OFF		= 0x03
} fp_led_ctrl_t;

typedef struct {
	unsigned char		source;
	unsigned char		time_minutes_hi;
	unsigned char		time_minutes_lo;
} fp_wakeup_data_t;

typedef enum {
	FP_WAKEUP_SOURCE_TIMER	= 0x01,
	FP_WAKEUP_SOURCE_BUTTON	= 0x02,
	FP_WAKEUP_SOURCE_REMOTE	= 0x04,
	FP_WAKEUP_SOURCE_PWLOST	= 0x7F,
	FP_WAKEUP_SOURCE_POWER	= 0xFF
} fp_wakeup_source;

typedef struct {
	unsigned short		addr;
	unsigned short		cmd;
} fp_standby_cmd_data_t;

#define IOC_FP_SET_BRIGHT	_IOW(0xDE,  1, unsigned char)	/* set the display brighness in 16 steps between 0 to 15 */
#define IOC_FP_CLEAR_ALL	_IOW(0xDE,  2, unsigned int)	/* clear the entire display (both text and icons) */
#define IOC_FP_SET_TEXT		_IOW(0xDE,  3, char*)		/* set a text to be displayed on the display. If arg == NULL, the text is cleared */
#define IOC_FP_SET_ICON		_IOW(0xDE,  4, fp_icon)	/* switch the given icon on */
#define IOC_FP_CLEAR_ICON	_IOW(0xDE,  5, fp_icon)	/* switch the given icon off */
#define IOC_FP_SET_OUTPUT	_IOW(0xDE,  6, unsigned char)	/* switch the given output on (supported by the controller, but not used in the hardware) */
#define IOC_FP_CLEAR_OUTPUT	_IOW(0xDE,  7, unsigned char)	/* switch the given output off (supported by the controller, but not used in the hardware) */
#define IOC_FP_STANDBY		_IOW(0xDE,  8, fp_standby_data_t *)/* switch the vfd/psu in standby (NEO and above only) */
#define IOC_FP_LED_CTRL		_IOW(0xDE,  9, unsigned char)   /* control the Frontpanles LED's (NEO and above only) */
#define IOC_FP_GET_WAKEUP	_IOW(0xDE, 10, fp_wakeup_data_t *) /* get wakeup data (NEO and above only) */
#define IOC_FP_STANDBY_CMD	_IOW(0xDE, 11, fp_standby_cmd_data_t *) /* get wakeup data (NEO and above only) */

#endif /* __CS_FRONTPANEL_H__ */
