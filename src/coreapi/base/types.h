/*
 * types.h - the value types the core API passes around
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

#ifndef __coreapi_types_h__
#define __coreapi_types_h__

#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#include <string>
#include <vector>

namespace coreapi
{

// Stated once: the low half carries the service, the high half the transport
// and the orbital position, so a 32 bit id names a different channel.
typedef uint64_t ChannelId;

/* The half of an identifier the programme guide keeps, leaving out the orbital
   position and the frequency above it. Two channels can share it, and where
   they do the guide holds one schedule for both. */
const ChannelId GUIDE_KEY_MASK = 0xFFFFFFFFFFFFULL;

enum class ServiceKind
{
	Unknown,
	Tv,
	Radio,
	WebTv,
	WebRadio
};

// The channel stack normalises every service to television or radio, and a url
// is what makes one playable without a tuner, so those two answer the whole
// range.
inline ServiceKind serviceKind(bool is_radio, bool has_url)
{
	if (is_radio)
		return has_url ? ServiceKind::WebRadio : ServiceKind::Radio;
	return has_url ? ServiceKind::WebTv : ServiceKind::Tv;
}

struct ChannelInfo
{
	ChannelId   id;
	// Channels that share their schedule carry a foreign id here, so it cannot
	// be derived from id.
	ChannelId   epg_id;
	// Signed, so an unset or negative position stays recognisable.
	int32_t     number;
	std::string name;
	// Set for the kinds that are played from a url instead of a tuner.
	std::string url;
	uint16_t    service_id;
	uint16_t    transport_stream_id;
	uint16_t    original_network_id;
	int         satellite_position;
	// Transponder key, not a frequency.
	uint16_t    freq_id;
	ServiceKind kind;
	bool        scrambled;
	bool        locked;

	ChannelInfo()
		: id(0), epg_id(0), number(0), service_id(0), transport_stream_id(0),
		  original_network_id(0), satellite_position(0), freq_id(0),
		  kind(ServiceKind::Unknown), scrambled(false), locked(false) {}
};

/* What a channel's picture is coded in.

   Unknown is not a codec but the answer for a channel whose stream layout the
   box has never read. A value of its own because the channel stack's own
   enumeration starts at nought with MPEG-2, which is also what a channel is
   constructed with, so down there a channel nobody has looked at and a channel
   carrying MPEG-2 are the same number.

   None is a channel carrying no picture at all, which is a radio service. Told
   apart from Unknown because one is knowledge and the other the lack of it.

   Mpeg4 and H264 are one value because the channel stack maps both stream types
   onto one, and so are HEVC and its scalable form. */
enum class VideoCodec
{
	Unknown,
	None,
	Mpeg2,
	H264,
	Hevc,
	Cavs
};

/* What one sound track is coded in.

   Mp2 is what the channel stack calls MPEG, layer I and layer II both; AacPlus
   is what it calls AACPLUS, which is AAC carried in LATM. Unknown is a track
   the box found and could not classify, which the stream layout allows: a
   private stream with none of the descriptors that would say what it is. */
enum class AudioCodec
{
	Unknown,
	Mp2,
	Ac3,
	Eac3,
	Aac,
	AacPlus,
	Dts,
	DtsHd,
	Lpcm
};

/* One sound track of a channel.

   description is what the stream says and not what this box would call it:
   usually a three letter language code, and the word the channel stack fills in
   where the stream named nothing. Left as it is, because translating it would
   be this layer deciding that a code it does not recognise means nothing. */
struct AudioTrack
{
	uint16_t    pid;
	AudioCodec  codec;
	std::string description;
	// The one the box is playing. At most one track carries it, and none does
	// while the box has not read the channel's stream layout.
	bool        selected;

	AudioTrack() : pid(0), codec(AudioCodec::Unknown), selected(false) {}
};

typedef std::vector<AudioTrack> AudioTrackList;

