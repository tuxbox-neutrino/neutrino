/*
 * $Id: frontend.cpp,v 1.55 2004/04/20 14:47:15 derget Exp $
 *
 * (C) 2002-2003 Andreas Oberritter <obi@tuxbox.org>
 *
 * (C) 2007-2013,2015-2016 Stefan Seyfried
 * Copyright (C) 2011 CoolStream International Ltd 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* system c */
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <unistd.h>
#include <cmath>
#include <fstream>
/* zapit */
#include <config.h>
#include <system/helpers.h>
#include <zapit/debug.h>
#include <zapit/settings.h>
#include <zapit/getservices.h>
#include <connection/basicserver.h>
#include <zapit/client/msgtypes.h>
#include <zapit/frontend_c.h>
#include <zapit/satconfig.h>
#include <driver/abstime.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/version.h>

#include <hardware_caps.h>

extern transponder_list_t transponders;
extern int zapit_debug;

#define SOUTH	0
#define NORTH	1
#define EAST	0
#define WEST	1
#define USALS

#define FE_MAX_PROPS 18

#define diff(x,y)	(max(x,y) - min(x,y))

#define FE_TIMER_INIT()					\
	int64_t timer_start;				\
	static uint32_t tmin = 2000, tmax = 0;		\
	uint32_t timer_msec = 0;

#define FE_TIMER_START()				\
	timer_start = time_monotonic_ms();

#define FE_TIMER_STOP(label)				\
	timer_msec = (uint32_t)(time_monotonic_ms() -	\
			timer_start);			\
	if(tmin > timer_msec) tmin = timer_msec;	\
	if(tmax < timer_msec) tmax = timer_msec;	\
	printf("[fe%d/%d] %s: %u msec (min %u max %u)\n", \
		adapter, fenumber, label, timer_msec, tmin, tmax);

#define SETCMD(c, d) {					\
	prop[cmdseq.num].cmd = (c);			\
	prop[cmdseq.num].u.data = (d);			\
	if (cmdseq.num++ > DTV_IOCTL_MAX_MSGS) {	\
		printf("ERROR: too many tuning commands on frontend %d/%d", adapter, fenumber); \
		return;					\
	}						\
}

// Internal Inner FEC representation
typedef enum dvb_fec {
	fAuto,
	f1_2,
	f2_3,
	f3_4,
	f5_6,
	f7_8,
	f8_9,
	f3_5,
	f4_5,
	f9_10,
	fNone = 15,
	f13_45,
	f9_20,
	f11_20,
	f23_36,
	f25_36,
	f13_18,
	f26_45,
	f28_45,
	f7_9,
	f77_90,
	f32_45,
	f11_15,
	f1_2_L,
	f8_15_L,
	f3_5_L,
	f2_3_L,
	f5_9_L,
	f26_45_L
} dvb_fec_t;

static fe_sec_voltage_t unicable_lowvolt = SEC_VOLTAGE_13;

#define TIME_STEP 200
#define TIMEOUT_MAX_MS (feTimeout*100)

/*********************************************************************************************************/

CFrontend::CFrontend(int Number, int Adapter)
{
	DBG("[fe%d/%d] New frontend on adapter %d\n", Adapter, Number, Adapter);
	fd			= -1;
	fenumber		= Number;
	adapter			= Adapter;
	slave			= false; /* is set in frontend->setMasterSlave() */
	standby			= true;
	locked			= false;
	usecount		= 0;

	femode			= FE_MODE_INDEPENDENT;
	masterkey		= 0;

	tuned			= false;
	uncommitedInput		= 255;

	currentDiseqc		= 255;
	config.diseqcType	= NO_DISEQC;
	config.diseqcRepeats	= 0;
	config.uni_scr		= 0;		/* the unicable SCR address 0-7 */
	config.uni_freq		= 0;		/* the unicable frequency in MHz */
	config.uni_lnb		= 0;		/* for two-position switches */
	config.uni_pin		= -1;		/* for MDU setups */
	config.highVoltage	= false;
	config.motorRotationSpeed = 0;		//in 0.1 degrees per second

	feTimeout		= 40;
	currentVoltage		= SEC_VOLTAGE_OFF;
	currentToneMode		= SEC_TONE_ON;

	/* some broken hardware (a coolstream neo on my desk) does not lower
	 * the voltage below 18V without enough DC load on the coax cable.
	 * with unicable bus setups, there is no DC load on the coax... leading
	 * to a completely blocked bus due to this broken hardware.
	 * Switching off the voltage completely works around this issue */
	if (getenv("UNICABLE_BROKEN_FRONTEND") != NULL)
		unicable_lowvolt = SEC_VOLTAGE_OFF;
	memset(&info, 0, sizeof(info));

	deliverySystemMask = UNKNOWN_DS;
	fe_can_multistream = false;
}

CFrontend::~CFrontend(void)
{
	DBG("[fe%d/%d] close frontend fd %d\n", adapter, fenumber, fd);
	if(fd >= 0)
		Close();
}

bool CFrontend::Open(bool init)
{
	if(!standby)
		return false;

	char filename[128];
	snprintf(filename, sizeof(filename), "/dev/dvb/adapter%d/frontend%d", adapter, fenumber);
	DBG("[fe%d/%d] open %s\n", adapter, fenumber, filename);

	if (adapter == -1) {
		deliverySystemMask |= DVB_S;
		deliverySystemMask |= DVB_S2;
		deliverySystemMask |= DVB_S2X;
		info.type = FE_QPSK;
		strcpy(info.name, "dummyfe");
		return false;
	}

	mutex.lock();
	if (fd < 0) {
		if ((fd = open(filename, O_RDWR | O_NONBLOCK | O_CLOEXEC)) < 0) {
			ERROR(filename);
			mutex.unlock();
			return false;
		}
		getFEInfo();
	}

	currentTransponder.setTransponderId(0);
	standby = false;

	mutex.unlock();

	if (init)
		Init();

	return true;
}

void CFrontend::getFEInfo(void)
{
	fop(ioctl, FE_GET_INFO, &info);

	//FIXME if (fenumber > 1) info.type = FE_QAM;

	printf("[fe%d/%d] frontend fd %d type %d\n", adapter, fenumber, fd, info.type);
	bool legacy = true;

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	std::ifstream in;
	if (adapter == 0)
		in.open("/proc/bus/nim_sockets");
	if (in.is_open())
	{
		std::string line;
		bool found = false;
		while (getline(in, line))
		{
			if (line.find("NIM Socket "+to_string(fenumber)+":") !=std::string::npos)
				found = true;

			if ((line.find("Name:") != std::string::npos) && found)
			{
				//printf("NIM SOCKET: %s\n",line.substr(line.find_first_of(":")+2).c_str());
#if BOXMODEL_VUPLUS_ALL
				sprintf(info.name,"%s", line.substr(line.find_first_of(":") + 9).c_str());
#else
				std::string tmp = info.name;
				sprintf(info.name,"%s (%s)", tmp.c_str(), line.substr(line.find_first_of(":") + 2).c_str());
#endif
				break;
			}
		}
	in.close();
	}
#endif // HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE

	deliverySystemMask = UNKNOWN_DS;
	forcedSystemMask = UNKNOWN_DS;

#if (DVB_API_VERSION >= 5) && (DVB_API_VERSION_MINOR >= 5)
	dtv_property prop[1];
	dtv_properties cmdseq;

	memset(prop, 0, sizeof(prop));
	memset(&cmdseq, 0, sizeof(cmdseq));
	cmdseq.props = prop;

	SETCMD(DTV_ENUM_DELSYS, 0);
	int ret = fop(ioctl, FE_GET_PROPERTY, &cmdseq);
	if (ret == 0) {
		for (uint32_t i = 0; i < prop[0].u.buffer.len; i++) {
			if (i >= MAX_DELSYS) {
				printf("[fe%d/%d] ERROR: too many delivery systems\n", adapter, fenumber);
				break;
			}

			switch ((fe_delivery_system_t)prop[0].u.buffer.data[i]) {
			case SYS_DVBC_ANNEX_A:
			case SYS_DVBC_ANNEX_B:
			case SYS_DVBC_ANNEX_C:
				deliverySystemMask |= DVB_C;
				printf("[fe%d/%d] add delivery system DVB-C (delivery_system: %d)\n", adapter, fenumber, (fe_delivery_system_t)prop[0].u.buffer.data[i]);
				break;
			case SYS_DVBT:
				deliverySystemMask |= DVB_T;
				printf("[fe%d/%d] add delivery system DVB-T (delivery_system: %d)\n", adapter, fenumber, (fe_delivery_system_t)prop[0].u.buffer.data[i]);
				break;
			case SYS_DVBT2:
				deliverySystemMask |= DVB_T2;
				fe_can_multistream = info.caps & FE_CAN_MULTISTREAM;
				printf("[fe%d/%d] add delivery system DVB-T2 (delivery_system: %d / Multistream: %s)\n", adapter, fenumber, (fe_delivery_system_t)prop[0].u.buffer.data[i], fe_can_multistream ? "yes" :"no");
				break;
			case SYS_DVBS:
				deliverySystemMask |= DVB_S;
				printf("[fe%d/%d] add delivery system DVB-S (delivery_system: %d)\n", adapter, fenumber, (fe_delivery_system_t)prop[0].u.buffer.data[i]);
				break;
			case SYS_DVBS2:
				deliverySystemMask |= DVB_S2;
				fe_can_multistream = info.caps & FE_CAN_MULTISTREAM;
				printf("[fe%d/%d] add delivery system DVB-S2 (delivery_system: %d / Multistream: %s)\n", adapter, fenumber, (fe_delivery_system_t)prop[0].u.buffer.data[i], fe_can_multistream ? "yes" :"no");
				break;
			case SYS_DVBS2X:
				deliverySystemMask |= DVB_S2X;
				fe_can_multistream = info.caps & FE_CAN_MULTISTREAM;
				printf("[fe%d/%d] add delivery system DVB-S2X (delivery_system: %d / Multistream: %s)\n", adapter, fenumber, (fe_delivery_system_t)prop[0].u.buffer.data[i], fe_can_multistream ? "yes" :"no");
				break;
			case SYS_DTMB:
				deliverySystemMask |= DTMB;
				printf("[fe%d/%d] add delivery system DTMB (delivery_system: %d)\n", adapter, fenumber, (fe_delivery_system_t)prop[0].u.buffer.data[i]);
				break;
			default:
				printf("[fe%d/%d] ERROR: delivery system unknown (delivery_system: %d)\n", adapter, fenumber, (fe_delivery_system_t)prop[0].u.buffer.data[i]);
				continue;
			}

		}
		legacy = false;
	} else {
		printf("WARNING: can't query delivery systems on frontend %d/%d - falling back to legacy mode\n", adapter, fenumber);
	}
#endif
	if (legacy) {
		// Legacy mode (DVB-API < 5.5):
		switch (info.type) {
		case FE_QPSK:
			deliverySystemMask |= DVB_S;
			deliverySystemMask |= DVB_S2;
			deliverySystemMask |= DVB_S2X;
			break;
		case FE_OFDM:
			deliverySystemMask |= DVB_T;
#ifdef SYS_DVBT2
			if (info.caps & FE_CAN_2G_MODULATION)
				deliverySystemMask |= DVB_T2;
#endif
			break;
		case FE_QAM:
			deliverySystemMask |= DVB_C;
			break;
		default:
			printf("ERROR: unknown frontend type %d on frontend %d/%d", info.type, adapter, fenumber);
		}
	}
}

