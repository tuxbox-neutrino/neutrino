#include <stdio.h>

#include "init_td.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <directfb.h>

extern "C" {
#include <tdpanel/ir_ruwido.h>
#include <hardware/avs/avs_inf.h>
#include <hardware/avs/bios_system_config.h>
}
#include "lt_dfbinput.h"
#include "pwrmngr.h"

#include "lt_debug.h"
#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_INIT, NULL, args)
#define lt_info(args...) _lt_info(TRIPLE_DEBUG_INIT, NULL, args)

static bool initialized = false;

/* the super interface */
IDirectFB *dfb;
/* the primary surface */
static IDirectFBSurface *primary;
IDirectFBSurface *dfbdest;
static IDirectFBDisplayLayer *layer;
int gfxfd = -1;

#define DFBCHECK(x...)                                                \
	err = x;                                                      \
	if (err != DFB_OK) {                                          \
		fprintf(stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
		DirectFBErrorFatal(#x, err );                         \
	}

static void dfb_init()
{
	int argc = 0;
	DFBResult err;
	DFBSurfaceDescription dsc;
	DFBSurfacePixelFormat pixelformat;
	int SW, SH;

	DFBCHECK(DirectFBInit(&argc, NULL));
	/* neutrino does its own VT handling */
	DirectFBSetOption("no-vt-switch", NULL);
	DirectFBSetOption("no-vt", NULL);
	/* signal handling seems to interfere with neutrino */
	DirectFBSetOption("no-sighandler", NULL);
	/* if DirectFB grabs the remote, neutrino does not get events */
	/* now we handle the input via a DFB thread and push it to
	 * neutrino via uinput, so reenable tdremote module
	DirectFBSetOption("disable-module", "tdremote");
	 */
	DirectFBSetOption("disable-module", "keyboard");
	DirectFBSetOption("disable-module", "linux_input");
	DFBCHECK(DirectFBCreate(&dfb));

	err = dfb->SetCooperativeLevel(dfb, DFSCL_FULLSCREEN);
	if (err)
		DirectFBError("Failed to get exclusive access", err);

	dsc.flags = DSDESC_CAPS;
	dsc.caps = DSCAPS_PRIMARY;

	DFBCHECK(dfb->CreateSurface( dfb, &dsc, &primary ));
	/* set pixel alpha mode */
	dfb->GetDisplayLayer(dfb, DLID_PRIMARY, &layer);
	DFBCHECK(layer->SetCooperativeLevel(layer, DLSCL_EXCLUSIVE));
	DFBDisplayLayerConfig conf;
	DFBCHECK(layer->GetConfiguration(layer, &conf));
	conf.flags   = DLCONF_OPTIONS;
	conf.options = (DFBDisplayLayerOptions)((conf.options & ~DLOP_OPACITY) | DLOP_ALPHACHANNEL);
	DFBCHECK(layer->SetConfiguration(layer, &conf));

	primary->GetPixelFormat(primary, &pixelformat);
	primary->GetSize(primary, &SW, &SH);
	primary->Clear(primary, 0, 0, 0, 0);
	primary->GetSubSurface(primary, NULL, &dfbdest);
	dfbdest->Clear(dfbdest, 0, 0, 0, 0);

	start_input_thread(dfb);
}

static void dfb_deinit()
{
	stop_input_thread();
	dfbdest->Release(dfbdest);
	primary->Release(primary);
	layer->Release(layer);
	dfb->Release(dfb);
}

static void rc_init()
{
	/* set remote control address from bootloader config */
	int fd = open("/dev/stb/tdsystem", O_RDWR);
	struct BIOS_CONFIG_AREA bca;
	unsigned short rc_addr = 0xff;
	if (ioctl(fd, IOC_AVS_GET_LOADERCONFIG, &bca) != 0)
		fprintf(stderr, "%s: IOC_AVS_GET_LOADERCONFIG failed: %m\n", __FUNCTION__);
	else
		rc_addr = bca.ir_adrs;
	close(fd);
	fd = open("/dev/stb/tdremote", O_RDWR);
	if (ioctl(fd, IOC_IR_SET_ADDRESS, rc_addr) < 0)
		fprintf(stderr, "%s: IOC_IR_SET_ADDRESS %d failed: %m\n", __FUNCTION__, rc_addr);
	/* short delay in the driver improves responsiveness and reduces spurious
	   "key up" events during zapping */
	//ioctl(fd, IOC_IR_SET_DELAY, 1);  TODO: needs more work in rcinput
	close(fd);
	lt_info("%s rc_addr=0x%02hx\n", __FUNCTION__, rc_addr);
}

void init_td_api()
{
	if (!initialized)
		lt_debug_init();
	lt_info("%s begin, initialized=%d, debug=0x%02x\n", __FUNCTION__, (int)initialized, debuglevel);
	if (!initialized)
	{
		/* leave standby early, this avoids popping noise on audio device */
		cCpuFreqManager f;
		f.SetCpuFreq(0);	/* CPUFREQ == 0 is the trigger for leaving standby */
		/* DirectFB does setpgid(0,0), which disconnects us from controlling terminal
		   and thus disables e.g. ctrl-C. work around that. */
		pid_t pid = getpgid(0);
		dfb_init();
		if (setpgid(0, pid))
			perror("setpgid");
		rc_init();
		gfxfd = open("/dev/stb/tdgfx", O_RDWR);
		if (gfxfd < 0)
			perror("open /dev/stb/tdgfx");
		fcntl(gfxfd, F_SETFD, FD_CLOEXEC);
	}
	/* load the module which converts the TD tuner to a Linux-DVB frontend... */
	system("/sbin/modprobe td-dvb-frontend");
	initialized = true;
	lt_info("%s end\n", __FUNCTION__);
}

void shutdown_td_api()
{
	lt_info("%s, initialized = %d\n", __FUNCTION__, (int)initialized);
	if (initialized)
		dfb_deinit();
	if (gfxfd > -1)
		close(gfxfd);
	gfxfd = -1;
	initialized = false;
}