/* How a channel's streams are laid out, as far as the box knows.

   known is about the sound tracks: the box builds that list only when it reads
   a channel's stream layout off the transponder, which happens when it switches
   to the channel. A channel nobody has switched to has no list, and an empty
   list is what this carries rather than the single track a channel scan happens
   to have written down, which carries neither a codec nor a language and would
   read as a whole answer.

   video does not follow known. A scan run with the box's own pid option turned
   on writes the picture pid and its coding into the service list, so the
   picture can be known for a channel whose tracks are not. The option is off by
   default. */
struct ChannelStreams
{
	bool           known;
	VideoCodec     video_codec;
	uint16_t       video_pid;
	AudioTrackList audio;

	ChannelStreams() : known(false), video_codec(VideoCodec::Unknown), video_pid(0) {}
};

struct BouquetInfo
{
	uint32_t    id;
	std::string name;
	bool        hidden;
	bool        locked;
	bool        user_bouquet;
	// A bouquet keeps its television and radio members in separate lists.
	uint32_t    tv_count;
	uint32_t    radio_count;

	BouquetInfo()
		: id(0), hidden(false), locked(false), user_bouquet(false),
		  tv_count(0), radio_count(0) {}
};

/* Two identifiers, because the file is not named after the one every route
   names the channel by: the box looks for a file called after the lower forty
   eight bits. path is the name that was found, resolved where it leads when it
   is a link and empty when it is not, so two equal paths never reach a reader
   who would take the second for somewhere else. Both empty is a listing asked
   not to touch the filesystem. */
struct LogoInfo
{
	ChannelId   id;
	ChannelId   short_id;
	std::string name;
	std::string path;
	std::string resolved;

	LogoInfo() : id(0), short_id(0) {}
};

// The layers below keep the event's name under the name description and its
// short text under a name of its own, so carrying those names up would put the
// title in a field called description.
struct EventInfo
{
	// Carries the channel in its upper bits, so two events of one channel are
	// only distinguished by the lower ones.
	uint64_t    event_id;
	ChannelId   channel_id;
	std::string title;
	std::string description;
	time_t      start;
	unsigned    duration;

	EventInfo() : event_id(0), channel_id(0), start(0), duration(0) {}
};

typedef std::vector<EventInfo> EventList;

// truncated says the caller's limit was reached and the guide held more, which
// a list cannot say on its own: one exactly as long as the limit looks whole.
struct SearchResult
{
	EventList events;
	bool      truncated;

	SearchResult() : truncated(false) {}
};

/* A type of its own and not three more members on EventInfo: the reads that
   answer a list carry none of the three below, and members left at their
   defaults there would say this event carries no rating and belongs to no
   class. Reached by the pair naming one event, an event broadcast several times
   carrying one entry per showing. */
struct EventDetail
{
	uint64_t    event_id;
	/* The channel, named the way the channel routes name one. The identifier
	   this is read off carries only the half the guide keeps, so the reads that
	   answer it widen that half through the channel list; a channel this box no
	   longer carries is the one case where the half is all there is. */
	ChannelId   channel_id;
	std::string title;
	// The short text beside the name.
	std::string description;
	// The one a list read does not carry. Empty means the event has none,
	// because this read asks the guide for the event itself.
	std::string long_description;
	time_t      start;
	unsigned    duration;
	/* The least age the event is broadcast for, in years, as the guide works it
	   out from the ratings the event carries. Nought is no rating and not an
	   event for everybody: below this layer the two are one value. Years and
	   not the byte off the wire, which means different ages in different
	   countries. */
	unsigned    rating;
	/* The content descriptor's own byte: the broad class in the upper four bits
	   and the narrower one under it. Nought is no classification. The number
	   and not a name, the names being the box's own screen text, which moves
	   with a setting rather than with the broadcast. */
	unsigned    genre;

	EventDetail() : event_id(0), channel_id(0), start(0), duration(0),
			rating(0), genre(0) {}
};

// An event occupies its start, its end and everything between them, which is
// how the event manager picks what a channel is showing. Both rules below say
// it, so neither can place an event the other would not.
//
// Both ask where an event ends as a difference against its start and not as a
// start plus a duration: a time near the top of the range plus a duration wraps
// into the past. Each difference follows a comparison that rules out underflow.

