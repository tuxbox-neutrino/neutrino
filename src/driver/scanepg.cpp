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
#include <zapit/getservices.h>
#include <zapit/debug.h>
#include <eitd/sectionsd.h>

#include <gui/bouquetlist.h>
#include <gui/channellist.h>

#include <driver/scanepg.h>
#include <driver/record.h>
#include <driver/streamts.h>
#include <driver/scanpause.h>

//#define EPG_RESCAN_TIME (24*60*60)

extern CBouquetList * bouquetList;
extern CBouquetList * TVfavList;
extern CBouquetList * RADIOfavList;
extern CBouquetList * TVbouquetList;
 
CEpgScan::CEpgScan()
{
	current_mode = 0;
	standby = false;
	rescan_timer = 0;
	scan_in_progress = false;
	paused = false;
	reading_tp = 0;
	reading_disturbed = false;
	reading_fe = NULL;
	reading_tunes = 0;
	rereads = 0;
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

void CEpgScan::ConfigureEIT()
{
	CEitManager::getInstance()->clearChannelFilters();
	if (g_settings.epg_save_mode == 0)
		return;

	int count = 0;

	/* The same lock, over both lists together, for the reason the walks below
	   this one are held under it: what a bouquet carries are borrowed pointers
	   into the map the service manager owns, and that map is destroyed whole
	   when the services are read again.

	   Safe to hold across what this calls out to. The event manager takes a
	   lock of its own for each filter it is handed, and nothing under that
	   lock reaches back into the channel manager, so the two are only ever
	   taken in this order. */
	CServiceManager::ChannelGuard guard;

	for (unsigned j = 0; j < TVfavList->Bouquets.size(); ++j) {
		CChannelList * clist = TVfavList->Bouquets[j]->channelList;
		for (unsigned i = 0; i < clist->Size(); i++) {
			CZapitChannel * chan = clist->getChannelFromIndex(i);
			CEitManager::getInstance()->addChannelFilter(chan->getOriginalNetworkId(), chan->getTransportStreamId(), chan->getServiceId());
			count++;
		}
	}
	for (unsigned j = 0; j < RADIOfavList->Bouquets.size(); ++j) {
		CChannelList * clist = RADIOfavList->Bouquets[j]->channelList;
		for (unsigned i = 0; i < clist->Size(); i++) {
			CZapitChannel * chan = clist->getChannelFromIndex(i);
			CEitManager::getInstance()->addChannelFilter(chan->getOriginalNetworkId(), chan->getTransportStreamId(), chan->getServiceId());
			count++;
		}
	}
	INFO("added %d channels to EIT white list", count);
}

void CEpgScan::Clear()
{
	scanmap.clear();
	current_bnum = -1;
	current_bmode = -1;
	next_chid = 0;
	allfav_done = false;
	selected_done = false;
	paused = false;
	reading_tp = 0;
	reading_disturbed = false;
	reading_fe = NULL;
	reading_tunes = 0;
	rereads = 0;
}

bool CEpgScan::Running()
{
	return (CheckMode() && !scanmap.empty());
}

/* The caller holds CServiceManager's channel lock, and has to hold it across
   every bouquet it walks rather than around each one: what the list carries is
   borrowed pointers into the channel map, the map is destroyed whole when the
   services are read again, and the lists are emptied from the thread that does
   it. A lock taken and dropped per bouquet leaves that window open between two
   of them.

   Nothing here asks whether a channel came back absent. Under the lock it
   cannot, and without the lock a passed over channel is one being freed under
   the walk: the reliable crash that reports it is worth more than a quiet read
   of a freed object. */
void CEpgScan::AddBouquet(CChannelList * clist)
{
	for (unsigned i = 0; i < clist->Size(); i++) {
		CZapitChannel * chan = clist->getChannelFromIndex(i);
		if (!IS_WEBCHAN(chan->getChannelID()) && scanned.find(chan->getTransponderId()) == scanned.end())
			scanmap.insert(eit_scanmap_pair_t(chan->getTransponderId(), chan->getChannelID()));
	}
}

bool CEpgScan::AddFavorites()
{
	INFO("allfav_done: %d", allfav_done);
	if ((g_settings.epg_scan != SCAN_FAV) || allfav_done)
		return false;

	/* The bouquet lists are built by channelsInit() and do not exist before
	   it, and this is reached earlier than that: the start up wizard runs a
	   message loop of its own, and a recording that ends while it is up is
	   dispatched through it. A question about whether the list is there at
	   all, which is not the question the walk must never ask about a channel
	   inside one. */
	if (!TVfavList)
		return false;

	allfav_done = true;
	unsigned old_size = scanmap.size();
	for (unsigned j = 0; j < TVfavList->Bouquets.size(); ++j) {
		CChannelList * clist = TVfavList->Bouquets[j]->channelList;
		AddBouquet(clist);
	}
	INFO("scan map size: %d -> %zd\n", old_size, scanmap.size());
	return (old_size != scanmap.size());
}

bool CEpgScan::AddSelected()
{
	INFO("selected_done: %d", selected_done);
	if ((g_settings.epg_scan != SCAN_SEL) || selected_done)
		return false;

	// The same two lists, and the same reason as above.
	if (!TVfavList || !TVbouquetList)
		return false;

	selected_done = true;
	unsigned old_size = scanmap.size();
	for (unsigned j = 0; j < TVfavList->Bouquets.size(); ++j) {
		if (TVfavList->Bouquets[j]->zapitBouquet && TVfavList->Bouquets[j]->zapitBouquet->bScanEpg) {
			CChannelList * clist = TVfavList->Bouquets[j]->channelList;
			AddBouquet(clist);
		}
	}
	for (unsigned j = 0; j < TVbouquetList->Bouquets.size(); ++j) {
		if (TVbouquetList->Bouquets[j]->zapitBouquet && TVbouquetList->Bouquets[j]->zapitBouquet->bScanEpg) {
			CChannelList * clist = TVbouquetList->Bouquets[j]->channelList;
			AddBouquet(clist);
		}
	}
	INFO("scan map size: %d -> %zd\n", old_size, scanmap.size());
	return (old_size != scanmap.size());
}

void CEpgScan::AddTransponders()
{
	if(!bouquetList || bouquetList->Bouquets.empty())
		return;

	/* Held over the whole of what follows, because every arm of it walks a
	   bouquet, and taking it per bouquet would leave the window open between
	   two of them. Nothing below reaches back into the channel manager, whose
	   lock is not recursive, and the two calls it makes into this class take
	   no lock of their own for that reason. */
	CServiceManager::ChannelGuard guard;

	if (current_mode != g_settings.epg_scan) {
		current_mode = g_settings.epg_scan;
		Clear();
	}

	int mode = CNeutrinoApp::getInstance()->GetChannelMode();
	if (g_settings.epg_scan == SCAN_SEL) {
		if (current_bmode != mode) {
			current_bmode = mode;
			current_bnum = -1;
		}
		int bnum = bouquetList->getActiveBouquetNumber();
		bool bscan = bouquetList->Bouquets[bnum]->zapitBouquet &&
			bouquetList->Bouquets[bnum]->zapitBouquet->bScanEpg;

		if ((current_bnum != bnum) && bscan) {
			current_bnum = bnum;
			AddBouquet(bouquetList->Bouquets[current_bnum]->channelList);
		} else {
			AddSelected();
		}
		return;
	}

	if ((g_settings.epg_scan == SCAN_CURRENT) || (mode == LIST_MODE_FAV)) {
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
			INFO("Added bouquet #%d, scan map size: %zd", current_bnum, scanmap.size());
		}
	} else if (g_settings.epg_scan == SCAN_FAV) {
		AddFavorites();
	}
}