void CFrontend::Init(void)
{
	/* if frontend was not enabled before, it might not be opened */
	Open();
	mutex.lock();
	// Set the voltage to On, and wait voltage to become stable
	// and wait for diseqc equipment to be ready.
	secSetVoltage(SEC_VOLTAGE_13, 100);
	secSetTone(SEC_TONE_OFF, 20);
	setDiseqcType((diseqc_t) config.diseqcType, true);
	setTsidOnid(0);
	mutex.unlock();
}

void CFrontend::Close(void)
{
	if(standby)
		return;

	INFO("[fe%d/%d] close frontend fd %d", adapter, fenumber, fd);

	if (!slave && config.diseqcType > MINI_DISEQC)
		sendDiseqcStandby();
	// Disable tone first as it's imposed on voltage
	secSetTone(SEC_TONE_OFF, 20);
	// Disable voltage immediately and wait 50ms to prevent
	// a fast power-off -> power-on sequence where diseqc equipment
	// might not reset properly.
	secSetVoltage(SEC_VOLTAGE_OFF, 50);

	tuned	= false;
	standby	= true;;
	close(fd);
	fd = -1;
}

void CFrontend::setMasterSlave(bool _slave)
{
	if(slave == _slave)
		return;

	if(_slave) {
		// Disable tone first as it's imposed on voltage
		secSetTone(SEC_TONE_OFF, 20);
		// Disable voltage immediately and wait 50ms to prevent
		// a fast power-off -> power-on sequence where diseqc equipment
		// might not reset properly.
		secSetVoltage(SEC_VOLTAGE_OFF, 50);
	}
	slave = _slave;
	if(!slave)
		Init();
}

void CFrontend::reset(void)
{
	// No-op
}

void CFrontend::Lock()
{
	usecount++;
	INFO("[fe%d/%d] usecount %d tp %" PRIx64, adapter, fenumber, usecount, getTsidOnid());
}

void CFrontend::Unlock()
{
	if(usecount > 0)
		usecount--;
	INFO("[fe%d/%d] usecount %d tp %" PRIx64, adapter, fenumber, usecount, getTsidOnid());
}

fe_code_rate_t CFrontend::getCFEC()
{
	if (isSat(currentTransponder.feparams.delsys) || isCable(currentTransponder.feparams.delsys))
		return currentTransponder.feparams.fec_inner;
	else
		return FEC_NONE;
}

fe_code_rate_t CFrontend::getCodeRate(const uint8_t fec_inner, delivery_system_t delsys)
{
	dvb_fec_t f = (dvb_fec_t) fec_inner;
	fe_code_rate_t fec;

	if (delsys == DVB_S || delsys == DVB_C || delsys == DVB_T || delsys == DVB_T2) {
		switch (f) {
		case fNone:
			fec = FEC_NONE;
			break;
		case f1_2:
			fec = FEC_1_2;
			break;
		case f2_3:
			fec = FEC_2_3;
			break;
		case f3_4:
			fec = FEC_3_4;
			break;
		case f5_6:
			fec = FEC_5_6;
			break;
		case f7_8:
			fec = FEC_7_8;
			break;
		default:
			if (zapit_debug)
				printf("no valid fec for DVB-%s set.. assume auto\n", (delsys == DVB_S ? "S" : (delsys == DVB_C ? "C" : "T/T2")));
			/* fall through */
		case fAuto:
			fec = FEC_AUTO;
			break;
		}
	} else {
		switch (f) {
		case f1_2:
			fec = FEC_1_2;
			break;
		case f2_3:
			fec = FEC_2_3;
			break;
		case f3_4:
			fec = FEC_3_4;
			break;
		case f3_5:
			fec = FEC_3_5;
			break;
		case f4_5:
			fec = FEC_4_5;
			break;
		case f5_6:
			fec = FEC_5_6;
			break;
		case f7_8:
			fec = FEC_7_8;
			break;
		case f8_9:
			fec = FEC_8_9;
			break;
		case f9_10:
			fec = FEC_9_10;
			break;
		case f13_45:
			fec = FEC_13_45;
			break;
		case f9_20:
			fec = FEC_9_20;
			break;
		case f11_20:
			fec = FEC_11_20;
			break;
		case f23_36:
			fec = FEC_23_36;
			break;
		case f25_36:
			fec = FEC_25_36;
			break;
		case f13_18:
			fec = FEC_13_18;
			break;
		case f26_45:
			fec = FEC_26_45;
			break;
		case f28_45:
			fec = FEC_28_45;
			break;
		case f7_9:
			fec = FEC_7_9;
			break;
		case f77_90:
			fec = FEC_77_90;
			break;
		case f32_45:
			fec = FEC_32_45;
			break;
		case f11_15:
			fec = FEC_11_15;
			break;
		case f1_2_L:
			fec = FEC_1_2_L;
			break;
		case f8_15_L:
			fec = FEC_8_15_L;
			break;
		case f3_5_L:
			fec = FEC_3_5_L;
			break;
		case f2_3_L:
			fec = FEC_2_3_L;
			break;
		case f5_9_L:
			fec = FEC_5_9_L;
			break;
		case f26_45_L:
			fec = FEC_26_45_L;
			break;
		default:
			if (zapit_debug)
				printf("no valid fec for DVB-S2/DVB-S2X set!\n");
			/* fall through */
		case fAuto:
			fec = FEC_AUTO;
			break;
		}
	}

	return fec;
}

fe_hierarchy_t CFrontend::getHierarchy(const uint8_t hierarchy)
{
	switch (hierarchy) {
	case 0x00:
		return HIERARCHY_NONE;
	case 0x01:
		return HIERARCHY_1;
	case 0x02:
		return HIERARCHY_2;
	case 0x03:
		return HIERARCHY_4;
	default:
		return HIERARCHY_AUTO;
	}
}

fe_rolloff_t CFrontend::getRolloff(const uint8_t rolloff)
{
	switch (rolloff) {
	case 0x00:
		return ROLLOFF_35;
	case 0x01:
		return ROLLOFF_25;
	case 0x02:
		return ROLLOFF_20;
	default:
		return ROLLOFF_AUTO;
	}
}

fe_modulation_t CFrontend::getModulation(const uint8_t modulation)
{
	switch (modulation) {
	case 0x00:
		return QPSK;
	case 0x01:
		return QAM_16;
	case 0x02:
		return QAM_32;
	case 0x03:
		return QAM_64;
	case 0x04:
		return QAM_128;
	case 0x05:
		return QAM_256;
	default:
		return QAM_AUTO;
	}
}

fe_bandwidth_t CFrontend::getBandwidth(const uint8_t bandwidth)
{
	switch (bandwidth) {
	case 0x00:
		return BANDWIDTH_8_MHZ;
	case 0x01:
		return BANDWIDTH_7_MHZ;
	case 0x02:
		return BANDWIDTH_6_MHZ;
#if _HAVE_DVB57
	case 0x03:
		return BANDWIDTH_5_MHZ;
#endif
	default:
		return BANDWIDTH_AUTO; // AUTO
	}
}

fe_guard_interval_t CFrontend::getGuardInterval(const uint8_t guard_interval)
{
	switch (guard_interval) {
	case 0x00:
		return GUARD_INTERVAL_1_32;
	case 0x01:
		return GUARD_INTERVAL_1_16;
	case 0x02:
		return GUARD_INTERVAL_1_8;
	case 0x03:
		return GUARD_INTERVAL_1_4;
#if defined GUARD_INTERVAL_1_128
	case 0x05:
		return GUARD_INTERVAL_1_128;
	case 0x06:
		return GUARD_INTERVAL_19_128;
	case 0x07:
		return GUARD_INTERVAL_19_256;
#endif
	default:
		return GUARD_INTERVAL_AUTO;
	}
}

fe_modulation_t CFrontend::getConstellation(const uint8_t constellation)
{
	switch (constellation) {
	case 0x00:
		return QPSK;
	case 0x01:
		return QAM_16;
	case 0x02:
		return QAM_64;
	case 0x04:
		return QAM_128;
	case 0x05:
		return QAM_256;
	default:
		return QAM_AUTO;
	}
}

fe_transmit_mode_t CFrontend::getTransmissionMode(const uint8_t transmission_mode)
{
	switch (transmission_mode) {
	case 0x00:
		return TRANSMISSION_MODE_2K;
	case 0x01:
		return TRANSMISSION_MODE_8K;
	case 0x02:
		return TRANSMISSION_MODE_4K;
#if defined TRANSMISSION_MODE_1K
	case 0x04:
		return TRANSMISSION_MODE_1K;
	case 0x05:
		return TRANSMISSION_MODE_16K;
	case 0x06:
		return TRANSMISSION_MODE_32K;
#endif
	default:
		return TRANSMISSION_MODE_AUTO;
	}
}

fe_pls_mode_t CFrontend::getPLSMode(const uint8_t pls_mode)
{
	switch (pls_mode) {
	case 0x00:
		return PLS_Root;
	case 0x01:
		return PLS_Gold;
	case 0x02:
		return PLS_Combo;
	case 0x03:
		return PLS_Unknown;
	default:
		return PLS_Root;
	}
}

uint8_t CFrontend::getPolarization(void) const
{
	return currentTransponder.getPolarization();
}

uint32_t CFrontend::getRate() const
{
	return currentTransponder.getSymbolRate();
}

fe_status_t CFrontend::getStatus(void) const
{
	struct dvb_frontend_event event;
	event.status = FE_REINIT;
	fop(ioctl, FE_READ_STATUS, &event.status);
	return (fe_status_t) (event.status & FE_HAS_LOCK);
}

#if 0
//never used
FrontendParameters CFrontend::getFrontend(void) const
{
	return currentTransponder.feparams;
}
#endif

uint32_t CFrontend::getBitErrorRate(void) const
{
	uint32_t ber = 0;
	fop(ioctl, FE_READ_BER, &ber);
	if (ber > 100000000) /* some drivers has useless values around 500.000.000 */
		ber = 0;

	return ber;
}

uint16_t CFrontend::getSignalStrength(void) const
{
	uint16_t strength = 0;
	fop(ioctl, FE_READ_SIGNAL_STRENGTH, &strength);

	return strength;
}

uint16_t CFrontend::getSignalNoiseRatio(void) const
{
	uint16_t snr = 0;
	fop(ioctl, FE_READ_SNR, &snr);
	return snr;
}

#if 0
//never used
uint32_t CFrontend::getUncorrectedBlocks(void) const
{
	uint32_t blocks = 0;
	fop(ioctl, FE_READ_UNCORRECTED_BLOCKS, &blocks);

	return blocks;
}
#endif

struct dvb_frontend_event CFrontend::getEvent(void)
{
	struct dvb_frontend_event event;
	struct pollfd pfd;
	static unsigned int timedout = 0;

	FE_TIMER_INIT();

	pfd.fd = fd;
	pfd.events = POLLIN | POLLPRI;
	pfd.revents = 0;

	memset(&event, 0, sizeof(struct dvb_frontend_event));

	FE_TIMER_START();

