/*
 * deps.h - the seams the core API is built on, and their wiring
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

#ifndef __coreapi_deps_h__
#define __coreapi_deps_h__

#include "result.h"
#include "schema.h"
#include "types.h"

#include <stddef.h>
#include <time.h>

#include <libeventserver/eventserver.h>

#include <string>

#include <driver/neutrino_msg_t.h>


namespace coreapi
{

// Injection points exist so the modules can be exercised on the host without a
// DVB stack. Reads answer with a status rather than a bool, because a bool
// cannot separate a missing bouquet manager from an absent bouquet.
struct ChannelSource
{
	virtual ~ChannelSource() {}
	virtual Status allChannels(bool tv, ChannelList &out) const = 0;
	virtual Status findChannel(ChannelId id, ChannelInfo &out) const = 0;
	/* The channel an identifier names and the key the guide files its events
	   under, which are two different numbers.

	   asked may be a whole identifier or its lower forty eight bits, since the
	   guide stores the lower half alone. whole is the identifier the channel
	   routes use; key is the channel's epg_id, which is another channel's
	   wherever a box maps one schedule onto two services. Two channels can
	   share the lower forty eight bits, and the implementation must settle that
	   the same way twice, because a caller reads one answer and then asks the
	   channel route about it. NotFound for an identifier the box has no channel
	   for. */
	virtual Status guideChannel(ChannelId asked, ChannelId &whole, ChannelId &key) const = 0;
	/* How one channel's streams are laid out. Apart from findChannel because a
	   listing copies a channel out of the map and a copy carries none of its
	   stream state by design, so this has to be read off the channel where it
	   sits and a listing of two thousand channels does not pay for it.

	   NotFound for a channel the box does not have. A channel it has and knows
	   no streams of is Ok with known unset. */
	virtual Status channelStreams(ChannelId id, ChannelStreams &out) const = 0;

	virtual Status allBouquets(BouquetList &out) const = 0;
	virtual Status bouquetChannels(uint32_t bouquet_id, ChannelList &out) const = 0;
	// NotFound while nothing is playing, Internal when the running id names a
	// channel the service manager does not have.
	virtual Status currentChannel(ChannelInfo &out) const = 0;
	// The mode the box is in, in the numbering the mode messages carry, which
	// is wider than television and radio: standby and the players are modes.
	virtual Status currentMode(int &out) const = 0;

	/* Which conditional access systems the running channel is scrambled under,
	   as the identifiers the stream carries. An empty list is a channel nothing
	   scrambles, NotFound a box with nothing playing.

	   The numbers alone: mapping them to systems is a table this program keeps,
	   applied a layer up. */
	virtual Status currentCaids(std::vector<uint16_t> &out) const = 0;

	/* Reads the box's channel lists again, which is what makes a list changed
	   underneath the box reach the screen without a restart.

	   save_services writes the services in memory out before reading, so memory
	   wins over disc; false lets the file win. The reason travels with it
	   because the box records why its web channel lists were reread. */
	virtual Status reloadChannels(bool save_services, const std::string &reason) = 0;

	/* Below this the same table is written, beside the reads and not behind a
	   seam of its own: a write to one object and a read from another would be
	   two views of one list.

	   A bouquet is named here by its position in allBouquets, counted from one
	   as BouquetInfo::id is. Callers name a bouquet by name, and findBouquet
	   below is the only place the two meet. */

	/* Which bouquet carries that name, or NotFound. What counts as carrying it
	   is stated where the answer comes from: the box keeps a bouquet's identity
	   and its printed caption in two fields.

	   Asked before every write below, so a new name's conflict and an old
	   name's absence are the same question asked once. */
	virtual Status bouquetByName(const std::string &name, uint32_t &id) const = 0;

	// At the end of the list. The caller has already found that nothing
	// carries the name, so an implementation does not ask again.
	virtual Status addBouquet(const std::string &name) = 0;

	// The name is the bouquet's identity and its printed caption at once, so an
	// implementation moves both or the box prints one name while answering to
	// another.
	virtual Status renameBouquet(uint32_t id, const std::string &name) = 0;

	// To a position the list has. The caller has already found that it does.
	virtual Status moveBouquet(uint32_t id, uint32_t to_id) = 0;

	virtual Status deleteBouquet(uint32_t id) = 0;
	virtual Status setBouquetHidden(uint32_t id, bool hidden) = 0;
	virtual Status setBouquetLock(uint32_t id, bool locked) = 0;

	/* What one half of a bouquet holds afterwards. One half at a time because a
	   bouquet keeps television and radio in separate lists.

	   Membership and order both: the numbers the box draws beside a channel
	   come out of that sequence.

	   Ordering moves a channel from where it is to where it belongs, never out
	   and back in: taking the last bouquet that holds a channel away takes the
	   channel off the box.

	   Every id has been looked up before this is called. */
	virtual Status setBouquetChannels(uint32_t id, const ChannelIdList &ids, bool tv) = 0;

	/* Writing the list out, and reading it back into what the box draws from.
	   Two calls because they fail for two causes, and a box that wrote the file
	   without rereading it is in a state a caller can see.

	   Every write above is in memory until these have run. The layer above runs
	   both after each write, so they are not the caller's to remember. */
	virtual Status saveBouquets() = 0;
	virtual Status reloadBouquets() = 0;

	/* Returns once the writes above have been carried out, and says nothing
	   about what they did.

	   None of them is answered by the side that carries it out, so a read taken
	   straight after one reads the list as it was. This is what belongs
	   between, and it is why the layer above can look for its own change
	   afterwards and believe what it finds.

	   Internal when there is no channel stack to wait on. */
	virtual Status awaitCommands() = 0;
};