bool CEpgScan::CheckMode()
{
	bool webchan = IS_WEBCHAN(CZapit::getInstance()->GetCurrentChannelID());
	if ((g_settings.epg_scan_mode == CEpgScan::MODE_OFF)
			|| (standby && !(g_settings.epg_scan_mode & MODE_STANDBY))
			|| (!standby && !(g_settings.epg_scan_mode & MODE_LIVE))
			|| (!standby && !webchan && (CFEManager::getInstance()->getEnabledCount() <= 1))) {
		return false;
	}
	return true;
			
}

void CEpgScan::Start(bool instandby)
{
	standby = instandby;
	live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
	AddTransponders();
	INFO("starting %s scan, scanning %d, scan map size: %zd", standby ? "standby" : "live", scan_in_progress, scanmap.size());
	if (standby || !scan_in_progress)
		Next();
}

void CEpgScan::Stop()
{
	if (g_settings.epg_scan_mode == CEpgScan::MODE_OFF)
		return;

	INFO("stopping %s scan...", standby ? "standby" : "live");
	paused = false;
	reading_tp = 0;
	reading_disturbed = false;
	reading_fe = NULL;
	reading_tunes = 0;
	rereads = 0;
	if (standby) {
		standby = false;
		CZapit::getInstance()->SetCurrentChannelID(live_channel_id);
	}
}

