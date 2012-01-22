#include <stdio.h>

#include "pwrmngr.h"
#include "lt_debug.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <avs/avs_inf.h>

#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_PWRMNGR, this, args)
void cCpuFreqManager::Up(void) { lt_debug("%s\n", __FUNCTION__); }
void cCpuFreqManager::Down(void) { lt_debug("%s\n", __FUNCTION__); }
void cCpuFreqManager::Reset(void) { lt_debug("%s\n", __FUNCTION__); }
/* those function dummies return true or "harmless" values */
bool cCpuFreqManager::SetDelta(unsigned long) { lt_debug("%s\n", __FUNCTION__); return true; }
unsigned long cCpuFreqManager::GetCpuFreq(void) { lt_debug("%s\n", __FUNCTION__); return 0; }
unsigned long cCpuFreqManager::GetDelta(void) { lt_debug("%s\n", __FUNCTION__); return 0; }
//
cCpuFreqManager::cCpuFreqManager(void) { lt_debug("%s\n", __FUNCTION__); }

bool cPowerManager::SetState(PWR_STATE) { lt_debug("%s\n", __FUNCTION__); return true; }

bool cPowerManager::Open(void) { lt_debug("%s\n", __FUNCTION__); return true; }
void cPowerManager::Close(void) { lt_debug("%s\n", __FUNCTION__); }
//
bool cPowerManager::SetStandby(bool Active, bool Passive)
{
	lt_debug("%s(%d, %d)\n", __FUNCTION__, Active, Passive);
	return true;
}

bool cCpuFreqManager::SetCpuFreq(unsigned long f)
{
	/* actually SetCpuFreq is used to determine if the system is in standby
	   this is an "elegant" hack, because:
	   * during a recording, cpu freq is kept "high", even if the box is sent to standby
	   * the "SetStandby" call is made even if a recording is running
	   On the TD, setting standby disables the frontend, so we must not do it
	   if a recording is running.
	   For now, the values in neutrino are hardcoded:
	   * f == 0        => max => not standby
	   * f == 50000000 => min => standby
	 */
	lt_debug("%s(%lu) => set standby = %s\n", __FUNCTION__, f, f?"true":"false");
	int fd = open("/dev/stb/tdsystem", O_RDONLY);
	if (fd < 0)
	{
		perror("open tdsystem");
		return false;
	}
	if (f)
	{
		ioctl(fd, IOC_AVS_SET_VOLUME, 31); /* mute AVS to avoid ugly noise */
		ioctl(fd, IOC_AVS_STANDBY_ENTER);
	}
	else
	{
		ioctl(fd, IOC_AVS_SET_VOLUME, 31); /* mute AVS to avoid ugly noise */
		ioctl(fd, IOC_AVS_STANDBY_LEAVE);
		/* unmute will be done by cAudio::do_mute(). Ugly, but prevents pops */
		// ioctl(fd, IOC_AVS_SET_VOLUME, 0); /* max gain */
	}

	close(fd);
	return true;
}

//
cPowerManager::cPowerManager(void) { lt_debug("%s\n", __FUNCTION__); }
cPowerManager::~cPowerManager() { lt_debug("%s\n", __FUNCTION__); }