ChannelSource &channelSource();
void setChannelSource(ChannelSource *s);

// Binds the accessors above to the running channel stack.
void installRealChannelSource();

// Defined by the application, because the mode is held by a class whose header
// reaches the GUI and this layer must not.
int applicationMode();

/* The same, for the flag that says the box still owes its user the question
   whether a channel list it noticed a change in should be reread. Rereading
   answers that question, so the flag comes down with it.

   A free function for the reason the two beside it are: the flag lives in a
   class whose header reaches the GUI, and a build holding no such flag still
   has to link. */
void clearPendingChannelReload();

// Reads answer with a status: an event manager that cannot be read at all and a
// channel with nothing scheduled are two answers, and only one is worth
// retrying.
struct EpgSource
{
	virtual ~EpgSource() {}
	/* Every event of the channel overlapping the window, in the order the event
	   manager keeps them.

	   The id is the guide's key, not the identifier the channel routes use. The
	   two part company on every channel a box maps another's schedule onto, and
	   converting needs the channel list, which this seam does not have. */
	virtual Status eventsForChannel(ChannelId id, time_t from, time_t to,
					EventList &out) const = 0;
	// The clock is the source's, so a caller can say what now is. An
	// implementation whose clock fails must answer with a status: a failed
	// clock puts every event in the future, and NotFound for that reports a
	// running channel as one with nothing on it.
	virtual Status currentEvent(ChannelId id, EventInfo &out) const = 0;
	/* Every channel the event manager holds, carrying the ids it stores rather
	   than the ones a channel list would widen them to.

	   The window and the ceiling are handed down so an implementation stops
	   copying once it has enough; ignoring them builds the whole match set
	   first, which on a full guide is the largest transient in this path.

	   An implementation must not return more than limit. The layer above asks
	   for one more than it means to hand back, so limit entries coming back is
	   how it learns there were more. */
	virtual Status search(const std::string &query, time_t from, time_t to,
			      size_t limit, EventList &out) const = 0;
	/* One event in full, named by its identifier and the moment it begins.

	   Its own read because the event manager holds it apart: the list reads
	   copy a record without the rating, the classification and the long text.

	   Both halves of the name are asked for because one identifier can carry
	   several showings. NotFound for an identifier the guide does not hold and
	   for one with no showing beginning then, because a caller does the same
	   thing about either. */
	virtual Status eventDetail(uint64_t event_id, time_t start,
				   EventDetail &out) const = 0;
};

EpgSource &epgSource();
void setEpgSource(EpgSource *s);

// Binds the accessors above to the running event manager.
void installRealEpgSource();

// The ceiling one call below waits under. Ten seconds against a server thread
// in this same process on a unix socket is enormously generous: a healthy
// exchange is microseconds, and only the daemon's own scan holding its event
// lock makes one slow. It bounds the whole call, not one wait inside it.
const unsigned TIMER_CALL_TIMEOUT_MS = 10000;

// The bound on one command, taken from the transport that carries it. A command
// goes over a unix socket in this same process and the loop at the other end
// reads it on the thread it draws on, so a command's cost to its sender is what
// that loop is busy with. A ceiling and not a wait: a loop that does not take
// the command inside this loses it rather than holding the sender.
const unsigned COMMAND_SEND_TIMEOUT_MS = EVENT_SEND_TIMEOUT_MS;

