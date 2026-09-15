/*
 * channelsource_real.cpp - channel source backed by the running zapit client
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "coreapi/base/deps.h"

#include "coreapi/bouquetorder.h"
#include "channelstreams_internal.h"

#include <cstring>
#include <vector>

#include <time.h>

#include <connection/basicclient.h>
#include <eitd/sectionsd.h>
#include <neutrinoMessages.h>
#include <timerdclient/timerdclient.h>
#include <timerdclient/timerdmsg.h>
#include <zapit/client/zapitclient.h>
#include <zapit/getservices.h>
#include <zapit/zapit.h>

extern CBouquetManager *g_bouquetManager;
extern CTimerdClient *g_Timerd;
extern CZapitClient *g_Zapit;

namespace coreapi
{

namespace
{

void fill(CZapitChannel &src, ChannelInfo &dst)
{
	dst.id                  = src.getChannelID();
	dst.epg_id              = src.getEpgID();
	dst.number              = src.number;
	dst.name                = src.getName();
	dst.url                 = src.getUrl();
	dst.service_id          = src.getServiceId();
	dst.transport_stream_id = src.getTransportStreamId();
	dst.original_network_id = src.getOriginalNetworkId();
	dst.satellite_position  = src.getSatellitePosition();
	dst.freq_id             = src.getFreqId();
	dst.kind                = serviceKind(src.getServiceType() == ST_DIGITAL_RADIO_SOUND_SERVICE,
					      !src.getUrl().empty());
	dst.scrambled           = (src.scrambled != 0);
	dst.locked              = src.Locked();
}

// Walked rather than sorted, because the two lists a bouquet is set from are as
// long as a bouquet is and a bouquet is tens of channels.
bool names(const ChannelIdList &ids, ChannelId one)
{
	for (size_t i = 0; i < ids.size(); i++)
		if (ids[i] == one)
			return true;
	return false;
}

// Caller holds the channel lock, the list is pointers into the channel map.
void append(const ZapitChannelList &from, ChannelList &to)
{
	for (size_t i = 0; i < from.size(); i++)
	{
		ChannelInfo ci;
		fill(*from[i], ci);
		to.push_back(ci);
	}
}

class RealChannelSource : public ChannelSource
{
	public:
		Status allChannels(bool tv, ChannelList &out) const
		{
			CServiceManager *manager = CServiceManager::getInstance();
			if (manager == NULL)
				return Status::Internal;
			out.clear();

			// The map under the channel lock, not a copy of it. A listing reads a dozen fields off
			// each channel and a channel is most of a kilobyte, so copying the map first costs
			// megabytes per reader on a box with a full satellite list.
			CServiceManager::ChannelGuard guard;
			tallchans *all = manager->GetAllChannels();
			if (all == NULL)
				return Status::Internal;
			for (tallchans_iterator it = all->begin(); it != all->end(); ++it)
			{
				const bool is_tv =
					(it->second.getServiceType() != ST_DIGITAL_RADIO_SOUND_SERVICE);
				// The same two the copy filtered on. A channel the rest of the
				// box hides for not being present must not appear here either.
				if (is_tv != tv || !(it->second.flags & CZapitChannel::PRESENT))
					continue;
				ChannelInfo ci;
				fill(it->second, ci);
				out.push_back(ci);
			}
			return Status::Ok;
		}

		Status findChannel(ChannelId id, ChannelInfo &out) const
		{
			// The empty name has to be typed, or an empty literal would pick
			// the webtv constructor, which leaves the object unbuilt.
			CZapitChannel ch(std::string(), 0, 0, 0, 0);
			if (!CServiceManager::getInstance()->CopyChannel((t_channel_id) id, ch))
				return Status::NotFound;
			fill(ch, out);
			return Status::Ok;
		}

		Status guideChannel(ChannelId asked, ChannelId &whole, ChannelId &key) const
		{
			CServiceManager *manager = CServiceManager::getInstance();
			if (manager == NULL)
				return Status::Internal;

			/* Pointers into the channel map, so the lock is held across both
			   reads off the channel. CopyChannel is not used and must not be:
			   it takes this same lock itself. */
			CServiceManager::ChannelGuard guard;

			CZapitChannel *ch = manager->FindChannel((t_channel_id) asked);
			/* The shortened form second and never first, so a whole identifier always
			   answers with its own channel even where another shares its lower half.
			   Which of several the shortened form picks is the box's own lookup, a
			   second rule here being a second answer the moment the two drift. */
			if (ch == NULL)
				ch = manager->FindChannel48((t_channel_id) asked);
			if (ch == NULL)
				return Status::NotFound;

			whole = ch->getChannelID();
			key = ch->getEpgID();
			return Status::Ok;
		}

		Status channelStreams(ChannelId id, ChannelStreams &out) const
		{
			CServiceManager *manager = CServiceManager::getInstance();
			if (manager == NULL)
				return Status::Internal;

			/* The channel where it sits and not a copy of it: a copied channel
			   carries no pids, no table and no track list at all. The same
			   lookup the running channel is read through, which also reaches
			   the subservices a copy could not find. */
			CServiceManager::ChannelGuard guard;
			CZapitChannel *ch = manager->FindChannel((t_channel_id) id);
			if (!ch)
				return Status::NotFound;
			internal::fillStreams(*ch, out);
			return Status::Ok;
		}

		/* The channel lock covers the bouquet vector as well as the channel
		   map: a reload frees every bouquet in it from another thread, and the
		   two counts below are the length of lists that thread empties and
		   fills. Held across the whole walk, because what it is holding off is
		   the freeing of what the walk is reading. */
		Status allBouquets(BouquetList &out) const
		{
			if (!g_bouquetManager)
				return Status::Internal;
			CServiceManager::ChannelGuard guard;
			out.clear();
			out.reserve(g_bouquetManager->Bouquets.size());
			for (size_t i = 0; i < g_bouquetManager->Bouquets.size(); i++)
			{
				const CZapitBouquet *b = g_bouquetManager->Bouquets[i];
				BouquetInfo bi;
				// The control API has always numbered bouquets from one.
				bi.id           = (uint32_t) i + 1;
				bi.name         = b->bName;
				bi.hidden       = b->bHidden;
				bi.locked       = b->bLocked;
				bi.user_bouquet = b->bUser;
				bi.tv_count     = (uint32_t) b->tvChannels.size();
				bi.radio_count  = (uint32_t) b->radioChannels.size();
				out.push_back(bi);
			}
			return Status::Ok;
		}

		Status bouquetChannels(uint32_t bouquet_id, ChannelList &out) const
		{
			// A bouquet manager that was never built is not the same answer
			// as an id nobody has.
			if (!g_bouquetManager)
				return Status::Internal;
			if (bouquet_id == 0)
				return Status::NotFound;
			size_t index = (size_t) bouquet_id - 1;

			// Taken before the bouquet is looked up and let go after the last
			// read off it, or the bouquet could be freed between the two.
			CServiceManager::ChannelGuard guard;
			if (index >= g_bouquetManager->Bouquets.size())
				return Status::NotFound;

			const CZapitBouquet *b = g_bouquetManager->Bouquets[index];
			out.clear();
			out.reserve(b->tvChannels.size() + b->radioChannels.size());

			append(b->tvChannels, out);
			append(b->radioChannels, out);
			return Status::Ok;
		}

		Status currentChannel(ChannelInfo &out) const
		{
			// Asked before the lock, because the channel manager's own
			// accessor for this resolves the id through the zapit instance
			// and back into the channel map.
			t_channel_id id = CZapit::getInstance()->GetCurrentChannelID();
			if (id == 0)
				return Status::NotFound;

			// Not CopyChannel: the running channel can be an NVOD subservice,
			// which lives in a map of its own that only this lookup searches.
			CServiceManager::ChannelGuard guard;
			CZapitChannel *ch = CServiceManager::getInstance()->FindChannel(id);
			// Zapit named an id the channel maps do not hold, which is a fault
			// here and not an idle box.
			if (!ch)
				return Status::Internal;
			fill(*ch, out);
			return Status::Ok;
		}

		Status currentCaids(std::vector<uint16_t> &out) const
		{
			/* Resolved the same two ways the running channel is above, so the
			   two reads cannot disagree about which channel is playing. */
			t_channel_id id = CZapit::getInstance()->GetCurrentChannelID();
			if (id == 0)
				return Status::NotFound;

			CServiceManager::ChannelGuard guard;
			CZapitChannel *ch = CServiceManager::getInstance()->FindChannel(id);
			// Zapit named an id the channel maps do not hold, which is a fault
			// here and not an idle box.
			if (!ch)
				return Status::Internal;

			out.clear();
			out.reserve(ch->camap.size());
			for (casys_map_t::const_iterator it = ch->camap.begin();
			     it != ch->camap.end(); ++it)
				out.push_back((uint16_t) *it);
			return Status::Ok;
		}

		Status reloadChannels(bool save_services, const std::string &reason)
		{
			if (!g_Zapit)
				return Status::Internal;

			/* Written out ahead of the reading, which is what makes what the
			   box is holding win over what is on the disc. Left out, the file
			   wins, and that is the other half of what the caller asked for. */
			if (save_services)
				CServiceManager::getInstance()->SaveServices(true, true);

			// Recorded before the reading rather than after it, because what
			// records it is read while the reading runs.
			if (g_bouquetManager)
				g_bouquetManager->setWebchannelsReloadReason(reason);

			g_Zapit->reinitChannels();

			/* The box no longer owes its user the question whether the list it
			   noticed a change in should be read again, because reading it is
			   that question answered. */
			clearPendingChannelReload();
			return Status::Ok;
		}

		Status currentMode(int &out) const
		{
			int m = applicationMode();
			// Before the application settles on one there is no mode to act
			// on, and it is a state the box passes through rather than a
			// fault.
			if (m == NeutrinoModes::mode_unknown)
				return Status::NotFound;
			out = m;
			return Status::Ok;
		}

		/* Both of the names a bouquet carries. Name is what it is written to file
		   under and what the channel stack compares a new name against; bName is the
		   caption every list on screen prints, and the two differ for the bouquets
		   whose caption comes from the locale. Reading only the first could not be
		   handed the name a reader was shown; reading only the second would let a
		   second bouquet be made under the identity of one already there. */
		Status bouquetByName(const std::string &name, uint32_t &id) const
		{
			if (!g_bouquetManager)
				return Status::Internal;
			CServiceManager::ChannelGuard guard;
			for (size_t i = 0; i < g_bouquetManager->Bouquets.size(); i++)
			{
				const CZapitBouquet *b = g_bouquetManager->Bouquets[i];
				if (b->Name != name && b->bName != name)
					continue;
				id = (uint32_t) i + 1;
				return Status::Ok;
			}
			return Status::NotFound;
		}

		/* Every write below goes to the channel stack over its own socket and not into
		   the vector directly, so the change is made on the thread that owns it. What
		   comes back is whether the command reached the channel stack at all: with the
		   socket gone the box goes on running. Reaching it is still not carrying it
		   out, which is why the layer above reads its work back whatever these say. */
		Status addBouquet(const std::string &name)
		{
			if (!g_Zapit)
				return Status::Internal;
			if (!g_Zapit->addBouquet(name.c_str()))
				return Status::Internal;
			return Status::Ok;
		}

		// The command behind this one moves both of the bouquet's names
		// through the one call that owns that rule. Nothing here writes either
		// field, which is what keeps the rule in one place.
		Status renameBouquet(uint32_t id, const std::string &name)
		{
			if (!g_Zapit)
				return Status::Internal;
			if (!g_Zapit->renameBouquet(id - 1, name.c_str()))
				return Status::Internal;
			return Status::Ok;
		}

		// The channel stack counts bouquets from nought and every read here
		// answers from one, so each of these is the one place that difference
		// is written down.
		Status moveBouquet(uint32_t id, uint32_t to_id)
		{
			if (!g_Zapit)
				return Status::Internal;
			if (!g_Zapit->moveBouquet(id - 1, to_id - 1))
				return Status::Internal;
			return Status::Ok;
		}

		Status deleteBouquet(uint32_t id)
		{
			if (!g_Zapit)
				return Status::Internal;
			if (!g_Zapit->deleteBouquet(id - 1))
				return Status::Internal;
			return Status::Ok;
		}

		Status setBouquetHidden(uint32_t id, bool hidden)
		{
			if (!g_Zapit)
				return Status::Internal;
			if (!g_Zapit->setBouquetHidden(id - 1, hidden))
				return Status::Internal;
			return Status::Ok;
		}

		Status setBouquetLock(uint32_t id, bool locked)
		{
			if (!g_Zapit)
				return Status::Internal;
			if (!g_Zapit->setBouquetLock(id - 1, locked))
				return Status::Internal;
			return Status::Ok;
		}

		/* Read twice by the call below, before it changes what is in the half
		   and again before it puts that in order, and written once here so the
		   two cannot read the half differently. */
		void readHalf(size_t index, bool tv, ChannelIdList &out) const
		{
			// Before the lookup, so the bouquet cannot be freed between being
			// found and being read. An index nobody has any more answers with
			// the empty list, which is what a bouquet that has gone holds.
			CServiceManager::ChannelGuard guard;
			out.clear();
			if (index >= g_bouquetManager->Bouquets.size())
				return;
			const CZapitBouquet *b = g_bouquetManager->Bouquets[index];
			const ZapitChannelList &from = tv ? b->tvChannels : b->radioChannels;
			out.reserve(from.size());
			for (size_t i = 0; i < from.size(); i++)
				out.push_back((ChannelId) from[i]->getChannelID());
		}

		Status setBouquetChannels(uint32_t id, const ChannelIdList &ids, bool tv)
		{
			if (!g_bouquetManager || !g_Zapit)
				return Status::Internal;
			const size_t index = (size_t) id - 1;
			{
				CServiceManager::ChannelGuard guard;
				if (index >= g_bouquetManager->Bouquets.size())
					return Status::NotFound;
			}

			// What that half holds now, taken before anything is sent, so the
			// walk below is not reading the list it is changing.
			ChannelIdList held;
			readHalf(index, tv, held);

			/* Put in before taking out, and never the other way round. Taking the last
			   bouquet that holds a channel away from it takes the channel off the box,
			   so emptying the half first would lose every channel this bouquet was the
			   only holder of. A command that did not land stops the walk. */
			for (size_t i = 0; i < ids.size(); i++)
			{
				if (names(held, ids[i]))
					continue;
				if (!g_Zapit->addChannelToBouquet(index, (t_channel_id) ids[i]))
					return Status::Internal;
			}
			for (size_t i = 0; i < held.size(); i++)
			{
				if (names(ids, held[i]))
					continue;
				if (!g_Zapit->removeChannelFromBouquet(index, (t_channel_id) held[i]))
					return Status::Internal;
			}

			/* Without this the read below would be the half as it was, and
			   every position taken off it would name the wrong channel. */
			(void) awaitCommands();

			/* And now the order. Read again rather than worked out from the two loops
			   above, so that a command which did not take leaves the order wrong and
			   nothing else.

			   The move is the one the two bouquet editors on the screen make. There is
			   no command for it, so this is a write into the list from whichever thread
			   the request arrived on. It shuffles a list without changing its length, so
			   nothing it does can invalidate what another reader is holding; what a
			   reader can see is a part of one order and a part of the other. */
			ChannelIdList now;
			readHalf(index, tv, now);

			ChannelMoveList moves;
			channelOrderMoves(now, ids, moves);

			const unsigned char kind = tv ? ST_DIGITAL_TELEVISION_SERVICE
						      : ST_DIGITAL_RADIO_SOUND_SERVICE;
			/* The bouquet found again under the lock for every move rather
			   than held across the run: a reload between two of them frees it,
			   and a pointer taken once would be written through afterwards.
			   The locked form of the move, because the lock is already held
			   here and it is not one a thread may take twice. */
			for (size_t i = 0; i < moves.size(); i++)
			{
				CServiceManager::ChannelGuard guard;
				if (index >= g_bouquetManager->Bouquets.size())
					break;
				g_bouquetManager->Bouquets[index]->moveServiceLocked(
					(unsigned int) moves[i].from,
					(unsigned int) moves[i].to, kind);
			}
			return Status::Ok;
		}

		// Writes both bouquet files and renumbers what the box draws from. The
		// refusal below is the daemon's answer about the files themselves and
		// no longer only about whether the command reached it.
		Status saveBouquets()
		{
			if (!g_Zapit)
				return Status::Internal;
			if (!g_Zapit->saveBouquets())
				return Status::Internal;
			return Status::Ok;
		}

		// The channel numbers the lists are drawn from, built again from the bouquets as they now
		// are. It is what makes a change visible without a restart, and what rebuilds the bouquet
		// of everything no other bouquet holds.
		Status reloadBouquets()
		{
			if (!g_Zapit)
				return Status::Internal;
			if (!g_Zapit->renumChannellist())
				return Status::Internal;
			return Status::Ok;
		}

		/* Every command above comes back without waiting, the stack answering none of
		   them. What makes a read taken afterwards a read of what they did is that the
		   stack serves one connection at a time and in order: a command it does answer
		   has every command sent before it behind it. Asking what mode the box is in is
		   the smallest question there is, so that is the one sent here. The answer is
		   thrown away on purpose. */
		Status awaitCommands()
		{
			if (!g_Zapit)
				return Status::Internal;
			(void) g_Zapit->getMode();
			return Status::Ok;
		}
};