	while ((int) timer_msec < TIMEOUT_MAX_MS) {
		int ret = poll(&pfd, 1, TIMEOUT_MAX_MS - timer_msec);
		if (ret < 0) {
			ERROR("poll");
			continue;
		}
		if (ret == 0) {
			FE_TIMER_STOP("############################## poll timeout, time");
			continue;
		}

		if (pfd.revents & (POLLIN | POLLPRI)) {
			//FE_TIMER_STOP("poll has event after");
			timer_msec = (uint32_t)(time_monotonic_ms() - timer_start); /* FE_TIMER_STOP does this :( */
			memset(&event, 0, sizeof(struct dvb_frontend_event));

			ret = ioctl(fd, FE_GET_EVENT, &event);
			if (ret < 0) {
				ERROR("ioctl");
				continue;
			}
			//printf("[fe%d/%d] poll events %d status %x\n", adapter, fenumber, pfd.revents, event.status);
			if (event.status == 0) /* some drivers always deliver an empty event after tune */
				continue;

			if (event.status & FE_HAS_LOCK) {
				INFO("[fe%d/%d] ******** FE_HAS_LOCK: freq %lu", adapter, fenumber, (long unsigned int)event.parameters.frequency);
				tuned = true;
				break;
			} else if (event.status & FE_TIMEDOUT) {
				if(timedout < timer_msec)
					timedout = timer_msec;
				INFO("[fe%d/%d] ######## FE_TIMEDOUT (max %d)", adapter, fenumber, timedout);
				/*break;*/
			} else {
				if (event.status & FE_HAS_SIGNAL)
					printf("[fe%d/%d] FE_HAS_SIGNAL\n", adapter, fenumber);
				if (event.status & FE_HAS_CARRIER)
					printf("[fe%d/%d] FE_HAS_CARRIER\n", adapter, fenumber);
				if (event.status & FE_HAS_VITERBI)
					printf("[fe%d/%d] FE_HAS_VITERBI\n", adapter, fenumber);
				if (event.status & FE_HAS_SYNC)
					printf("[fe%d/%d] FE_HAS_SYNC\n", adapter, fenumber);
				if (event.status & FE_REINIT)
					printf("[fe%d/%d] FE_REINIT\n", adapter, fenumber);
				/* msec = TIME_STEP; */
			}
		} else if (pfd.revents & POLLHUP) {
			FE_TIMER_STOP("poll hup after");
			reset();
		}
	}
	//printf("[fe%d/%d] event after: %d\n", adapter, fenumber, tmsec);
	return event;
}

void CFrontend::getDelSys(int f, int m, const char *&fec, const char *&sys, const char *&mod)
{
	return getDelSys(getCurrentDeliverySystem(), f, m, fec, sys, mod);
}

void CFrontend::forceDelSys(int i)
{
	switch (i) {
		case 1:
			forcedSystemMask = ALL_TERR;
			break;
		case 2:
			forcedSystemMask = ALL_CABLE;
			break;
		default:
			forcedSystemMask = UNKNOWN_DS;
			break;
	}
}

void CFrontend::getXMLDelsysFEC(fe_code_rate_t xmlfec, delivery_system_t & delsys, fe_modulation_t &mod, fe_code_rate_t & fec)
{
	if ((int)xmlfec < FEC_S2_QPSK_1_2) {
		delsys = DVB_S;
		mod = QPSK;
	} else if ((int)xmlfec < FEC_S2_8PSK_1_2) {
		delsys = DVB_S2;
		mod = QPSK;
	} else {
		delsys = DVB_S2;
		mod = PSK_8;
	}

	switch ((int)xmlfec) {
	case FEC_1_2:
	case FEC_S2_QPSK_1_2:
	case FEC_S2_8PSK_1_2:
		fec = FEC_1_2;
		break;
	case FEC_2_3:
	case FEC_S2_QPSK_2_3:
	case FEC_S2_8PSK_2_3:
		fec = FEC_2_3;
		break;
	case FEC_3_4:
	case FEC_S2_QPSK_3_4:
	case FEC_S2_8PSK_3_4:
		fec = FEC_3_4;
		break;
	case FEC_S2_QPSK_3_5:
	case FEC_S2_8PSK_3_5:
		fec = FEC_3_5;
		break;
	case FEC_4_5:
	case FEC_S2_QPSK_4_5:
	case FEC_S2_8PSK_4_5:
		fec = FEC_4_5;
		break;
	case FEC_5_6:
	case FEC_S2_QPSK_5_6:
	case FEC_S2_8PSK_5_6:
		fec = FEC_5_6;
		break;
	case FEC_6_7:
		fec = FEC_6_7;
		break;
	case FEC_7_8:
	case FEC_S2_QPSK_7_8:
	case FEC_S2_8PSK_7_8:
		fec = FEC_7_8;
		break;
	case FEC_8_9:
	case FEC_S2_QPSK_8_9:
	case FEC_S2_8PSK_8_9:
		fec = FEC_8_9;
		break;
	case FEC_S2_QPSK_9_10:
	case FEC_S2_8PSK_9_10:
		fec = FEC_9_10;
		break;
#if 0 // TODO
	case FEC_13_45:
		fec = FEC_13_45;
		break;
	case FEC_9_20:
		fec = FEC_9_20;
		break;
	case FEC_11_20:
		fec = FEC_11_20;
		break;
	case FEC_23_36:
		fec = FEC_23_36;
		break;
	case FEC_25_36:
		fec = FEC_25_36;
		break;
	case FEC_13_18:
		fec = FEC_13_18;
		break;
	case FEC_26_45:
		fec = FEC_26_45;
		break;
	case FEC_28_45:
		fec = FEC_28_45;
		break;
	case FEC_7_9:
		fec = FEC_7_9;
		break;
	case FEC_77_90:
		fec = FEC_77_90;
		break;
	case FEC_32_45:
		fec = FEC_32_45;
		break;
	case FEC_11_15:
		fec = FEC_11_15;
		break;
	case FEC_1_2_L:
		fec = FEC_1_2_L;
		break;
	case FEC_8_15_L:
		fec = FEC_8_15_L;
		break;
	case FEC_3_5_L:
		fec = FEC_3_5_L;
		break;
	case FEC_2_3_L:
		fec = FEC_2_3_L;
		break;
	case FEC_5_9_L:
		fec = FEC_5_9_L;
		break;
	case FEC_26_45_L:
		fec = FEC_26_45_L;
		break;
#endif
	default:
		printf("[frontend] getXMLDelsysFEC: unknown FEC: %d !!!\n", xmlfec);
		/* fall through */
	case FEC_S2_AUTO:
	case FEC_AUTO:
		fec = FEC_AUTO;
		break;
	}
}

void CFrontend::getDelSys(delivery_system_t delsys, int f, int m, const char *&fec, const char *&sys, const char *&mod)
{
	switch (delsys) {
	case DVB_S:
		sys = "DVB";
		mod = "QPSK";
		break;
	case DVB_S2:
		sys = "DVB-S2";
		switch (m) {
		case QPSK:
			mod = "QPSK";
			break;
		case PSK_8:
			mod = "8PSK";
			break;
		default:
			printf("[frontend] unknown DVB-S2 modulation %d!\n", m);
			mod = "UNKNOWN";
		}
		break;
	case DVB_S2X:
		sys = "DVB-S2X";
		switch (m) {
		case QPSK:
			mod = "QPSK";
			break;
		case PSK_8:
			mod = "8PSK";
			break;
		case APSK_8:
			mod = "8APSK";
			break;
		case APSK_16:
			mod = "16APSK";
			break;
		case APSK_32:
			mod = "32APSK";
			break;
		default:
			printf("[frontend] unknown DVB-S2X modulation %d!\n", m);
			mod = "UNKNOWN";
		}
		break;
	case DVB_C:
	case DVB_T:
	case DVB_T2:
	case DTMB:
		switch(delsys) {
		case DVB_C:
			sys = "DVB-C (Annex A)";
			break;
		case DVB_T:
			sys = "DVB-T";
			break;
		case DVB_T2:
			sys = "DVB-T2";
			break;
		case DTMB:
			sys = "DTMB";
			break;
		default:
			printf("[frontend] unknown delsys %d!\n", delsys);
			sys = "UNKNOWN";
			break;
		}

		switch (m) {
		case QAM_16:
			mod = "QAM_16";
			break;
		case QAM_32:
			mod = "QAM_32";
			break;
		case QAM_64:
			mod = "QAM_64";
			break;
		case QAM_128:
			mod = "QAM_128";
			break;
		case QAM_256:
			mod = "QAM_256";
			break;
#if _HAVE_DVB57
		case QAM_4_NR:
			mod = "QAM_4_NR";
			break;
#endif
		case QPSK:
			if (delsys == DVB_T || delsys == DVB_T2 || delsys == DTMB) {
				mod = "QPSK"; // AKA QAM_4
				break;
			}
			/* fall through */
		case QAM_AUTO:
		default:
			mod = "QAM_AUTO";
			break;
		}
		break;
	default:
		INFO("[frontend] unknown delsys %d!", delsys);
		sys = "UNKNOWN";
		mod = "UNKNOWN";
		break;
	}

	switch (f) {
	case FEC_1_2:
		fec = "1/2";
		break;
	case FEC_2_3:
		fec = "2/3";
		break;
	case FEC_3_4:
		fec = "3/4";
		break;
	case FEC_4_5:
		fec = "4/5";
		break;
	case FEC_5_6:
		fec = "5/6";
		break;
	case FEC_6_7:
		fec = "6/7";
		break;
	case FEC_7_8:
		fec = "7/8";
		break;
	case FEC_8_9:
		fec = "8/9";
		break;
	case FEC_3_5:
		fec = "3/5";
		break;
	case FEC_9_10:
		fec = "9/10";
		break;
#if _HAVE_DVB57
	case FEC_2_5:
		fec = "2/5";
		break;
	case FEC_NONE:
		fec = "0";
		break;
#endif
	case FEC_13_45:
		fec = "13/45";
		break;
	case FEC_9_20:
		fec = "9/20";
		break;
	case FEC_11_20:
		fec = "11/20";
		break;
	case FEC_23_36:
		fec = "23/36";
		break;
	case FEC_25_36:
		fec = "25/36";
		break;
	case FEC_13_18:
		fec = "13/18";
		break;
	case FEC_26_45:
		fec = "26/45";
		break;
	case FEC_28_45:
		fec = "28/45";
		break;
	case FEC_7_9:
		fec = "7/9";
		break;
	case FEC_77_90:
		fec = "77/90";
		break;
	case FEC_32_45:
		fec = "32/45";
		break;
	case FEC_11_15:
		fec = "11/15";
		break;
	case FEC_1_2_L:
		fec = "1/2L";
		break;
	case FEC_8_15_L:
		fec = "8/15L";
		break;
	case FEC_3_5_L:
		fec = "3/5L";
		break;
	case FEC_2_3_L:
		fec = "2/3L";
		break;
	case FEC_5_9_L:
		fec = "5/9L";
		break;
	case FEC_26_45_L:
		fec = "26/45L";
		break;
	default:
		INFO("[frontend] getDelSys: unknown FEC: %d !!!\n", f);
		/* fall through */
	case FEC_AUTO:
		fec = "AUTO";
		break;
	}
}

fe_delivery_system_t CFrontend::getFEDeliverySystem(delivery_system_t Delsys)
{
	fe_delivery_system_t delsys;

	switch (Delsys) {
	case DVB_S:
		delsys = SYS_DVBS;
		break;
	case DVB_S2:
	case DVB_S2X:
		delsys = SYS_DVBS2;
		break;
	case DVB_T:
		delsys = SYS_DVBT;
		break;
#if _HAVE_DVB57
	case DVB_T2:
		delsys = SYS_DVBT2;
		break;
#endif
	case DVB_C:
		delsys = SYS_DVBC_ANNEX_A;
		break;
	//case DVB_C2: // not supported yet
	//	delsys = SYS_DVBC2;
		//break;
	case ISDBT:
		delsys = SYS_ISDBT;
		break;
	case ISDBC:
		delsys = SYS_ISDBC;
		break;
	case ISDBS:
		delsys = SYS_ISDBS;
		break;
	case DSS:
		delsys = SYS_DSS;
		break;
#if _HAVE_DVB57
	case DTMB:
		delsys = SYS_DTMB;
		break;
	case TURBO:
		delsys = SYS_TURBO;
		break;
#endif
	default:
		delsys = SYS_UNDEFINED;
		break;
	}

	return delsys;
}