// The timer daemon is a server thread inside this process reached over a unix
// socket, so every call below blocks the calling thread until it answers.
//
// The transport's own timeout bounds one wait and not one call: a list read is
// one wait per timer, the retry path of a write waits per chunk, and the
// connect had no bound at all. An implementation must therefore set a
// CBasicClient::Deadline of TIMER_CALL_TIMEOUT_MS around each of these, which
// cuts every wait down to what is left of that moment and cuts off the connect
// with it. Sixty seconds, from TIMEOUT_SEC in lib/connection/basicclient.cpp,
// is what one wait may still take on its own.
//
// The lock keeping two callers from trading descriptors mid exchange is taken
// inside each of the client's own methods and is not recursive. Nothing here
// may hold it across two of them: what keeps the abort out of reach is that no
// guarded method of the client calls another.
//
// So a call composed of several exchanges is not atomic, and another thread can
// act between them.
struct TimerSource
{
	virtual ~TimerSource() {}
	// Every timer the daemon still holds, in the order it keeps them.
	virtual Status list(TimerList &out) const = 0;
	// The id the daemon gave the timer, which is never zero. Conflict when the
	// daemon refuses it as one it already has.
	virtual Status add(const TimerInfo &t, uint32_t &new_id) = 0;
	// Times, repeat and audio selection of a timer that exists. Its kind and
	// its channel are not carried by the daemon's protocol and do not move.
	virtual Status modify(const TimerInfo &t) = 0;
	// The daemon acknowledges nothing here, so an Ok says only that the
	// message went out. Whether the timer is gone has to be read back.
	virtual Status remove(uint32_t id) = 0;
	// The clock is the source's, for the reason it is in the event source: a
	// caller has to be able to say what now is, and a failed clock must answer
	// with a status rather than let every moment fall on one side of it.
	virtual Status now(time_t &out) const = 0;
};

TimerSource &timerSource();
void setTimerSource(TimerSource *s);

// Binds the accessors above to the running timer daemon.
void installRealTimerSource();

// What the box is and how loud it is. Behind an injection point because the
// hardware names come from a library this layer is not linked against on the
// host.
struct SystemSource
{
	virtual ~SystemSource() {}
	virtual Status boxInfo(BoxInfo &out) const = 0;
	// A percentage. The channel stack keeps one number for both sides and
	// answers it for either of them.
	virtual Status volume(int &out) const = 0;
	// The box's own flag and not the decoder's. See audioMuted below.
	virtual Status muted(bool &out) const = 0;

	/* How the box is wired to descramble, which is the three reads below.

	   Here rather than behind a seam of their own: nothing but this file
	   installs a seam, and a read whose install was forgotten ends the process
	   at the first request rather than answering.

	   Three reads and not one, so the walk over the slots belongs above and can
	   be driven with no slots at all. A box built without a common interface is
	   not a box whose module is absent. */

	// How many common interface slots the box has, nought where it has none.
	virtual Status ciSlotCount(unsigned &out) const = 0;
	// Whether a module is seated and ready in that slot. False for a number
	// above the count, which is a slot the box does not have.
	virtual Status ciModulePresent(unsigned slot, bool &out) const = 0;
	/* Whether the box is sending what it shows through a module rather than a
	   softcam. Decided when the channel was tuned and not a property of any
	   slot, which is why it is read apart from the two above: a box with a
	   module seated and this false is descrambling some other way. */
	virtual Status ciInUse(bool &out) const = 0;
};

SystemSource &systemSource();
void setSystemSource(SystemSource *s);

// Binds the accessors above to the running box.
void installRealSystemSource();

/* Whether the box is muted, defined by the application.

   This is the flag the mute command sets and everything drawing the box reads,
   deliberately not the decoder's own. Two paths turn the sound off without the
   decoder being told: a box driving the amplifier over the television link
   mutes at the amplifier, and a build with no audio hardware has a decoder flag
   nothing writes. Asking the decoder answers those two with sound that is on
   while the box shows the mute icon. */
bool audioMuted();

/* The catalog a label_key or an Enum choice's label_key names text under, read
   back in whatever language the box has loaded. Behind a seam because the
   catalog and the loaded language live in Neutrino globals, CLocaleManager and
   g_Locale, that this layer must not include.

   NotFound for a name the catalog carries nothing under, which covers a name
   nobody wrote and a box that has loaded no catalog at all. What it must never
   do is answer Ok with the key's own spelling: that is the defect this seam
   exists to keep out of the schema. */
