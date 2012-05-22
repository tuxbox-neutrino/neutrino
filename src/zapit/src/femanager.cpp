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
#include <zapit/cam.h>
#include <zapit/channel.h>
#include <zapit/getservices.h>
#include <zapit/zapit.h>
#include <zapit/client/zapittools.h>
#include <zapit/femanager.h>
#include <dmx_cs.h>

extern transponder_list_t transponders;

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
	INFO("found %d frontends\n", femap.size());
	if(femap.size() == 0)
		return false;
#if 0
	if(femap.size() == 1)
		mode = FE_MODE_SINGLE;
#endif
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

	fe_mode_t newmode = (fe_mode_t) configfile.getInt32("mode", (int) FE_MODE_SINGLE);
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * fe = it->second;
		frontend_config_t & fe_config = fe->getConfig();
		INFO("load config for fe%d", fe->fenumber);

		//fe_config.diseqcType		= (diseqc_t) getConfigValue(fe, "diseqcType", NO_DISEQC);
		diseqc_t diseqcType		= (diseqc_t) getConfigValue(fe, "diseqcType", NO_DISEQC);
		fe_config.diseqcRepeats		= getConfigValue(fe, "diseqcRepeats", 0);
		fe_config.motorRotationSpeed	= getConfigValue(fe, "motorRotationSpeed", 18);
		fe_config.highVoltage		= getConfigValue(fe, "highVoltage", 0);
		fe_config.uni_scr		= getConfigValue(fe, "uni_scr", -1);
		fe_config.uni_qrg		= getConfigValue(fe, "uni_qrg", 0);

		fe->setRotorSatellitePosition(getConfigValue(fe, "lastSatellitePosition", 0));

		//fe->setDiseqcType((diseqc_t) fe_config.diseqcType);
		fe->setDiseqcType(diseqcType);

		char cfg_key[81];
		sprintf(cfg_key, "fe%d_satellites", fe->fenumber);
		std::vector<int> satList = configfile.getInt32Vector(cfg_key);
		satellite_map_t & satmap = fe->getSatellites();
		satmap.clear();
#if 0
		for(unsigned int i = 0; i < satList.size(); i++) 
			t_satellite_position position = satList[i];
#endif
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
	setMode(newmode, true);
	return true;
}

void CFEManager::saveSettings(bool write)
{
	configfile.setInt32("mode", (int) mode);
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * fe = it->second;
		frontend_config_t & fe_config = fe->getConfig();

		INFO("fe%d", fe->fenumber);
		if(fe->fenumber && mode != FE_MODE_ALONE) {
			CFrontend * fe0 = getFE(0);
			fe->setConfig(fe0->getConfig());
			fe->setSatellites(fe0->getSatellites());
		}

		setConfigValue(fe, "diseqcType", fe_config.diseqcType);
		setConfigValue(fe, "diseqcRepeats", fe_config.diseqcRepeats);
		setConfigValue(fe, "motorRotationSpeed", fe_config.motorRotationSpeed);
		setConfigValue(fe, "highVoltage", fe_config.highVoltage);
		setConfigValue(fe, "uni_scr", fe_config.uni_scr);
		setConfigValue(fe, "uni_qrg", fe_config.uni_qrg);
		setConfigValue(fe, "lastSatellitePosition", fe->getRotorSatellitePosition());

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

void CFEManager::setMode(fe_mode_t newmode, bool initial)
{
	if(newmode == mode)
		return;

	if(femap.size() == 1) {
		mode = FE_MODE_SINGLE;
		return;
	}
	mode = newmode;
	bool setslave = (mode == FE_MODE_LOOP);
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		if(it != femap.begin()) {
			CFrontend * fe = it->second;
			INFO("Frontend %d as slave: %s", fe->fenumber, setslave ? "yes" : "no");
			fe->setMasterSlave(setslave);
		}
	}
	if(setslave && !initial) {
		CFrontend * fe = getFE(0);
		fe->Close();
		fe->Open();
	}
}

void CFEManager::Open()
{
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * fe = it->second;
		if(!fe->Locked())
			fe->Open();
	}
}

