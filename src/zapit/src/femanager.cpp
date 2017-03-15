/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2011 CoolStream International Ltd
	Copyright (C) 2012-2016 Stefan Seyfried

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include <zapit/debug.h>
#include <zapit/channel.h>
#include <zapit/getservices.h>
#include <zapit/zapit.h>
#include <zapit/client/zapittools.h>
#include <zapit/femanager.h>
#include <zapit/capmt.h>
#include <dmx_cs.h>
#include <OpenThreads/ScopedLock>

static int fedebug = 0;
static int unused_demux;

#define FEDEBUG(fmt, args...)					\
        do {							\
                if (fedebug)					\
			INFO(fmt, ##args);			\
        } while (0)

CFeDmx::CFeDmx(int i)
{
	num = i;
	tpid = 0;
	usecount = 0;
}

void CFeDmx::Lock(transponder_id_t id)
{
	tpid = id;
	usecount++;
	INFO("[dmx%d] lock, usecount %d tp %" PRIx64, num, usecount, tpid);
}

void CFeDmx::Unlock()
{
	if(usecount > 0)
		usecount--;
	else
		tpid = 0;
	INFO("[dmx%d] unlock, usecount %d tp %" PRIx64, num, usecount, tpid);
}

CFEManager * CFEManager::manager = NULL;

CFEManager::CFEManager() : configfile(',', true)
{
	livefe = NULL;
	mode = FE_MODE_SINGLE;
	config_exist = false;
	have_locked = false;
	enabled_count = 0;
}

bool CFEManager::Init()
{
	CFrontend * fe;
	unsigned short fekey;

	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	have_sat = have_cable = have_terr = false;
	for(int i = 0; i < MAX_ADAPTERS; i++) {
		for(int j = 0; j < MAX_FE; j++) {
			fe = new CFrontend(j, i);
			if(fe->Open()) {
				fekey = MAKE_FE_KEY(i, j);
				femap.insert(std::pair <unsigned short, CFrontend*> (fekey, fe));
				INFO("add fe %d", fe->fenumber);
				if(livefe == NULL)
					livefe = fe;
				if (fe->hasSat())
					have_sat = true;
				if (fe->hasCable())
					have_cable = true;
				if (fe->hasTerr())
					have_terr = true;
			} else
				delete fe;
		}
	}
	for (unsigned i = 0; i < MAX_DMX_UNITS; i++)
		dmap.push_back(CFeDmx(i));

	INFO("found %d frontends, %d demuxes", (int)femap.size(), (int)dmap.size());
	/* for testing without a frontend, export SIMULATE_FE=1 */
	if (femap.empty() && getenv("SIMULATE_FE")) {
		INFO("SIMULATE_FE is set, adding dummy frontend for testing");
		fe = new CFrontend(0, -1);
		fekey = MAKE_FE_KEY(0, 0);
		femap.insert(std::pair <unsigned short, CFrontend*> (fekey, fe));
		fe->Open();
		livefe = fe;
		have_sat = true;
	}
	if (femap.empty())
		return false;

	return true;
}

CFEManager::~CFEManager()
{
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++)
		delete it->second;
	dmap.clear();
}

CFEManager * CFEManager::getInstance()
{
	if(manager == NULL)
		manager = new CFEManager();
	return manager;
}

uint32_t CFEManager::getConfigValue(CFrontend * fe, const char * name, uint32_t defval)
{
	char cfg_key[81];
	sprintf(cfg_key, "fe%d_%s", fe->fenumber, name);
	return configfile.getInt32(cfg_key, defval);
}

void CFEManager::setConfigValue(CFrontend * fe, const char * name, uint32_t val)
{
	char cfg_key[81];
	sprintf(cfg_key, "fe%d_%s", fe->fenumber, name);
	configfile.setInt32(cfg_key, val);
}