struct LocaleSource
{
	virtual ~LocaleSource() {}
	virtual Status text(const char *key, std::string &out) const = 0;
};

/* Not one of the seams whose accessor ends the process when nothing is
   installed, for the reason the settings source is not: a box with no catalog
   wired in can still answer that the name resolves to nothing, and a build that
   only wants the schema can ask before installing anything. */
LocaleSource &localeSource();
void setLocaleSource(LocaleSource *s);

// Binds the accessor above to CLocaleManager and to whatever language g_Locale
// currently has loaded.
void installRealLocaleSource();

/* Where a setting's value is read and written. The two kinds are told apart at
   the seam rather than below it, because what a key means is known here and
   nowhere further down.

   A key nothing declares answers NotFound; a setting whose value this layer
   cannot reach answers NotSupported. A few settings live only in the settings
   file rather than in a field, and those are declarable but not readable. */
struct SettingsSource
{
	virtual ~SettingsSource() {}
	// NotFound is a key the store does not hold, which is not the same as one
	// it holds as zero or as an empty string. A caller falls back to what the
	// setting declares for the first and to nothing for the second, so a
	// source that answered them alike would silently reset a value.
	virtual Status readInt(const char *key, long &out) const = 0;
	virtual Status readString(const char *key, std::string &out) const = 0;
	/* A write is kept and reads back at once; where it goes after that is the
	   implementation's business. Saving is a call of its own so that a request
	   changing several settings costs one save. The source on the box carries
	   the writes and the save to the program's loop together, because a reload
	   arriving between them would throw the writes away. */
	virtual Status writeInt(const char *key, long value) = 0;
	virtual Status writeString(const char *key, const std::string &value) = 0;
	/* Ok says the box was asked to save, not that a file was written: the loop
	   that saves answers nothing, and waiting for it deadlocks.

	   Anything else says nobody was asked, and what this call would have
	   carried is taken back: those writes stop reading back and no later call
	   carries them in. Otherwise a caller told the value was not written would
	   see its refused write applied by the next write of some other setting.

	   What one call carries and what a refused one takes back is what the
	   calling thread wrote, so two callers neither promise nor drop each
	   other's values. A refusal cannot promise that nothing landed: another
	   caller's message may be draining the store as this one is refused. */
	virtual Status persist() = 0;
};

/* The one accessor here that answers with nothing installed instead of ending
   the process. What the settings are is declared in this layer and readable
   with no store behind it, so NotSupported is an answer a case can drive where
   an abort is not.

   What keeps a forgotten installation from being silently answered that way is
   the check over the application in the tests, which refuses a build whose
   startup does not call every install below. */
SettingsSource &settingsSource();
void setSettingsSource(SettingsSource *s);

/* Binds the accessors above to the values the program is running on and to the
   program's own save. The table saying where each value lives is read through
   the one seam this layer already has for it, so a key lookup and the field
   lookup behind it cannot be looking at two tables.

   The values and not the settings file: between a load and a save the file is a
   copy of what was last written, so a read of it answers a setting the box has
   already changed and a write to it is undone by the next save. */
void installRealSettingsSource(SNeutrinoSettings *values, bool (*save)());

/* How a written setting takes effect. Every other seam here is reached
   downwards, this one upwards on purpose: what applies a change is a notifier a
   setup screen owns, and this layer may not reach into the screens.

   A section at a time, because one notifier stands behind a whole screen and
   the key tells it which setting moved.

   Called on the thread that carries the write into the program's settings and
   after it has landed, so an applier may read the new value and touch what that
   thread owns. Not on the thread that asked for the write: that one is a worker
   of the web layer.

   apply() answers whether the applier knew the key. It reaches no caller,
   because the write was answered before this runs. A virtual call and not a
   function object, so that nothing here allocates. */
struct SettingsApplier
{
	virtual ~SettingsApplier() {}
	virtual bool apply(const char *key) = 0;
};

/* How many sections may have one at once. The registry is an array of this size
   rather than a map, so registering allocates nothing and cannot fail for want
   of memory in the middle of startup. */
const size_t SETTINGS_APPLIER_LIMIT = 32;