int CEpgScan::handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data)
{
	if ((msg == NeutrinoMessages::EVT_TIMER) && (data == rescan_timer)) {
		INFO("rescan timer in %s mode, scanning %d", standby ? "standby" : "live", scan_in_progress);
		scanned.clear();
		Clear();
		g_RCInput->killTimer(rescan_timer);
		if (CheckMode()) {
			if (standby)
				CNeutrinoApp::getInstance()->wakeupFromStandby();
			Start(standby);
		} else {
			AddTimer();
		}
		return messages_return::handled;
	}
	if (!CheckMode()) {
		int ret = messages_return::handled;
		if (msg == NeutrinoMessages::EVT_EIT_COMPLETE)
			scan_in_progress = false;
		else if (msg == NeutrinoMessages::EVT_BACK_ZAP_COMPLETE)
			scan_in_progress = true;
		else
			ret = messages_return::unhandled;
		return ret;
	}

	if (msg == NeutrinoMessages::EVT_STREAM_START) {
		/* The stream may have retuned the frontend sectionsd reads the
		   transponder from. The program's own handler still has to see
		   the message. */
		if (reading_tp)
			reading_disturbed = true;
		return messages_return::unhandled;
	}
	if (msg == NeutrinoMessages::EVT_STREAM_STOP) {
		/* data 1: a setup that started no stream asked zapit to hold a
		   frontend, which zapit may have retuned under the read. The
		   program's own handler still has to see the message. */
		if (reading_tp && data == 1)
			reading_disturbed = true;
		return messages_return::unhandled;
	}

	CZapitChannel * newchan;
	if (msg == NeutrinoMessages::EVT_ZAP_COMPLETE) {
		/* live channel changed, block scan channel change by timer */
		scan_in_progress = true;
		AddTransponders();
		INFO("EVT_ZAP_COMPLETE, scan map size: %zd\n", scanmap.size());
#if 0
		t_channel_id chid = *(t_channel_id *)data;
		if (IS_WEBCHAN(chid))
			Next();
#endif
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_EIT_COMPLETE) {
		scan_in_progress = false;
		t_channel_id chid = *(t_channel_id *)data;
		// Before the channel lock: the stream server takes a lock of its own.
		const bool busy = CStreamManager::getInstance()->StreamStatus();
		/* A stream's mark, or a tune of the frontend read since the scan's
		   own zap tuned it, which nothing else may have announced. */
		const bool disturbed = scanpause::readDisturbed(reading_disturbed, reading_fe != NULL, reading_tunes,
								reading_fe ? reading_fe->getTuneCount() : 0);
		bool again = false;
		int disturbed_reads = 0;
		std::string chan_name;
		{
			/* The channel this names is a pointer into the map the service
			   manager owns, so reading anything off it belongs under that
			   lock. Given back before the call below, which takes the same
			   lock and it is not recursive. */
			CServiceManager::ChannelGuard guard;
			newchan = CServiceManager::getInstance()->FindChannel(chid);
			if (newchan) {
				const transponder_id_t tpid = newchan->getTransponderId();
				const scanpause::EitComplete done =
					scanpause::onEitComplete(reading_tp != 0 && tpid == reading_tp,
								 disturbed, busy, rereads);
				again = done == scanpause::ReadAgain;
				if (done == scanpause::CountDisturbed)
					disturbed_reads = rereads + 1;
				chan_name = newchan->getName();
				if (!again) {
					scanned.insert(tpid);
					scanmap.erase(tpid);
					if (tpid == reading_tp) {
						reading_tp = 0;
						reading_disturbed = false;
						reading_fe = NULL;
						rereads = 0;
					}
				}
			}
		}
		if (again) {
			/* The mark stays until the transponder is handed over again,
			   so that a later pass reported meanwhile does not count
			   either; the end of the stream resumes the scan. */
			reading_disturbed = true;
			paused = true;
			INFO("read again after a stream [%s], scan map size: %zd", chan_name.c_str(), scanmap.size());
			if (!busy)
				Resume();
			return messages_return::handled;
		}
		if (disturbed_reads)
			INFO("counted [%s] after %d disturbed reads", chan_name.c_str(), disturbed_reads);
		INFO("EIT read complete [" PRINTF_CHANNEL_ID_TYPE "], scan map size: %zd", chid, scanmap.size());

		Next();
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_BACK_ZAP_COMPLETE) {
		scan_in_progress = true;
		t_channel_id chid = *(t_channel_id *)data;
		INFO("EVT_BACK_ZAP_COMPLETE [" PRINTF_CHANNEL_ID_TYPE "]", chid);
		if (next_chid) {
			/* Read out under the lock and acted on after it. What follows
			   talks to another daemon over a socket and can call back into
			   this class, and neither belongs under a lock that every reader
			   taking it has to wait for; the one below takes it again
			   itself, and it is not recursive. */
			std::string chan_name;
			t_channel_id chan_id = 0;
			transponder_id_t chan_tpid = 0;
			int chan_demux = 0;
			bool found = false;
			{
				CServiceManager::ChannelGuard guard;
				newchan = CServiceManager::getInstance()->FindChannel(next_chid);
				if (newchan) {
					found = true;
					chan_name = newchan->getName();
					chan_id = newchan->getChannelID();
					chan_tpid = newchan->getTransponderId();
					chan_demux = newchan->getRecordDemux();
				}
			}
			if (found) {
				/* A stream that came in since Next() looked, or one whose
				   hold took the frontend before this zap, keeps the
				   transponder from being read now, not for good. */
				switch (scanpause::onBackZap(chid != 0, CStreamManager::getInstance()->StreamStatus())) {
				case scanpause::Read:
					if (!CRecordManager::getInstance()->RecordingStatus()) {
						INFO("try to scan [%s]", chan_name.c_str());
						if (standby && !g_Sectionsd->getIsScanningActive())
							g_Sectionsd->setPauseScanning(false);
						g_Sectionsd->setServiceChanged(chan_id, false, chan_demux);
						rereads = scanpause::rereadsAtHandOver(chan_tpid == reading_tp,
										     reading_disturbed, rereads);
						reading_tp = chan_tpid;
						reading_disturbed = false;
						/* zapit's record of the scan's zap, taken for the
						   channel handed over only: with a second scan zap
						   under way it names another channel or none, and
						   the read counts as disturbed. */
						if (!CZapit::getInstance()->GetEpgScanTune(next_chid, reading_fe, reading_tunes))
							reading_fe = NULL;
					}
					break;
				case scanpause::Pause:
					INFO("paused for a stream [%s]", chan_name.c_str());
					paused = true;
					break;
				case scanpause::Skip:
					INFO("tune failed [%s]", chan_name.c_str());
					scanmap.erase(chan_tpid);
					Next();
					break;
				}
			}
		}
		return messages_return::handled;
	}
	return messages_return::unhandled;
}

