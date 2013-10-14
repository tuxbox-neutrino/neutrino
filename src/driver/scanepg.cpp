/*
        Copyright (C) 2013 CoolStream International Ltd

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

#include <global.h>
#include <neutrino.h>

#include <system/helpers.h>
#include <system/setting_helpers.h>
#include <daemonc/remotecontrol.h>

#include <zapit/zapit.h>
#include <zapit/debug.h>
#include <eitd/sectionsd.h>

#include <gui/bouquetlist.h>
#include <gui/channellist.h>

#include <driver/scanepg.h>
#include <driver/record.h>

extern CBouquetList * bouquetList;
extern CBouquetList * TVfavList;
 
CEpgScan::CEpgScan()
{
	current_mode = 0;
	standby = false;
	Clear();
}

CEpgScan::~CEpgScan()
{
}

CEpgScan * CEpgScan::getInstance()
{
	static CEpgScan * inst;
	if (inst == NULL)
		inst = new CEpgScan();
	return inst;
}

void CEpgScan::Clear()
{
	scanmap.clear();
	current_bnum = -1;
	current_bmode = -1;
	next_chid = 0;
	allfav_done = false;
}

bool CEpgScan::Running()
{
	return (g_settings.epg_scan && !scanmap.empty());
}

void CEpgScan::AddBouquet(CChannelList * clist)
{
	for (unsigned i = 0; i < clist->Size(); i++) {
		CZapitChannel * chan = clist->getChannelFromIndex(i);
		if (scanned.find(chan->getTransponderId()) == scanned.end())
			scanmap.insert(eit_scanmap_pair_t(chan->getTransponderId(), chan->getChannelID()));
	}
}

bool CEpgScan::AddFavorites()
{
	INFO("allfav_done: %d", allfav_done);
	if ((g_settings.epg_scan != 2) || allfav_done)
		return false;

	allfav_done = true;
	unsigned old_size = scanmap.size();
	for (unsigned j = 0; j < TVfavList->Bouquets.size(); ++j) {
		CChannelList * clist = TVfavList->Bouquets[j]->channelList;
		AddBouquet(clist);
	}
	INFO("scan map size: %d -> %d\n", old_size, scanmap.size());
	return (old_size != scanmap.size());
}

void CEpgScan::AddTransponders()
{
	if(bouquetList->Bouquets.empty())
		return;

	if (current_mode != g_settings.epg_scan) {
		current_mode = g_settings.epg_scan;
		Clear();
	}
	/* TODO: add interval check to clear scanned ? */

	int mode = CNeutrinoApp::getInstance()->GetChannelMode();
	if ((g_settings.epg_scan == 1) || (mode == LIST_MODE_FAV)) {
		/* current bouquet mode */
		if (current_bmode != mode) {
			current_bmode = mode;
			current_bnum = -1;
		}
		if (current_bnum != bouquetList->getActiveBouquetNumber()) {
			allfav_done = false;
			scanmap.clear();
			current_bnum = bouquetList->getActiveBouquetNumber();
			AddBouquet(bouquetList->Bouquets[current_bnum]->channelList);
			INFO("Added bouquet #%d, scan map size: %d", current_bnum, scanmap.size());
		}
	} else {
		AddFavorites();
	}
}

void CEpgScan::StartStandby()
{
	if (!g_settings.epg_scan)
		return;

	live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
	AddTransponders();
	INFO("starting standby scan, scan map size: %d", scanmap.size());
	if (!scanmap.empty()) {
		standby = true;
		Next();
	}
}

void CEpgScan::StopStandby()
{
	if (!g_settings.epg_scan)
		return;

	INFO("stopping standby scan...");
	standby = false;
	CZapit::getInstance()->SetCurrentChannelID(live_channel_id);
}