#define SATCONFIG_SIZE 12
void CFEManager::setSatelliteConfig(CFrontend * fe, sat_config_t &satconfig)
{
	char cfg_key[81];
	std::vector<int> satConfig;

	satConfig.push_back(satconfig.position);
	satConfig.push_back(satconfig.diseqc);
	satConfig.push_back(satconfig.commited);
	satConfig.push_back(satconfig.uncommited);
	satConfig.push_back(satconfig.motor_position);
	satConfig.push_back(satconfig.diseqc_order);
	satConfig.push_back(satconfig.lnbOffsetLow);
	satConfig.push_back(satconfig.lnbOffsetHigh);
	satConfig.push_back(satconfig.lnbSwitch);
	satConfig.push_back(satconfig.use_in_scan);
	satConfig.push_back(satconfig.use_usals);
	satConfig.push_back(satconfig.configured);

	sprintf(cfg_key, "fe%d_position_%d", fe->fenumber, satconfig.position);
	//INFO("set %s", cfg_key);
	configfile.setInt32Vector(cfg_key, satConfig);
}

bool CFEManager::getSatelliteConfig(CFrontend * fe, sat_config_t &satconfig)
{
	char cfg_key[81];
	int i = 1;

	sprintf(cfg_key, "fe%d_position_%d", fe->fenumber, satconfig.position);
	std::vector<int> satConfig = configfile.getInt32Vector(cfg_key);
	//INFO("get %s: size %d", cfg_key, satConfig.size());
	if(satConfig.size() >= SATCONFIG_SIZE) {
		satconfig.diseqc		= satConfig[i++];
		satconfig.commited		= satConfig[i++];
		satconfig.uncommited		= satConfig[i++];
		satconfig.motor_position	= satConfig[i++];
		satconfig.diseqc_order		= satConfig[i++];
		satconfig.lnbOffsetLow		= satConfig[i++];
		satconfig.lnbOffsetHigh		= satConfig[i++];
		satconfig.lnbSwitch		= satConfig[i++];
		satconfig.use_in_scan		= satConfig[i++];
		satconfig.use_usals		= satConfig[i++];
		satconfig.configured		= satConfig[i++];
		return true;
	}
	return false;
}