// The event manager takes its search item as a number and gives that number a
// name only in a GUI header, which this layer must not reach. Five is the
// widest of them: the name, the short text and the extended text together.
const char SEARCH_NAME_AND_TEXTS = 5;

/* The channel id is passed in rather than read off the event, because the two reads
   below disagree about what that field holds: the one that answers for a single
   channel stamps the key it was asked for, the one that answers for all of them
   stamps the key the event carries. Either way it is the key the guide files a
   channel under and never the identifier a channel route answers on. */
void fill(const CChannelEvent &src, ChannelId channel_id, EventInfo &dst)
{
	dst.event_id    = src.eventID;
	dst.channel_id  = channel_id;
	// The event manager's description is the event's name and its text is the
	// short text below it, which is the other way round from the names here.
	dst.title       = src.description;
	dst.description = src.text;
	dst.start       = src.startTime;
	dst.duration    = src.duration;
}

// The event manager holds its own read lock across each of these calls and
// copies out by value, so nothing here adds a lock of its own.
class RealEpgSource : public EpgSource
{
	public:
		Status eventsForChannel(ChannelId id, time_t from, time_t to,
					EventList &out) const
		{
			CChannelEventList events;
			CEitManager::getInstance()->getEventsServiceKey((t_channel_id) id, events);
			out.clear();
			out.reserve(events.size());
			for (size_t i = 0; i < events.size(); i++)
			{
				if (!eventInWindow(events[i].startTime, events[i].duration, from, to))
					continue;
				EventInfo ei;
				fill(events[i], id, ei);
				out.push_back(ei);
			}
			return Status::Ok;
		}