/* How long the scan waits for zapit to give back the holds of ended streams. */
static const unsigned kCollectBudgetMs = 2000;

void CEpgScan::Resume()
{
	if (!scanpause::resumeNow(paused, CheckMode()))
		return;
	/* The holds of the streams that ended are given back first, or the
	   scan's zap could find their frontends still held. The deadline is
	   the thread's for every zapit call while it lives, so it ends before
	   Next(), which can enter standby and run a message loop. */
	{
		CBasicClient::Deadline until(kCollectBudgetMs);
		CZapitClient::responseStreamYield reply;
		g_Zapit->yieldStreams(0, kCollectBudgetMs, reply);
	}
	Next();
}

void CEpgScan::AddTimer()
{
	if (rescan_timer == 0)
		rescan_timer = g_RCInput->addTimer((g_settings.epg_scan_rescan*60*60)*1000ULL*1000ULL, true);
	INFO("rescan timer id %d", rescan_timer);
	INFO("rescan time is %d*60*60", g_settings.epg_scan_rescan);
}

void CEpgScan::EnterStandby()
{
	AddTimer();
	if (standby) {
		CZapit::getInstance()->SetCurrentChannelID(live_channel_id);
		CNeutrinoApp::getInstance()->standbyToStandby();

		if (g_settings.epg_save && g_settings.epg_save_standby)
			CNeutrinoApp::getInstance()->saveEpg(NeutrinoModes::mode_standby);
	}
}

