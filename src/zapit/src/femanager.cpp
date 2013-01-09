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
#include <dmx_cs.h>
#include <OpenThreads/ScopedLock>

static int fedebug = 0;
#define FEDEBUG(fmt, args...)					\
        do {							\
                if (fedebug)					\
                        fprintf(stdout, "[%s:%s:%d] " fmt "\n",	\
                                __FILE__, __FUNCTION__,		\
                                __LINE__ , ## args);		\
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
	INFO("[dmx%d] lock, usecount %d tp %llx", num, usecount, tpid);
}

void CFeDmx::Unlock()
{
	if(usecount > 0)
		usecount--;
	else
		tpid = 0;
	INFO("[dmx%d] unlock, usecount %d tp %llx", num, usecount, tpid);
}

CFEManager * CFEManager::manager = NULL;

CFEManager::CFEManager() : configfile(',', true)
{
	livefe = NULL;
	mode = FE_MODE_SINGLE;
	config_exist = false;
	have_locked = false;
}

bool CFEManager::Init()
{
	CFrontend * fe;
	unsigned short fekey;

	for(int i = 0; i < MAX_ADAPTERS; i++) {
		for(int j = 0; j < MAX_FE; j++) {
			fe = new CFrontend(j, i);
			if(fe->Open()) {
				fekey = MAKE_FE_KEY(i, j);
				femap.insert(std::pair <unsigned short, CFrontend*> (fekey, fe));
				INFO("add fe %d", fe->fenumber);
				if(livefe == NULL)
					livefe = fe;
			} else
				delete fe;
		}
	}
	for (int i = 0; i < MAX_DMX_UNITS; i++)
		dmap.push_back(CFeDmx(i));

	INFO("found %d frontends, %d demuxes\n", femap.size(), dmap.size());
	if( femap.empty() )
		return false;

	return true;
}

CFEManager::~CFEManager()
{
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++)
		delete it->second;
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
		//return false;
	}

#if 0
	fe_mode_t newmode = (fe_mode_t) configfile.getInt32("mode", (int) FE_MODE_SINGLE);
#endif
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * fe = it->second;
		frontend_config_t & fe_config = fe->getConfig();
		INFO("load config for fe%d", fe->fenumber);

		fe_config.diseqcType		= (diseqc_t) getConfigValue(fe, "diseqcType", NO_DISEQC);
		fe_config.diseqcRepeats		= getConfigValue(fe, "diseqcRepeats", 0);
		fe_config.motorRotationSpeed	= getConfigValue(fe, "motorRotationSpeed", 18);
		fe_config.highVoltage		= getConfigValue(fe, "highVoltage", 0);
		fe_config.uni_scr		= getConfigValue(fe, "uni_scr", -1);
		fe_config.uni_qrg		= getConfigValue(fe, "uni_qrg", 0);

		fe->setRotorSatellitePosition(getConfigValue(fe, "lastSatellitePosition", 0));
		int def_mode = fe->fenumber ? CFrontend::FE_MODE_UNUSED : CFrontend::FE_MODE_INDEPENDENT;
		fe->setMode(getConfigValue(fe, "mode", def_mode));
		fe->setMaster(getConfigValue(fe, "master", 0));

		char cfg_key[81];
		sprintf(cfg_key, "fe%d_satellites", fe->fenumber);
		satellite_map_t & satmap = fe->getSatellites();
		satmap.clear();

		satellite_map_t satlist = CServiceManager::getInstance()->SatelliteList();
		for(sat_iterator_t sit = satlist.begin(); sit != satlist.end(); ++sit)
		{
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

			satmap.insert(satellite_pair_t(position, satconfig));

			if(getSatelliteConfig(fe, satconfig))
				satmap[position] = satconfig; // overwrite if exist

		}
	}
	//setMode(newmode, true);
	//FIXME backward compatible settings for mode ?
	linkFrontends();
	return true;
}