// Closed at from and open at to, so windows laid end to end hold every moment
// once. An event belongs when it begins inside, or began earlier and had not
// ended when it opened. The first is asked before the second and not folded
// into it, so an event carrying no duration is placed by where it starts rather
// than dropped from every window.
inline bool eventInWindow(time_t start, unsigned duration, time_t from, time_t to)
{
	if (start >= to)
		return false;
	if (start >= from)
		return true;
	return (int64_t) duration >= (int64_t) from - (int64_t) start;
}

inline bool eventCoversTime(time_t start, unsigned duration, time_t when)
{
	if (start > when)
		return false;
	return (int64_t) duration >= (int64_t) when - (int64_t) start;
}

// In the numbering the daemon writes to its own file. Two is absent because the
// daemon keeps that slot for a kind it no longer has, and renumbering the rest
// would reinterpret every timer already stored. Remotebox is not one the daemon
// stores at all: it labels a timer read off another box, and it is here because
// that label travels in the same field.
enum class TimerType
{
	Shutdown        = 1,
	Zapto           = 3,
	Standby         = 4,
	Record          = 5,
	Remind          = 6,
	Sleeptimer      = 7,
	ExecPlugin      = 8,
	ImmediateRecord = 9,
	Remotebox       = 10
};

// The daemon's protocol is not symmetric: the last two fields can be sent to it
// and never come back.
struct TimerInfo
{
	uint32_t      id;
	// Held as a number and not as the enum above, because a timer file written
	// by another image can name a kind this build has no name for, and a read
	// that dropped those would report fewer timers than the box has.
	int           type;
	ChannelId     channel_id;
	// When the timer fires, and when it stops. A timer that has nothing to
	// stop carries no stop time at all rather than one equal to its start.
	time_t        start;
	time_t        stop;
	// One slot for three of the daemon's texts: the programme of a recording or
	// a zap, the words of a reminder, the plugin of an exec timer. Which of
	// them it holds follows from type, and nothing else distinguishes them.
	std::string   title;
	int           repeat;
	// What the daemon is doing with the timer, in its own numbering.
	int           state;
	// When the box announces the timer, which the daemon keeps separately
	// rather than deriving from start.
	time_t        announce;
	// How many times a repeating timer still runs. Zero is unlimited.
	uint32_t      repeat_count;
	uint64_t      epg_id;
	time_t        epg_start;
	// Which audio the recording takes, as the daemon's own bit set. Zero means
	// the box decides, so apids_set below says whether this field was meant.
	unsigned char apids;
	bool          apids_set;
	std::string   recording_dir;
	bool          standby_on;
	// Write only: the daemon applies them when the timer is made and does not
	// report them afterwards.
	bool          recording_safety;
	bool          auto_adjust;

	// Four fields of the daemon's answer are left out: channel_ci says whether
	// the channel is behind a module, and the three remotebox ones are filled in
	// by the timer screen after it has read another box, never by the daemon.

	TimerInfo()
		: id(0), type(0), channel_id(0), start(0), stop(0), repeat(0),
		  state(0), announce(0), repeat_count(0), epg_id(0), epg_start(0),
		  apids(0), apids_set(false), standby_on(false),
		  recording_safety(false), auto_adjust(false) {}
};

// The first three come from the hardware layer and name the device, the rest
// name the system on it. Vendor is beside model because one box family carries
// several names and the name alone does not say who built it.
struct BoxInfo
{
	std::string vendor;
	std::string model;
	std::string chipset;
	std::string image_version;
	std::string kernel;
	std::string hostname;
	// How long the box has been up, not the moment it came up, so that two
	// readings taken a minute apart differ by a minute.
	time_t      uptime;

