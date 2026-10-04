/*
 * channels.h - channels, bouquets and zapping, above the zapit client
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (additions, fixes)
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

#ifndef __coreapi_channels_h__
#define __coreapi_channels_h__

#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include <string>

namespace coreapi
{
namespace channels
{

// tv is the channel stack's own split, radio against everything else, so it
// picks two kinds at once: true is Tv with WebTv, false is Radio with WebRadio.
// Channels the stack marks absent are left out.
Result<ChannelList> list(bool tv);

Result<ChannelInfo> get(ChannelId id);

// An idle box is an answer, not a fault, so this is NotFound and never a null
// channel handed to the caller.
Result<ChannelInfo> current();

// current(), but NotFound in standby and while a recording or a file plays, where
// the channel stack still names one.
Result<ChannelInfo> playing();

/* How that channel's streams are laid out, as far as the box holds it.

   Apart from the channel, because a channel is answered by the thousand and this
   is read off the one channel it is asked about: a listing that carried it would
   read the track list of every channel on the box to answer a page of a hundred.

   NotFound for a channel the box does not have. A channel it has and has never
   switched to answers Ok with nothing known: the box reads a channel's stream
   layout when it plays it, and what a scan writes down beforehand carries no
   sound tracks at all. */
Result<ChannelStreams> streams(ChannelId id);

Result<BouquetList> bouquets();

// Bouquets are numbered from one.
Result<ChannelList> bouquetChannels(uint32_t bouquet_id);

enum class Mode { Tv, Radio };

// The two calls below are commands, posted and forgotten: an ok says the queue
// took the message and nothing about what the box did with it.

// The channel is looked up before the message goes out, because a posted command
// reports nothing back and a zap to an id nobody has would otherwise be
// unobservable. NotFound then, and nothing is posted. Conflict while a recording
// holds the tuner the channel needs, while the movie player plays a file unless
// stop_playback is set, and in standby unless wake is set, in that order.
Result<void> zap(ChannelId id, bool wake, bool stop_playback);

// Conflict in standby unless wake is set.
Result<void> setMode(Mode m, bool wake);

// The standby rule of the two above, for any other command the loop wakes the box for.
Result<void> standbyAllows(bool wake);

// A read, and wider than the enum above: standby and the players are modes as
// well. A NeutrinoModes value, src/include/neutrinoMessages.h, and never
// carrying the rezap flag above mode_mask, so a caller compares it whole.
// NotFound while the box has not settled on a mode, which it passes at startup.
Result<int> mode();

/* A path on the box's own filesystem and never the picture itself: nothing here
   opens the file. NotFound for a channel that has no picture. A channel the list
   does not hold is not refused here, because the box names a picture file after
   the channel's identifier as well as after its name, so one can be there for a
   channel the list has since lost. */
Result<std::string> logo(ChannelId id);

/* files says whether the filesystem is asked at all, and it is the caller's to
   say: asking is a walk of the picture directories once per channel, and a box
   carrying a full satellite list has thousands of them. A channel with no
   picture stays in the list with both of its path members empty. */
Result<LogoList> logos(bool tv, bool files);

/* Each as the identifier the stream carries and the name this program has for it
   where it has one. An empty list is a channel nothing is scrambling, NotFound a
   box with nothing playing, as playing() reads it: two answers, because a caller
   comes back for the second and not for the first. */
Result<CaidList> currentCaids();

/* hard says the services the box is holding are not written out first, so what
   is on the disc wins over what is in memory. A caller that has just changed
   something through this server means the other one, which is why that is what
   an unset flag asks for. */
Result<void> reloadChannels(bool hard);

/* Editing bouquets. Every call below names a bouquet by its name and never by
   its position in the list: a position is what the box renumbers the moment
   anything before it is taken away or moved, while a name stays with the bouquet
   it was given to. Names are unique, and these calls are what keeps them so.

   Every one of them writes the list out, waits for the box to have carried the
   change out, and looks for that change in the list. So an ok is the list
   carrying what was asked for and not a command having been written to a
   socket, and it covers the files the list is saved to as well. A save that
   failed is answered as bouquet-not-saved when the list carries the change,
   and said on the event bus like an ok: the box carries a change out before it
   is asked to save it, so the list it runs has the change and only the files
   may not. The next write that saves puts it in the files too, every save
   writing the whole list.

   The cost is paid per call, five changes being five writes and five reads. */

// Conflict for a name a bouquet already carries.
Result<void> addBouquet(const std::string &name);

/* The key moves with the name, so a caller holding the old one holds nothing
   after this. NotFound for a name nothing carries and Conflict for one something
   else already does. */
Result<void> renameBouquet(const std::string &from, const std::string &to);

// Up is towards the front of the list, which is where the box starts drawing
// it.
enum class Direction { Up, Down };

// One place at a time, and refused at either end of the list rather than
// answered ok having moved nothing.
Result<void> moveBouquet(const std::string &name, Direction d);

Result<void> deleteBouquet(const std::string &name);

Result<void> setBouquetHidden(const std::string &name, bool hidden);
Result<void> setBouquetLock(const std::string &name, bool locked);

/* What one half of the bouquet holds afterwards. The half is named because a
   bouquet keeps its television and its radio members apart, and a call taking
   both would make a caller meaning one hand the other back. Membership and order
   both: the numbers the box draws beside a channel are counted off that order.
   Every channel named is looked up before anything is changed, so a list naming
   one the box does not have is refused with the bouquet as it was.

   The box forgets a channel that leaves the last bouquet holding it. A list that
   leaves such a channel out is refused as channel-would-be-removed, with the
   bouquet as it was, unless remove_from_box says to take it off the box. */
Result<void> setBouquetChannels(const std::string &name, const ChannelIdList &ids, Mode kind,
                                bool remove_from_box = false);

/* Says on the event bus that the bouquets are not what a reader was last handed,
   and waits for the box before saying it. Every call above ends with this, and
   so does every screen that writes the bouquets out through the channel daemon
   past these calls: the bouquet editor when it saves or throws its changes away,
   and the favourites. A reload of the channel lists is said by the message
   bridge, off the box's own message, whoever asked for it. What the legacy
   control API and the command line client change command by command, and save,
   goes unsaid.

   The waiting cannot be left out. The box answers a write of the lists as soon as
   the files are out and numbers the lists again after answering, so a reader
   woken before that reads the change with the numbering and the bouquet of
   everything no other bouquet holds from before it. */
Result<void> announceBouquetsChanged();

/* The saying without the waiting, for a screen that wrote the bouquets out
   itself on the main loop, past the channel daemon: the channel list once its
   edit mode or the bouquet list changed something. There is nothing of the
   box's to wait for, and waiting would put the main loop behind whatever the
   daemon is doing, a zap the list itself asked for among it. */
void publishBouquetsChanged();

} // namespace channels
} // namespace coreapi

#endif