void CFEManager::Close()
{
	if(have_locked && (mode == FE_MODE_LOOP))
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

transponder * CFEManager::getChannelTransponder(CZapitChannel * channel)
{
	transponder_list_t::iterator tpI = transponders.find(channel->getTransponderId());
	if(tpI != transponders.end())
		return &tpI->second;

	INFO("Transponder %llx not found", channel->getTransponderId());
	return NULL;
}

/* try to find fe with same tid, or unlocked. fe with same tid is preffered */
CFrontend * CFEManager::findFrontend(CZapitChannel * channel)
{
	CFrontend * same_tid_fe = NULL;
	CFrontend * free_frontend = NULL;
	transponder_id_t channel_tid = channel->getTransponderId();

	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * fe = it->second;
		INFO("Check fe%d: locked %d TP %llx - channel TP %llx", fe->fenumber, fe->Locked(), fe->getTsidOnid(), channel_tid);
		if(fe->tuned && fe->sameTsidOnid(channel->getTransponderId())) {
			same_tid_fe = fe;
			break;
		}
		else if(!fe->Locked() && !free_frontend)
			free_frontend = fe;
	}

	CFrontend * ret = same_tid_fe ? same_tid_fe : free_frontend;
	INFO("Selected fe: %d", ret ? ret->fenumber : -1);
	return ret;
}

/* compare polarization and band with fe values */
bool CFEManager::loopCanTune(CFrontend * fe, CZapitChannel * channel)
{
	if(fe->getInfo()->type != FE_QPSK)
		return true;

	if(fe->tuned && (fe->getCurrentSatellitePosition() != channel->getSatellitePosition()))
		return false;

#if 0
	transponder * tp = getChannelTransponder(channel);
	if(tp == NULL)
		return false;

	bool tp_band = ((int)tp->feparams.frequency >= fe->lnbSwitch);
	uint8_t tp_pol = tp->polarization & 1;
#else
	bool tp_band = ((int)channel->getFreqId()*1000 >= fe->lnbSwitch);
	uint8_t tp_pol = channel->polarization & 1;
#endif
	uint8_t fe_pol = fe->getPolarization() & 1;

	DBG("Check fe%d: locked %d pol:band %d:%d vs %d:%d (%d:%d)", fe->fenumber, fe->Locked(), fe_pol, fe->getHighBand(), tp_pol, tp_band, fe->getFrequency(), channel->getFreqId()*1000);
	if(!fe->tuned || (fe_pol == tp_pol && fe->getHighBand() == tp_band))
		return true;
	return false;
}

CFrontend * CFEManager::getLoopFE(CZapitChannel * channel)
{
	CFrontend * free_frontend = NULL;
	CFrontend * same_tid_fe = NULL;

	/* check is there any locked fe, remember fe with same transponder */
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * fe = it->second;
		INFO("Check fe%d: locked %d freq %d TP %llx - channel freq %d TP %llx", fe->fenumber, fe->Locked(), fe->getFrequency(), fe->getTsidOnid(), channel->getFreqId(), channel->getTransponderId());
#if 0
		if(fe->tuned && fe->sameTsidOnid(channel->getTransponderId())) {
			same_tid_fe = fe; // first with same tp id
			break;
		}
#endif
		if(fe->Locked() /* && !same_tid_fe*/) {
			INFO("fe %d locked", fe->fenumber);
			if(!loopCanTune(fe, channel)) {
				free_frontend = NULL;
				break;
			}
			if(fe->tuned && fe->sameTsidOnid(channel->getTransponderId())) {
				same_tid_fe = fe; // first with same tp id
			}
		} else if(!free_frontend)
			free_frontend = fe; // first unlocked
	}

	CFrontend * ret = same_tid_fe ? same_tid_fe : free_frontend;
	INFO("Selected fe: %d", ret ? ret->fenumber : -1);
	return ret;
}

CFrontend * CFEManager::getIndependentFE(CZapitChannel * channel)
{
	CFrontend * free_frontend = NULL;
	CFrontend * same_tid_fe = NULL;

	t_satellite_position satellitePosition = channel->getSatellitePosition();
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * fe = it->second;
		satellite_map_t & satmap = fe->getSatellites();
		sat_iterator_t sit = satmap.find(satellitePosition);
		bool configured = ((sit != satmap.end()) && sit->second.configured);
		INFO("Check fe%d: locked %d freq %d TP %llx - channel freq %d TP %llx has sat %d: %s",
				fe->fenumber, fe->Locked(), fe->getFrequency(), fe->getTsidOnid(), 
				channel->getFreqId(), channel->getTransponderId(), satellitePosition, configured ? "yes" : "no");
		if(!configured)
			continue;

		if(fe->tuned && fe->sameTsidOnid(channel->getTransponderId())) {
			same_tid_fe = fe;
			break;
		}
		else if(!fe->Locked() && !free_frontend)
			free_frontend = fe;
	}
	CFrontend * ret = same_tid_fe ? same_tid_fe : free_frontend;
	INFO("Selected fe: %d", ret ? ret->fenumber : -1);
	return ret;
}

