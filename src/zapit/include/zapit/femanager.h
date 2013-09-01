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

#include <OpenThreads/ReentrantMutex>

#define MAX_FE          4
#define MAX_ADAPTERS    1
//#define DYNAMIC_DEMUX
//#define MAKE_FE_KEY(adapter, number) ((adapter << 8) | (number & 0xFF))

#define FECONFIGFILE      CONFIGDIR "/zapit/frontend.conf"

typedef std::map<unsigned short, CFrontend*> fe_map_t;
typedef fe_map_t::iterator fe_map_iterator_t;

typedef std::map<CFrontend*, t_channel_id> fe_channel_map_t;
typedef fe_channel_map_t::iterator fe_channel_map_iterator_t;

typedef std::set<fe_type_t> fe_type_list_t;

typedef struct common_fe_config {
	double gotoXXLatitude, gotoXXLongitude;
	int gotoXXLaDirection, gotoXXLoDirection;
	int repeatUsals;
	int feTimeout;
} common_fe_config_t;

class CFeDmx
{
	private:
		int num;
		transponder_id_t tpid;
		int usecount;

		friend class CFEManager;
	public:
		CFeDmx(int i);
		void Lock(transponder_id_t id);
		void Unlock();
};

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
		int			enabled_count;
		CConfigFile		configfile;
		common_fe_config_t	config;
		bool			config_exist;

		bool			have_sat;
		bool			have_cable;
		bool			have_terr;
		bool			have_locked;
		OpenThreads::ReentrantMutex	mutex;

		std::vector<CFeDmx>	dmap;

		CFrontend *		livefe;

		uint32_t	getConfigValue(CFrontend * fe, const char * name, uint32_t defval);
		void		setConfigValue(CFrontend * fe, const char * name, uint32_t val);
		void		setSatelliteConfig(CFrontend * fe, sat_config_t &satconfig);
		bool		getSatelliteConfig(CFrontend * fe, sat_config_t &satconfig);

		bool		loopCanTune(CFrontend * fe, CZapitChannel * channel);
		CFrontend *	getFrontend(CZapitChannel * channel);
		void		copySettings(CFrontend * from, CFrontend * to);

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

		CFrontend *	allocateFE(CZapitChannel * channel, bool forrecord = false);

		fe_mode_t	getMode() { return mode; };
		void		setMode(fe_mode_t newmode, bool initial = false);

		int		getFrontendCount() { return femap.size(); };
		int		getEnabledCount();

		CFrontend *	getScanFrontend(t_satellite_position satellitePosition);
		bool		canTune(CZapitChannel * channel);

		bool		configExist() { return config_exist; };
		bool		loadSettings();
		void		saveSettings(bool write = true);

		bool		lockFrontend(CFrontend * fe, CZapitChannel * channel = NULL);
		bool		unlockFrontend(CFrontend * fe, bool unlock_demux = false);
		bool		haveFreeFrontend();
		void		linkFrontends(bool init = true);
		void		copySettings(CFrontend * fe);
		int		getDemux(transponder_id_t id);
		bool		lockDemux(int i, transponder_id_t id);
		void		unlockDemux(int i);
		bool		haveFreeDemux();
		bool		haveSat() { return have_sat; }
		bool		haveCable() { return have_cable; }
		bool		haveTerr() { return have_terr; }
		bool		satOnly() { return (have_sat && !have_cable && !have_terr); }
		bool		cableOnly() { return (have_cable && !have_sat && ! have_terr); }
		bool		terrOnly() { return (have_terr && !have_sat && ! have_cable); }
		void		Lock() { mutex.lock(); }
		void		Unlock() { mutex.unlock(); }

};
#endif /* __femanager_h__ */