bool CFEManager::loadSettings()
{
	config_exist = true;
	configfile.clear();
	if (!configfile.loadConfig(FECONFIGFILE)) {
		WARN("%s not found", FECONFIGFILE);
		config_exist = false;
	}

	int def_mode0 = CFrontend::FE_MODE_INDEPENDENT;
	int def_modeX = CFrontend::FE_MODE_UNUSED;
	if (cableOnly())
		def_modeX = CFrontend::FE_MODE_INDEPENDENT;

	int newmode = (fe_mode_t) configfile.getInt32("mode", -1);
	if (newmode >= 0) {
		INFO("old mode param: %d\n", newmode);
		if (newmode == FE_MODE_LOOP) {
			def_mode0 = CFrontend::FE_MODE_MASTER;
			def_modeX = CFrontend::FE_MODE_LINK_LOOP;
		} else if (newmode == FE_MODE_TWIN) {
			def_mode0 = CFrontend::FE_MODE_MASTER;
			def_modeX = CFrontend::FE_MODE_LINK_TWIN;
		} else if (newmode == FE_MODE_ALONE) {
			def_mode0 = CFrontend::FE_MODE_INDEPENDENT;
			def_modeX = CFrontend::FE_MODE_INDEPENDENT;
		}
	}
	bool fsat = true;
	//bool fcable = true;
	bool fterr = true;
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * fe = it->second;
		frontend_config_t & fe_config = fe->getConfig();
		INFO("load config for fe%d", fe->fenumber);

		fe_config.diseqcType		= (diseqc_t) getConfigValue(fe, "diseqcType", NO_DISEQC);
		fe_config.diseqcRepeats		= getConfigValue(fe, "diseqcRepeats", 0);
		fe_config.motorRotationSpeed	= getConfigValue(fe, "motorRotationSpeed", 18);
		fe_config.highVoltage		= getConfigValue(fe, "highVoltage", 0);
		fe_config.uni_scr		= getConfigValue(fe, "uni_scr", 0);
		fe_config.uni_qrg		= getConfigValue(fe, "uni_qrg", 0);
		fe_config.uni_pin		= getConfigValue(fe, "uni_pin", -1);
		fe_config.diseqc_order		= getConfigValue(fe, "diseqc_order", UNCOMMITED_FIRST);
		fe_config.use_usals		= getConfigValue(fe, "use_usals", 0);
		fe_config.rotor_swap		= getConfigValue(fe, "rotor_swap", 0);

		fe->setRotorSatellitePosition(getConfigValue(fe, "lastSatellitePosition", 0));

		/* default mode for first / next frontends */
		int def_mode = def_modeX;

		if (fe->hasSat() && fsat) {
			fsat = false;
			def_mode = def_mode0;
		}

		if (fe->hasCable()) {
#if 0
			if (fcable) {
				fcable = false;
				def_mode = def_mode0;
			}
			if (def_mode > CFrontend::FE_MODE_INDEPENDENT)
				def_mode = CFrontend::FE_MODE_INDEPENDENT;
#endif
			def_mode = CFrontend::FE_MODE_INDEPENDENT;
		}

		if (fe->hasTerr()) {
			if (fterr) {
				fterr = false;
				def_mode = def_mode0;
			}
			if (def_mode > CFrontend::FE_MODE_INDEPENDENT)
				def_mode = CFrontend::FE_MODE_INDEPENDENT;
		}
		if (femap.size() == 1)
			def_mode = CFrontend::FE_MODE_INDEPENDENT;

		fe->setMode(getConfigValue(fe, "mode", def_mode));
		fe->setMaster(getConfigValue(fe, "master", 0));

		char cfg_key[81];
		sprintf(cfg_key, "fe%d_satellites", fe->fenumber);
		satellite_map_t & satmap = fe->getSatellites();
		satmap.clear();

		satellite_map_t &satlist = CServiceManager::getInstance()->SatelliteList();
		for(sat_iterator_t sit = satlist.begin(); sit != satlist.end(); ++sit)
		{
			if (!fe->supportsDelivery(sit->second.delsys))
				continue;

			t_satellite_position position = sit->first;
			sat_config_t satconfig;
			/* defaults, to replace CServiceManager::InitSatPosition/LoadMotorPositions
			 * in the future */
			satconfig.position = position;
			satconfig.diseqc = -1;
			satconfig.commited = -1;
			satconfig.uncommited = -1;
			satconfig.motor_position = 0;
			satconfig.diseqc_order = 0;
			satconfig.lnbOffsetLow = 9750;
			satconfig.lnbOffsetHigh = 10600;
			satconfig.lnbSwitch = 11700;
			satconfig.use_in_scan = 0;
			satconfig.use_usals = 0;
			satconfig.input = 0;
			satconfig.configured = 0;
			satconfig.delsys = UNKNOWN_DS;

			satmap.insert(satellite_pair_t(position, satconfig));

			if(getSatelliteConfig(fe, satconfig))
				satmap[position] = satconfig; // overwrite if exist
			if (satconfig.use_in_scan)
				sit->second.use_in_scan = satconfig.use_in_scan;

		}
	}
	linkFrontends();
	return true;
}