delivery_system_t CFrontend::getZapitDeliverySystem(uint32_t delnr)
{
	return (delivery_system_t)ZAPIT_DS_BIT_MASK(delnr);
}

uint32_t CFrontend::getXMLDeliverySystem(delivery_system_t delsys)
{
	// WARNING: this nr is directly mapped to the bit mask specified in frontend_types.h
	uint32_t delnr = 0;

	switch (delsys) {
	case DVB_S:
		delnr = 0;
		break;
	case DVB_S2:
		delnr = 1;
		break;
	case DVB_C:
		delnr = 2;
		break;
	//case DVB_C2: // not supported yet
	//	delnr = SYS_DVBC2;
		//break;
	case DVB_T:
		delnr = 4;
		break;
	case DVB_T2:
		delnr = 5;
		break;
	case DTMB:
		delnr = 6;
		break;
	case DSS:
		delnr = 7;
		break;
	case TURBO:
		delnr = 8;
		break;
	case ISDBS:
		delnr = 9;
		break;
	case ISDBC:
		delnr = 10;
		break;
	case ISDBT:
		delnr = 11;
		break;
	case DVB_S2X:
		delnr = 12;
		break;
	default:
		printf("%s: unknown delivery system (%d)\n", __FUNCTION__, delsys);
		delnr = 0;
		break;
	}

	return delnr;
}

uint32_t CFrontend::getFEBandwidth(fe_bandwidth_t bandwidth)
{
	uint32_t bandwidth_hz;

	switch (bandwidth) {
	case BANDWIDTH_8_MHZ:
	default:
		bandwidth_hz  = 8000000;
		break;
	case BANDWIDTH_7_MHZ:
		bandwidth_hz  = 7000000;
		break;
	case BANDWIDTH_6_MHZ:
		bandwidth_hz  = 6000000;
		break;
#if _HAVE_DVB57
	case BANDWIDTH_5_MHZ:
		bandwidth_hz  = 5000000;
		break;
	case BANDWIDTH_1_712_MHZ:
		bandwidth_hz  = 1712000;
		break;
	case BANDWIDTH_10_MHZ:
		bandwidth_hz  = 10000000;
		break;
#endif
	case BANDWIDTH_AUTO:
		bandwidth_hz  = 0;
	}

	return bandwidth_hz;
}

int CFrontend::setFrontend(const FrontendParameters *feparams, bool nowait)
{
	tuned = false;

	struct dvb_frontend_event ev;
	{
		// Erase previous events
		while (1) {
			if (ioctl(fd, FE_GET_EVENT, &ev) < 0)
				break;
			//printf("[fe%d/%d] DEMOD: event status %d\n", adapter, fenumber, ev.status);
		}
	}

	fe_pilot_t pilot = PILOT_OFF;
	int fec = FEC_AUTO;
	fe_code_rate_t fec_inner = feparams->fec_inner;

	/* cast to int is ncesessary because many of the FEC_S2 values are not
	 * properly defined in the enum, thus the compiler complains... :-( */
	switch ((int)fec_inner)
	{
	case FEC_1_2:
		fec = FEC_1_2;
		break;
	case FEC_2_3:
		fec = FEC_2_3;
		if ((getFEDeliverySystem(feparams->delsys) == SYS_DVBS2) && feparams->modulation == PSK_8)
#if BOXMODEL_VUPLUS_ARM
			pilot = PILOT_AUTO;
#else
			pilot = PILOT_ON;
#endif
		break;
	case FEC_3_4:
		fec = FEC_3_4;
		if ((getFEDeliverySystem(feparams->delsys) == SYS_DVBS2) && feparams->modulation == PSK_8)
#if BOXMODEL_VUPLUS_ARM
			pilot = PILOT_AUTO;
#else
			pilot = PILOT_ON;
#endif
		break;
	case FEC_4_5:
		fec = FEC_4_5;
		break;
	case FEC_5_6:
		fec = FEC_5_6;
		if ((getFEDeliverySystem(feparams->delsys) == SYS_DVBS2) && feparams->modulation == PSK_8)
#if BOXMODEL_VUPLUS_ARM
			pilot = PILOT_AUTO;
#else
			pilot = PILOT_ON;
#endif
		break;
	case FEC_6_7:
		fec = FEC_6_7;
		break;
	case FEC_7_8:
		fec = FEC_7_8;
		break;
	case FEC_8_9:
		fec = FEC_8_9;
		break;
	case FEC_3_5:
		fec = FEC_3_5;
		if ((getFEDeliverySystem(feparams->delsys) == SYS_DVBS2) && feparams->modulation == PSK_8)
#if BOXMODEL_VUPLUS_ARM
			pilot = PILOT_AUTO;
#else
			pilot = PILOT_ON;
#endif
		break;
	case FEC_9_10:
		fec = FEC_9_10;
		break;
#if _HAVE_DVB57
	case FEC_2_5:
		fec = FEC_2_5;
		break;
	case FEC_NONE:
		fec = FEC_NONE;
		break;
#endif
	case FEC_13_45:
		fec = FEC_13_45;
		break;
	case FEC_9_20:
		fec = FEC_9_20;
		break;
	case FEC_11_20:
		fec = FEC_11_20;
		break;
	case FEC_23_36:
		fec = FEC_23_36;
		break;
	case FEC_25_36:
		fec = FEC_25_36;
		break;
	case FEC_13_18:
		fec = FEC_13_18;
		break;
	case FEC_26_45:
		fec = FEC_26_45;
		break;
	case FEC_28_45:
		fec = FEC_28_45;
		break;
	case FEC_7_9:
		fec = FEC_7_9;
		break;
	case FEC_77_90:
		fec = FEC_77_90;
		break;
	case FEC_32_45:
		fec = FEC_32_45;
		break;
	case FEC_11_15:
		fec = FEC_11_15;
		break;
	case FEC_1_2_L:
		fec = FEC_1_2_L;
		break;
	case FEC_8_15_L:
		fec = FEC_8_15_L;
		break;
	case FEC_3_5_L:
		fec = FEC_3_5_L;
		break;
	case FEC_2_3_L:
		fec = FEC_2_3_L;
		break;
	case FEC_5_9_L:
		fec = FEC_5_9_L;
		break;
	case FEC_26_45_L:
		fec = FEC_26_45_L;
		break;
	default:
		INFO("[fe%d/%d] DEMOD: unknown FEC: %d\n", adapter, fenumber, fec_inner);
	/* fall through */
	case FEC_AUTO:
		fec = FEC_AUTO;
		break;
	}
	switch(feparams->pilot)
	{
	case ZPILOT_ON:
		pilot = PILOT_ON;
		break;
	case ZPILOT_OFF:
		pilot = PILOT_OFF;
		break;
	case ZPILOT_AUTO:
	default:
		pilot = PILOT_AUTO;
		break;
	}

	struct dtv_property p[FE_MAX_PROPS];
	memset(p, 0, sizeof(p));
	struct dtv_properties cmdseq;
	cmdseq.props = p;
	cmdseq.num   = 0;
	p[cmdseq.num].cmd = DTV_CLEAR; cmdseq.num++;

	switch (feparams->delsys)
	{
	case DVB_S:
	case DVB_S2:
	case DVB_S2X:
		p[cmdseq.num].cmd = DTV_DELIVERY_SYSTEM,	p[cmdseq.num].u.data = getFEDeliverySystem(feparams->delsys),	cmdseq.num++;
		if (config.diseqcType == DISEQC_UNICABLE)
			p[cmdseq.num].cmd = DTV_FREQUENCY,
			p[cmdseq.num].u.data = sendEN50494TuningCommand(feparams->frequency, currentToneMode == SEC_TONE_ON,
											currentVoltage == SEC_VOLTAGE_18, !!config.uni_lnb),					cmdseq.num++;

		else if (config.diseqcType == DISEQC_UNICABLE2)
			p[cmdseq.num].cmd = DTV_FREQUENCY,
			p[cmdseq.num].u.data = sendEN50607TuningCommand(feparams->frequency, currentToneMode == SEC_TONE_ON,
											currentVoltage == SEC_VOLTAGE_18, config.uni_lnb),						cmdseq.num++;
		else
			p[cmdseq.num].cmd = DTV_FREQUENCY,		p[cmdseq.num].u.data = feparams->frequency,						cmdseq.num++;
		p[cmdseq.num].cmd = DTV_MODULATION,			p[cmdseq.num].u.data = feparams->modulation,					cmdseq.num++;
		p[cmdseq.num].cmd = DTV_SYMBOL_RATE,		p[cmdseq.num].u.data = feparams->symbol_rate,					cmdseq.num++;
		p[cmdseq.num].cmd = DTV_INNER_FEC,			p[cmdseq.num].u.data = fec,										cmdseq.num++;
		p[cmdseq.num].cmd = DTV_INVERSION,			p[cmdseq.num].u.data = feparams->inversion,						cmdseq.num++;
		if (getFEDeliverySystem(feparams->delsys) == SYS_DVBS2)
		{
			p[cmdseq.num].cmd = DTV_ROLLOFF,		p[cmdseq.num].u.data = feparams->rolloff,						cmdseq.num++;
			p[cmdseq.num].cmd = DTV_PILOT,			p[cmdseq.num].u.data = pilot,									cmdseq.num++;
			if (fe_can_multistream)
			{
#if (DVB_API_VERSION >= 5 && DVB_API_VERSION_MINOR >= 11)
				p[cmdseq.num].cmd = DTV_STREAM_ID, p[cmdseq.num].u.data = feparams->plp_id,							cmdseq.num++;
				p[cmdseq.num].cmd = DTV_SCRAMBLING_SEQUENCE_INDEX, p[cmdseq.num].u.data = feparams->pls_code,		cmdseq.num++;
#else
				p[cmdseq.num].cmd = DTV_STREAM_ID, p[cmdseq.num].u.data = feparams->plp_id | (feparams->pls_code << 8) | (feparams->pls_mode << 26), cmdseq.num++;
#endif
			}
			p[cmdseq.num].cmd = DTV_ISDBT_SB_SEGMENT_IDX, p[cmdseq.num].u.data = (feparams->plp_id == NO_STREAM_ID_FILTER ? 0 : (0x80000000 | (T2MI_Default_Pid << 16) | feparams->plp_id)), cmdseq.num++;
		}
		if (fe_can_multistream)
			INFO("[fe%d/%d] tuner pilot %d (feparams %d) streamid (%d/%d/%d)\n", adapter, fenumber, pilot, feparams->pilot, feparams->plp_id, feparams->pls_code, feparams->pls_mode );
		else
			INFO("[fe%d/%d] tuner pilot %d (feparams %d)\n", adapter, fenumber, pilot, feparams->pilot);
		break;
	case DVB_C:
		p[cmdseq.num].cmd = DTV_DELIVERY_SYSTEM,	p[cmdseq.num].u.data = getFEDeliverySystem(feparams->delsys),	cmdseq.num++;
		p[cmdseq.num].cmd = DTV_FREQUENCY,			p[cmdseq.num].u.data = feparams->frequency,						cmdseq.num++;
		p[cmdseq.num].cmd = DTV_MODULATION,			p[cmdseq.num].u.data = feparams->modulation,					cmdseq.num++;
		p[cmdseq.num].cmd = DTV_SYMBOL_RATE,		p[cmdseq.num].u.data = feparams->symbol_rate,					cmdseq.num++;
		p[cmdseq.num].cmd = DTV_INNER_FEC,			p[cmdseq.num].u.data = feparams->fec_inner,						cmdseq.num++;
		p[cmdseq.num].cmd = DTV_INVERSION,			p[cmdseq.num].u.data = feparams->inversion,						cmdseq.num++;
		break;
	case DVB_T:
	case DVB_T2:
	case DTMB:
		p[cmdseq.num].cmd = DTV_DELIVERY_SYSTEM,	p[cmdseq.num].u.data = getFEDeliverySystem(feparams->delsys),	cmdseq.num++;
		p[cmdseq.num].cmd = DTV_FREQUENCY,			p[cmdseq.num].u.data = feparams->frequency,						cmdseq.num++;
		p[cmdseq.num].cmd = DTV_CODE_RATE_LP,		p[cmdseq.num].u.data = feparams->code_rate_LP,					cmdseq.num++;
		p[cmdseq.num].cmd = DTV_CODE_RATE_HP,		p[cmdseq.num].u.data = feparams->code_rate_HP,					cmdseq.num++;
		p[cmdseq.num].cmd = DTV_MODULATION,			p[cmdseq.num].u.data = feparams->modulation,					cmdseq.num++;
		p[cmdseq.num].cmd = DTV_TRANSMISSION_MODE,	p[cmdseq.num].u.data = feparams->transmission_mode,				cmdseq.num++;
		p[cmdseq.num].cmd = DTV_GUARD_INTERVAL,		p[cmdseq.num].u.data = feparams->guard_interval,				cmdseq.num++;
		p[cmdseq.num].cmd = DTV_HIERARCHY,			p[cmdseq.num].u.data = feparams->hierarchy,						cmdseq.num++;
		p[cmdseq.num].cmd = DTV_BANDWIDTH_HZ,		p[cmdseq.num].u.data = getFEBandwidth(feparams->bandwidth),		cmdseq.num++;
		p[cmdseq.num].cmd = DTV_INVERSION,			p[cmdseq.num].u.data = feparams->inversion,						cmdseq.num++;
		if (getFEDeliverySystem(feparams->delsys) == SYS_DVBT2)
		{
#if defined DTV_STREAM_ID
			p[cmdseq.num].cmd = DTV_STREAM_ID,		p[cmdseq.num].u.data = feparams->plp_id,						cmdseq.num++;
#elif defined DTV_DVBT2_PLP_ID
			p[cmdseq.num].cmd = DTV_DVBT2_PLP_ID,	p[cmdseq.num].u.data = feparams->plp_id,						cmdseq.num++;
#endif
		}
		break;
	default:
		INFO("[fe%d/%d] unknown frontend type, exiting\n", adapter, fenumber);
		return 0;
	}
	p[cmdseq.num].cmd = DTV_TUNE, cmdseq.num++;

	if ((ioctl(fd, FE_SET_PROPERTY, &cmdseq)) < 0) {
		ERROR("FE_SET_PROPERTY");
		return 0;
	}

	if (nowait)
		return 0;
	{
		FE_TIMER_INIT();
		FE_TIMER_START();

		struct dvb_frontend_event event;
		event = getEvent();

		if(tuned) {
			FE_TIMER_STOP("tuning took");
		}
	}

	return tuned;
}