int CEpgScan::handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data)
{
	if (!g_settings.epg_scan || (!standby && (CFEManager::getInstance()->getEnabledCount() <= 1)))
		return messages_return::unhandled;

	CZapitChannel * newchan;
	if(msg == NeutrinoMessages::EVT_ZAP_COMPLETE) {
		AddTransponders();
		INFO("EVT_ZAP_COMPLETE, scan map size: %d\n", scanmap.size());
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_EIT_COMPLETE) {
		t_channel_id chid = *(t_channel_id *)data;
		newchan = CServiceManager::getInstance()->FindChannel(chid);
		if (newchan) {
			scanned.insert(newchan->getTransponderId());
			scanmap.erase(newchan->getTransponderId());
		}
		INFO("EIT read complete [" PRINTF_CHANNEL_ID_TYPE "], scan map size: %d", chid, scanmap.size());

		Next();
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_BACK_ZAP_COMPLETE) {
		t_channel_id chid = *(t_channel_id *)data;
		INFO("EVT_BACK_ZAP_COMPLETE [" PRINTF_CHANNEL_ID_TYPE "]", chid);
		if (next_chid) {
			newchan = CServiceManager::getInstance()->FindChannel(next_chid);
			if (newchan) {
				if(chid) {
					if (!CRecordManager::getInstance()->RecordingStatus()) {
						INFO("try to scan [%s]", newchan->getName().c_str());
						if (standby && !g_Sectionsd->getIsScanningActive())
							g_Sectionsd->setPauseScanning(false);
						g_Sectionsd->setServiceChanged(newchan->getChannelID(), false, newchan->getRecordDemux());
					}
				} else {
					INFO("tune failed [%s]", newchan->getName().c_str());
					scanmap.erase(newchan->getTransponderId());
					Next();
				}
			}
		}
		return messages_return::handled;
	}
	return messages_return::unhandled;
}

void CEpgScan::EnterStandby()
{
	if (standby) {
		CZapit::getInstance()->SetCurrentChannelID(live_channel_id);
		g_Zapit->setStandby(true);
		g_Sectionsd->setPauseScanning(true);
	}
}

void CEpgScan::Next()
{
	bool locked = false;

	next_chid = 0;
	if (!g_settings.epg_scan)
		return;
	if (!standby && CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_standby)
		return;
	if (CRecordManager::getInstance()->RecordingStatus())
		return;

	if (g_settings.epg_scan == 2 && scanmap.empty())
		AddFavorites();

	if (scanmap.empty()) {
		EnterStandby();
		return;
	}

	/* executed in neutrino thread - possible race with locks in zapit zap NOWAIT :
	   send zapTo_NOWAIT -> EIT_COMPLETE from sectionsd -> zap and this at the same time
	*/
	CFEManager::getInstance()->Lock();
	CFrontend *live_fe = NULL, *pip_fe = NULL;
	if (!standby) {
		locked = true;
		live_fe = CZapit::getInstance()->GetLiveFrontend();
		CFEManager::getInstance()->lockFrontend(live_fe);
#ifdef ENABLE_PIP
		pip_fe = CZapit::getInstance()->GetPipFrontend();
		if (pip_fe && pip_fe != live_fe)
			CFEManager::getInstance()->lockFrontend(pip_fe);
#endif
	}
_repeat:
	for (eit_scanmap_iterator_t it = scanmap.begin(); it != scanmap.end(); /* ++it*/) {
		CZapitChannel * newchan = CServiceManager::getInstance()->FindChannel(it->second);
		if (newchan == NULL) {
			scanmap.erase(it++);
			continue;
		}
		if (CFEManager::getInstance()->canTune(newchan)) {
			INFO("try to tune [%s]", newchan->getName().c_str());
			next_chid = newchan->getChannelID();
			break;
		} else
			INFO("skip [%s], cannot tune", newchan->getName().c_str());
		++it;
	}
	if (!next_chid && AddFavorites())
		goto _repeat;

	if (locked) {
		CFEManager::getInstance()->unlockFrontend(live_fe);
#ifdef ENABLE_PIP
		if (pip_fe && pip_fe != live_fe)
			CFEManager::getInstance()->unlockFrontend(pip_fe);
#endif
	}
	CFEManager::getInstance()->Unlock();
	if (next_chid)
		g_Zapit->zapTo_epg(next_chid, standby);
	else 
		EnterStandby();
}
