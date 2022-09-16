/*
	Routines to drive simple one-line text display

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

#include <driver/simple_display.h>
#include <driver/framebuffer.h>
#include <system/helpers.h>
#include <system/proc_tools.h>
#include <system/set_threadname.h>

#include <global.h>
#include <neutrino.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
//#include <math.h>
#include <sys/stat.h>

#if HAVE_GENERIC_HARDWARE
#define DISPLAY_DEV "/dev/null"
static bool usb_icon = false;
static bool timer_icon = false;
#endif

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
#if BOXMODEL_MULTIBOX || BOXMODEL_MULTIBOXSE
#define DISPLAY_DEV "/dev/null"
#else
#define DISPLAY_DEV "/dev/dbox/oled0"
#endif
#include <zapit/zapit.h>
static bool usb_icon = false;
static bool timer_icon = false;
#endif

static char volume = 0;
//static char percent = 0;
static bool power = true;
static bool muted = false;
static bool showclock = true;
static time_t last_display = 0;
static char display_text[64] = { 0 };
static bool led_r = false;
static bool led_g = false;
static bool upd_display = false;
static bool vol_active = false;

static inline int dev_open()
{
	int fd = open(DISPLAY_DEV, O_RDWR);
	if (fd < 0)
		fprintf(stderr, "[neutrino] simple_display: open " DISPLAY_DEV ": %m\n");
	return fd;
}

static void replace_umlauts(std::string &s)
{
	/* this is crude, it just replaces ÄÖÜ with AOU since the display can't show them anyway */
	/*                       Ä           ä           Ö           ö           Ü           ü   */
	char tofind[][3] = { "\xc3\x84", "\xc3\xa4", "\xc3\x96", "\xc3\xb6", "\xc3\x9c", "\xc3\xbc" };
	char toreplace[] = { "AaOoUu" };
	char repl[2];
	repl[1] = '\0';
	int i = 0;
	size_t pos;
	// print("%s:>> '%s'\n", __func__, s.c_str());
	while (toreplace[i] != 0x0) {
		pos = s.find(tofind[i]);
		if (pos == std::string::npos) {
			i++;
			continue;
		}
		repl[0] = toreplace[i];
		s.replace(pos, 2, std::string(repl));
	}
	// printf("%s:<< '%s'\n", __func__, s.c_str());
}

CLCD::CLCD()
{
	/* do not show menu in neutrino...,at Line Display true, because there is th GLCD Menu */
	if (g_info.hw_caps->display_type == HW_DISPLAY_LINE_TEXT)
		has_lcd = true;
	else
		has_lcd = false;

	mode = MODE_TVRADIO;
	switch_name_time_cnt = 0;
	timeout_cnt = 0;
	servicename = "";
	servicenumber = -1;
	thread_running = false;
}

CLCD::~CLCD()
{
	if (thread_running)
	{
		thread_running = false;
		pthread_cancel(thrTime);
		pthread_join(thrTime, NULL);
	}
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
	if (
		   g_info.hw_caps->display_type == HW_DISPLAY_LINE_TEXT
		|| g_info.hw_caps->display_type == HW_DISPLAY_LED_NUM
		|| g_info.hw_caps->display_type == HW_DISPLAY_LED_ONLY
	)
	{
		if (atoi(g_settings.lcd_setting_dim_time.c_str()) > 0)
		{
			timeout_cnt = atoi(g_settings.lcd_setting_dim_time.c_str());
			if (g_settings.lcd_setting_dim_brightness > -1)
				setBrightness(g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS]);
			else
				setPower(1);
		}
		else
			setPower(1);

		if (g_settings.lcd_info_line)
			switch_name_time_cnt = 10;
	}
}

void* CLCD::TimeThread(void *)
{
	set_threadname("n:boxdisplay"); /* to not confuse with TV display */
	while (CLCD::getInstance()->thread_running)
	{
		sleep(1);
		if (
			   g_info.hw_caps->display_type == HW_DISPLAY_LINE_TEXT
			|| g_info.hw_caps->display_type == HW_DISPLAY_LED_NUM
		)
		{
			struct stat buf;
			if (stat("/tmp/vfd.locked", &buf) == -1)
			{
				CLCD::getInstance()->showTime();
				CLCD::getInstance()->count_down();
			}
			else
				CLCD::getInstance()->wake_up();
		}
		else
			CLCD::getInstance()->showTime();
	}
	return NULL;
}

