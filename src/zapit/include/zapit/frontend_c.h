/*
 * $Id: frontend.h,v 1.30 2004/06/10 19:56:12 rasc Exp $
 *
 * (C) 2002-2003 Andreas Oberritter <obi@tuxbox.org>
 *
 * Copyright (C) 2011 CoolStream International Ltd
 * Copyright (C) 2009,2010,2012,2013 Stefan Seyfried
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
 *
 */

#ifndef __zapit_frontend_h__
#define __zapit_frontend_h__

#include <inttypes.h>
#include <OpenThreads/Thread>

#include <zapit/types.h>
#include <zapit/channel.h>
#include <zapit/satconfig.h>
#include <zapit/frontend_types.h>
#include <map>

#define FEC_S2_QPSK_BASE (fe_code_rate_t)(FEC_AUTO+1)
#define FEC_S2_QPSK_1_2 (fe_code_rate_t)(FEC_S2_QPSK_BASE+0)	//10
#define FEC_S2_QPSK_2_3 (fe_code_rate_t)(FEC_S2_QPSK_BASE+1)	//11
#define FEC_S2_QPSK_3_4 (fe_code_rate_t)(FEC_S2_QPSK_BASE+2)	//12
#define FEC_S2_QPSK_5_6 (fe_code_rate_t)(FEC_S2_QPSK_BASE+3)	//13
#define FEC_S2_QPSK_7_8 (fe_code_rate_t)(FEC_S2_QPSK_BASE+4)	//14
#define FEC_S2_QPSK_8_9 (fe_code_rate_t)(FEC_S2_QPSK_BASE+5)	//15
#define FEC_S2_QPSK_3_5 (fe_code_rate_t)(FEC_S2_QPSK_BASE+6)	//16
#define FEC_S2_QPSK_4_5 (fe_code_rate_t)(FEC_S2_QPSK_BASE+7)	//17
#define FEC_S2_QPSK_9_10 (fe_code_rate_t)(FEC_S2_QPSK_BASE+8)	//18

#define FEC_S2_8PSK_BASE (fe_code_rate_t)(FEC_S2_QPSK_9_10+1)
#define FEC_S2_8PSK_1_2 (fe_code_rate_t)(FEC_S2_8PSK_BASE+0)	//19
#define FEC_S2_8PSK_2_3 (fe_code_rate_t)(FEC_S2_8PSK_BASE+1)	//20
#define FEC_S2_8PSK_3_4 (fe_code_rate_t)(FEC_S2_8PSK_BASE+2)	//21
#define FEC_S2_8PSK_5_6 (fe_code_rate_t)(FEC_S2_8PSK_BASE+3)	//22
#define FEC_S2_8PSK_7_8 (fe_code_rate_t)(FEC_S2_8PSK_BASE+4)	//23
#define FEC_S2_8PSK_8_9 (fe_code_rate_t)(FEC_S2_8PSK_BASE+5)	//24
#define FEC_S2_8PSK_3_5 (fe_code_rate_t)(FEC_S2_8PSK_BASE+6)	//25
#define FEC_S2_8PSK_4_5 (fe_code_rate_t)(FEC_S2_8PSK_BASE+7)	//26
#define FEC_S2_8PSK_9_10 (fe_code_rate_t)(FEC_S2_8PSK_BASE+8)	//27
#define FEC_S2_AUTO      (fe_code_rate_t)(FEC_S2_8PSK_BASE+9)	//28

#define MAKE_FE_KEY(adapter, number) ((adapter << 8) | (number & 0xFF))

static inline fe_modulation_t dvbs_get_modulation(fe_code_rate_t fec)
{
	if((fec < FEC_S2_QPSK_1_2) || (fec < FEC_S2_8PSK_1_2))
		return QPSK;
	else 
		return PSK_8;
}

static inline fe_delivery_system_t dvbs_get_delsys(fe_code_rate_t fec)
{
	if(fec < FEC_S2_QPSK_1_2)
		return SYS_DVBS;
	else
		return SYS_DVBS2;
}

static inline fe_rolloff_t dvbs_get_rolloff(fe_delivery_system_t delsys)
{
	if(delsys == SYS_DVBS2)
		return ROLLOFF_25;
	else
		return ROLLOFF_35;
}

class CFEManager;

class CFrontend;
typedef std::vector<CFrontend*> fe_linkmap_t;