void CFrontend::secSetTone(const fe_sec_tone_mode_t toneMode, const uint32_t ms)
{
	if (slave || info.type != FE_QPSK)
		return;

	if (currentToneMode == toneMode)
		return;

	if (config.diseqcType == DISEQC_UNICABLE || config.diseqcType == DISEQC_UNICABLE2) {
		/* this is too ugly for words. the "currentToneMode" is the only place
		   where the global "highband" state is saved. So we need to fake it for
		   unicable and still set the tone on... */
		currentToneMode = toneMode; /* need to know polarization for unicable */
		fop(ioctl, FE_SET_TONE, SEC_TONE_OFF); /* tone must be off except for the time
							  of sending commands */
		return;
	}

	if (zapit_debug) printf("[fe%d/%d] tone %s\n", adapter, fenumber, toneMode == SEC_TONE_ON ? "on" : "off");
	//FE_TIMER_INIT();
	//FE_TIMER_START();
	if (fop(ioctl, FE_SET_TONE, toneMode) == 0) {
		currentToneMode = toneMode;
		//FE_TIMER_STOP("FE_SET_TONE took");
		usleep(1000 * ms);
	}
}

void CFrontend::secSetVoltage(const fe_sec_voltage_t voltage, const uint32_t ms)
{
	if (slave || ((info.type != FE_QPSK) && (info.type != FE_OFDM)))
		return;

	if (currentVoltage == voltage)
		return;

	if (zapit_debug) printf("[fe%d/%d] voltage %s\n", adapter, fenumber, voltage == SEC_VOLTAGE_OFF ? "OFF" : voltage == SEC_VOLTAGE_13 ? "13" : "18");
	if ((config.diseqcType == DISEQC_UNICABLE || config.diseqcType == DISEQC_UNICABLE2) && voltage != SEC_VOLTAGE_OFF) {
		/* see my comment in secSetTone... */
		currentVoltage = voltage; /* need to know polarization for unicable */
		fop(ioctl, FE_SET_VOLTAGE, unicable_lowvolt); /* voltage must not be 18V */
		return;
	}

	if (fop(ioctl, FE_SET_VOLTAGE, voltage) == 0) {
		currentVoltage = voltage;
		usleep(1000 * ms);
	}
}

void CFrontend::sendDiseqcCommand(const struct dvb_diseqc_master_cmd *cmd, const uint32_t ms)
{
	if (slave || info.type != FE_QPSK)
		return;

	printf("[fe%d/%d] Diseqc cmd: ", adapter, fenumber);
	for (int i = 0; i < cmd->msg_len; i++)
		printf("0x%X ", cmd->msg[i]);
	printf("\n");

	if (fop(ioctl, FE_DISEQC_SEND_MASTER_CMD, cmd) == 0)
		usleep(1000 * ms);
}

void CFrontend::sendToneBurst(const fe_sec_mini_cmd_t burst, const uint32_t ms)
{
	if (slave || info.type != FE_QPSK)
		return;
	if (fop(ioctl, FE_DISEQC_SEND_BURST, burst) == 0)
		usleep(1000 * ms);
}

void CFrontend::setDiseqcType(const diseqc_t newDiseqcType, bool force)
{
	switch (newDiseqcType) {
	case NO_DISEQC:
		INFO("fe%d: NO_DISEQC", fenumber);
		break;
	case MINI_DISEQC:
		INFO("fe%d: MINI_DISEQC", fenumber);
		break;
	case SMATV_REMOTE_TUNING:
		INFO("fe%d: SMATV_REMOTE_TUNING", fenumber);
		break;
	case DISEQC_1_0:
		INFO("fe%d: DISEQC_1_0", fenumber);
		break;
	case DISEQC_1_1:
		INFO("fe%d: DISEQC_1_1", fenumber);
		break;
	case DISEQC_1_2:
		INFO("fe%d: DISEQC_1_2", fenumber);
		break;
	case DISEQC_ADVANCED:
		INFO("fe%d: DISEQC_ADVANCED", fenumber);
		break;
	case DISEQC_UNICABLE:
		INFO("fe%d: DISEQC_UNICABLE", fenumber);
		break;
	case DISEQC_UNICABLE2:
		INFO("fe%d: DISEQC_UNICABLE2", fenumber);
		break;
#if 0
	case DISEQC_2_0:
		INFO("DISEQC_2_0");
		break;
	case DISEQC_2_1:
		INFO("DISEQC_2_1");
		break;
	case DISEQC_2_2:
		INFO("DISEQC_2_2");
		break;
#endif
	default:
		WARN("Invalid DiSEqC type");
		return;
	}

	if (newDiseqcType == DISEQC_UNICABLE || newDiseqcType == DISEQC_UNICABLE2) {
		secSetTone(SEC_TONE_OFF, 0);
		secSetVoltage(unicable_lowvolt, 0);
	}
	else if ((force && (newDiseqcType != NO_DISEQC)) ||
		 ((config.diseqcType <= MINI_DISEQC) && (newDiseqcType > MINI_DISEQC))) {
		secSetTone(SEC_TONE_OFF, 15);
		sendDiseqcReset();
		sendDiseqcPowerOn();
		secSetTone(SEC_TONE_ON, 50);
	}

	config.diseqcType = newDiseqcType;
}

void CFrontend::setLnbOffsets(int32_t _lnbOffsetLow, int32_t _lnbOffsetHigh, int32_t _lnbSwitch)
{
	lnbOffsetLow = _lnbOffsetLow * 1000;
	lnbOffsetHigh = _lnbOffsetHigh * 1000;
	lnbSwitch = _lnbSwitch * 1000;
	//printf("[fe%d/%d] setLnbOffsets %d/%d/%d\n", adapter, fenumber, lnbOffsetLow, lnbOffsetHigh, lnbSwitch);
}

void CFrontend::sendMotorCommand(uint8_t cmdtype, uint8_t address, uint8_t command, uint8_t num_parameters, uint8_t parameter1, uint8_t parameter2, int repeat)
{
	struct dvb_diseqc_master_cmd cmd;
	int i;
	fe_sec_tone_mode_t oldTone = currentToneMode;

	printf("[fe%d/%d] sendMotorCommand: cmdtype   = %x, address = %x, cmd   = %x\n", adapter, fenumber, cmdtype, address, command);
	printf("[fe%d/%d] sendMotorCommand: num_parms = %d, parm1   = %x, parm2 = %x\n", adapter, fenumber, num_parameters, parameter1, parameter2);

	cmd.msg[0] = cmdtype;	//command type
	cmd.msg[1] = address;	//address
	cmd.msg[2] = command;	//command
	cmd.msg[3] = parameter1;
	cmd.msg[4] = parameter2;
	cmd.msg_len = 3 + num_parameters;

	secSetTone(SEC_TONE_OFF, 15);
#if 0
	fe_sec_voltage_t oldVoltage = currentVoltage;
	//secSetVoltage(config.highVoltage ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13, 15);
	//secSetVoltage(SEC_VOLTAGE_13, 100);
#endif

	for(i = 0; i <= repeat; i++)
		sendDiseqcCommand(&cmd, 50);

	//secSetVoltage(oldVoltage, 15);
	secSetTone(oldTone, 15);
	printf("[fe%d/%d] motor command sent.\n", adapter, fenumber);

}

void CFrontend::positionMotor(uint8_t motorPosition)
{
	struct dvb_diseqc_master_cmd cmd = {
		{0xE0, 0x31, 0x6B, 0x00, 0x00, 0x00}, 4
	};

	if (motorPosition != 0) {
		secSetTone(SEC_TONE_OFF, 25);
		secSetVoltage(config.highVoltage ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13, 15);
		cmd.msg[3] = motorPosition;

		for (int i = 0; i <= repeatUsals; ++i)
			sendDiseqcCommand(&cmd, 50);

		printf("[fe%d/%d] motor positioning command sent.\n", adapter, fenumber);
	}
}