void CFEManager::saveSettings(bool write)
{
	configfile.clear();
	satellite_map_t &satlist = CServiceManager::getInstance()->SatelliteList();
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * fe = it->second;
		frontend_config_t & fe_config = fe->getConfig();

		INFO("fe%d", fe->fenumber);

		setConfigValue(fe, "diseqcType", fe_config.diseqcType);
		setConfigValue(fe, "diseqcRepeats", fe_config.diseqcRepeats);
		setConfigValue(fe, "motorRotationSpeed", fe_config.motorRotationSpeed);
		setConfigValue(fe, "highVoltage", fe_config.highVoltage);
		setConfigValue(fe, "uni_scr", fe_config.uni_scr);
		setConfigValue(fe, "uni_qrg", fe_config.uni_qrg);
		setConfigValue(fe, "uni_pin", fe_config.uni_pin);
		setConfigValue(fe, "diseqc_order", fe_config.diseqc_order);
		setConfigValue(fe, "use_usals", fe_config.use_usals);
		setConfigValue(fe, "rotor_swap", fe_config.rotor_swap);
		setConfigValue(fe, "lastSatellitePosition", fe->getRotorSatellitePosition());
		setConfigValue(fe, "mode", fe->getMode());
		setConfigValue(fe, "master", fe->getMaster());

		std::vector<int> satList;
		satellite_map_t satellites = fe->getSatellites();
		for(sat_iterator_t sit = satellites.begin(); sit != satellites.end(); ++sit) {
			if (sit->second.configured) {
				sat_iterator_t tit = satlist.find(sit->first);
				if (tit != satlist.end())
					sit->second.use_in_scan = tit->second.use_in_scan;
				satList.push_back(sit->first);
				setSatelliteConfig(fe, sit->second);
			}
		}
		char cfg_key[81];
		sprintf(cfg_key, "fe%d_satellites", fe->fenumber);
		configfile.setInt32Vector(cfg_key, satList);
	}
	if (write && configfile.getModifiedFlag()) {
		config_exist = configfile.saveConfig(FECONFIGFILE);
		//configfile.setModifiedFlag(false);
	}
}

void CFEManager::copySettings(CFrontend * from, CFrontend * to)
{
	INFO("Copy settings fe %d -> fe %d", from->fenumber, to->fenumber);
	if (to->config.diseqcType != DISEQC_UNICABLE || to->config.diseqcType != DISEQC_UNICABLE2 || to->getMode() == CFrontend::FE_MODE_LINK_LOOP)
		to->config.diseqcType = from->config.diseqcType;

	to->config.diseqcRepeats = from->config.diseqcRepeats;
	to->config.motorRotationSpeed = from->config.motorRotationSpeed;
	to->config.highVoltage = from->config.highVoltage;
	to->setSatellites(from->getSatellites());
}

void CFEManager::copySettings(CFrontend * fe)
{
	if (CFrontend::linked(fe->getMode())) {
		for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
			if (it->second->fenumber == fe->getMaster()) {
				copySettings(it->second, fe);
				break;
			}
		}
	} else if (fe->getMode() == CFrontend::FE_MODE_MASTER) {
		for (unsigned int i = 1; i < fe->linkmap.size(); i++) {
			CFrontend * lfe = fe->linkmap[i];
			copySettings(fe, lfe);
		}
	}
}