void CEpgScan::Next()
{
	bool llocked = false;
#if ENABLE_PIP
	bool plocked = false;
#endif
	next_chid = 0;
	paused = false;

	if (!standby && CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_standby)
		return;
	if (CRecordManager::getInstance()->RecordingStatus() || CStreamManager::getInstance()->StreamStatus()) {
		paused = true;
		return;
	}

	{
		/* The same lock and the same reason as in AddTransponders, in a scope
		   of its own so that it is given back before the tuning below rather
		   than held across it. Every thread that takes this lock waits while
		   it is held, and a tune is the longest thing this function does. */
		CServiceManager::ChannelGuard guard;
		if (g_settings.epg_scan == SCAN_FAV && scanmap.empty())
			AddFavorites();
		if (g_settings.epg_scan == SCAN_SEL && scanmap.empty())
			AddSelected();
	}

	if (!CheckMode() || scanmap.empty()) {
		EnterStandby();
		return;
	}

	/* executed in neutrino thread - possible race with locks in zapit zap NOWAIT :
	   send zapTo_NOWAIT -> EIT_COMPLETE from sectionsd -> zap and this at the same time
	*/
	CFEManager::getInstance()->Lock();
	CFrontend *live_fe = NULL;
#if ENABLE_PIP
	CFrontend *pip_fe = NULL;
#endif
	if (!standby) {
		bool webchan = IS_WEBCHAN(CZapit::getInstance()->GetCurrentChannelID());
		if (!webchan) {
			llocked = true;
			live_fe = CZapit::getInstance()->GetLiveFrontend();
			CFEManager::getInstance()->lockFrontend(live_fe);
		}
#if ENABLE_PIP
		pip_fe = CZapit::getInstance()->GetPipFrontend();
		if (pip_fe /* && pip_fe != live_fe*/) {
			plocked = true;
			CFEManager::getInstance()->lockFrontend(pip_fe);
		}
#endif
	}
	{
		/* Held over the search and over the two calls that widen it, which is
		   what the lookup needs: the channel this asks for by identifier is a
		   pointer into the map, and reading a name off one the service manager
		   has since destroyed is the same fault the bouquet walks carry.

		   This is the one place that holds the channel lock while a frontend
		   lock is held, so the order between the two is stated here: the
		   frontends first. Nothing takes them the other way round today, which
		   is what makes this safe, and a caller that took the channel lock and
		   then reached for a frontend would be the thing that breaks it. */
		CServiceManager::ChannelGuard guard;
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
		if (!next_chid && ((g_settings.epg_scan == SCAN_FAV) && AddFavorites()))
			goto _repeat;
		if (!next_chid && ((g_settings.epg_scan == SCAN_SEL) && AddSelected()))
			goto _repeat;
	}

	if (llocked)
		CFEManager::getInstance()->unlockFrontend(live_fe);
#if ENABLE_PIP
	if (plocked)
		CFEManager::getInstance()->unlockFrontend(pip_fe);
#endif

	CFEManager::getInstance()->Unlock();
	if (next_chid)
		g_Zapit->zapTo_epg(next_chid, standby);
	else 
		EnterStandby();
}