bool CFrontend::setInput(CZapitChannel * channel, bool nvod)
{
	transponder_list_t::iterator tpI;
	transponder_id_t ct = nvod ? (channel->getTransponderId() & 0xFFFFFFFFULL) : channel->getTransponderId();
	transponder_id_t current_id = nvod ? (currentTransponder.getTransponderId() & 0xFFFFFFFFULL) : currentTransponder.getTransponderId();
	//printf("CFrontend::setInput tuned %d nvod %d current_id %llx new %llx\n\n", tuned, nvod, current_id, ct);

	if (tuned && (ct == current_id))
		return false;

	if (nvod) {
		for (tpI = transponders.begin(); tpI != transponders.end(); ++tpI) {
			if ((ct & 0xFFFFFFFFULL) == (tpI->first & 0xFFFFFFFFULL))
				break;
		}
	} else
		tpI = transponders.find(channel->getTransponderId());

	if (tpI == transponders.end()) {
		printf("Transponder %" PRIx64 " for channel %" PRIx64 " not found\n", ct, channel->getChannelID());
		return false;
	}

	currentTransponder.setTransponderId(tpI->first);

	currentSatellitePosition = channel->getSatellitePosition();
	setInput(channel->getSatellitePosition(), tpI->second.getFrequency(), tpI->second.getPolarization());
	return true;
}

void CFrontend::setInput(t_satellite_position satellitePosition, uint32_t frequency, uint8_t polarization)
{
	sat_iterator_t sit = satellites.find(satellitePosition);

	if (sit == satellites.end()){
		return;
	}
	/* unicable uses diseqc parameter for input selection */
	config.uni_lnb = sit->second.diseqc;

	setLnbOffsets(sit->second.lnbOffsetLow, sit->second.lnbOffsetHigh, sit->second.lnbSwitch);
	if (config.diseqcType == DISEQC_UNICABLE || config.diseqcType == DISEQC_UNICABLE2)
		return;

	if ((config.diseqcType != DISEQC_1_1) && (config.diseqcType != DISEQC_ADVANCED)) {
		setDiseqc(sit->second.diseqc, polarization, frequency);
		return;
	}
	if (config.diseqc_order /*sit->second.diseqc_order*/ == COMMITED_FIRST) {
		if (setDiseqcSimple(sit->second.commited, polarization, frequency))
			uncommitedInput = 255;
		sendUncommittedSwitchesCommand(sit->second.uncommited);
	} else {
		if (sendUncommittedSwitchesCommand(sit->second.uncommited))
			currentDiseqc = -1;
		setDiseqcSimple(sit->second.commited, polarization, frequency);
	}
}

/* frequency is the IF-frequency (950-2100), what a stupid spec...
   high_band, horizontal, bank are actually bool (0/1)
   bank specifies the "switch bank" (as in Mini-DiSEqC A/B)
   bank == 2 => send standby command */
uint32_t CFrontend::sendEN50494TuningCommand(const uint32_t frequency, const int high_band,
					     const int horizontal, const int bank)
{
	uint32_t bpf = config.uni_freq;
	int pin = config.uni_pin;
	if (config.uni_scr < 0 || config.uni_scr > 7) {
		WARN("uni_scr out of range (%d)", config.uni_scr);
		return 0;
	}

	struct dvb_diseqc_master_cmd cmd = {
		{0xe0, 0x10, 0x5a, 0x00, 0x00, 0x00}, 5
	};
	unsigned int t = (frequency / 1000 + bpf + 2) / 4 - 350;
	if (bank < 2 && t >= 1024)
	{
		WARN("ooops. t > 1024? (%d)", t);
		return 0;
	}
	if (pin > 0 && pin < 0x100) {
		cmd.msg[2] = 0x5c;
		cmd.msg[5] = config.uni_pin;
		cmd.msg_len = 6;
	}
	uint32_t ret = (t + 350) * 4000 - frequency;
	INFO("[fe%d/%d] 18V=%d 22k=%d freq=%d uni_freq=%d uni_scr=%d bank=%d pin=%d ret=%d",
		adapter, fenumber, horizontal, high_band, frequency, bpf, config.uni_scr, bank, pin, ret);
	if (!slave && info.type == FE_QPSK) {
		cmd.msg[3] = (config.uni_scr << 5);		/* adress */
		if (bank < 2) { /* bank = 0/1 => tune, bank = 2 => standby */
			cmd.msg[3] |= (t >> 8)		|	/* highest 2 bits of t */
				(bank << 4)		|	/* input 0/1 */
				(horizontal << 3)	|	/* horizontal == 0x08 */
				(high_band) << 2;		/* high_band  == 0x04 */
			cmd.msg[4] = t & 0xFF;
		}
		fop(ioctl, FE_SET_VOLTAGE, SEC_VOLTAGE_18);
		usleep(20 * 1000);		/* en50494 says: >4ms and < 22 ms */
		sendDiseqcCommand(&cmd, 120);	/* en50494 says: >2ms and < 60 ms -- it seems we must add the lengthe of telegramm itself (~65ms)*/
		fop(ioctl, FE_SET_VOLTAGE, unicable_lowvolt);
	}
	return ret;
}

uint32_t CFrontend::sendEN50607TuningCommand(const uint32_t frequency, const int high_band, const int horizontal, const int bank)
{
	uint32_t bpf = config.uni_freq;
	int pin = config.uni_pin;
	struct dvb_diseqc_master_cmd cmd = { {0x70, 0x00, 0x00, 0x00, 0x00, 0x00}, 4 };

	if (pin > 0 && pin < 0x100) {
		cmd.msg[0] = 0x71;
		cmd.msg[4] = config.uni_pin;
		cmd.msg_len = 5;
	}

	unsigned int t = frequency / 1000 - 100;
	if (t < 0x800 && config.uni_scr >= 0 && config.uni_scr < 32)
	{
		uint32_t ret = bpf * 1000;
		INFO("[unicable-JESS] 18V=%d TONE=%d, freq=%d uni_freq=%d uni_scr=%d bank=%d pin=%d ret=%d", currentVoltage == SEC_VOLTAGE_18, currentToneMode == SEC_TONE_ON, frequency, bpf, config.uni_scr, bank, pin, ret);
		if (!slave && info.type == FE_QPSK)
		{
			cmd.msg[1] = ((config.uni_scr & 0x1F) << 3)	|	/* user band adress ( 0 to 31) */
			/* max. possible tuning word = 0x7FF */
				((t >> 8) & 0x07);				/* highest 3 bits of t (MSB) */
			cmd.msg[2] = t & 0xFF;					/* tuning word (LSB) */
			cmd.msg[3] = (0 << 4)				|	/* no commited switch */
			/* I really don't know if the combines of option and position bits are right here,
			because I can'test it, assuming here 4 sat positions */
				((bank & 0x03) << 2)			|	/* input 0/1/2/3 */
				(horizontal << 1)			|	/* horizontal == 0x02 */
				high_band;					/* high_band  == 0x01 */
			fop(ioctl, FE_SET_VOLTAGE, SEC_VOLTAGE_18);
			usleep(20 * 1000);					/* en50494 says: >4ms and < 22 ms */
			sendDiseqcCommand(&cmd, 120);				/* en50494 says: >2ms and < 60 ms -- it seems we must add the lengthe of telegramm itself (~65ms)*/
			fop(ioctl, FE_SET_VOLTAGE, unicable_lowvolt);
		}
		return ret;
	}
	WARN("ooops. t > 2047? (%d) or uni_scr out of range? (%d)", t, config.uni_scr);
	return 0;
}

bool CFrontend::tuneChannel(CZapitChannel * /*channel*/, bool /*nvod*/)
{
	transponder_list_t::iterator transponder = transponders.find(currentTransponder.getTransponderId());
	if (transponder == transponders.end())
		return false;
	return tuneFrequency(&transponder->second.feparams, false);
}

#if 0
bool CFrontend::retuneChannel(void)
{
	mutex.lock();
	setInput(currentSatellitePosition, currentTransponder.feparams.frequency, currentTransponder.feparams.polarization);
	transponder_list_t::iterator transponder = transponders.find(currentTransponder.TP_id);
	if (transponder == transponders.end())
		return false;
	mutex.unlock();
	return tuneFrequency(&transponder->second.feparams, transponder->second.feparams.polarization, true);
}
#endif

int CFrontend::tuneFrequency(FrontendParameters *feparams, bool nowait)
{
	transponder TP;

	currentTransponder.feparams = TP.feparams = *feparams;

	return setParameters(&TP, nowait);
}

int CFrontend::setParameters(transponder *TP, bool nowait)
{
	int freq_offset = 0, freq;
	FrontendParameters feparams;

	/* Copy the data for local use */
	feparams = TP->feparams;

	if (!supportsDelivery(feparams.delsys)) {
		printf("[fe%d/%d]: does not support delivery system %d\n", adapter, fenumber, feparams.delsys);
		return 0;
	}


	freq		= (int) feparams.frequency;
	const char *f, *s, *m;
	bool high_band;

	switch (feparams.delsys) {
	case DVB_S:
	case DVB_S2:
	case DVB_S2X:
		if (freq < lnbSwitch) {
			high_band = false;
			freq_offset = lnbOffsetLow;
		} else {
			high_band = true;
			freq_offset = lnbOffsetHigh;
		}

		feparams.frequency = abs(freq - freq_offset);
		setSec(TP->feparams.polarization, high_band);
		getDelSys(feparams.delsys, feparams.fec_inner, feparams.modulation,  f, s, m);
		break;
	case DVB_C:
		if (freq < 1000*1000)
			feparams.frequency = freq * 1000;
		getDelSys(feparams.delsys, feparams.fec_inner, feparams.modulation,  f, s, m);
		secSetVoltage(SEC_VOLTAGE_OFF, 100);
		break;
	case DVB_T:
	case DVB_T2:
	case DTMB:
		if (freq < 1000*1000)
			feparams.frequency = freq * 1000;
		getDelSys(feparams.delsys, feparams.fec_inner, feparams.modulation,  f, s, m);
		secSetVoltage(config.powered ? SEC_VOLTAGE_13 : SEC_VOLTAGE_OFF, 100);
		break;
	default:
		printf("[fe%d/%d] unknown delsys %d\n", adapter, fenumber, feparams.delsys);
		break;
	}

	printf("[fe%d/%d] tune to %d %s %s %s %s srate %d pli %d plc %d plm %d (tuner %d offset %d timeout %d)\n", adapter, fenumber, freq, s, m, f,
			feparams.polarization & 1 ? "V/R" : "H/L", feparams.symbol_rate, feparams.plp_id, feparams.pls_code, feparams.pls_mode, feparams.frequency, freq_offset, TIMEOUT_MAX_MS);
	setFrontend(&feparams, nowait);

	return tuned;
}

bool CFrontend::sendUncommittedSwitchesCommand(int input)
{
	struct dvb_diseqc_master_cmd cmd = {
		{0xe0, 0x10, 0x39, 0x00, 0x00, 0x00}, 4
	};

	printf("[fe%d/%d] uncommitted  %d -> %d\n", adapter, fenumber, uncommitedInput, input);
	if ((input < 0) /* || (uncommitedInput == input) */)
		return false;

	uncommitedInput = input;
	cmd.msg[3] = 0xF0 + input;

	secSetTone(SEC_TONE_OFF, 20);
	sendDiseqcCommand(&cmd, 125);
	return true;
}