		Status currentEvent(ChannelId id, EventInfo &out) const
		{
			time_t now = time(NULL);
			// A clock that failed would leave every event ahead of now and
			// answer NotFound, which reads as a channel with nothing on it.
			if (now == (time_t) -1)
				return Status::Internal;

			CChannelEventList events;
			CEitManager::getInstance()->getEventsServiceKey((t_channel_id) id, events);
			for (size_t i = 0; i < events.size(); i++)
			{
				if (!eventCoversTime(events[i].startTime, events[i].duration, now))
					continue;
				fill(events[i], id, out);
				return Status::Ok;
			}
			return Status::NotFound;
		}

		Status search(const std::string &query, time_t from, time_t to,
			      size_t limit, EventList &out) const
		{
			CChannelEventList events;
			CEitManager::getInstance()->getEventsServiceKey(0, events,
								       SEARCH_NAME_AND_TEXTS,
								       query, true);
			out.clear();
			// What is kept and not what was matched. The guide is walked whole below this whatever is
			// asked for; what this can do is not build a second copy of the whole of it before the
			// layer above throws most of it away.
			out.reserve(events.size() < limit ? events.size() : limit);
			for (size_t i = 0; i < events.size() && out.size() < limit; i++)
			{
				if (!eventInWindow(events[i].startTime,
						   events[i].duration, from, to))
					continue;
				EventInfo ei;
				// The all channel read is the one that fills this field, and
				// it fills it with the key the guide filed the event under.
				fill(events[i], events[i].channelID, ei);
				out.push_back(ei);
			}
			return Status::Ok;
		}