void CFEManager::linkFrontends(bool init)
{
	INFO("linking..");
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	enabled_count = 0;
	unused_demux = 0;
	int demuxes[MAX_DMX_UNITS];
	for(unsigned i = 0; i < MAX_DMX_UNITS; i++)
		demuxes[i] = 0;
	demuxes[0] = 1;
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * fe = it->second;
		int femode = fe->getMode();
		fe->slave = false;
		fe->have_loop = false;
		fe->have_rotor = false;
		fe->linkmap.clear();
		if (femode == CFrontend::FE_MODE_MASTER) {
			fe->linkmap.push_back(fe);
			/* fe is master, find all linked */
			for(fe_map_iterator_t it2 = femap.begin(); it2 != femap.end(); it2++) {
				CFrontend * fe2 = it2->second;
				if (!CFrontend::linked(fe2->getMode()))
					continue;
				if (fe2->getType() != fe->getType() || (fe2->getMaster() != fe->fenumber))
					continue;

				fe->linkmap.push_back(fe2);
				if (fe2->getMode() == CFrontend::FE_MODE_LINK_LOOP) {
					INFO("Frontend #%d: link to master %d as LOOP", fe2->fenumber, fe->fenumber);
					fe->have_loop = true;
				} else {
					INFO("Frontend #%d: link to master %d as TWIN", fe2->fenumber, fe->fenumber);
				}
			}
			frontend_config_t & fe_config = fe->getConfig();
			satellite_map_t &satellites = fe->getSatellites();
			for(sat_iterator_t sit = satellites.begin(); sit != satellites.end(); ++sit) {
				if (fe_config.use_usals || (sit->second.configured && (sit->second.motor_position || sit->second.use_usals))) {
					fe->have_rotor = true;
					break;
				}
			}
			INFO("Frontend #%d: is master, with loop: %s, with rotor: %s", fe->fenumber,
					fe->have_loop ? "yes" : "no", fe->have_rotor ? "yes" : "no");
		} else if (femode == CFrontend::FE_MODE_LINK_LOOP) {
			INFO("Frontend #%d: is LOOP, master %d", fe->fenumber, fe->getMaster());
			if (init)
				fe->setMasterSlave(true);
			//fe->slave = true;
		} else if (femode == CFrontend::FE_MODE_LINK_TWIN) {
			INFO("Frontend #%d: is TWIN, master %d", fe->fenumber, fe->getMaster());
		} else if (femode == CFrontend::FE_MODE_INDEPENDENT) {
			INFO("Frontend #%d: is independent", fe->fenumber);
		}
		if (init && femode != CFrontend::FE_MODE_UNUSED)
			fe->Init();
		if (femode != CFrontend::FE_MODE_UNUSED)
		{
			enabled_count++;
			if ((fe->fenumber + 1) < (int) MAX_DMX_UNITS)
				demuxes[fe->fenumber + 1] = 1;
		}
		else {	/* unused -> no need to keep open */
			fe->Close();
		}
	}
	for(unsigned i = 0; i < MAX_DMX_UNITS; i++) {
		if (demuxes[i] == 0) {
			unused_demux = i;
			INFO("pip demux: %d", unused_demux);
			break;
		}
	}
}

void CFEManager::Open()
{
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * fe = it->second;
		if (!fe->Locked() && fe->getMode() != CFrontend::FE_MODE_UNUSED)
			fe->Open(true);
	}
}

void CFEManager::Close()
{
	if(have_locked)
		return;

	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * fe = it->second;
		if(!fe->Locked())
			fe->Close();
	}
}

CFrontend * CFEManager::getFE(int index)
{
	int i = 0;
	/* the naive approach of just returning "femap[index]" does not work, since
	 * the first frontend (index = 0) does not necessary live on adapter0/frontend0 */
	for (fe_map_iterator_t it = femap.begin(); it != femap.end(); it++)
	{
		if (index == i)
			return it->second;
		i++;
	}
	INFO("Frontend #%d not found", index);
	return NULL;
}

/* compare polarization and band with fe values */
bool CFEManager::loopCanTune(CFrontend * fe, CZapitChannel * channel)
{
	if(!fe->hasSat())
		return true;

	if(fe->tuned && (fe->getCurrentSatellitePosition() != channel->getSatellitePosition()))
		return false;

	bool tp_band = ((int)channel->getFreqId()*1000 >= fe->lnbSwitch);
	uint8_t tp_pol = channel->polarization & 1;
	uint8_t fe_pol = fe->getPolarization() & 1;

	DBG("Check fe%d: locked %d pol:band %d:%d vs %d:%d (%d:%d)", fe->fenumber, fe->Locked(), fe_pol, fe->getHighBand(), tp_pol, tp_band, fe->getFrequency(), channel->getFreqId()*1000);
	if(!fe->tuned || (fe_pol == tp_pol && fe->getHighBand() == tp_band))
		return true;
	return false;
}