/* Puts a on the section, replacing whoever was there, or takes the section's
   applier away when a is NULL. Undoing a registration that was never made is Ok
   rather than an error.

   The name is kept and not copied, because nothing here allocates, so it has to
   outlive the registration. InvalidArgument for a section with no name;
   Internal when the table is full, which is a fault here and not at the
   caller. */
Status registerSettingsApplier(const char *section, SettingsApplier *a);

// Who applies that section, or NULL for one nobody registered for. Not an
// error: a setting that nothing running has to be told about is written and
// that is the whole of it.
SettingsApplier *settingsApplier(const char *section);

/* The other half of that source, run by the message loop when the command
   persist() sends arrives: it puts what was written into the values, asks for
   the save, and then asks whoever applies each written setting. All three on
   the loop's thread, and in that order, so that nothing is applied before the
   value it stands for is there to be read.

   Does nothing while no real source is installed. */
void applyPendingSettings();

// Commands are posted and forgotten. The message loop is reentrant from about a
// hundred places and the socket an answer would travel on is accepted by the
// same thread that would have to send it, so waiting for one deadlocks.
//
// The message and its data keep the loop's own widths: a message number is
// above the signed range of an int, and narrowing one turns it into a different
// message.
struct CommandSink
{
	virtual ~CommandSink() {}
	/* Whatever the data word points at belongs to the sink from an Ok answer
	   onwards, because the loop frees what it was handed. On any other answer
	   it belongs to the caller again, with one exception: the queue frees a
	   refused block itself for the messages whose number says they carry one.
	   Busy is what says the queue turned this message away, so both directions
	   have to hold. An implementation must not report Busy for anything the
	   queue never saw, or the block is released by nobody; and it must report
	   Busy and nothing else when the queue did turn the message away, or the
	   block is released twice. A sink answering Ok without queuing leaves the
	   block with nobody.

	   A status rather than a bool: a refused queue and a box with no message
	   loop are two answers, and a caller can retry only one. */
	virtual Status post(neutrino_msg_t msg, neutrino_msg_data_t data) = 0;
};

CommandSink &commandSink();
void setCommandSink(CommandSink *s);

// Binds the sink above to the running message loop.
void installRealCommandSink();

/* The second way a command reaches the message loop, and the one the web layer
   has always used. It is not interchangeable with the sink above.

   A command sent this way is written to the loop's own unix socket. The loop
   reads a header carrying an event number and an initiator, and picks which
   table it reads that number through by the initiator. So the initiator is part
   of the command rather than a label on it, and a sink carrying a message and a
   data word cannot send one.

   Delivery differs too. A send blocks until the socket takes the connection, so
   an unreachable loop stalls the sender instead of refusing it. What comes back
   says whether a connection was made, not whether what was written arrived.

   An event nobody registered is refused here rather than sent, because the send
   itself walks an empty client list without a word and the command would
   vanish. The list below is what may be sent, and the registration is derived
   from it. */
enum class BoxEvent
{
	StandbyOn,
	StandbyOff,
	Shutdown,
	Reboot,
	Restart,
	HdmiCecViewOn,
	HdmiCecStandby,
	Hint,
	Message,
	SetVolume,
	SetMute,
	LockRemote,
	UnlockRemote,
	StartPlugin,
	ReloadPlugins,
	ReloadSetup,
	/* Begin and end the shift the box keeps of what it is showing. Two commands
	   and not one flag, because a caller that toggled would be acting on state
	   it read a moment ago. The loop is where each is refused if the box is
	   already in that state, that being the only place the question and the act
	   are not separated by a thread. */
	StartTimeshift,
	StopTimeshift,
	// Not an event. The number of them, which is what the registration counts.
	Count
};

// The number the event travels under, or zero for Count and for anything cast
// in from outside the enum. One table, so that a new entry above without one
// there does not compile; that the table is in this order is pinned by a case
// per command and not by the compiler.
unsigned eventNumber(BoxEvent e);

struct EventSink
{
	virtual ~EventSink() {}
	// The body is copied out before this returns and nothing here owns it.
	// NotSupported for an event the sink was not built to carry.
	virtual Status send(unsigned event_id, const void *body, size_t size) = 0;
};

EventSink &eventSink();
void setEventSink(EventSink *s);

// Binds the sink above to the running message loop's event socket.
void installRealEventSink();

// The two calls below are how a module sends a command, so that no module has
// to know how the loop carries one.