class CFrontend
{
	public:
		typedef enum {
			FE_MODE_UNUSED,
			FE_MODE_INDEPENDENT,
			FE_MODE_MASTER,
			FE_MODE_LINK_LOOP,
			FE_MODE_LINK_TWIN
		} fe_work_mode_t;

	private:
		/* frontend filedescriptor */
		int fd;

		OpenThreads::Mutex      mutex;
		/* use count for locking purposes */
		int usecount;
		/* current adapter where this frontend is on */
		int adapter;
		bool locked;
		/* information about the used frontend type */
		struct dvb_frontend_info info;
		/* current 22kHz tone mode */
		fe_sec_tone_mode_t currentToneMode;
		fe_sec_voltage_t currentVoltage;
		/* current satellite position */
		int32_t currentSatellitePosition;
		/* rotor satellite position */
		int32_t rotorSatellitePosition;

		/* SETTINGS */
		frontend_config_t config;

		satellite_map_t satellites;

		double gotoXXLatitude, gotoXXLongitude;
		int gotoXXLaDirection, gotoXXLoDirection;
		int repeatUsals;
		int feTimeout;

		uint8_t uncommitedInput;
		/* lnb offsets */
		int32_t lnbOffsetLow;
		int32_t lnbOffsetHigh;
		int32_t lnbSwitch;
		/* current Transponderdata */
		TP_params currentTransponder;
		bool slave;
		fe_work_mode_t femode;
		int masterkey;
		fe_linkmap_t linkmap;
		int fenumber;
		bool standby;
		bool buildProperties(const FrontendParameters*, struct dtv_properties &);

		FrontendParameters		getFrontend(void) const;
		void				secSetTone(const fe_sec_tone_mode_t mode, const uint32_t ms);
		void				secSetVoltage(const fe_sec_voltage_t voltage, const uint32_t ms);
		void				sendDiseqcCommand(const struct dvb_diseqc_master_cmd *cmd, const uint32_t ms);
		void				sendDiseqcStandby(uint32_t ms = 100);
		void				sendDiseqcPowerOn(uint32_t ms = 100);
		void				sendDiseqcReset(uint32_t ms = 40);
		void				sendDiseqcSmatvRemoteTuningCommand(const uint32_t frequency);
		uint32_t			sendEN50494TuningCommand(const uint32_t frequency, const int high_band, const int horizontal, const int bank);
		void				sendDiseqcZeroByteCommand(const uint8_t frm, const uint8_t addr, const uint8_t cmd, uint32_t ms = 15);
		void				sendToneBurst(const fe_sec_mini_cmd_t burst, const uint32_t ms);
		int				setFrontend(const FrontendParameters *feparams, bool nowait = false);
		void				setSec(const uint8_t sat_no, const uint8_t pol, const bool high_band);
		void				reset(void);
		/* Private constructor */
		CFrontend(int Number = 0, int Adapter = 0);
		bool				Open(bool init = false);
		void				Close(void);
		void				Init(void);

		friend class CFEManager;
	public:
		/* tuning finished flag */
		bool tuned;

		~CFrontend(void);

		static fe_code_rate_t		getCodeRate(const uint8_t fec_inner, int system = 0);
		uint8_t				getDiseqcPosition(void) const		{ return currentTransponder.diseqc; }
		uint8_t				getDiseqcRepeats(void) const		{ return (uint8_t) config.diseqcRepeats; }
		diseqc_t			getDiseqcType(void) const		{ return (diseqc_t) config.diseqcType; }
		uint32_t			getFrequency(void) const		{ return currentTransponder.feparams.dvb_feparams.frequency; }
		bool				getHighBand()				{ return (int) getFrequency() >= lnbSwitch; }
		static fe_modulation_t		getModulation(const uint8_t modulation);
		uint8_t				getPolarization(void) const;
		const struct dvb_frontend_info *getInfo(void) const			{ return &info; };

		uint32_t			getBitErrorRate(void) const;
		uint16_t			getSignalNoiseRatio(void) const;
		uint16_t			getSignalStrength(void) const;
		fe_status_t			getStatus(void) const;
		uint32_t			getUncorrectedBlocks(void) const;
		void				getDelSys(int f, int m, char * &fec, char * &sys, char * &mod);