CFrontend * CFEManager::getFrontend(CZapitChannel * channel)
{
	CFrontend * retfe = NULL;

	if (livefe && livefe->tuned && livefe->sameTsidOnid(channel->getTransponderId())) {
		FEDEBUG("live fe %d on the same TP", livefe->fenumber);
		return livefe;
	}

	t_satellite_position satellitePosition = channel->getSatellitePosition();
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * mfe = it->second;

		if (!mfe->supportsDelivery(channel->delsys))
			continue;
		if (mfe->getMode() == CFrontend::FE_MODE_UNUSED || CFrontend::linked(mfe->getMode()))
			continue;

		if (mfe->hasSat()) {
			satellite_map_t & satmap = mfe->getSatellites();
			sat_iterator_t sit = satmap.find(satellitePosition);
			if ((sit == satmap.end()) || !sit->second.configured)
				continue;
		}

		if (mfe->getMode() == CFrontend::FE_MODE_MASTER) {
			bool have_loop = mfe->have_loop;
			CFrontend * free_frontend = NULL;
			CFrontend * free_twin = NULL;
			bool loop_busy = false;
			for (unsigned int i = 0; i < mfe->linkmap.size(); i++) {
				CFrontend * fe = mfe->linkmap[i];
				FEDEBUG("Check fe%d: mode %d locked %d freq %d TP %" PRIx64 " - channel freq %d TP %" PRIx64, fe->fenumber, fe->getMode(),
						fe->Locked(), fe->getFrequency(), fe->getTsidOnid(), channel->getFreqId(), channel->getTransponderId());

				if(fe->Locked()) {
					if (mfe->have_rotor && (fe->getCurrentSatellitePosition() != satellitePosition)) {
						free_frontend = NULL;
						free_twin = NULL;
						break;
					}
					if (fe->tuned && fe->sameTsidOnid(channel->getTransponderId())) {
						FEDEBUG("fe %d on the same TP", fe->fenumber);
						return fe;
					}
					if (!loop_busy && fe->getMode() != CFrontend::FE_MODE_LINK_TWIN) {
						if (have_loop && !loopCanTune(fe, channel)) {
							free_frontend = NULL;
							loop_busy = true;
						}
					}
				} else {
					if (fe->getMode() == CFrontend::FE_MODE_LINK_TWIN) {
						if (!free_twin)
							free_twin = fe;
					} else if(!loop_busy && !free_frontend) {
						free_frontend = fe;
					}
				}
			}
			if (!free_frontend)
				free_frontend = free_twin;
			if (free_frontend && !retfe)
				retfe = free_frontend;
		}
		if (mfe->getMode() == CFrontend::FE_MODE_INDEPENDENT) {
			FEDEBUG("Check fe%d: mode %d locked %d freq %d TP %" PRIx64 " - channel freq %d TP %" PRIx64, mfe->fenumber, mfe->getMode(),
					mfe->Locked(), mfe->getFrequency(), mfe->getTsidOnid(), channel->getFreqId(), channel->getTransponderId());
			if(mfe->Locked()) {
				if(mfe->tuned && mfe->sameTsidOnid(channel->getTransponderId())) {
					FEDEBUG("fe %d on the same TP", mfe->fenumber);
					return mfe;
				}
			} else if(!retfe)
				retfe = mfe;
		}
	}
	FEDEBUG("Selected fe: %d", retfe ? retfe->fenumber : -1);
	return retfe;
}

#ifdef DYNAMIC_DEMUX
int CFEManager::getDemux(transponder_id_t id)
{
	for (unsigned int i = 1; i < dmap.size(); i++) {
		if((dmap[i].usecount == 0) || dmap[i].tpid == id)
			return i;
	}
	return 0;
}

bool CFEManager::lockDemux(int i, transponder_id_t id)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	if ((unsigned int) i < dmap.size()) {
		if ((dmap[i].usecount == 0) || (dmap[i].tpid == id)) {
			dmap[i].Lock(id);
			return true;
		}
		INFO("faled to lock demux %d (on tp %llx, new %llx)", i, dmap[i].tpid, id);
	}
	return false;
}

void CFEManager::unlockDemux(int i)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	if ((unsigned int) i < dmap.size())
		dmap[i].Unlock();
}

bool CFEManager::haveFreeDemux()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	for (unsigned int i = 1; i < dmap.size(); i++) {
		if (dmap[i].usecount == 0)
			return true;
	}
	return false;
}
#endif // DYNAMIC_DEMUX

