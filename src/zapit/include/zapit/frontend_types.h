/*
 * Copyright (C) 2012 CoolStream International Ltd
 *
 * License: GPLv2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;
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
#ifndef __FRONTEND_TYPES_H__
#define __FRONTEND_TYPES_H__

#include <linux/dvb/frontend.h>

typedef struct {
	struct dvb_frontend_parameters	dvb_feparams;
	fe_delivery_system_t		delsys;
	fe_rolloff_t			rolloff;
} FrontendParameters;


typedef struct frontend_config {
	int diseqcRepeats;
	int diseqcType;
	int uni_scr;
	int uni_qrg;
	int uni_lnb;
	int uni_pin;
	int motorRotationSpeed;
	int highVoltage;
	int diseqc_order;
	int use_usals;
} frontend_config_t;

#endif // __FRONTEND_TYPES_H__