		int32_t				getCurrentSatellitePosition() { return currentSatellitePosition; }
		int32_t				getRotorSatellitePosition() { return rotorSatellitePosition; }

		void				setDiseqcRepeats(const uint8_t repeats)	{ config.diseqcRepeats = repeats; }
		void				setDiseqcType(const diseqc_t type, bool force = false);
		void				setTimeout(int timeout) { feTimeout = timeout; };
		void				configUsals(double Latitude, double Longitude, int LaDirection, int LoDirection, bool _repeatUsals)
		{
			gotoXXLatitude = Latitude;
			gotoXXLongitude = Longitude;
			gotoXXLaDirection = LaDirection;
			gotoXXLoDirection = LoDirection;
			repeatUsals = _repeatUsals;
		};
		void				configRotor(int _motorRotationSpeed, bool _highVoltage)
							{ config.motorRotationSpeed = _motorRotationSpeed; config.highVoltage = _highVoltage; };

		frontend_config_t&		getConfig() { return config; };
		void				setConfig(frontend_config_t cfg) { setDiseqcType((diseqc_t) cfg.diseqcType); config = cfg; };

		int				setParameters(TP_params *TP, bool nowait = 0);
		int				tuneFrequency (FrontendParameters * feparams, uint8_t polarization, bool nowait = false);
		const TP_params*		getParameters(void) const { return &currentTransponder; };
		void				setCurrentSatellitePosition(int32_t satellitePosition) {currentSatellitePosition = satellitePosition; }
		void				setRotorSatellitePosition(int32_t satellitePosition) {rotorSatellitePosition = satellitePosition; }

		void				positionMotor(uint8_t motorPosition);
		void				sendMotorCommand(uint8_t cmdtype, uint8_t address, uint8_t command, uint8_t num_parameters, uint8_t parameter1, uint8_t parameter2, int repeat = 0);
		void				gotoXX(t_satellite_position pos);
		bool				tuneChannel(CZapitChannel *channel, bool nvod);
		bool				retuneChannel(void);

		fe_code_rate_t 			getCFEC ();
		transponder_id_t		getTsidOnid() { return currentTransponder.TP_id; }
		bool				sameTsidOnid(transponder_id_t tpid)
		{
			return (currentTransponder.TP_id == 0)
				|| (tpid == currentTransponder.TP_id);
		}
		void 				setTsidOnid(transponder_id_t newid)  { currentTransponder.TP_id = newid; }
		uint32_t 			getRate ();

		void				Lock();
		void				Unlock();

		bool sendUncommittedSwitchesCommand(int input);
		bool setInput(CZapitChannel *channel, bool nvod);
		void setInput(t_satellite_position satellitePosition, uint32_t frequency, uint8_t polarization);
		bool setDiseqcSimple(int sat_no, const uint8_t pol, const uint32_t frequency);
		void setDiseqc(int sat_no, const uint8_t pol, const uint32_t frequency);
		void setMasterSlave(bool _slave);
		int driveToSatellitePosition(t_satellite_position satellitePosition, bool from_scan = false);
		void setLnbOffsets(int32_t _lnbOffsetLow, int32_t _lnbOffsetHigh, int32_t _lnbSwitch);
		struct dvb_frontend_event getEvent(void);
		bool				Locked() { return usecount; };
		satellite_map_t &		getSatellites() { return satellites; }
		void				setSatellites(satellite_map_t satmap) { satellites = satmap; }
		int				getNumber() { return fenumber; };
		static void			getDelSys(uint8_t type, int f, int m, char * &fec, char * &sys, char * &mod);
		fe_work_mode_t			getMode() { return femode; }
		void				setMode(int mode) {femode = (fe_work_mode_t) mode; }
		int				getMaster() { return masterkey; }
		void				setMaster(int key) { masterkey = key; }
		bool				hasLinks() { return (femode == FE_MODE_MASTER) && (linkmap.size() > 1); }
		static bool			linked(int mode)
		{
			if ((mode == FE_MODE_LINK_LOOP) || (mode == FE_MODE_LINK_TWIN))
				return true;
			return false;
		}
		bool				isCable() { return (info.type == FE_QAM); }
		bool				isSat() { return (info.type == FE_QPSK); }
		bool				isTerr() { return (info.type == FE_OFDM); }
		fe_type_t			getType() { return info.type; }
};
#endif /* __zapit_frontend_h__ */