/* off = low band, on - hi band , vertical 13v, horizontal 18v */
bool CFrontend::setDiseqcSimple(int sat_no, const uint8_t pol, const uint32_t frequency)
{
	fe_sec_voltage_t v = (pol & 1) ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18;
	//fe_sec_mini_cmd_t b = (sat_no & 1) ? SEC_MINI_B : SEC_MINI_A;
	bool high_band = ((int)frequency >= lnbSwitch);

	struct dvb_diseqc_master_cmd cmd = {
		{0xe0, 0x10, 0x38, 0x00, 0x00, 0x00}, 4
	};

	INFO("[fe%d/%d] diseqc input  %d -> %d", adapter, fenumber, currentDiseqc, sat_no);
	currentDiseqc = sat_no;
	if (slave)
		return true;
	if ((sat_no >= 0) /* && (diseqc != sat_no)*/) {
		cmd.msg[3] = 0xf0 | (((sat_no * 4) & 0x0f) | (high_band ? 1 : 0) | ((pol & 1) ? 0 : 2));

		secSetTone(SEC_TONE_OFF, 20);
		secSetVoltage(v, 100);
		sendDiseqcCommand(&cmd, 100);
		return true;
	}
	return false;
#if 0				//do we need this in advanced setup ?
	if (config.diseqcType == SMATV_REMOTE_TUNING)
		sendDiseqcSmatvRemoteTuningCommand(frequency);

	if (config.diseqcType == MINI_DISEQC)
		sendToneBurst(b, 15);
	currentDiseqc = sat_no;
#endif
}

void CFrontend::setDiseqc(int sat_no, const uint8_t pol, const uint32_t frequency)
{
	uint8_t loop;
	bool high_band = ((int)frequency >= lnbSwitch);
	struct dvb_diseqc_master_cmd cmd = { {0xE0, 0x10, 0x38, 0xF0, 0x00, 0x00}, 4 };
	fe_sec_mini_cmd_t b = (sat_no & 1) ? SEC_MINI_B : SEC_MINI_A;
	int delay = 0;

	if ((config.diseqcType == NO_DISEQC) || sat_no < 0)
		return;

	if (zapit_debug) printf("[fe%d/%d] diseqc input  %d -> %d\n", adapter, fenumber, currentDiseqc, sat_no);
	currentDiseqc = sat_no;
	if (slave)
		return;

	// Disable tone for at least 15ms
	secSetTone(SEC_TONE_OFF, 20);
	// Set wanted voltage and wait at least 100 ms, in case voltage was off.
	secSetVoltage((pol & 1) ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18, 100);

#if 0 /* FIXME: is this necessary ? */
	sendDiseqcReset();
	sendDiseqcPowerOn();
#endif

	for (loop = 0; loop <= config.diseqcRepeats; loop++) {
		delay = 0;

		if (config.diseqcType == DISEQC_1_1) {	/* setup the uncommited switch first */

			delay = 100;	// delay for 1.0 after 1.1 command
			cmd.msg[2] = 0x39;	/* port group = uncommited switches */
#if 0
			/* new code */
			sat_no &= 0x0F;
			cmd.msg[3] = 0xF0 | sat_no;
			sendDiseqcCommand(&cmd, delay);
			cmd.msg[2] = 0x38;	/* port group = commited switches */
			cmd.msg[3] = 0xF0 | ((pol & 1) ? 0 : 2) | (high_band ? 1 : 0);
			sendDiseqcCommand(&cmd, delay);
#endif
#if 1
			/* old code */
			/* for 16 inputs */
			cmd.msg[3] = 0xF0 | ((sat_no / 4) & 0x03);
			//send the command to setup second uncommited switch and
			// wait 100 ms.
			sendDiseqcCommand(&cmd, 100);
#else
			/* for 64 inputs */
			uint8_t cascade_input[16] = {0xF0, 0xF4, 0xF8, 0xFC, 0xF1, 0xF5, 0xF9, 0xFD, 0xF2, 0xF6, 0xFA,
				0xFE, 0xF3, 0xF7, 0xFB, 0xFF
			};
			cmd.msg[3] = cascade_input[(sat_no / 4) & 0xFF];
			sendDiseqcCommand(&cmd, 100);	/* send the command to setup first uncommited switch and wait 100 ms !!! */
			cmd.msg[3] &= 0xCF;
			sendDiseqcCommand(&cmd, 100);	/* send the command to setup second uncommited switch and wait 100 ms !!! */
#endif
		}

		if (config.diseqcType >= DISEQC_1_0) {	/* DISEQC 1.0 or 1.1 */
			usleep(delay * 1000);
			//cmd.msg[0] |= 0x01;	/* repeated transmission */
			cmd.msg[2] = 0x38;	/* port group = commited switches */
			cmd.msg[3] = 0xF0 | ((sat_no & 0x0F) << 2) | ((pol & 1) ? 0 : 2) | (high_band ? 1 : 0);
			sendDiseqcCommand(&cmd, 100);	/* send the command to setup second commited switch */
		}
		cmd.msg[0] = 0xE1;
		usleep(50 * 1000);	/* sleep at least 50 milli seconds */
	}

	// Toneburst should not be repeated and should be sent AFTER
	// all diseqc, this means we don't support a toneburst switch 
	// before any diseqc equipment.
	if (config.diseqcType == MINI_DISEQC)
		sendToneBurst(b, 1);

	usleep(25 * 1000);

	if (config.diseqcType == SMATV_REMOTE_TUNING)
		sendDiseqcSmatvRemoteTuningCommand(frequency);

	usleep(25 * 1000);
}

void CFrontend::setSec(const uint8_t pol, const bool high_band)
{
	fe_sec_voltage_t v = (pol & 1) ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18;
	fe_sec_tone_mode_t t = high_band ? SEC_TONE_ON : SEC_TONE_OFF;

	// set tone off first
	secSetTone(SEC_TONE_OFF, 20);
	// set the desired voltage
	secSetVoltage(v, 100);
	// set tone according to what's needed.
	secSetTone(t, 15);
}

void CFrontend::sendDiseqcPowerOn(uint32_t ms)
{
	printf("[fe%d/%d] diseqc power on\n", adapter, fenumber);
	// Send power on to 'all' equipment
	sendDiseqcZeroByteCommand(0xe0, 0x00, 0x03, ms);
}

void CFrontend::sendDiseqcReset(uint32_t ms)
{
	printf("[fe%d/%d] diseqc reset\n", adapter, fenumber);
	// Send reset to 'all' equipment
	sendDiseqcZeroByteCommand(0xe0, 0x00, 0x00, ms);
}

void CFrontend::sendDiseqcStandby(uint32_t ms)
{
	printf("[fe%d/%d] diseqc standby\n", adapter, fenumber);
	if (config.diseqcType > DISEQC_ADVANCED)
	{
		/* use ODU_Power_OFF command for unicable or jess here
		to set the used UB frequency of the frontend to standby */
		struct dvb_diseqc_master_cmd cmd = {{0}, 6};
		printf("[fe%d/%d] standby scr: %d\n", adapter, fenumber, config.uni_scr);
		if (config.diseqcType == DISEQC_UNICABLE)
		{
			cmd.msg[0] = 0xe0;
			cmd.msg[1] = 0x10;
			cmd.msg[2] = 0x5a;
			cmd.msg[3] = ((config.uni_scr & 0x07) << 5);
			cmd.msg_len = 5;
		}
		if (config.diseqcType == DISEQC_UNICABLE2)
		{
			cmd.msg[0] = 0x70;
			cmd.msg[1] = ((config.uni_scr & 0x1F) << 3);
			cmd.msg_len = 4;
		}
		fop(ioctl, FE_SET_VOLTAGE, SEC_VOLTAGE_18);
		usleep(15 * 1000);
		sendDiseqcCommand(&cmd, ms);
		fop(ioctl, FE_SET_VOLTAGE, SEC_VOLTAGE_13);
		return;
	}
	/* en50494 switches don't seem to be hurt by this */
	// Send power off to 'all' equipment
	sendDiseqcZeroByteCommand(0xe0, 0x00, 0x02, ms);
}

void CFrontend::sendDiseqcZeroByteCommand(uint8_t framing_byte, uint8_t address, uint8_t command, uint32_t ms)
{
	struct dvb_diseqc_master_cmd diseqc_cmd = {
		{framing_byte, address, command, 0x00, 0x00, 0x00}, 3
	};

	sendDiseqcCommand(&diseqc_cmd, ms);
}

void CFrontend::sendDiseqcSmatvRemoteTuningCommand(const uint32_t frequency)
{
	/* [0] from master, no reply, 1st transmission
	 * [1] intelligent slave interface for multi-master bus
	 * [2] write channel frequency
	 * [3] frequency
	 * [4] frequency
	 * [5] frequency */

	struct dvb_diseqc_master_cmd cmd = {
		{0xe0, 0x71, 0x58, 0x00, 0x00, 0x00}, 6
	};

	cmd.msg[3] = (((frequency / 10000000) << 4) & 0xF0) | ((frequency / 1000000) & 0x0F);
	cmd.msg[4] = (((frequency / 100000) << 4) & 0xF0) | ((frequency / 10000) & 0x0F);
	cmd.msg[5] = (((frequency / 1000) << 4) & 0xF0) | ((frequency / 100) & 0x0F);

	sendDiseqcCommand(&cmd, 15);
}

int CFrontend::driveToSatellitePosition(t_satellite_position satellitePosition, bool from_scan)
{
	int waitForMotor = 0;
	int new_position = 0, old_position = 0;
	bool use_usals = 0;

	if (CFrontend::linked(femode) || femode == CFrontend::FE_MODE_UNUSED) {
		rotorSatellitePosition = satellitePosition;
		return 0;
	}
	//if(config.diseqcType == DISEQC_ADVANCED) //FIXME testing
	{
		bool moved = false;

		sat_iterator_t sit = satellites.find(satellitePosition);
		if (sit == satellites.end()) {
			printf("[fe%d/%d] satellite position %d not found!\n", adapter, fenumber, satellitePosition);
			return 0;
		} else {
			new_position = sit->second.motor_position;
			use_usals = sit->second.use_usals;
		}
		sit = satellites.find(rotorSatellitePosition);
		if (sit != satellites.end())
			old_position = sit->second.motor_position;

		if (zapit_debug) printf("[fe%d/%d] sat pos %d -> %d motor pos %d -> %d usals %s\n", adapter, fenumber, rotorSatellitePosition, satellitePosition, old_position, new_position, use_usals ? "on" : "off");

		if (rotorSatellitePosition == satellitePosition)
			return 0;

		if (use_usals || config.use_usals) {
			gotoXX(satellitePosition);
			moved = true;
		} else {
			if (new_position > 0) {
				positionMotor(new_position);
				moved = true;
			}
		}

		if (from_scan || (new_position > 0 && old_position > 0)) {
			waitForMotor = config.motorRotationSpeed ? 2 + abs(satellitePosition - rotorSatellitePosition) / config.motorRotationSpeed : 0;
		}
		if (moved) {
			waitForMotor = config.motorRotationSpeed ? 2 + abs(satellitePosition - rotorSatellitePosition) / config.motorRotationSpeed : 0;
			rotorSatellitePosition = satellitePosition;
		}
	}

	return waitForMotor;
}

static const int gotoXTable[10] = { 0x00, 0x02, 0x03, 0x05, 0x06, 0x08, 0x0A, 0x0B, 0x0D, 0x0E };