// The data word is passed on as it stands and nothing here owns it. For the
// messages whose number tells the queue they carry a block, a refusal means the
// queue has already released whatever the word pointed at, so a caller that
// allocated one and releases it on failure releases it twice. Pass those
// through postPayload instead, which owns the block from end to end.
Result<void> postCommand(neutrino_msg_t msg, neutrino_msg_data_t data);

// For the messages whose data word is a block the loop reads an event out of
// and then frees. The bytes are copied into a block of the right kind here. A
// refused post releases it again, except for the messages whose number tells
// the queue they carry one: those the queue has already released, and a second
// release would end the process.
Result<void> postPayload(neutrino_msg_t msg, const void *payload, size_t size);

// How a module sends one of the events the loop reads off its socket. Only the
// ones named above are carried, and anything else is refused here rather than
// sent. The body is written to the socket before this returns, so a caller may
// hand it a local. An ok answer says a connection was made and the bytes were
// written to it. It does not say they arrived, because the write's own answer
// is discarded a layer below.
Result<void> postEvent(BoxEvent e, const void *body = 0, size_t size = 0);

/* Below this, one place per subject for the seams the layers above still have
   to be given.

   Kept apart on purpose. A seam is a struct of pure virtual calls, an accessor,
   and the call that installs the object binding it to the running box. Written
   where the last one happened to end, two written at the same time are two
   rewrites of the same lines. The places stay after they are filled because
   what they group by is what a reader looks a seam up by. */

/* Writing bouquets: making one, taking one away, moving one, and saying what is
   in one. On the channel source above, beside the reads of the same things,
   because one table read through two objects is two tables to a caller that
   writes and reads back. */

// The remote control and the screen. Both are behind a seam for the reason the
// channel stack is: the objects that drive them are not linked into a build
// that only wants this layer, and a direct call would pull the whole driver in
// behind it or fail to link.

/* The remote control, as the two things about it that no message can carry.

   Two calls because on the box they have nothing to fail over. A key leaves as
   an event written to an input node, and what stops it is that node missing or
   unwritable. Whether the box is ignoring the remote is a flag the program
   holds, and what stops that reading is a build holding no such flag.

   Locking and unlocking are not here: both are messages to the loop that owns
   the flag. The reading has no message to go by, because nothing answers one. */
struct InputDevice
{
	virtual ~InputDevice() {}
	/* The key in the numbering of the table the input layer is generated from,
	   and not in the box's own. Turning one into the other belongs to whatever
	   writes to the node, which is the only place that knows which node it
	   is. */
	virtual Status sendKey(unsigned long code) = 0;
	virtual Status locked(bool &out) const = 0;
};

InputDevice &inputDevice();
void setInputDevice(InputDevice *d);

// Binds the accessors above to the box's own input node and to whatever holds
// the flag.
void installRealInputDevice();

// Defined by the application, because the flag is held by a screen whose header
// reaches the GUI and this layer must not. False is a box that is not ignoring
// its remote control, which is also what a build holding no such flag answers.
bool remoteControlLocked();

/* Taking a picture of what the box is showing, which is two pictures of two
   different things.

   Two calls: the television picture comes off the video decoder and the
   framebuffer, the small display on the front of the box off whatever buffer
   drives it. Most boxes have no second display, so that one answers
   NotSupported about the box rather than failing, and through one call the two
   could not be told apart.

   Where the picture goes is handed down so it is settled in one place; an Ok
   says there is a readable file of that name. */
struct ScreenshotSource
{
	virtual ~ScreenshotSource() {}
	/* Which halves go in, what the box drew over the picture and the picture
	   itself, and which of the two forms the file is written in. The format
	   travels with the capture rather than being applied afterwards, because
	   nothing between here and the encoder holds the picture as pixels. */
	virtual Status captureScreen(bool osd, bool video, PictureFormat format,
				     const std::string &path) = 0;
	virtual Status captureDisplay(const std::string &path) = 0;
};

ScreenshotSource &screenshotSource();
void setScreenshotSource(ScreenshotSource *s);

// Binds the accessors above to the box's own screen and display.
void installRealScreenshotSource();

/* The tuner: which the box has, what each is doing, and what it is locked to.

   A box with no tuner at all is a box these answer about rather than one they
   fail on, and it is also what every build that only links this layer looks
   like: an empty list of frontends, and a refusal for the reading that says the
   box does not do this rather than that something here broke. */