	/* What is left of the two places a box runs out of, in bytes.

	   Bytes and not the units either is read in, because a caller holding a
	   page size and a block size is a caller doing this layer's arithmetic.
	   Nought for both members of a pair the box could not read at all, which is
	   why the total is carried beside the free: a free of nought under a total
	   of nought is a reading that failed, and under a total that is not it is a
	   place that is full.

	   The root filesystem is here because on the smallest box it is the
	   twenty-eight megabytes an image has to fit in. */
	unsigned long long memory_total;
	unsigned long long memory_free;
	unsigned long long root_total;
	unsigned long long root_free;

	BoxInfo()
		: uptime(0), memory_total(0), memory_free(0), root_total(0), root_free(0) {}
};

/* The pair of numbers is what names it: a box can carry several adapters and
   several frontends on each, and neither number alone is unique. Three flags
   and not one kind, because a frontend taking more than one is a single device
   and the one it happens to be tuned to would say it cannot receive the others.
   in_use and live differ: something holds a frontend while it records, and that
   is not the live one. */
struct FrontendInfo
{
	int         adapter;
	int         number;
	// What the driver calls it, which is the only name a frontend has.
	std::string name;
	bool        satellite;
	bool        cable;
	bool        terrestrial;
	bool        in_use;
	bool        live;

	FrontendInfo()
		: adapter(0), number(0), satellite(false), cable(false),
		  terrestrial(false), in_use(false), live(false) {}
};

/* The readings are raw and the scale beside them says what they are out of, so
   a reader wanting a percentage has both halves of the division. Turning them
   into one number here would be a presentation decision made for every reader
   at once, and the places in this tree that made it separately made it
   differently. The error rate has no scale, being a count the driver keeps.
   locked tells a weak signal from no signal. */
struct SignalInfo
{
	// Which frontend answered, in the two numbers a frontend list names it by.
	int      adapter;
	int      number;
	uint16_t strength;
	uint16_t snr;
	/* What the two above are out of. Carried rather than known, because it
	   belongs to whatever read them: a source reading a narrower range would
	   otherwise have every reading taken for a weak signal. */
	uint32_t full_scale;
	uint32_t bit_error_rate;
	bool     locked;

	SignalInfo()
		: adapter(0), number(0), strength(0), snr(0), full_scale(0),
		  bit_error_rate(0), locked(false) {}
};

// A value for every kind a directory can report. One short of that and a reader
// has to guess which kind it was folded into. Unknown is what a filesystem that
// does not keep the kind beside the name answers.
enum class FileKind
{
	Unknown,
	Fifo,
	CharDevice,
	Dir,
	BlockDevice,
	Regular,
	Link,
	Socket,
	Whiteout
};

// kind is what the directory reports; everything from attributes_read down is
// what one stat of that name answered, so a link carries the kind of a link and
// the size of what it points at. attributes_read says whether that stat
// succeeded: a name it did not, a broken link above all, stays in the listing
// with everything below at zero, and a reader has to leave those fields out
// rather than print the zeroes. The owner stays the numbers the kernel gave,
// turning one into a name being presentation and not thread safe.
struct FileEntry
{
	std::string name;
	FileKind    kind;
	bool        attributes_read;
	uint32_t    mode;
	uint64_t    nlink;
	uint64_t    size;
	time_t      mtime;
	uid_t       uid;
	gid_t       gid;

	FileEntry()
		: kind(FileKind::Unknown), attributes_read(false), mode(0), nlink(0),
		  size(0), mtime(0), uid(0), gid(0) {}
};

// free is what is still available rather than what is unallocated, because the
// reserve a filesystem keeps for the superuser is not room a recording can use.
struct MountInfo
{
	std::string device;
	std::string mountpoint;
	std::string fstype;
	uint64_t    total;
	uint64_t    free;

	MountInfo() : total(0), free(0) {}
};

// In bytes rather than in blocks: a reader wants the room, and the shape the
// kernel keeps it in is the kernel's business.
struct FsUsage
{
	uint64_t total;
	uint64_t free;

	FsUsage() : total(0), free(0) {}
};

/* The number and the name apart rather than printed into one string. The number
   is what the stream itself said and is the half that is certain; the name
   comes out of a table this program keeps. The name is empty for an identifier
   no row names, a guess being a statement about a system nobody here knows. */