void CFEManager::saveSettings(bool write)
{
	configfile.setInt32("mode", (int) mode);
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * fe = it->second;
		frontend_config_t & fe_config = fe->getConfig();

		INFO("fe%d", fe->fenumber);
#if 0
		if(fe->fenumber && mode != FE_MODE_ALONE) {
			CFrontend * fe0 = getFE(0);
			fe->setSatellites(fe0->getSatellites());
			//fe->setConfig(fe0->getConfig());
			fe->config.diseqcType = fe0->config.diseqcType;
			fe->config.diseqcRepeats = fe0->config.diseqcRepeats;
			fe->config.motorRotationSpeed = fe0->config.motorRotationSpeed;
			fe->config.highVoltage = fe0->config.highVoltage;
		}
#endif

		setConfigValue(fe, "diseqcType", fe_config.diseqcType);
		setConfigValue(fe, "diseqcRepeats", fe_config.diseqcRepeats);
		setConfigValue(fe, "motorRotationSpeed", fe_config.motorRotationSpeed);
		setConfigValue(fe, "highVoltage", fe_config.highVoltage);
		setConfigValue(fe, "uni_scr", fe_config.uni_scr);
		setConfigValue(fe, "uni_qrg", fe_config.uni_qrg);
		setConfigValue(fe, "lastSatellitePosition", fe->getRotorSatellitePosition());
		setConfigValue(fe, "mode", fe->getMode());
		setConfigValue(fe, "master", fe->getMaster());

		std::vector<int> satList;
		satellite_map_t satellites = fe->getSatellites();
		for(sat_iterator_t sit = satellites.begin(); sit != satellites.end(); ++sit) {
			satList.push_back(sit->first);
			setSatelliteConfig(fe, sit->second);
		}
		char cfg_key[81];
		sprintf(cfg_key, "fe%d_satellites", fe->fenumber);
		configfile.setInt32Vector(cfg_key, satList);
	}
	//setInt32Vector dont set modified flag !
	if (write /*&& configfile.getModifiedFlag()*/) {
		config_exist = configfile.saveConfig(FECONFIGFILE);
		//configfile.setModifiedFlag(false);
	}
}

void CFEManager::copySettings(CFrontend * from, CFrontend * to)
{
	INFO("Copy settings fe %d -> fe %d", from->fenumber, to->fenumber);
	to->config.diseqcType = from->config.diseqcType;
	to->config.diseqcRepeats = from->config.diseqcRepeats;
	to->config.motorRotationSpeed = from->config.motorRotationSpeed;
	to->config.highVoltage = from->config.highVoltage;
	to->setSatellites(from->getSatellites());
}

void CFEManager::copySettings(CFrontend * fe)
{
	//FIXME copy on master settings change too
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
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * fe = it->second;
		if (fe->getInfo()->type != FE_QPSK)
			fe->setMode(CFrontend::FE_MODE_INDEPENDENT);

		int femode = fe->getMode();
		fe->slave = false;
		fe->linkmap.clear();
		if (femode == CFrontend::FE_MODE_MASTER) {
			INFO("Frontend #%d: is master", fe->fenumber);
			fe->linkmap.push_back(fe);
			/* fe is master, find all linked */
			for(fe_map_iterator_t it2 = femap.begin(); it2 != femap.end(); it2++) {
				CFrontend * fe2 = it2->second;
				if (!CFrontend::linked(fe2->getMode()))
					continue;
				if (fe2->getMaster() != fe->fenumber)
					continue;
#if 0
				int mnum = fe2->getMaster();
				if (mnum != fe->fenumber)
					continue;
				CFrontend * mfe = getFE(mnum);
				if (!mfe) {
					INFO("Frontend %d: master %d not found", fe->fenumber, mnum);
					continue;
				}

				mfe->linkmap.push_back(fe2);
#endif
				fe->linkmap.push_back(fe2);
				if (fe2->getMode() == CFrontend::FE_MODE_LINK_LOOP) {
					INFO("Frontend #%d: link to master %d as LOOP", fe2->fenumber, fe->fenumber);
				} else {
					INFO("Frontend #%d: link to master %d as TWIN", fe2->fenumber, fe->fenumber);
				}
				
			}
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
	}
}

#if 0
void CFEManager::setMode(fe_mode_t newmode, bool initial)
{
	if(!initial && (newmode == mode))
		return;

	mode = newmode;
	if(femap.size() == 1)
		mode = FE_MODE_SINGLE;

	bool setslave = (mode == FE_MODE_LOOP) || (mode == FE_MODE_SINGLE);
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * fe = it->second;
		if(it != femap.begin()) {
			INFO("Frontend %d as slave: %s", fe->fenumber, setslave ? "yes" : "no");
			fe->setMasterSlave(setslave);
		} else
			fe->Init();
	}
}
#endif
void CFEManager::Open()
{
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * fe = it->second;
		if(!fe->Locked())
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
	if((unsigned int) index < femap.size())
		return femap[index];
	INFO("Frontend #%d not found", index);
	return NULL;
}