		Status eventDetail(uint64_t event_id, time_t start, EventDetail &out) const
		{
			CEPGData held;
			if (!CEitManager::getInstance()->getEPGid((t_event_id) event_id,
								 start, &held))
				return Status::NotFound;

			out.event_id = held.eventID;
			// The identifier carries it, which is the only place this read can
			// answer it from: the event manager files an event under the
			// channel the identifier names and hands back no channel of its own.
			out.channel_id = (ChannelId) GET_CHANNEL_ID_FROM_EVENT_ID(held.eventID);
			out.title = held.title;
			// Two texts and not one: the event manager keeps the short text under info1 and the long
			// one under info2. The reads that answer lists put a slice of the long text into the short
			// field where an event has no short text, which is why they are not where these come
			// from.
			out.description = held.info1;
			out.long_description = held.info2;
			out.start = held.epg_times.startzeit;
			out.duration = held.epg_times.dauer;
			// Signed on the way out of the event manager and a count of years,
			// so the cast is through the unsigned byte: an age above the signed
			// range would otherwise arrive as an enormous number.
			out.rating = (unsigned char) held.fsk;
#ifdef FULL_CONTENT_CLASSIFICATION
			/* Several bytes where the event carried several classifications.
			   The first is the one the box's own screen reads and the one this
			   answers, so the two agree about what an event is. */
			out.genre = held.contentClassification.empty()
				? 0u : (unsigned char) held.contentClassification[0];
#else
			out.genre = (unsigned char) held.contentClassification;
#endif
			return Status::Ok;
		}
};

