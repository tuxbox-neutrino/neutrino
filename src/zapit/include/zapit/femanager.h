/*
 * Copyright (C) 2011 CoolStream International Ltd
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

#ifndef __femanager_h__
#define __femanager_h__

#include <inttypes.h>
#include <configfile.h>
#include <zapit/types.h>
#include <zapit/settings.h>
#include <zapit/getservices.h>
#include <zapit/frontend_c.h>
#include <map>

#include <OpenThreads/Mutex>

#define MAX_FE          2
#define MAX_ADAPTERS    1
#define MAKE_FE_KEY(adapter, number) ((adapter << 8) | (number & 0xFF))

#define FECONFIGFILE      CONFIGDIR "/zapit/frontend.conf"

typedef std::map<unsigned short, CFrontend*> fe_map_t;
typedef fe_map_t::iterator fe_map_iterator_t;

typedef std::map<CFrontend*, t_channel_id> fe_channel_map_t;
typedef fe_channel_map_t::iterator fe_channel_map_iterator_t;

typedef struct common_fe_config {
	double gotoXXLatitude, gotoXXLongitude;
	int gotoXXLaDirection, gotoXXLoDirection;
	int repeatUsals;
	int feTimeout;
} common_fe_config_t;

class CFEManager
{
	public:
		typedef enum {
			FE_MODE_SINGLE,
			FE_MODE_LOOP,
			FE_MODE_TWIN,
			FE_MODE_ALONE
		} fe_mode_t;
	private:
		fe_map_t		femap;
		fe_mode_t		mode;
		CConfigFile		configfile;
		common_fe_config_t	config;
		bool			config_exist;
		/* loop cache */
		bool			high_band;
		uint8_t			polarization;

		bool			have_locked;
		OpenThreads::Mutex	mutex;

		CFrontend *		livefe;

		CFrontend *	findFrontend(CZapitChannel * channel);
		uint32_t	getConfigValue(CFrontend * fe, const char * name, uint32_t defval);
		void		setConfigValue(CFrontend * fe, const char * name, uint32_t val);
		void		setSatelliteConfig(CFrontend * fe, sat_config_t &satconfig);
		bool		getSatelliteConfig(CFrontend * fe, sat_config_t &satconfig);

		static CFEManager * manager;
		CFEManager();
	public:
		static CFEManager *getInstance();
		~CFEManager();

		bool		Init();
		void		Close();
		void		Open();

		CFrontend *	getFE(int index = 0);
		CFrontend *	getLiveFE() { return livefe; };
		void		setLiveFE(CFrontend * fe);

		transponder *	getChannelTransponder(CZapitChannel * channel);
		CFrontend *	allocateFE(CZapitChannel * channel);
		bool		loopCanTune(CFrontend * fe, CZapitChannel * channel);
		CFrontend *	getLoopFE(CZapitChannel * channel);
		CFrontend *	getIndependentFE(CZapitChannel * channel);

		fe_mode_t	getMode() { return mode; };
		void		setMode(fe_mode_t newmode, bool initial = false);

		int		getFrontendCount() { return femap.size(); };

		CFrontend *	getScanFrontend(t_satellite_position satellitePosition);
		bool		canTune(CZapitChannel * channel);

		bool		configExist() { return config_exist; };
		bool		loadSettings();
		void		saveSettings(bool write = true);

		bool		lockFrontend(CFrontend * fe);
		bool		unlockFrontend(CFrontend * fe);
		bool		haveFreeFrontend();
};
#endif /* __femanager_h__ */