void CLCD::init(const char *, const char *, const char *, const char *, const char *, const char *)
{
	setMode(MODE_TVRADIO);
	thread_running = true;
	if (pthread_create (&thrTime, NULL, TimeThread, NULL) != 0 ) {
		perror("[neutino] CLCD::init() pthread_create(TimeThread)");
		thread_running = false;
		return ;
	}
}

void CLCD::setlcdparameter(void)
{
	if (g_info.hw_caps->display_can_set_brightness)
	{
		last_toggle_state_power = g_settings.lcd_setting[SNeutrinoSettings::LCD_POWER];

		if (mode == MODE_STANDBY)
			setlcdparameter(g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS], last_toggle_state_power);
		else if (mode == MODE_SHUTDOWN)
			setlcdparameter(g_settings.lcd_setting[SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS], last_toggle_state_power);
		else
			setlcdparameter(g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS], last_toggle_state_power);
	}
}

void CLCD::showServicename(std::string name, const int num, bool)
{
	servicename = name;
	servicenumber = num;

	if (mode != MODE_TVRADIO && mode != MODE_AUDIO)
		return;

	if (g_info.hw_caps->display_type == HW_DISPLAY_LED_NUM)
	{
		std::string s;
		if (num > 0) // dont show channel 0 at boot
		{
			s = to_string(servicenumber);
			while ((int)s.length() < g_info.hw_caps->display_xres) {
				s = " " + s;
			}
		}
		else
			return;
		strncpy(display_text, s.c_str(), sizeof(display_text) - 1);
		display_text[sizeof(display_text) - 1] = '\0';
	}
	else
	{
		if (!g_info.hw_caps->display_can_umlauts)
			replace_umlauts(servicename);
		strncpy(display_text, servicename.c_str(), sizeof(display_text) - 1);
		display_text[sizeof(display_text) - 1] = '\0';
	}
	upd_display = true;
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	wake_up();
#endif
}