// The daemon's text fields arrive as fixed arrays over a socket and a short
// answer leaves the tail as it was, so the end is taken from the array rather
// than trusted to be marked in it.
std::string bounded(const char *field, size_t size)
{
	size_t len = 0;
	while (len < size && field[len] != 0)
		len++;
	return std::string(field, len);
}

void fill(const CTimerd::responseGetTimer &src, TimerInfo &dst)
{
	dst.id            = (uint32_t) src.eventID;
	dst.type          = (int) src.eventType;
	dst.channel_id    = (ChannelId) src.channel_id;
	dst.start         = src.alarmTime;
	dst.stop          = src.stopTime;
	dst.repeat        = (int) src.eventRepeat;
	dst.state         = (int) src.eventState;
	dst.announce      = src.announceTime;
	dst.repeat_count  = src.repeatCount;
	dst.epg_id        = src.epg_id;
	dst.epg_start     = src.epg_starttime;
	dst.apids         = src.apids;
	// The daemon reports what it holds, so a timer read always carries an
	// audio selection even where the kind of timer makes no use of one.
	dst.apids_set     = true;
	dst.recording_dir = bounded(src.recordingDir, sizeof(src.recordingDir));
	dst.standby_on    = src.standby_on;

	// The kind picks which of the three texts the daemon filled.
	switch (src.eventType)
	{
		case CTimerd::TIMER_RECORD:
		case CTimerd::TIMER_ZAPTO:
			dst.title = bounded(src.epgTitle, sizeof(src.epgTitle));
			break;
		case CTimerd::TIMER_REMIND:
			dst.title = bounded(src.message, sizeof(src.message));
			break;
		case CTimerd::TIMER_EXEC_PLUGIN:
			dst.title = bounded(src.pluginName, sizeof(src.pluginName));
			break;
		default:
			break;
	}
}