/*----------------------------------------------------------------------------*/
double factorial_div(double value, int x)
{
	if (!x)
		return 1;
	else {
		while (x > 1) {
			value = value / x--;
		}
	}
	return value;
}

/*----------------------------------------------------------------------------*/
double powerd(double x, int y)
{
	int i = 0;
	double ans = 1.0;

	if (!y)
		return 1.000;
	else {
		while (i < y) {
			i++;
			ans = ans * x;
		}
	}
	return ans;
}

/*----------------------------------------------------------------------------*/
double SIN(double x)
{
	int i = 0;
	int j = 1;
	int sign = 1;
	double y1 = 0.0;
	double diff = 1000.0;

	if (x < 0.0) {
		x = -1 * x;
		sign = -1;
	}

	while (x > 360.0 * M_PI / 180) {
		x = x - 360 * M_PI / 180;
	}

	if (x > (270.0 * M_PI / 180)) {
		sign = sign * -1;
		x = 360.0 * M_PI / 180 - x;
	} else if (x > (180.0 * M_PI / 180)) {
		sign = sign * -1;
		x = x - 180.0 * M_PI / 180;
	} else if (x > (90.0 * M_PI / 180)) {
		x = 180.0 * M_PI / 180 - x;
	}

	while (powerd(diff, 2) > 1.0E-16) {
		i++;
		diff = j * factorial_div(powerd(x, (2 * i - 1)), (2 * i - 1));
		y1 = y1 + diff;
		j = -1 * j;
	}
	return (sign * y1);
}

double COS(double x)
{
	return SIN(90 * M_PI / 180 - x);
}

double ATAN(double x)
{
	int i = 0;		/* counter for terms in binomial series */
	int j = 1;		/* sign of nth term in series */
	int k = 0;
	int sign = 1;		/* sign of the input x */
	double y = 0.0;		/* the output */
	double deltay = 1.0;	/* the value of the next term in the series */
	double addangle = 0.0;	/* used if arctan > 22.5 degrees */

	if (x < 0.0) {
		x = -1 * x;
		sign = -1;
	}

	while (x > 0.3249196962) {
		k++;
		x = (x - 0.3249196962) / (1 + x * 0.3249196962);
	}
	addangle = k * 18.0 * M_PI / 180;

	while (powerd(deltay, 2) > 1.0E-16) {
		i++;
		deltay = j * powerd(x, (2 * i - 1)) / (2 * i - 1);
		y = y + deltay;
		j = -1 * j;
	}
	return (sign * (y + addangle));
}

double ASIN(double x)
{
	return 2 * ATAN(x / (1 + std::sqrt(1.0 - x * x)));
}

double Radians(double number)
{
	return number * M_PI / 180;
}

double Deg(double number)
{
	return number * 180 / M_PI;
}

double Rev(double number)
{
	return number - std::floor(number / 360.0) * 360;
}

double calcElevation(double SatLon, double SiteLat, double SiteLon, int Height_over_ocean = 0)
{
	const double a0 = 0.58804392, a1 = -0.17941557, a2 = 0.29906946E-1, a3 = -0.25187400E-2, a4 = 0.82622101E-4;
	const double f = 1.00 / 298.257;	// Earth flattning factor
	const double r_sat = 42164.57;	// Distance from earth centre to satellite
	const double r_eq = 6378.14;	// Earth radius
	double sinRadSiteLat = SIN(Radians(SiteLat)), cosRadSiteLat = COS(Radians(SiteLat));
	double Rstation = r_eq / (std::sqrt(1.00 - f * (2.00 - f) * sinRadSiteLat * sinRadSiteLat));
	double Ra = (Rstation + Height_over_ocean) * cosRadSiteLat;
	double Rz = Rstation * (1.00 - f) * (1.00 - f) * sinRadSiteLat;
	double alfa_rx = r_sat * COS(Radians(SatLon - SiteLon)) - Ra;
	double alfa_ry = r_sat * SIN(Radians(SatLon - SiteLon));
	double alfa_rz = -Rz, alfa_r_north = -alfa_rx * sinRadSiteLat + alfa_rz * cosRadSiteLat;
	double alfa_r_zenith = alfa_rx * cosRadSiteLat + alfa_rz * sinRadSiteLat;
	double El_geometric = Deg(ATAN(alfa_r_zenith / std::sqrt(alfa_r_north * alfa_r_north + alfa_ry * alfa_ry)));
	double x = std::fabs(El_geometric + 0.589);
	double refraction = std::fabs(a0 + a1 * x + a2 * x * x + a3 * x * x * x + a4 * x * x * x * x);
	double El_observed = 0.00;

	if (El_geometric > 10.2)
		El_observed = El_geometric + 0.01617 * (COS(Radians(std::fabs(El_geometric))) / SIN(Radians(std::fabs(El_geometric))));
	else {
		El_observed = El_geometric + refraction;
	}

	if (alfa_r_zenith < -3000)
		El_observed = -99;

	return El_observed;
}

double calcAzimuth(double SatLon, double SiteLat, double SiteLon, int Height_over_ocean = 0)
{
	const double f = 1.00 / 298.257;	// Earth flattning factor
	const double r_sat = 42164.57;	// Distance from earth centre to satellite
	const double r_eq = 6378.14;	// Earth radius

	double sinRadSiteLat = SIN(Radians(SiteLat)), cosRadSiteLat = COS(Radians(SiteLat));
	double Rstation = r_eq / (std::sqrt(1 - f * (2 - f) * sinRadSiteLat * sinRadSiteLat));
	double Ra = (Rstation + Height_over_ocean) * cosRadSiteLat;
	double Rz = Rstation * (1 - f) * (1 - f) * sinRadSiteLat;
	double alfa_rx = r_sat * COS(Radians(SatLon - SiteLon)) - Ra;
	double alfa_ry = r_sat * SIN(Radians(SatLon - SiteLon));
	double alfa_rz = -Rz;
	double alfa_r_north = -alfa_rx * sinRadSiteLat + alfa_rz * cosRadSiteLat;
	double Azimuth = 0.00;

	if (alfa_r_north < 0)
		Azimuth = 180 + Deg(ATAN(alfa_ry / alfa_r_north));
	else
		Azimuth = Rev(360 + Deg(ATAN(alfa_ry / alfa_r_north)));

	return Azimuth;
}

double calcDeclination(double SiteLat, double Azimuth, double Elevation)
{
	return Deg(ASIN(SIN(Radians(Elevation)) * SIN(Radians(SiteLat)) + COS(Radians(Elevation)) * COS(Radians(SiteLat)) + COS(Radians(Azimuth))));
}

double calcSatHourangle(double Azimuth, double Elevation, double Declination, double Lat)
{
	double a = -COS(Radians(Elevation)) * SIN(Radians(Azimuth));
	double b = SIN(Radians(Elevation)) * COS(Radians(Lat)) - COS(Radians(Elevation)) * SIN(Radians(Lat)) * COS(Radians(Azimuth));

	// Works for all azimuths (northern & sourhern hemisphere)
	double returnvalue = 180 + Deg(ATAN(a / b));

	(void)Declination;

	if (Azimuth > 270) {
		returnvalue = ((returnvalue - 180) + 360);
		if (returnvalue > 360)
			returnvalue = 360 - (returnvalue - 360);
	}

	if (Azimuth < 90)
		returnvalue = (180 - returnvalue);

	return returnvalue;
}

void CFrontend::gotoXX(t_satellite_position pos)
{
	int RotorCmd;
	int satDir = pos < 0 ? WEST : EAST;
	double SatLon = abs(pos) / 10.00;
	double SiteLat = gotoXXLatitude;
	double SiteLon = gotoXXLongitude;

	if (gotoXXLaDirection == SOUTH)
		SiteLat = -SiteLat;
	if (gotoXXLoDirection == WEST)
		SiteLon = 360 - SiteLon;
	if (satDir == WEST)
		SatLon = 360 - SatLon;
	printf("siteLatitude = %f, siteLongitude = %f, %f degrees\n", SiteLat, SiteLon, SatLon);

	double azimuth = calcAzimuth(SatLon, SiteLat, SiteLon);
	double elevation = calcElevation(SatLon, SiteLat, SiteLon);
	double declination = calcDeclination(SiteLat, azimuth, elevation);
	double satHourAngle = calcSatHourangle(azimuth, elevation, declination, SiteLat);
	printf("azimuth=%f, elevation=%f, declination=%f, PolarmountHourAngle=%f\n", azimuth, elevation, declination, satHourAngle);
	if (SiteLat >= 0) {
		int tmp = (int)round(fabs(180 - satHourAngle) * 10.0);
		RotorCmd = (tmp / 10) * 0x10 + gotoXTable[tmp % 10];
		if (satHourAngle < 180)	// the east
			RotorCmd |= 0xE000;
		else
			RotorCmd |= 0xD000;
	} else {
		if (satHourAngle < 180) {
			int tmp = (int)round(fabs(satHourAngle) * 10.0);
			RotorCmd = (tmp / 10) * 0x10 + gotoXTable[tmp % 10];
			RotorCmd |= 0xD000;
		} else {
			int tmp = (int)round(fabs(360 - satHourAngle) * 10.0);
			RotorCmd = (tmp / 10) * 0x10 + gotoXTable[tmp % 10];
			RotorCmd |= 0xE000;
		}
	}

	printf("RotorCmd = %04x\n", RotorCmd);
	if (config.highVoltage)
		secSetVoltage(SEC_VOLTAGE_18, 100);
	sendMotorCommand(0xE0, 0x31, 0x6E, 2, ((RotorCmd & 0xFF00) / 0x100), RotorCmd & 0xFF, repeatUsals);
	//secSetVoltage(config.highVoltage ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13, 15); //FIXME ?
}

bool CFrontend::isCable(delivery_system_t delsys)
{
	return ZAPIT_DS_IS_CABLE(delsys);
}

bool CFrontend::isSat(delivery_system_t delsys)
{
	return ZAPIT_DS_IS_SAT(delsys);
}

bool CFrontend::isTerr(delivery_system_t delsys)
{
	return ZAPIT_DS_IS_TERR(delsys);
}

bool CFrontend::hasCable(void)
{
	return ZAPIT_DS_IS_CABLE(deliverySystemMask);
}

bool CFrontend::hasSat(void)
{
	return ZAPIT_DS_IS_SAT(deliverySystemMask);
}

bool CFrontend::hasTerr(void)
{
	return ZAPIT_DS_IS_TERR(deliverySystemMask);
}

bool CFrontend::isHybrid(void)
{
	if (hasSat() && hasCable())
		return true;
	if (hasSat() && hasTerr())
		return true;
	if (hasCable() && hasTerr())
		return true;

	return false;
}

void CFrontend::setName(const char* _name)
{
	memset(info.name, '\0', sizeof(info.name));
	snprintf(info.name, sizeof(info.name)-1, "%s", _name);
}

bool CFrontend::supportsDelivery(delivery_system_t delsys)
{
	return (deliverySystemMask & delsys) != 0;
}

bool CFrontend::forcedDelivery(delivery_system_t delsys)
{
	return (forcedSystemMask & delsys) != 0;
}

delivery_system_t CFrontend::getCurrentDeliverySystem(void)
{
	// FIXME: this should come from demod information
	//if (tuned)
		return currentTransponder.feparams.delsys;
	//else
	//	return UNKNOWN_DS;
}

uint32_t CFrontend::getSupportedDeliverySystems(void) const
{
	return deliverySystemMask;
}