void CLCD::setled(int /*red*/, int /*green*/)
{
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
	if (upd_display)
	{
		ShowText(display_text);
		upd_display = false;
	}
	else if (power && (force || (showclock && (now - last_display) > 4)))
	{
		char timestr[64]; /* todo: change if we have a simple display with 63+ chars ;) */
		struct tm *t;
		static int hour = 0, minute = 0;

		t = localtime(&now);
		if (force || last_display || (switch_name_time_cnt == 0 && ((hour != t->tm_hour) || (minute != t->tm_min)))) {
			hour = t->tm_hour;
			minute = t->tm_min;
#if !HAVE_ARM_HARDWARE && !HAVE_MIPS_HARDWARE
			int ret = -1;
#endif
			if (
				   g_info.hw_caps->display_type == HW_DISPLAY_LINE_TEXT
				|| g_info.hw_caps->display_type == HW_DISPLAY_LED_NUM
			)
			{
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
				if (mode == MODE_STANDBY || (g_settings.lcd_info_line && mode == MODE_TVRADIO))
#else
				if (ret < 0 && servicename.empty() && servicenumber == -1)
#endif
				{
					if (g_info.hw_caps->display_xres == 4 && g_info.hw_caps->display_has_colon == 1)
						sprintf(timestr, "%02d:%02d", hour, minute);
					else if (g_info.hw_caps->display_xres < 5)
						sprintf(timestr, "%02d%02d", hour, minute);
					else	/* pad with spaces on the left side to center the time string */
						sprintf(timestr, "%*s%02d:%02d",(g_info.hw_caps->display_xres - 5)/2, "", hour, minute);
					ShowText(timestr, false);
				}
				else
				{
					if (vol_active)
					{
						showServicename(servicename, servicenumber);
						vol_active = false;
					}
				}
				last_display = 0;
			}
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

void CLCD::showRCLock(int duration)
{
	if (g_info.hw_caps->display_type != HW_DISPLAY_LINE_TEXT || !g_settings.lcd_notify_rclock)
	{
		sleep(duration);
		return;
	}

	ShowText(g_Locale->getText(LOCALE_RCLOCK_LOCKED));
	sleep(duration);
	ShowText(servicename.c_str(), servicenumber);
}

/* update is default true, the mute code sets it to false
 * to force an update => inverted logic! */
void CLCD::showVolume(const char vol, const bool update)
{
	char s[32];
	const int type = (g_info.hw_caps->display_xres < 5) + (g_info.hw_caps->display_type == HW_DISPLAY_LED_NUM);
	const char *vol_fmt[] = { "Vol:%3d%%", "%4d", "%4d" };
	const char *mutestr[] = { "Vol:MUTE", "mute", " -0-"};

	if (vol == volume && update)
		return;

	setVolume(vol);

	if (muted)
	{
		strcpy(s, mutestr[type]);
	} else {
		sprintf(s, vol_fmt[type], volume);
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
		if (g_info.hw_caps->display_type == HW_DISPLAY_LINE_TEXT)
			sprintf(s,"%.*s", volume*g_info.hw_caps->display_xres/100, "================");
#endif
	}

	ShowText(s);
	vol_active = true;
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	wake_up();
#endif
}

void CLCD::setVolume(const char vol)
{
	if (vol == volume)
		return;

	volume = vol;

	/* char is unsigned, so volume is never < 0 */
	if (volume > 100)
		volume = 100;
}

void CLCD::showPercentOver(const unsigned char /*perc*/, const bool /*perform_update*/, const MODES)
{
}

void CLCD::showMenuText(const int, const char *text, const int, const bool)
{
	if (mode != MODE_MENU_UTF8)
		return;
	std::string tmp = text;
	if (!g_info.hw_caps->display_can_umlauts)
		replace_umlauts(tmp);
	ShowText(tmp.c_str());
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	wake_up();
#endif
}

void CLCD::showAudioTrack(const std::string &, const std::string & title, const std::string &)
{
	if (mode != MODE_AUDIO)
		return;
	std::string tmp = title;
	if (!g_info.hw_caps->display_can_umlauts)
		replace_umlauts(tmp);
	ShowText(tmp.c_str());
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	wake_up();
#endif
}

void CLCD::showAudioPlayMode(AUDIOMODES)
{
}

void CLCD::showAudioProgress(const char, bool)
{
}

void CLCD::setMode(const MODES m, const char * const title)
{
	if (strlen(title))
		ShowText(title);

	mode = m;

	setlcdparameter();
	if (g_info.hw_caps->display_type == HW_DISPLAY_NONE)
		proc_put("/proc/stb/power/powerled", "on");
	proc_put("/proc/stb/lcd/show_symbols", true);
	switch (m) {
	case MODE_TVRADIO:
		setled(0, 0);
		showclock = true;
		power = true;
		showServicename(servicename, servicenumber);
		showTime();
		if (g_settings.lcd_info_line)
			switch_name_time_cnt = 10;
		break;
	case MODE_MENU_UTF8:
		showclock = false;
		break;
	case MODE_SHUTDOWN:
		showclock = false;
		Clear();
		if (g_info.hw_caps->display_type == HW_DISPLAY_NONE)
			proc_put("/proc/stb/power/powerled", "off");
		proc_put("/proc/stb/lcd/show_symbols", false);
		break;
	case MODE_STANDBY:
		setled(0, 1);
		showclock = true;
		showTime(true);
		if (g_info.hw_caps->display_type == HW_DISPLAY_NONE)
			proc_put("/proc/stb/power/powerled", "off");
		proc_put("/proc/stb/lcd/show_symbols", false);
		timeout_cnt = 0;
		break;
	default:
		showclock = true;
		showTime();
	}
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	if (m != MODE_SHUTDOWN && m != MODE_STANDBY)
		wake_up();
#endif
}

void CLCD::setBrightness(int dimm)
{
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	std::string value = to_string(255/15*dimm);
	if (access("/proc/stb/lcd/oled_brightness", F_OK) == 0)
		proc_put("/proc/stb/lcd/oled_brightness", value.c_str(), value.length());
	else if (access("/proc/stb/fp/oled_brightness", F_OK) == 0)
		proc_put("/proc/stb/fp/oled_brightness", value.c_str(), value.length());
#else
	(void)dimm; // avoid compiler warning
#endif
}

int CLCD::getBrightness()
{
	if (g_info.hw_caps->display_can_set_brightness)
	{
		if (g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] > 15)
			g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = 15;
		return g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS];
	} else
		return 0;
}

void CLCD::setBrightnessStandby(int bright)
{
	if (g_info.hw_caps->display_can_set_brightness)
	{
		g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] = bright;
		setlcdparameter();
	}
}