// Nothing here holds the client's request lock: every method of the client takes it for
// itself and it is not recursive, so composing two of them under one lock would end the
// process. What keeps that out of reach is that no guarded method of the client calls
// another. Each call sets a deadline for the whole of itself, the transport's own timeout
// bounding one wait where a call is made of several.
class RealTimerSource : public TimerSource
{
	public:
		Status list(TimerList &out) const
		{
			if (!g_Timerd)
				return Status::Internal;

			CBasicClient::Deadline bound(TIMER_CALL_TIMEOUT_MS);
			CTimerd::TimerList held;
			/* An answer cut short arrives as a shorter list, and a shorter list is what a
			   box with fewer timers looks like, so a list that is not whole is a failure
			   and not a list. */
			if (!g_Timerd->getTimerList(held))
				return Status::Internal;
			out.clear();
			out.reserve(held.size());
			for (size_t i = 0; i < held.size(); i++)
			{
				TimerInfo ti;
				fill(held[i], ti);
				out.push_back(ti);
			}
			return Status::Ok;
		}

		Status add(const TimerInfo &t, uint32_t &new_id)
		{
			if (!g_Timerd)
				return Status::Internal;

			CBasicClient::Deadline bound(TIMER_CALL_TIMEOUT_MS);
			CTimerd::CTimerEventTypes type = (CTimerd::CTimerEventTypes) t.type;
			CTimerd::CTimerEventRepeat repeat = (CTimerd::CTimerEventRepeat) t.repeat;

			// Each kind carries its own block and the daemon reads exactly as many bytes as that
			// kind's block has. Every field is written, because they are plain structures with no
			// initialisation of their own and the whole block goes on the wire.
			CTimerd::RecordingInfo recording;
			CTimerd::EventInfo event;
			CTimerdMsg::commandSetStandby standby;
			CTimerdMsg::commandRemind remind;
			CTimerdMsg::commandExecPlugin plugin;
			void *data = 0;

			switch (type)
			{
				case CTimerd::TIMER_RECORD:
					fillEvent(t, recording);
					recording.apids = eventApids(t);
					recording.eventID = 0;
					copyText(recording.recordingDir, sizeof(recording.recordingDir),
						 t.recording_dir);
					copyText(recording.epgTitle, sizeof(recording.epgTitle), t.title);
					data = &recording;
					break;
				case CTimerd::TIMER_ZAPTO:
				case CTimerd::TIMER_IMMEDIATE_RECORD:
					fillEvent(t, event);
					data = &event;
					break;
				case CTimerd::TIMER_STANDBY:
					standby.standby_on = t.standby_on;
					data = &standby;
					break;
				case CTimerd::TIMER_REMIND:
					copyText(remind.message, sizeof(remind.message), t.title);
					data = &remind;
					break;
				case CTimerd::TIMER_EXEC_PLUGIN:
					copyText(plugin.name, sizeof(plugin.name), t.title);
					data = &plugin;
					break;
				default:
					break;
			}

			int id = g_Timerd->addTimerEvent(type, data, t.announce, t.start, t.stop,
							 repeat, t.repeat_count);
			// The daemon numbers from one, so neither of the two answers below
			// can be a timer. Minus one is the client's own refusal of a
			// duplicate; zero is the daemon declining to build one.
			if (id < 0)
				return Status::Conflict;
			if (id == 0)
				return Status::Internal;
			new_id = (uint32_t) id;
			return Status::Ok;
		}

