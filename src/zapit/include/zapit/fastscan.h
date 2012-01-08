/*
        Neutrino-GUI  -   DBoxII-Project

        Copyright (C) 2011 CoolStream International Ltd

        License: GPLv2

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation;

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef _FASTSCAN_H_
#define _FASTSCAN_H_

typedef enum fs_operator {
	OPERATOR_CD,
	OPERATOR_TVV,
	OPERATOR_TELESAT,
	OPERATOR_MAX
} fs_operator_t;

#define CD_OPERATOR_ID		(106)
#define TVV_OPERATOR_ID		(108)
#define TELESAT_OPERATOR_ID	(109)

#define FAST_SCAN_SD 1
#define FAST_SCAN_HD 2
#define FAST_SCAN_ALL 3

typedef struct fast_scan_operator {
	int id;
	unsigned short sd_pid;
	unsigned short hd_pid;
	char * name;
} fast_scan_operator_t;

typedef struct fast_scan_type {
	fs_operator_t op;
	int type;
} fast_scan_type_t;

extern fast_scan_operator_t fast_scan_operators [OPERATOR_MAX];

#endif
