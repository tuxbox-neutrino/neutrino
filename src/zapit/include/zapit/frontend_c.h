/*
 * $Id: frontend.h,v 1.30 2004/06/10 19:56:12 rasc Exp $
 *
 * (C) 2002-2003 Andreas Oberritter <obi@tuxbox.org>
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
#include "types.h"
#include "channel.h"

#define FEC_S2_QPSK_1_2 (fe_code_rate_t)(FEC_AUTO+1)		//10
#define FEC_S2_QPSK_2_3 (fe_code_rate_t)(FEC_S2_QPSK_1_2+1)	//11
#define FEC_S2_QPSK_3_4 (fe_code_rate_t)(FEC_S2_QPSK_2_3+1)	//12
#define FEC_S2_QPSK_5_6 (fe_code_rate_t)(FEC_S2_QPSK_3_4+1)	//13
#define FEC_S2_QPSK_7_8 (fe_code_rate_t)(FEC_S2_QPSK_5_6+1)	//14
#define FEC_S2_QPSK_8_9 (fe_code_rate_t)(FEC_S2_QPSK_7_8+1)	//15
#define FEC_S2_QPSK_3_5 (fe_code_rate_t)(FEC_S2_QPSK_8_9+1)	//16
#define FEC_S2_QPSK_4_5 (fe_code_rate_t)(FEC_S2_QPSK_3_5+1)	//17
#define FEC_S2_QPSK_9_10 (fe_code_rate_t)(FEC_S2_QPSK_4_5+1)	//18

#define FEC_S2_8PSK_1_2 (fe_code_rate_t)(FEC_S2_QPSK_9_10+1)	//19
#define FEC_S2_8PSK_2_3 (fe_code_rate_t)(FEC_S2_8PSK_1_2+1)	//20
#define FEC_S2_8PSK_3_4 (fe_code_rate_t)(FEC_S2_8PSK_2_3+1)	//21
#define FEC_S2_8PSK_5_6 (fe_code_rate_t)(FEC_S2_8PSK_3_4+1)	//22
#define FEC_S2_8PSK_7_8 (fe_code_rate_t)(FEC_S2_8PSK_5_6+1)	//23
#define FEC_S2_8PSK_8_9 (fe_code_rate_t)(FEC_S2_8PSK_7_8+1)	//24
#define FEC_S2_8PSK_3_5 (fe_code_rate_t)(FEC_S2_8PSK_8_9+1)	//25
#define FEC_S2_8PSK_4_5 (fe_code_rate_t)(FEC_S2_8PSK_3_5+1)	//26
#define FEC_S2_8PSK_9_10 (fe_code_rate_t)(FEC_S2_8PSK_4_5+1)	//27
#define FEC_S2_AUTO      (fe_code_rate_t)(FEC_S2_8PSK_9_10+1)	//28

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

typedef struct dvb_frontend_parameters FrontendParameters;

#define MAX_LNBS	64	/* due to Diseqc 1.1  (2003-01-10 rasc) */

class CFrontend
{
	private:
		int fd;

		/* tuning finished flag */
		bool tuned;
		/* information about the used frontend type */
		struct dvb_frontend_info info;
		/* current 22kHz tone mode */
		fe_sec_tone_mode_t currentToneMode;
		int currentDiseqc;
		fe_sec_voltage_t currentVoltage;
		/* current satellite position */
		int32_t currentSatellitePosition;
		/* how often to repeat DiSEqC 1.1 commands */
		uint8_t diseqcRepeats;
		/* DiSEqC type of attached hardware */
		diseqc_t diseqcType;
		int diseqc;
		uint8_t uncommitedInput;
		/* lnb offsets */
		int32_t lnbOffsetLow;
		int32_t lnbOffsetHigh;
		int32_t lnbSwitch;
		/* current Transponderdata */
		TP_params currentTransponder;
		struct dvb_frontend_parameters curfe;
		bool slave;
		int fenumber;
		bool standby;
		uint32_t			getDiseqcReply(const int timeout_ms) const;
		struct dvb_frontend_parameters	getFrontend(void) const;

