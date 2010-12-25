/*
 * compatibility stuff for conversion of Tripledragon API values to DVB API
 * and vice versa
 *
 * (C) 2009 Stefan Seyfried
 *
 * Released under the GPL V2.
 */

#ifndef _td_value_compat_
#define _td_value_compat_

#undef FE_GET_INFO
#undef FE_READ_BER
#undef FE_READ_SIGNAL_STRENGTH
#undef FE_READ_SNR
#undef FE_READ_UNCORRECTED_BLOCKS
#undef FE_GET_EVENT
#undef FE_READ_STATUS
#undef FE_SET_PROPERTY
#undef FE_GET_EVENT
#undef FE_GET_EVENT
#undef FE_SET_PROPERTY
#undef FE_SET_TONE
#undef FE_ENABLE_HIGH_LNB_VOLTAGE
#undef FE_SET_VOLTAGE
#undef FE_DISEQC_SEND_MASTER_CMD
#undef FE_DISEQC_SEND_BURST
/* hack, linux/dvb/frontend.h already defines fe_status */
#define fe_status	td_fe_status
#define fe_status_t	td_fe_status_t
#define FE_HAS_SIGNAL	TD_FE_HAS_SIGNAL
#define FE_HAS_CARRIER	TD_FE_HAS_CARRIER
#define FE_HAS_VITERBI	TD_FE_HAS_VITERBI
#define FE_HAS_SYNC	TD_FE_HAS_SYNC
#define FE_HAS_LOCK	TD_FE_HAS_LOCK
#define FE_TIMEDOUT	TD_FE_TIMEDOUT
#define FE_REINIT	TD_FE_REINIT
#include <tdtuner/tuner_inf.h>
#undef fe_status
#undef fe_status_t
#undef FE_HAS_SIGNAL
#undef FE_HAS_CARRIER
#undef FE_HAS_VITERBI
#undef FE_HAS_SYNC
#undef FE_HAS_LOCK
#undef FE_TIMEDOUT
#undef FE_REINIT
enum td_code_rate {
        TD_FEC_AUTO = 0,
        TD_FEC_1_2,
        TD_FEC_2_3,
        TD_FEC_3_4,
        TD_FEC_5_6,
        TD_FEC_7_8
};

static inline unsigned int dvbfec2tdfec(fe_code_rate_t fec)
{
	switch (fec) {
	case FEC_1_2: // FEC_1_2 ... FEC_3_4 are equal to TD_FEC_1_2 ... TD_FEC_3_4
	case FEC_2_3:
	case FEC_3_4:
		return (unsigned int)fec;
	case FEC_5_6:
		return TD_FEC_5_6;
	case FEC_7_8:
		return TD_FEC_7_8;
	default:
		break;
	}
	return TD_FEC_AUTO;
}

static inline fe_code_rate_t tdfec2dvbfec(unsigned int tdfec)
{
	switch (tdfec)
	{
	case TD_FEC_1_2:
	case TD_FEC_2_3:
	case TD_FEC_3_4:
		return (fe_code_rate_t)tdfec;
	case TD_FEC_5_6:
		return FEC_5_6;
	case TD_FEC_7_8:
		return FEC_7_8;
	default:
		break;
	}
	return FEC_AUTO;
}

#endif /* _td_value_compat_ */