int CLCD::getBrightnessStandby()
{
	if (g_info.hw_caps->display_can_set_brightness)
	{
		if (g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] > 15)
			g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] = 15;
		return g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS];
	}
	else
		return 0;
}

void CLCD::setScrollMode(int scroll_repeats)
{
	printf("CLCD::%s scroll_repeats:%d\n", __func__, scroll_repeats);
	if (scroll_repeats)
	{
		proc_put("/proc/stb/lcd/initial_scroll_delay", "1000");
		proc_put("/proc/stb/lcd/final_scroll_delay", "1000");
		proc_put("/proc/stb/lcd/scroll_delay", "150");
		proc_put("/proc/stb/lcd/scroll_repeats", to_string(scroll_repeats).c_str());
	}
	else
	{
		proc_put("/proc/stb/lcd/initial_scroll_delay", false);
		proc_put("/proc/stb/lcd/final_scroll_delay", false);
		proc_put("/proc/stb/lcd/scroll_delay", false);
		proc_put("/proc/stb/lcd/scroll_repeats", false);
	}
}

void CLCD::setPower(int)
{
}

int CLCD::getPower()
{
	if (
		   g_info.hw_caps->display_type == HW_DISPLAY_LINE_TEXT
		|| g_info.hw_caps->display_type == HW_DISPLAY_LED_NUM
		|| g_info.hw_caps->display_type == HW_DISPLAY_LED_ONLY
	)
		return g_settings.lcd_setting[SNeutrinoSettings::LCD_POWER];
	else
		return 0;
}

void CLCD::togglePower(void)
{
	power = !power;
	if (!power)
		Clear();
	else
		showTime(true);

	if (g_info.hw_caps->display_can_set_brightness)
	{
		last_toggle_state_power = 1 - last_toggle_state_power;

		if (mode == MODE_STANDBY)
			setlcdparameter(g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS], last_toggle_state_power);
		else if (mode == MODE_SHUTDOWN)
			setlcdparameter(g_settings.lcd_setting[SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS], last_toggle_state_power);
		else
			setlcdparameter(g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS], last_toggle_state_power);
	}
}

void CLCD::setMuted(bool mu)
{
	printf("CLCD::%s %d\n", __func__, mu);
	muted = mu;
	if (muted)
		showVolume(volume, false);
	ShowIcon(FP_ICON_MUTE, muted);

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	wake_up();
#endif
}

void CLCD::resume()
{
}

void CLCD::pause()
{
}

void CLCD::Lock()
{
	creat("/tmp/vfd.locked", 0);
}

void CLCD::Unlock()
{
	unlink("/tmp/vfd.locked");
}

void CLCD::Clear()
{
	ShowText(" ", false);
}

void CLCD::count_down()
{
	if (timeout_cnt > 0) {
		timeout_cnt--;
		if (timeout_cnt == 0 ) {
			if (g_settings.lcd_setting_dim_brightness > -1) {
				// save lcd brightness, setBrightness() changes global setting
				int b = g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS];
				setBrightness(g_settings.lcd_setting_dim_brightness);
				g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = b;
			} else {
				setPower(0);
			}
		}
	}

	if (switch_name_time_cnt > 0) {
		switch_name_time_cnt--;
		if (switch_name_time_cnt == 0) {
			if (g_settings.lcd_setting_dim_brightness > -1) {
				CLCD::getInstance()->showTime(true);
			}
		}
	}
}