		void				secResetOverload(void);
		void				secSetTone(const fe_sec_tone_mode_t mode, const uint32_t ms);
		void				secSetVoltage(const fe_sec_voltage_t voltage, const uint32_t ms);
		void				sendDiseqcCommand(const struct dvb_diseqc_master_cmd *cmd, const uint32_t ms);
		void				sendDiseqcPowerOn(void);
		void				sendDiseqcReset(void);
		void				sendDiseqcSmatvRemoteTuningCommand(const uint32_t frequency);
		void				sendDiseqcStandby(void);
		void				sendDiseqcZeroByteCommand(const uint8_t frm, const uint8_t addr, const uint8_t cmd);
		void				sendToneBurst(const fe_sec_mini_cmd_t burst, const uint32_t ms);
		int				setFrontend(const struct dvb_frontend_parameters *feparams, bool nowait = false);
		void				setSec(const uint8_t sat_no, const uint8_t pol, const bool high_band);
		void				set12V(bool enable);
		void				reset(void);


	public:
		CFrontend(int fe = 0);
		~CFrontend(void);

		static fe_code_rate_t		getCodeRate(const uint8_t fec_inner, int system = 0);
		uint8_t				getDiseqcPosition(void) const		{ return currentTransponder.diseqc; }
		uint8_t				getDiseqcRepeats(void) const		{ return diseqcRepeats; }
		diseqc_t			getDiseqcType(void) const		{ return diseqcType; }
		uint32_t			getFrequency(void) const;
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

		void				setDiseqcRepeats(const uint8_t repeats)	{ diseqcRepeats = repeats; }
		void				setDiseqcType(const diseqc_t type);
		int				setParameters(TP_params *TP, bool nowait = 0);
		int				tuneFrequency (struct dvb_frontend_parameters * feparams, uint8_t polarization, bool nowait = 0);
		const TP_params*		getParameters(void) const { return &currentTransponder; };
		struct dvb_frontend_event*	setParametersResponse(TP_params *TP);
		void 				setCurrentSatellitePosition(int32_t satellitePosition) {currentSatellitePosition = satellitePosition; }

		void 				positionMotor(uint8_t motorPosition);
		void				sendMotorCommand(uint8_t cmdtype, uint8_t address, uint8_t command, uint8_t num_parameters, uint8_t parameter1, uint8_t parameter2, int repeat = 0);
		void 				gotoXX(t_satellite_position pos);
		bool				tuneChannel(CZapitChannel *channel, bool nvod);
		bool				retuneChannel(void);
		bool				retuneTP(bool nowait = true);

		fe_code_rate_t 			getCFEC ();
		transponder_id_t		getTsidOnid() { return currentTransponder.TP_id; }
		void 				setTsidOnid(transponder_id_t newid)  { currentTransponder.TP_id = newid; }
		uint32_t 			getRate ();
                void Close();
                void Open();
		bool sendUncommittedSwitchesCommand(int input);
		bool setInput(CZapitChannel *channel, bool nvod);
		void setInput(t_satellite_position satellitePosition, uint32_t frequency, uint8_t polarization);
		bool setDiseqcSimple(int sat_no, const uint8_t pol, const uint32_t frequency);
		void setDiseqc(int sat_no, const uint8_t pol, const uint32_t frequency);
		void setMasterSlave(bool _slave) { slave = _slave; };
		int driveToSatellitePosition(t_satellite_position satellitePosition, bool from_scan = false);
		void setLnbOffsets(int32_t _lnbOffsetLow, int32_t _lnbOffsetHigh, int32_t _lnbSwitch);
		struct dvb_frontend_event getEvent(void);
};

#endif /* __zapit_frontend_h__ */