		Status modify(const TimerInfo &t)
		{
			if (!g_Timerd)
				return Status::Internal;

			CBasicClient::Deadline bound(TIMER_CALL_TIMEOUT_MS);
			bool taken;
			if (t.type == (int) CTimerd::TIMER_RECORD)
				taken = g_Timerd->modifyRecordTimerEvent((int) t.id, t.announce,
									 t.start, t.stop,
									 (CTimerd::CTimerEventRepeat) t.repeat,
									 t.repeat_count,
									 t.recording_dir.c_str());
			else
				taken = g_Timerd->modifyTimerEvent((int) t.id, t.announce,
								   t.start, t.stop,
								   (CTimerd::CTimerEventRepeat) t.repeat,
								   t.repeat_count);
			if (!taken)
				return Status::Internal;

			// A second exchange, deliberately outside the first: the audio
			// selection has a command of its own and the times do not carry it.
			// The daemon acknowledges it with nothing.
			if (t.apids_set && !g_Timerd->modifyTimerAPid((int) t.id, t.apids))
				return Status::Internal;
			return Status::Ok;
		}

		Status remove(uint32_t id)
		{
			if (!g_Timerd)
				return Status::Internal;

			CBasicClient::Deadline bound(TIMER_CALL_TIMEOUT_MS);
			// The daemon acknowledges a removal with nothing, so this says the
			// command went out and no more. Whether the timer is gone is read
			// back by the caller.
			if (!g_Timerd->removeTimerEvent((int) id))
				return Status::Internal;
			return Status::Ok;
		}