CFrontend * CFEManager::allocateFE(CZapitChannel * channel)
{
	CFrontend * frontend = NULL, *fe;

	mutex.lock();
	switch(mode) {
		case FE_MODE_SINGLE:
			if((fe = getFE(0))) {
				if(!fe->Locked() || fe->sameTsidOnid(channel->getTransponderId()))
					frontend = fe;
			}
			break;
		case FE_MODE_LOOP:
			frontend = getLoopFE(channel);
			break;
		case FE_MODE_TWIN:
			frontend = findFrontend(channel);
			break;
		case FE_MODE_ALONE:
			frontend = getIndependentFE(channel);
			break;
	}
	//FIXME for testing only
	if(frontend) {
		channel->setRecordDemux(frontend->fenumber+1);
		if(femap.size() > 1)
			cDemux::SetSource(frontend->fenumber+1, frontend->fenumber);
	}
	mutex.unlock();
	return frontend;
}

void CFEManager::setLiveFE(CFrontend * fe)
{
	mutex.lock();
	livefe = fe; 
	if(femap.size() > 1)
		cDemux::SetSource(0, livefe->fenumber);
	mutex.unlock();
}

bool CFEManager::canTune(CZapitChannel * channel)
{
	/* TODO: for faster processing, cache ? FE_MODE_LOOP: pol and band, 
	 * is there unlocked or not, what else ?
	 */
	CFrontend * fe;
	bool ret = false;
#if 0
	if(!have_locked)
		return true;
#endif
	switch(mode) {
		case FE_MODE_SINGLE:
			if((fe = getFE(0))) {
				if(!fe->Locked() || fe->sameTsidOnid(channel->getTransponderId()))
					ret = true;
			}
			break;
		case FE_MODE_LOOP:
#if 1
			for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
				fe = it->second;
				if(fe->tuned && fe->sameTsidOnid(channel->getTransponderId()))
					return true;
				if(fe->Locked()) {
					if(!loopCanTune(fe, channel)) {
						return false;
					}
				} else
					ret = true;
			}
#else
			ret = (getLoopFE(channel) != NULL);
#endif
			break;
		case FE_MODE_TWIN:
			for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
				fe = it->second;
				if(!fe->Locked() || fe->sameTsidOnid(channel->getTransponderId()))
					return true;
			}
			break;
		case FE_MODE_ALONE:
			for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
				fe = it->second;
				satellite_map_t & satmap = fe->getSatellites();
				sat_iterator_t sit = satmap.find(channel->getSatellitePosition());
				if(((sit != satmap.end()) && sit->second.configured)) {
					if(!fe->Locked() || fe->sameTsidOnid(channel->getTransponderId()))
						return true;
				}
			}
			break;
	}
	return ret;
}

CFrontend * CFEManager::getScanFrontend(t_satellite_position satellitePosition)
{
	CFrontend * frontend = NULL;
	switch(mode) {
		case FE_MODE_SINGLE:
		case FE_MODE_LOOP:
		//FIXME scan while recording ?
		case FE_MODE_TWIN:
			frontend = getFE(0);
			break;
		case FE_MODE_ALONE:
			for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
				CFrontend * fe = it->second;
				satellite_map_t & satmap = fe->getSatellites();
				sat_iterator_t sit = satmap.find(satellitePosition);
				if(((sit != satmap.end()) && sit->second.configured)) {
					frontend = fe;
					break;
				}
			}
			break;
	}
	INFO("Selected fe: %d", frontend ? frontend->fenumber : -1);
	return frontend;
}

bool CFEManager::lockFrontend(CFrontend * frontend)
{
	mutex.lock();
	frontend->Lock();
	have_locked = true;
	polarization = frontend->getPolarization() & 1;
	high_band = frontend->getHighBand();
	mutex.unlock();
	return true;
}

bool CFEManager::unlockFrontend(CFrontend * frontend)
{
	mutex.lock();
	have_locked = false;
	frontend->Unlock();
	for(fe_map_iterator_t it = femap.begin(); it != femap.end(); it++) {
		CFrontend * fe = it->second;
		if(fe->Locked()) {
			have_locked = true;
			polarization = fe->getPolarization() & 1;
			high_band = fe->getHighBand();
			break;
		}
	}
	mutex.unlock();
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
#if 0
		if(mode == FE_MODE_LOOP)
			return false;
#endif
		return false;
	}
	return true;
}