void CLCD::setlcdparameter(int dimm, const int _power)
{
	if(dimm < 0)
		dimm = 0;
	else if(dimm > 15)
		dimm = 15;

	if(!_power)
		dimm = 0;

	if(brightness == dimm)
		return;

	brightness = dimm;

	printf("CLCD::%s(dimm %d power %d)\n", __func__, dimm, _power);
	setBrightness(dimm);
}

void CLCD::SetIcons(int, bool)
{
}

void CLCD::ShowDiskLevel()
{
	int percent = 0;
	uint64_t t, u;
	if (get_fs_usage(g_settings.network_nfs_recordingdir.c_str(), t, u))
	{
		percent = (int)((u * 100ULL) / t);
		printf("CLCD::%s %d\n", __func__, percent);
		proc_put("/proc/stb/lcd/symbol_hddprogress", percent);
	}
}
void CLCD::UpdateIcons()
{
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	CZapitChannel * chan = CZapit::getInstance()->GetCurrentChannel();
	if (chan)
	{
		ShowIcon(FP_ICON_HD,chan->isHD());
		ShowIcon(FP_ICON_LOCK,!chan->camap.empty());
		ShowIcon(FP_ICON_SCRAMBLED, chan->scrambled);
		if (chan->getAudioChannel() != NULL)
		{
			ShowIcon(FP_ICON_DD, chan->getAudioChannel()->audioChannelType == CZapitAudioChannel::AC3);
		}
	}
	ShowDiskLevel();
#endif
}

void CLCD::ShowIcon(fp_icon i, bool on)
{
	switch (i)
	{
		case FP_ICON_CAM1:
			led_r = on;
			if (g_info.hw_caps->display_type == HW_DISPLAY_LINE_TEXT)
			{
				proc_put("/proc/stb/lcd/symbol_recording", on);
			}
			else
				setled(led_r, -1); /* switch instant on / switch off if disabling */
			break;
		case FP_ICON_PLAY:
			led_g = on;
			if (g_info.hw_caps->display_type == HW_DISPLAY_LINE_TEXT)
			{
				proc_put("/proc/stb/lcd/symbol_playback", on);
			}
			else
				setled(-1, led_g);
			break;
		case FP_ICON_USB:
			usb_icon = on;
			proc_put("/proc/stb/lcd/symbol_usb", on);
			break;
		case FP_ICON_HDD:
			proc_put("/proc/stb/lcd/symbol_hdd", on);
			break;
		case FP_ICON_PAUSE:
			break;
		case FP_ICON_FF:
			break;
		case FP_ICON_FR:
			break;
		case FP_ICON_DD:
			break;
		case FP_ICON_LOCK:
			proc_put("/proc/stb/lcd/symbol_scrambled", on);
			break;
		case FP_ICON_RADIO:
			break;
		case FP_ICON_TV:
			proc_put("/proc/stb/lcd/symbol_tv", on);
			break;
		case FP_ICON_HD:
			break;
		case FP_ICON_CLOCK:
			timer_icon = on;
			proc_put("/proc/stb/lcd/symbol_timeshift", on);
			break;
		case FP_ICON_MUTE:
			proc_put("/proc/stb/lcd/symbol_mute", on);
			break;
		case FP_ICON_SCRAMBLED:
			proc_put("/proc/stb/lcd/symbol_scrambled", on);
			break;
		default:
			break;
	}
}

void CLCD::ShowText(const char * str, bool update_timestamp)
{
	int fd = dev_open();
	int len = strlen(str);
	if (fd < 0)
		return;
	printf("%s '%s'\n", __func__, str);
	write(fd, str, len);
	close(fd);
	if (update_timestamp)
	{
		last_display = time(NULL);
		/* increase timeout to ensure that everything is displayed
		 * the driver displays 5 characters per second */
		if (len > g_info.hw_caps->display_xres)
			last_display += (len - g_info.hw_caps->display_xres) / 5;
	}
}

void CLCD::setEPGTitle(const std::string)
{
}