struct TunerSource
{
	virtual ~TunerSource() {}
	// Every frontend the box has, in the order the frontends are kept in. A box
	// with none answers Ok and an empty list, that being what having none is.
	virtual Status frontends(FrontendList &out) const = 0;
	/* What the frontend carrying the picture is measuring.

	   NotSupported when no frontend is carrying one, which covers a box with no
	   tuner and a tuner given nothing to receive. Anything else says the
	   frontends could not be reached at all, and only that is a fault. */
	virtual Status liveSignal(SignalInfo &out) const = 0;

	/* Puts the tuner stack down and brings it back up, and retunes the running
	   channel, which is what a viewer does about a picture that has gone.

	   The one call here that changes something, and not const for that reason.

	   Ok says the box was asked and took it, and says nothing about whether a
	   picture returned: the tuning happens on the channel stack's own thread
	   and is answered for by the reading beside this. */
	virtual Status reset() = 0;
};

TunerSource &tunerSource();
void setTunerSource(TunerSource *s);

// Binds the accessors above to the frontends the box has.
void installRealTunerSource();

// Reading and writing the programme file, for whatever of it cannot be done
// through the seams above.

/* The plugins the box carries: which it has, and asking for one to be run. Its
   own subject rather than a corner of the box source, because what it answers
   about is a set of scripts found on a disc and rescanned while the box runs. */
struct PluginSource
{
	virtual ~PluginSource() {}
	/* Every plugin the box found the last time it read its directories, in the
	   order it keeps them. A box carrying none answers Ok and an empty list; a
	   build holding no plugin list at all is what a status other than Ok is
	   for.

	   Reading only. Asking the box to reread its directories is a message and
	   not a call: this answers out of the list the screens are drawing from,
	   and rebuilding it under them from whatever thread asked is a race. */
	virtual Status list(PluginList &out) const = 0;
};

PluginSource &pluginSource();
void setPluginSource(PluginSource *s);

// Binds the accessor above to whatever holds the plugins the box found.
void installRealPluginSource();

/* Defined by the application, and read by nothing else, for the reason the
   remote control's flag is: what holds the plugins is an object whose header
   reaches the GUI. The seam above is what a case puts something else behind;
   this is what the one real thing behind it is made of.

   Ok and an empty list is a box carrying no plugins. Anything else says the
   list could not be reached at all. */
Status boxPlugins(PluginList &out);


/* The picture a channel is shown with, which is a file found under a name
   derived from the channel and not anything the channel list holds. Its own
   subject: one seam answering both would answer about the filesystem under a
   name that says it answers about channels, and a build wanting the channel
   stack and not the picture would have to link both. */

struct LogoSource
{
	virtual ~LogoSource() {}
	/* Where the picture of that channel is, or NotFound for a channel with
	   none. The search asks a few directory names in turn whether a file of
	   that name can be read.

	   One call and not two, because both halves would be the same walk over the
	   same directories and neither could fail where the other did not.

	   The name is handed in beside the identifier because the search uses both,
	   and only the layer above knows which name the box has for that channel.
	   An empty one narrows the search to the identifier rather than stopping
	   it.

	   The link a path may be is left as it is. Following it is filesystem work
	   with no hardware under it, so it belongs where a case can drive it. */
	virtual Status logoFor(ChannelId id, const std::string &name,
			       std::string &path) const = 0;
};

LogoSource &logoSource();
void setLogoSource(LogoSource *s);

// Binds the accessor above to the directories the box keeps its pictures in.
void installRealLogoSource();

/* What the box is writing to disc right now, and the one act on it that is not
   a message: asking that one of them stop.

   Its own subject rather than a corner of the timer source. A timer is a row in
   a file saying what the box will do; a recording is a file being written and a
   tuner being held, and the daemon that keeps the first knows neither. They
   overlap only in that the box names a running recording by the timer it
   carries, which is why ending one is asked of the daemon and listing them is
   asked of the program. */
struct RecordingSource
{
	virtual ~RecordingSource() {}
	/* Every recording the box is taking, in the order of the numbers that name
	   them. A box recording nothing answers Ok and an empty list.

	   File size is not among what this answers. That is a question for whatever
	   holds the file, and a seam answering it would be asking the filesystem
	   from inside a lock the box's own thread needs back. */
	virtual Status list(RecordingList &out) const = 0;

