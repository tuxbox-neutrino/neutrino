/*
 * compatibility stuff for Tripledragon frontend API
 *
 * (C) 2009 Stefan Seyfried
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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

#ifndef __td_frontend_compat_h__
#define __td_frontend_compat_h__

#ifdef __cplusplus
extern "C" {
#endif
	#include <tdtuner/tuner_inf.h>
#ifdef __cplusplus
}
#endif

/* I know that those are different. But functions that get a
   dvb_frontend_parameters struct passed on dbox/dreambox will most likely
   get a tunersetup struct on TD, so it keeps the differences in headers
   and function prototypes small. Of course, the functions itself will have
   #ifdef TRIPLEDRAGON or similar... */
#define dvb_frontend_parameters tunersetup

/* compat stuff for settings.cpp */
enum {
	INVERSION_OFF,
	INVERSION_ON,
	INVERSION_AUTO
};
typedef enum fe_code_rate {
	FEC_NONE = 0,
	FEC_1_2,
	FEC_2_3,
	FEC_3_4,
	FEC_4_5,
	FEC_5_6,
	FEC_6_7,
	FEC_7_8,
	FEC_8_9,
	FEC_AUTO
} fe_code_rate_t;

enum td_code_rate {
	TD_FEC_AUTO = 0,
	TD_FEC_1_2,
	TD_FEC_2_3,
	TD_FEC_3_4,
	TD_FEC_5_6,
	TD_FEC_7_8
};

typedef enum fe_sec_tone_mode {
	SEC_TONE_ON,
	SEC_TONE_OFF
} fe_sec_tone_mode_t;

typedef enum fe_sec_voltage {
	SEC_VOLTAGE_13,
	SEC_VOLTAGE_18,
	SEC_VOLTAGE_OFF
} fe_sec_voltage_t;

typedef enum fe_sec_mini_cmd {
	SEC_MINI_A,
	SEC_MINI_B
} fe_sec_mini_cmd_t;

struct dvb_diseqc_master_cmd {
	unsigned char msg [6];   /*  { framing, address, command, data [3] } */
	unsigned char msg_len;   /*  valid values are 3...6  */
};

typedef enum fe_type {
	FE_QPSK,
	FE_QAM,
	FE_OFDM,
	FE_ATSC
} fe_type_t;

struct dvb_frontend_info {
//	char       name[128];
	fe_type_t  type;
#if 0
	__u32      frequency_min;
	__u32      frequency_max;
	__u32      frequency_stepsize;
	__u32      frequency_tolerance;
	__u32      symbol_rate_min;
	__u32      symbol_rate_max;
	__u32      symbol_rate_tolerance;	/* ppm */
	__u32      notifier_delay;		/* DEPRECATED */
	fe_caps_t  caps;
#endif
};

struct dvb_frontend_event {
	fe_status_t status;
	tunersetup parameters;
};

#ifdef _DVBFRONTEND_H_
#error _DVBFRONTEND_H_ included
#endif

#endif /* __td_frontend_compat_h__ */
