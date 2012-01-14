/*
 * Simulate a linux input device via uinput
 * Get td remote events via DirectFB and inject them via uinput
 *
 * (C) 2012 Stefan Seyfried
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* the C++ compiler does not like this code, so let's put it into a
 * separate file and compile with gcc insead of g++...
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#include <linux/ioctl.h>
#include <linux/input.h>
#include <linux/uinput.h>

#include <directfb.h>
#include "lt_dfbinput.h"

/* needed for videodecoder watchdog */
#include "video_td.h"
extern cVideo *videoDecoder;

/* same defines as in neutrino's rcinput.h */
#define KEY_TTTV	KEY_FN_1
#define KEY_TTZOOM	KEY_FN_2
#define KEY_REVEAL	KEY_FN_D
/* only defined in newer kernels / headers... */
#ifndef KEY_ZOOMIN
#define KEY_ZOOMIN	KEY_FN_E
#endif
#ifndef KEY_ZOOMOUT
#define KEY_ZOOMOUT	KEY_FN_F
#endif

#define DFBCHECK(x...)                                                \
	err = x;                                                      \
	if (err != DFB_OK) {                                          \
		fprintf(stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
		DirectFBErrorFatal(#x, err );                         \
	}

typedef struct _DeviceInfo DeviceInfo;
struct _DeviceInfo {
	DFBInputDeviceID           device_id;
	DFBInputDeviceDescription  desc;
	DeviceInfo                *next;
};

static const int key_list[] = {
	KEY_0,
	KEY_1,
	KEY_2,
	KEY_3,
	KEY_4,
	KEY_5,
	KEY_6,
	KEY_7,
	KEY_8,
	KEY_9,
	KEY_OK,
	KEY_TIME,
	KEY_FAVORITES,
	KEY_ZOOMOUT,
	KEY_ZOOMIN,
	KEY_NEXT,
	KEY_POWER,
	KEY_MUTE,
	KEY_MENU,
	KEY_EPG,
	KEY_INFO,
	KEY_EXIT,
	KEY_PAGEUP,
	KEY_PAGEDOWN,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_UP,
	KEY_DOWN,
	KEY_VOLUMEUP,
	KEY_VOLUMEDOWN,
	KEY_RED,
	KEY_GREEN,
	KEY_YELLOW,
	KEY_BLUE,
	KEY_TV,
	KEY_VIDEO,
	KEY_AUDIO,
	KEY_AUX,
	KEY_TEXT,
	KEY_TTTV,
	KEY_TTZOOM,
	KEY_REVEAL,
	KEY_REWIND,
	KEY_STOP,
	KEY_PAUSE,
	KEY_FORWARD,
/*	KEY_PREV, */
	KEY_EJECTCD,
	KEY_RECORD,
/*	KEY_NEXT, */
	-1
};

static IDirectFBEventBuffer *events;
static DeviceInfo *inputs = NULL;

static pthread_t thread;
static int thread_running;

static DFBEnumerationResult enum_input_device(DFBInputDeviceID device_id,
					      DFBInputDeviceDescription desc,
					      void *data)
{
	DeviceInfo **devices = (DeviceInfo **)data;
	DeviceInfo  *device;

	device = (DeviceInfo *)malloc(sizeof(DeviceInfo));

	device->device_id = device_id;
	device->desc      = desc;
	device->next      = *devices;

	*devices = device;

	return DFENUM_OK;
}

static void *input_thread(void *data)
{
	int uinput;
	int i;
	struct input_event u;
	struct uinput_user_dev ud;
	FILE *f;

	DFBResult err;
	IDirectFB *dfb = (IDirectFB *)data;
	fprintf(stderr, "DFB input converter thread starting...\n");

	/* modprobe does not complain if the module is already loaded... */
	system("/sbin/modprobe uinput");
	system("/sbin/modprobe evdev");
	uinput = open("/dev/misc/uinput", O_WRONLY|O_NDELAY);
	if (uinput < 0)
	{
		fprintf(stderr, "DFB input thread: unable to open /dev/misc/uinput (%m)\n");
		return NULL;
	}

	fcntl(uinput, F_SETFD, FD_CLOEXEC);
	ioctl(uinput, UI_SET_EVBIT, EV_KEY);
	/* do not use kernel repeat EV_REP since neutrino will be confused by the
	 * generated SYN_REPORT events...
	ioctl(uinput, UI_SET_EVBIT, EV_REP);
	 */
	/* register keys */
	for (i = 0; key_list[i] != -1; i++)
		ioctl(uinput, UI_SET_KEYBIT, key_list[i]);

	/* configure the device */
	memset(&ud, 0, sizeof(ud));
	strncpy(ud.name, "Neutrino TD to Input Device converter", UINPUT_MAX_NAME_SIZE);
	ud.id.version = 0x42;
	ud.id.vendor  = 0x1234;
	ud.id.product = 0x5678;
	ud.id.bustype = BUS_I2C; /* ?? */
	write(uinput, &ud, sizeof(ud));

	if (ioctl(uinput, UI_DEV_CREATE))
	{
		perror("DFB input thread UI_DEV_CREATE");
		close(uinput);
		return NULL;
	}

	/* this is ugly: parse the new input device from /proc/...devices
	 * and symlink it to /dev/input/nevis_ir... */
#define DEVLINE "I: Bus=0018 Vendor=1234 Product=5678 Version=0042"
	f = fopen("/proc/bus/input/devices", "r");
	if (f)
	{
		int found = 0;
		int evdev = -1;
		size_t n = 0;
		char *line = NULL;
		char *p;
		char newdev[20];
		while (getline(&line, &n, f) != -1)
		{
			switch(line[0])
			{
				case 'I':
					if (strncmp(line, DEVLINE, strlen(DEVLINE)) == 0)
						found = 1;
					break;
				case 'H':
					if (! found)
						break;
					p = strstr(line, " event");
					if (! p)
					{
						evdev = -1;
						break;
					}
					evdev = atoi(p + 6);
					sprintf(newdev, "event%d", evdev);
					fprintf(stderr, "DFB input thread: symlink /dev/input/nevis_ir to %s\n", newdev);
					unlink("/dev/input/nevis_ir");
					symlink(newdev, "/dev/input/nevis_ir");
					break;
				default:
					break;
			}
			if (evdev != -1)
				break;
		}
		fclose(f);
		free(line);
	}

	u.type = EV_KEY;
	u.value = 0; /* initialize: first event wil be a key press */

	dfb->EnumInputDevices(dfb, enum_input_device, &inputs);
	DFBCHECK(dfb->CreateInputEventBuffer(dfb, DICAPS_ALL, DFB_FALSE, &events));

	thread_running = 1;
	while (thread_running)
	{
		/* check every 250ms (if a key is pressed on remote, we might
		 * even check earlier, but it does not really hurt... */
		if (videoDecoder)
			videoDecoder->VideoParamWatchdog();

		if (events->WaitForEventWithTimeout(events, 0, 250) == DFB_TIMEOUT)
			continue;
		DFBInputEvent e;
		while (events->GetEvent(events, DFB_EVENT(&e)) == DFB_OK)
		{
#if 0
			fprintf(stderr, "type: %x devid: %x flags: %03x "
					"key_id: %4x key_sym: %4x keycode: %d\n",
					e.type, e.device_id, e.flags,
					e.key_id, e.key_symbol, e.key_code);
#endif
			switch (e.key_symbol)
			{
				/* will a lookup table be more efficient? */
				case 0x0030: u.code = KEY_0;		break;
				case 0x0031: u.code = KEY_1;		break;
				case 0x0032: u.code = KEY_2;		break;
				case 0x0033: u.code = KEY_3;		break;
				case 0x0034: u.code = KEY_4;		break;
				case 0x0035: u.code = KEY_5;		break;
				case 0x0036: u.code = KEY_6;		break;
				case 0x0037: u.code = KEY_7;		break;
				case 0x0038: u.code = KEY_8;		break;
				case 0x0039: u.code = KEY_9;		break;
				case 0x000d: u.code = KEY_OK;		break;
				case 0xf504: u.code = KEY_TIME;		break;
				case 0xf01a: u.code = KEY_FAVORITES;	break; /* blue heart */
				case 0xf021: u.code = KEY_ZOOMOUT;	break;
				case 0xf022: u.code = KEY_ZOOMIN;	break;
				case 0xf505: u.code = KEY_NEXT;		break; /* red hand */
				case 0xf00f: u.code = KEY_POWER;	break;
				case 0xf04e: u.code = KEY_MUTE;		break;
				case 0xf012: u.code = KEY_MENU;		break;
				case 0xf01b: u.code = KEY_EPG;		break;
				case 0xf014: u.code = KEY_INFO;		break;
				case 0x001b: u.code = KEY_EXIT;		break;
				case 0xf046: u.code = KEY_PAGEUP;	break;
				case 0xf047: u.code = KEY_PAGEDOWN;	break;
				case 0xf000: u.code = KEY_LEFT;		break;
				case 0xf001: u.code = KEY_RIGHT;	break;
				case 0xf002: u.code = KEY_UP;		break;
				case 0xf003: u.code = KEY_DOWN;		break;
				case 0xf04c: u.code = KEY_VOLUMEUP;	break;
				case 0xf04d: u.code = KEY_VOLUMEDOWN;	break;
				case 0xf042: u.code = KEY_RED;		break;
				case 0xf043: u.code = KEY_GREEN;	break;
				case 0xf044: u.code = KEY_YELLOW;	break;
				case 0xf045: u.code = KEY_BLUE;		break;
				case 0xf027: u.code = KEY_TV;		break;
				case 0xf035: u.code = KEY_VIDEO;	break;
				case 0xf033: u.code = KEY_AUDIO;	break;
				case 0xf034: u.code = KEY_AUX;		break;
				case 0xf032: u.code = KEY_TEXT;		break;
				case 0xf501: u.code = KEY_TTTV;		break;
				case 0xf502: u.code = KEY_TTZOOM;	break;
				case 0xf503: u.code = KEY_REVEAL;	break;
				case 0xf059: u.code = KEY_REWIND;	break;
				case 0xf052: u.code = KEY_STOP;		break;
				case 0xf051: u.code = KEY_PAUSE;	break;
				case 0xf05a: u.code = KEY_FORWARD;	break;
			/*	case 0xf05b: u.code = KEY_PREV;		break; */
				case 0xf057: u.code = KEY_EJECTCD;	break;
				case 0xf056: u.code = KEY_RECORD;	break;
			/*	case 0xf05c: u.code = KEY_NEXT;		break; */
				default:
					continue;
			}
			switch (e.type)
			{
				case 1: if (u.value < 2)	/* 1 = key press */
						u.value++;	/* 2 = key repeat */
					break;
				case 2: u.value = 0; break;	/* 0 = key release */
				default:
					continue;
			}
			// fprintf(stderr, "uinput write: value: %d code: %d\n", u.value, u.code);
			write(uinput, &u, sizeof(u));
		}
	}
	/* clean up */
	ioctl(uinput, UI_DEV_DESTROY);
	while (inputs) {
		DeviceInfo *next = inputs->next;
		free(inputs);
		inputs = next;
	}
	events->Release(events);
	return NULL;
}

void start_input_thread(IDirectFB *dfb)
{
	if (pthread_create(&thread, 0, input_thread, dfb) != 0)
	{
		perror("DFB input thread pthread_create");
		thread_running = 0;
		return;
	}
	/* wait until the device is created before continuing */
	while (! thread_running)
		usleep(1000);
}

void stop_input_thread(void)
{
	if (! thread_running)
		return;
	thread_running = 0;
	pthread_join(thread, NULL);
}