		Status now(time_t &out) const
		{
			time_t n = time(NULL);
			if (n == (time_t) -1)
				return Status::Internal;
			out = n;
			return Status::Ok;
		}

	private:
		// The audio selection the daemon should apply. Its own zero means the
		// box decides, which is also what a caller that named none wants.
		static unsigned char eventApids(const TimerInfo &t)
		{
			return t.apids_set ? t.apids : (unsigned char) TIMERD_APIDS_CONF;
		}

		static void fillEvent(const TimerInfo &t, CTimerd::EventInfo &e)
		{
			e.epg_id          = t.epg_id;
			e.epg_starttime   = t.epg_start;
			e.channel_id      = (t_channel_id) t.channel_id;
			e.apids           = eventApids(t);
			e.recordingSafety = t.recording_safety;
			e.autoAdjustToEPG = t.auto_adjust;
			e.channel_ci      = false;
		}

		// Truncates rather than refuses, because the daemon's field is the
		// limit and a caller that wrote a longer text still means the timer.
		static void copyText(char *field, size_t size, const std::string &text)
		{
			size_t len = text.size();
			if (len > size - 1)
				len = size - 1;
			std::memcpy(field, text.data(), len);
			std::memset(field + len, 0, size - len);
		}
};

RealTimerSource g_real_timer;

RealEpgSource g_real_epg;

RealChannelSource g_real;

} // anonymous namespace

void installRealChannelSource() { setChannelSource(&g_real); }

void installRealEpgSource() { setEpgSource(&g_real_epg); }

void installRealTimerSource() { setTimerSource(&g_real_timer); }

} // namespace coreapi
