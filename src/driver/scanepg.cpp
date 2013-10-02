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

extern CBouquetList * bouquetList;
extern CBouquetList * TVfavList;
 
CEpgScan::CEpgScan()
{
	current_bnum = -1;
	next_chid = 0;
	current_mode = 0;
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
	next_chid = 0;
}

void CEpgScan::handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data)
{
	if (!g_settings.epg_scan || CFEManager::getInstance()->getEnabledCount() <= 1)
		return;

	CZapitChannel * newchan;
	if(msg == NeutrinoMessages::EVT_ZAP_COMPLETE) {
		if(bouquetList->Bouquets.empty())
			return;

		if (current_mode != g_settings.epg_scan) {
			current_mode = g_settings.epg_scan;
			Clear();
		}
		if (g_settings.epg_scan == 1) {
			/* current bouquet mode */
			if (current_bnum != bouquetList->getActiveBouquetNumber()) {
				scanmap.clear();
				current_bnum = bouquetList->getActiveBouquetNumber();
				CChannelList * clist = bouquetList->Bouquets[current_bnum]->channelList;
				for (unsigned i = 0; i < clist->Size(); i++) {
					CZapitChannel * chan = clist->getChannelFromIndex(i);
					/* TODO: add interval check to clear scanned ? */
					if (scanned.find(chan->getTransponderId()) == scanned.end())
						scanmap.insert(eit_scanmap_pair_t(chan->getTransponderId(), chan->getChannelID()));
				}
				INFO("EVT_ZAP_COMPLETE, scan map size: %d\n", scanmap.size());
			}
		} else {
			/* all favorites mode */
			if (scanmap.empty()) {
				for (unsigned j = 0; j < TVfavList->Bouquets.size(); ++j) {
					CChannelList * clist = TVfavList->Bouquets[j]->channelList;
					for (unsigned i = 0; i < clist->Size(); i++) {
						CZapitChannel * chan = clist->getChannelFromIndex(i);
						if (scanned.find(chan->getTransponderId()) == scanned.end())
							scanmap.insert(eit_scanmap_pair_t(chan->getTransponderId(), chan->getChannelID()));
					}
				}
				INFO("EVT_ZAP_COMPLETE, scan map size: %d\n", scanmap.size());
			}
		}
	}
	else if (msg == NeutrinoMessages::EVT_EIT_COMPLETE) {
		t_channel_id chid = *(t_channel_id *)data;
		newchan = CServiceManager::getInstance()->FindChannel(chid);
		if (newchan) {
			scanned.insert(newchan->getTransponderId());
			scanmap.erase(newchan->getTransponderId());
		}
		INFO("EIT read complete [" PRINTF_CHANNEL_ID_TYPE "], scan map size: %d", chid, scanmap.size());

		if (scanmap.empty())
			return;

		Next();
	}
	else if (msg == NeutrinoMessages::EVT_BACK_ZAP_COMPLETE) {
		t_channel_id chid = *(t_channel_id *)data;
		INFO("EVT_BACK_ZAP_COMPLETE [" PRINTF_CHANNEL_ID_TYPE "]", chid);
		if (next_chid) {
			newchan = CServiceManager::getInstance()->FindChannel(next_chid);
			if (newchan) {
				if(chid) {
					INFO("try to scan [%s]", newchan->getName().c_str());
					g_Sectionsd->setServiceChanged(newchan->getChannelID(), false, newchan->getRecordDemux());
				} else {
					INFO("tune failed [%s]", newchan->getName().c_str());
					scanmap.erase(newchan->getTransponderId());
					Next();
				}
			}
		}
	}
}

void CEpgScan::Next()
{
	next_chid = 0;
	if (CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_standby)
		return;

	t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();

	/* executed in neutrino thread - possible race with locks in zapit zap NOWAIT :
	   send zapTo_NOWAIT -> EIT_COMPLETE from sectionsd -> zap and this at the same time
	*/
	CFEManager::getInstance()->Lock();
	CFrontend *live_fe = CZapit::getInstance()->GetLiveFrontend();
	CFEManager::getInstance()->lockFrontend(live_fe);
#ifdef ENABLE_PIP
	CFrontend *pip_fe = CZapit::getInstance()->GetPipFrontend();
	if (pip_fe && pip_fe != live_fe)
		CFEManager::getInstance()->lockFrontend(pip_fe);
#endif
	for (eit_scanmap_iterator_t it = scanmap.begin(); it != scanmap.end(); /* ++it*/) {
		CZapitChannel * newchan = CServiceManager::getInstance()->FindChannel(it->second);
		if ((newchan == NULL) || SAME_TRANSPONDER(live_channel_id, newchan->getChannelID())) {
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
	CFEManager::getInstance()->unlockFrontend(live_fe);
#ifdef ENABLE_PIP
	if (pip_fe && pip_fe != live_fe)
		CFEManager::getInstance()->unlockFrontend(pip_fe);
#endif
	CFEManager::getInstance()->Unlock();
	if (next_chid)
		g_Zapit->zapTo_epg(next_chid);
}