	/* Asks that the recording of that id end.

	   Asked of the timer daemon and not of the loop, which keeps the two sides
	   of a recording from coming apart: the daemon holds the timer that carries
	   the recording's end, and a stop reaching only the loop would leave that
	   timer running against a recording that had gone.

	   So an Ok says the daemon was asked, not that anything has stopped.
	   NotFound is not among what it says, because whether such a recording
	   exists is read from the list above before this is reached. */
	virtual Status stop(uint32_t id) = 0;
};

RecordingSource &recordingSource();
void setRecordingSource(RecordingSource *s);

// Binds the accessors above to the record manager and the timer daemon.
void installRealRecordingSource();

/* Defined by the application and read by nothing else, for the reason the
   plugins above are: what holds the recordings is a class whose header reaches
   the GUI.

   Ok and an empty list is a box recording nothing. Anything else says what
   holds them could not be reached at all. */
Status boxRecordings(RecordingList &out);


/* Whether every seam above whose accessor ends the process when nothing is
   installed has something behind it.

   Twelve of them do: the channel, event, timer, tuner, plugin and recording
   sources, the box source, the input device, the screen, the channel pictures,
   and the two sinks. Reaching one of those with nothing installed is an abort
   and not an error a caller can be told about, so a consumer that answers
   requests has to be able to ask before it starts answering. The settings
   source is not among them because its accessor answers rather than aborting.

   Read from anywhere and written only by the install calls, which run once
   during startup. True says the installs have run, not that a tuner or a daemon
   behind any of them is well. */
bool dependenciesInstalled();

/* How long before a programme a recording starts and how long after it stops.
   The timer daemon holds the pair; the settings struct has two members under
   those names, but they are the buffer the setup screen fills as it opens,
   src/gui/record_setup.cpp:302, so nothing but this answers what the box is
   really running on.

   Seconds, because that is what the daemon keeps. The API offers minutes, and
   the conversion is made where the row is declared.

   Both at once in either direction, because the daemon takes them together: a
   call writing one would have to state the other, and stating it from anything
   but the daemon's own answer overwrites a value nobody asked about.

   The calls block, for the reason the timer source blocks. Nothing may hold a
   lock of this layer across one. */
struct RecordingSafetySource
{
	virtual ~RecordingSafetySource() {}
	virtual Status read(int &before, int &after) const = 0;
	virtual Status write(int before, int after) = 0;
};

/* Answers rather than ending the process while nothing is installed, as the
   settings source does: a build with no daemon behind it can still be asked
   what a row of this kind stands for. */
RecordingSafetySource &recordingSafetySource();
void setRecordingSafetySource(RecordingSafetySource *s);

// Binds the accessor above to the running timer daemon.
void installRealRecordingSafetySource();

/* The values a setting offers, where the box numbers them itself and the screen
   builds the list as it opens. A list written into a table would be right for
   one box model and quietly wrong for the next.

   Reached upwards like the applier above: the lists are the screens' own and
   this layer may not reach into the screens.

   NotFound for a name nobody registered, which is not an empty list: a set that
   is really empty is a setting the box offers nothing for, and one nobody
   answers for is a row whose values cannot be told at all. Either way no value
   is accepted for it.

   The text is what a person reads, already in the box's language. Not a name
   for the catalog to resolve: half of these lists carry words the catalog has
   no name for at all, "1080p 50Hz" among them. */
struct SettingChoices
{
	virtual ~SettingChoices() {}
	virtual Status values(const char *name, std::vector<SettingChoice> &out) const = 0;
};

SettingChoices &settingChoices();
void setSettingChoices(SettingChoices *s);

/* Which of the sizes the box draws its own screen at. The settings struct has a
   member under that name, filled at load, but it is not what the program saves:
   the save writes the copy another object keeps, src/neutrino.cpp:2026, because
   the running value can be forced down to the smaller size while the copy keeps
   what was chosen. A row over the member would be a value the next save quietly
   dropped.

   The mode and not a position in a list. The two the program has are
   src/gui/osd_helpers.h:7, and every driver-built resolution list is in that
   order, so a position happens to be the same number; what reads the value
   reads it as a mode, src/neutrino.cpp:1431.

   Reached upwards like the applier and the value sets above. A write changes
   what the box is drawing at once, on the calling thread, which is the thread
   the setup screen does the same thing on. */
struct OsdResolutionSource
{
	virtual ~OsdResolutionSource() {}
	virtual Status read(int &mode) const = 0;
	virtual Status write(int mode) = 0;
};

OsdResolutionSource &osdResolutionSource();
void setOsdResolutionSource(OsdResolutionSource *s);

} // namespace coreapi

#endif
