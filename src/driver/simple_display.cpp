/*
	Routines to drive simple one-line text or SPARK's 4 digit LED display

	(C) 2012 Stefan Seyfried

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <driver/lcdd.h>
#include <driver/framebuffer.h>

#include <global.h>
#include <neutrino.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
//#include <math.h>
#include <sys/stat.h>

#include "spark_led.h"

static char volume = 0;
//static char percent = 0;
static bool power = true;
static bool muted = false;
static bool showclock = true;
static time_t last_display = 0;
static char display_text[64] = { 0 };
static bool led_r = false;
static bool led_g = false;

static inline int dev_open()
{
	int fd = open("/dev/vfd", O_RDWR);
	if (fd < 0)
		fprintf(stderr, "[neutrino] simple_display: open " DISPLAY_DEV ": %m\n");
	return fd;
}

static void display(const char *s, bool update_timestamp = true)
{
	int fd = dev_open();
	int len = strlen(s);
	if (fd < 0)
		return;
printf("%s '%s'\n", __func__, s);
	write(fd, s, len);
	close(fd);
	if (update_timestamp)
	{
		last_display = time(NULL);
		/* increase timeout to ensure that everything is displayed
		 * the driver displays 5 characters per second */
		if (len > 4)
			last_display += (len - 4) / 5;
	}
}

CLCD::CLCD()
{
	/* do not show menu in neutrino... */
	has_lcd = false;
}

CLCD* CLCD::getInstance()
{
	static CLCD* lcdd = NULL;
	if (lcdd == NULL)
		lcdd = new CLCD();
	return lcdd;
}

void CLCD::wake_up()
{
}

void* CLCD::TimeThread(void *)
{
	while(1) {
		sleep(1);
		CLCD::getInstance()->showTime();
		/* hack, just if we missed the blit() somewhere
		 * this will update the framebuffer once per second */
		if (getenv("SPARK_NOBLIT") == NULL)
			CFrameBuffer::getInstance()->blit();
	}
	return NULL;
}

void CLCD::init(const char *, const char *, const char *, const char *, const char *, const char *)
{
	setMode(MODE_TVRADIO);
	if (pthread_create (&thrTime, NULL, TimeThread, NULL) != 0 ) {
		perror("[neutino] CLCD::init pthread_create(TimeThread)");
		return ;
	}
}

void CLCD::setlcdparameter(void)
{
}

void CLCD::showServicename(std::string, bool)
{
}

void CLCD::setled(int red, int green)
{
	struct aotom_ioctl_data d;
	int leds[2] = { red, green };
	int i;
	int fd = dev_open();
	if (fd < 0)
		return;

printf("%s red:%d green:%d\n", __func__, red, green);

	for (i = 0; i < 2; i++)
	{
		if (leds[i] == -1)
			continue;
		d.u.led.led_nr = i;
		d.u.led.on = leds[i];
		if (ioctl(fd, VFDSETLED, &d) < 0)
			fprintf(stderr, "[neutrino] %s setled VFDSETLED: %m\n", __func__);
	}
	close(fd);
}

void CLCD::showTime(bool force)
{
	static bool blink = false;
	int red = -1, green = -1;

	if (mode == MODE_SHUTDOWN)
	{
		setled(1, 1);
		return;
	}

	time_t now = time(NULL);
	if (display_text[0])
	{
		display(display_text);
		display_text[0] = '\0';
	}
	else if (power && (force || (showclock && (now - last_display) > 4)))
	{
		char timestr[5];
		struct tm *t;
		static int hour = 0, minute = 0;

		t = localtime(&now);
		if (force || last_display || (hour != t->tm_hour) || (minute != t->tm_min)) {
			hour = t->tm_hour;
			minute = t->tm_min;
			sprintf(timestr, "%02d%02d", hour, minute);
			display(timestr, false);
			last_display = 0;
		}
	}

	if (led_r)
		red = blink;
	blink = !blink;
	if (led_g)
		green = blink;

	if (led_r || led_g)
		setled(red, green);
}

void CLCD::showRCLock(int)
{
}

/* update is default true, the mute code sets it to false
 * to force an update => inverted logic! */
void CLCD::showVolume(const char vol, const bool update)
{
	char s[5];
	if (vol == volume && update)
		return;

	volume = vol;
	/* char is unsigned, so vol is never < 0 */
	if (volume > 100)
		volume = 100;

	if (muted)
		strcpy(s, "mute");
	else
		sprintf(s, "%4d", volume);

	display(s);
}

void CLCD::showPercentOver(const unsigned char perc, const bool /*perform_update*/, const MODES)
{
}

void CLCD::showMenuText(const int, const char *text, const int, const bool)
{
	if (mode != MODE_MENU_UTF8)
		return;
	strncpy(display_text, text, sizeof(display_text) - 1);
	display_text[sizeof(display_text) - 1] = '\0';
}

void CLCD::showAudioTrack(const std::string &, const std::string & title, const std::string &)
{
	if (mode != MODE_AUDIO)
		return;
//	ShowText(title.c_str());
}

void CLCD::showAudioPlayMode(AUDIOMODES)
{
}

void CLCD::showAudioProgress(const char, bool)
{
}

void CLCD::setMode(const MODES m, const char * const)
{
	mode = m;

	switch (m) {
	case MODE_TVRADIO:
		setled(0, 0);
		showclock = true;
		power = true;
		showTime();
		break;
	case MODE_SHUTDOWN:
		showclock = false;
		Clear();
		break;
	case MODE_STANDBY:
		setled(0, 1);
		showclock = true;
		showTime(true);
		break;
	default:
		showclock = true;
		showTime();
	}
}

void CLCD::setBrightness(int)
{
}

int CLCD::getBrightness()
{
	return 0;
}

void CLCD::setBrightnessStandby(int)
{
}

int CLCD::getBrightnessStandby()
{
	return 0;
}

void CLCD::setPower(int)
{
}

int CLCD::getPower()
{
	return 0;
}

void CLCD::togglePower(void)
{
	power = !power;
	if (!power)
		Clear();
	else
		showTime(true);
}

void CLCD::setMuted(bool mu)
{
printf("spark_led:%s %d\n", __func__, mu);
	muted = mu;
	showVolume(volume, false);
}

void CLCD::resume()
{
}

void CLCD::pause()
{
}

void CLCD::Lock()
{
}

void CLCD::Unlock()
{
}

void CLCD::Clear()
{
	int fd = dev_open();
	if (fd < 0)
		return;
	int ret = ioctl(fd, VFDDISPLAYCLR);
	if(ret < 0)
		perror("[neutrino] spark_led Clear() VFDDISPLAYCLR");
	close(fd);
printf("spark_led:%s\n", __func__);
}

void CLCD::ShowIcon(vfd_icon i, bool on)
{
	switch (i)
	{
		case VFD_ICON_CAM1:
			led_r = on;
			setled(led_r, -1); /* switch instant on / switch off if disabling */
			break;
		case VFD_ICON_PLAY:
			led_g = on;
			setled(-1, led_g);
			break;
		default:
			break;
	}
}

void CLCD::setEPGTitle(const std::string)
{
}