CFrontend * CFEManager::allocateFE(CZapitChannel * channel, bool forrecord)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);

	if (forrecord)
		fedebug = 1;
	CFrontend * frontend = getFrontend(channel);
	if (frontend) {
#ifdef DYNAMIC_DEMUX
		int dnum = getDemux(channel->getTransponderId());
		INFO("record demux: %d", dnum);
		channel->setRecordDemux(dnum);
		if (forrecord && !dnum) {
			frontend = NULL;
		} else {
			cDemux::SetSource(dnum, frontend->fenumber);
		}
#else
		channel->setRecordDemux(frontend->fenumber+1);
		channel->setPipDemux(frontend->fenumber+1);
#if HAVE_COOL_HARDWARE
		/* I don't know if this check is necessary on cs, but it hurts on other hardware */
		if(femap.size() > 1)
#endif
			cDemux::SetSource(frontend->fenumber+1, frontend->fenumber);
#ifdef ENABLE_PIP
		/* FIXME until proper demux management */
		if (unused_demux)
			channel->setPipDemux(unused_demux);
		INFO("pip demux: %d", channel->getPipDemux());
#endif
#endif

	}
	return frontend;
}

void CFEManager::setLiveFE(CFrontend * fe)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	livefe = fe; 
#if HAVE_COOL_HARDWARE
	if(femap.size() > 1)
#endif
		cDemux::SetSource(0, livefe->fenumber);
}

bool CFEManager::canTune(CZapitChannel * channel)
{
	fedebug = 0;
	CFrontend * fe = getFrontend(channel);
	return (fe != NULL);
}

CFrontend * CFEManager::getScanFrontend(t_satellite_position satellitePosition)
{
	CFrontend * frontend = NULL;
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * mfe = it->second;
		if (mfe->hasCable() && SAT_POSITION_CABLE(satellitePosition)) {
			if (mfe->getMode() != CFrontend::FE_MODE_UNUSED) {
				frontend = mfe;
				break;
			}
		} else if (mfe->hasTerr() && SAT_POSITION_TERR(satellitePosition)) {
			if (mfe->getMode() != CFrontend::FE_MODE_UNUSED) {
				frontend = mfe;
				break;
			}
		} else if (mfe->hasSat()) {
			if (mfe->getMode() == CFrontend::FE_MODE_UNUSED || CFrontend::linked(mfe->getMode()))
				continue;
			satellite_map_t & satmap = mfe->getSatellites();
			sat_iterator_t sit = satmap.find(satellitePosition);
			if ((sit != satmap.end()) && sit->second.configured) {
				frontend = mfe;
				break;
			}
		}
	}
	INFO("Selected fe: %d", frontend ? frontend->fenumber : -1);
	return frontend;
}

bool CFEManager::lockFrontend(CFrontend * frontend, CZapitChannel * channel)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	frontend->Lock();
	have_locked = true;

	if (channel) {
#ifdef DYNAMIC_DEMUX
		int di = channel->getRecordDemux();
		if ((unsigned int) di < dmap.size())
			dmap[di].Lock(channel->getTransponderId());
#endif
	}
	return true;
}

bool CFEManager::unlockFrontend(CFrontend * frontend, bool unlock_demux)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	have_locked = false;
	frontend->Unlock();
	if (unlock_demux) {
#ifdef DYNAMIC_DEMUX
		for (unsigned int i = 1; i < dmap.size(); i++) {
			if(dmap[i].tpid == frontend->getTsidOnid()) {
				dmap[i].Unlock();
				break;
			}
		}
#endif
	}
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * fe = it->second;
		if(fe->Locked()) {
			have_locked = true;
			break;
		}
	}
	return true;
}

// FIXME
bool CFEManager::haveFreeFrontend()
{
	if(have_locked) {
		CFrontend * fe = getFE(0);
		if((mode == FE_MODE_TWIN) || (fe->getInfo()->type != FE_QPSK)) {
			for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
				fe = it->second;
				INFO("check %d: locked %d", fe->fenumber, fe->Locked());
				if(!fe->Locked())
					return true;
			}
		}
		return false;
	}
	return true;
}

int CFEManager::getEnabledCount()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	return enabled_count;
}