struct CaidInfo
{
	uint16_t    caid;
	std::string system;

	CaidInfo() : caid(0) {}
};

/* Named here rather than carried as the number the box writes down, because
   that number is a bit in a mask the box uses to pick which plugins a menu
   shows, and a caller told a bit would be reading a menu's arithmetic. Unknown
   is a box that has learnt a sort this does not name. */
enum class PluginKind
{
	Unknown,
	Disabled,
	Game,
	Tool,
	Script,
	Lua
};

/* Two names, because the box starts a plugin by the name of its files and what
   the plugin calls itself is what a person reads. They are the same string
   often enough for one to pass for the other until a plugin whose title carries
   a space arrives. hidden is carried rather than used to drop the plugin here:
   what a menu shows is the menu's decision. */
struct PluginInfo
{
	std::string name;
	std::string title;
	std::string description;
	PluginKind  kind;
	bool        hidden;
	/* The file the box read this plugin out of, which is also the only place
	   the flag above can be written back to. Carried and never answered: it is
	   a path on the box and a caller has nothing to do with it. Empty for a
	   plugin whose source could not say where it came from. */
	std::string config;

	PluginInfo() : kind(PluginKind::Unknown), hidden(false) {}
};

typedef std::vector<ChannelInfo> ChannelList;
typedef std::vector<BouquetInfo> BouquetList;
typedef std::vector<LogoInfo> LogoList;
typedef std::vector<CaidInfo> CaidList;
typedef std::vector<TimerInfo> TimerList;
typedef std::vector<FrontendInfo> FrontendList;
typedef std::vector<PluginInfo> PluginList;

// Channels named and nothing more, for the one direction where a caller says
// which channels it means. A list of the whole of each would leave a caller
// deciding which of a dozen fields the box already holds it may disagree with.
typedef std::vector<ChannelId> ChannelIdList;

// Names and not numbers: the number behind a key is the input layer's own and
// is not the same one on two boxes.
typedef std::vector<std::string> KeyNameList;

/* Two, where the capture in this tree can write three. The third is a bitmap
   with nothing done to it, megabytes for a screen this size before any of it
   reaches a socket, and the reason anybody asks for anything but the first is
   that the answer should be smaller. A format the box cannot write is refused
   where a request is read. */
enum class PictureFormat
{
	Png,
	Jpeg
};


/* A list of these and not a single answer: this box records several channels at
   once, and a reader told about "the recording" would be told about whichever
   came first.

   id is the timer daemon's event id, the number the box ends a recording by,
   and it does not move while the recording runs. Neither the channel nor the
   file could take its place: two recordings of one channel are a thing this box
   does, and the file a shift is written to is renamed the moment somebody
   decides to keep it.

   from_timer says the daemon already held a timer when the box began. Every
   recording has one shortly after it begins, because the box makes one to carry
   the end time of a recording started by hand, so the timer that exists now
   says nothing and the answer is read where the recording begins; the daemon
   cannot be asked either, filing a timer made to run at once under the same
   kind as a scheduled one. size_known says the file could be reached at all,
   nought being a real size for a recording that has just begun. */
struct RecordingInfo
{
	uint32_t    id;
	ChannelId   channel_id;
	// What the guide called the programme when the recording began, and empty
	// for one begun with nothing in the guide to read.
	std::string title;
	// When the box began writing, and not when the timer was due: a recording
	// that started late started when it started.
	time_t      start;
	// The whole name of the file being written, not the directory holding it.
	std::string path;
	uint64_t    size;
	bool        size_known;
	// Whether this is the shift the box keeps of what it is showing, which is
	// a recording like any other here and is not one anybody asked to keep.
	bool        timeshift;
	bool        from_timer;

	RecordingInfo()
		: id(0), channel_id(0), start(0), size(0), size_known(false),
		  timeshift(false), from_timer(false) {}
};

typedef std::vector<RecordingInfo> RecordingList;

} // namespace coreapi

#endif