/* compare polarization and band with fe values */
bool CFEManager::loopCanTune(CFrontend * fe, CZapitChannel * channel)
{
	if(fe->getInfo()->type != FE_QPSK)
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
	CFrontend * free_frontend = NULL;
	CFrontend * same_tid_fe = NULL;

	if (livefe && livefe->sameTsidOnid(channel->getTransponderId()))
		return livefe;

	t_satellite_position satellitePosition = channel->getSatellitePosition();
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * mfe = it->second;

		if (mfe->getMode() == CFrontend::FE_MODE_UNUSED || CFrontend::linked(mfe->getMode()))
			continue;

		if (mfe->getInfo()->type == FE_QPSK) {
			satellite_map_t & satmap = mfe->getSatellites();
			sat_iterator_t sit = satmap.find(satellitePosition);
			if ((sit == satmap.end()) || !sit->second.configured)
				continue;
		}

		if (mfe->getMode() == CFrontend::FE_MODE_MASTER) {
			bool have_loop = false;
			for (unsigned int i = 0; i < mfe->linkmap.size(); i++) {
				CFrontend * fe = mfe->linkmap[i];
				if (fe->getMode() == CFrontend::FE_MODE_LINK_LOOP) {
					have_loop = true;
					break;
				}
			}
			CFrontend * free_twin = NULL;
			bool loop_busy = false;
			for (unsigned int i = 0; i < mfe->linkmap.size(); i++) {
				CFrontend * fe = mfe->linkmap[i];
				FEDEBUG("Check fe%d: mode %d locked %d freq %d TP %llx - channel freq %d TP %llx", fe->fenumber, fe->getMode(),
						fe->Locked(), fe->getFrequency(), fe->getTsidOnid(), channel->getFreqId(), channel->getTransponderId());

				if(fe->Locked()) {
					if ((fe->getCurrentSatellitePosition() != satellitePosition)) {
						free_frontend = NULL;
						free_twin = NULL;
						break;
					}
					if (fe->tuned && fe->sameTsidOnid(channel->getTransponderId())) {
						FEDEBUG("fe %d on the same TP", fe->fenumber);
						return same_tid_fe;
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
		}
		if (mfe->getMode() == CFrontend::FE_MODE_INDEPENDENT) {
			FEDEBUG("Check fe%d: mode %d locked %d freq %d TP %llx - channel freq %d TP %llx", mfe->fenumber, mfe->getMode(),
					mfe->Locked(), mfe->getFrequency(), mfe->getTsidOnid(), channel->getFreqId(), channel->getTransponderId());
			if(mfe->Locked()) {
				if(mfe->tuned && mfe->sameTsidOnid(channel->getTransponderId())) {
					FEDEBUG("fe %d on the same TP", mfe->fenumber);
					return same_tid_fe;
				}
			} else if(!free_frontend)
				free_frontend = mfe;
		}
	}
	CFrontend * ret = same_tid_fe ? same_tid_fe : free_frontend;
	FEDEBUG("Selected fe: %d", ret ? ret->fenumber : -1);
	return ret;
}

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

CFrontend * CFEManager::allocateFE(CZapitChannel * channel, bool forrecord)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);

	fedebug = 1;
	CFrontend * frontend = getFrontend(channel);
	if (frontend) {
		int dnum = getDemux(channel->getTransponderId());
		INFO("record demux: %d", dnum);
		channel->setRecordDemux(dnum);
		if (forrecord && !dnum) {
			frontend = NULL;
		} else {
			cDemux::SetSource(dnum, frontend->fenumber);
		}
	}
	return frontend;
}

void CFEManager::setLiveFE(CFrontend * fe)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	livefe = fe; 
	if(femap.size() > 1)
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
		satellite_map_t & satmap = mfe->getSatellites();
		sat_iterator_t sit = satmap.find(satellitePosition);
		if ((sit != satmap.end()) && sit->second.configured) {
			frontend = mfe;
			break;
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
		int di = channel->getRecordDemux();
		if ((unsigned int) di < dmap.size())
			dmap[di].Lock(channel->getTransponderId());
	}
	return true;
}

bool CFEManager::unlockFrontend(CFrontend * frontend, bool unlock_demux)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	have_locked = false;
	frontend->Unlock();
	if (unlock_demux) {
		for (unsigned int i = 1; i < dmap.size(); i++) {
			if(dmap[i].tpid == frontend->getTsidOnid()) {
				dmap[i].Unlock();
				break;
			}
		}
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
