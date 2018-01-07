/*
 * (C) 2018 Stefan Seyfried
 * SPDX-License-Identifier: GPL-2.0
 *
 * define neutrino_msg_t and friends
 * pulled out of rcinput.h to reduce impact of that header
 */

#ifndef __neutrino_msg_t_h__
#define __neutrino_msg_t_h__

typedef unsigned long neutrino_msg_t;
typedef unsigned long neutrino_msg_data_t;

/* ugly hack to avoid including rcinput.h for key default values */
#define RC_NOKEY 0xFFFFFFFE

#define NEUTRINO_UDS_NAME "/tmp/neutrino.sock"

#endif
