/*
 * errors.h - the error codes the core API answers with
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

#ifndef __coreapi_errors_h__
#define __coreapi_errors_h__

namespace coreapi
{

/* The machine readable half of an error and the whole of what a consumer
   branches on: Status says which class of thing went wrong, this says which
   thing.

   Two rules hold across the set: everything a lookup did not find is
   no-such-something, and every code answers exactly one outcome.

   The strings are the contract and the enumerators are not, so a change to one
   of them once a frontend reads it is a break. The case in the tests that walks
   every code keeps this list and the projection below from drifting apart, and
   the walk is held to this enum by a scan of it. */
enum class ErrorCode
{
	// Nothing of that name.
	NoSuchChannel,
	NoSuchBouquet,
	NoSuchTimer,
	NoSuchDaemon,
	NoSuchVerb,
	NoSuchCommand,
	/* A path this server does not route, and a method it does not answer at a
	   path it does route. Two codes, because a caller either asks for a path
	   that is there or asks for this one the way it answers. Apart from
	   NoSuchVerb above, which is a daemon and the verbs that daemon takes. */
	NoSuchRoute,
	NoSuchMethod,
	NoSuchParameter,
	NoSuchTimerType,
	NoSuchName,
	NoSuchParent,
	NoSuchFilesystem,
	UnknownSetting,
	ScriptMissing,
	NoMountTable,
	/* A key name the box's own table does not carry. Not the code for a value
	   this could not read: the set of names is a thing a caller can ask for. */
	NoSuchKey,
	/* A plugin the box does not carry, under the rule that everything a lookup
	   did not find is a no-such-something. */
	/* A channel with no picture of its own. Not the code above it for a channel
	   nothing knows: a caller told no such channel would go looking for an
	   identifier it already has right. */
	NoSuchLogo,
	/* A guide entry named by an identifier and a moment, where the guide holds
	   no showing that begins then. One code and not two, although the
	   identifier being unknown and the moment not matching differ below: the
	   guide is rebuilt as the box receives it, so either means the entry the
	   caller was holding has been replaced. */
	NoSuchEvent,
	/* Nothing is being recorded under that number, whether there never was one
	   or it ended between the listing and the act built on it. */
	NoSuchRecording,
	/* Nothing was minted under that token, or what was is older than a minted
	   address is kept for. Either way the caller reads the playlist again. */
	NoSuchPart,

	// Nothing to answer with, though the thing asked about is known.
	NoRunningChannel,
	NoMode,
	NoCurrentEvent,
	/* No frontend is carrying a picture, which is what a box with no tuner and
	   a tuner given nothing to receive both look like. One code, because a
	   caller does the same about either. */
	NoTuner,
	/* The box is keeping no shift of what it is showing, where one was to be
	   ended. Not a no-such-something: nothing was named, the shift is a single
	   thing the box either keeps or does not. */
	NoTimeshift,

	// The request itself is wrong.
	EmptyWindow,
	QueryTooShort,
	NoRoomForAResult,
	LimitTooLarge,
	EmptyMessage,
	MessageTooLong,
	VolumeOutOfRange,
	EmptyPath,
	RelativePath,
	PathHasZeroByte,
	PathTooLong,
	OutsideRoots,
	/* The name is a web server's own store of who may reach this box. Not the
	   code above: that one says a name is outside the permitted directories,
	   and this one is inside them. Not a refusal about the caller either,
	   because no credential changes it. */
	AccessStore,
	BadName,
	TimerWithoutId,
	TimerWithoutChannel,
	RecordingWithoutDuration,
	TimerInThePast,
	NotACommand,
	DuplicateParameter,
	MissingParameter,
	/* Two parameters that are two ways of saying one thing, and both were
	   written. Not the code above it for a name written twice: the names
	   differ. Refused rather than one of them being preferred, because a
	   request answered out of half of what it said is one whose author never
	   learns the other half was dropped. */
	ConflictingParameters,
	LeadingDash,
	BadInt,
	BadBool,
	BadEnum,
	/* Asked to move the first bouquet up or the last one down. Refused rather
	   than passed over, because a move that answered ok and shifted nothing is
	   the one answer a caller cannot tell from the one it wanted.

	   Not one of the no-such names: the bouquet is there. */
	AlreadyAtTheEnd,
	BadString,
	BadPath,
	ValueHasZeroByte,
	ValueTooLong,
	/* The whole of what was sent, rather than one value inside it. A value too
	   long is one field to shorten; this is a request to send in pieces or not
	   at all. */
	BodyTooLarge,
	/* The stretch of a file asked for begins at or past its end, or asks for
	   none of its bytes. Not one of the no-such names: the file is there, and a
	   caller told otherwise would go looking for a name it already has right. */
	RangeOutsideFile,
	/* More channels named for one half of one bouquet than that half takes.
	   Counted in channels and not in bytes, because the ceiling stands between
	   the box and the work of placing each of them; the size of what arrived is
	   answered for above.

	   Named rather than met by taking the first of them. */
	TooManyChannels,
	/* A value offered for a setting, against the row that declares it. Three
	   codes, because a caller either sends a number at all, sends one inside
	   the range, or sends one of the values the setting lists. */
	NotANumber,
	OutOfRange,
	NotAListedValue,
	/* Nothing offered for a credential. A read of one answers nothing, so a
	   form that redraws itself from what it read sends nothing back, and taking
	   that would wipe the value the read protected. */
	EmptyCredential,
	/* A setting that is declared and is not a credential, offered where only a
	   credential is taken. Not the code for a key nothing declares: the key is
	   there. */
	NotACredential,
	/* Refused for who the caller is rather than for what it asked. It sits with
	   the wrong requests because the caller acts on it the same way, by sending
	   a credential or a different one. */
	NotPermitted,
	/* A request for an address of a stream that named no authority to build one
	   under. The caller says under which name it reached this box. */
	NoAuthority,
	/* The channel is one the box tunes to rather than one it plays from an
	   address, so there is nothing here to fetch and pass on. Not
	   no-such-channel: the channel is there. */
	NotAWebChannel,
	/* The channel leads to an address this box will not fetch from, whether
	   directly or through a redirection. The one refusal here that is a
	   decision of this box rather than anything the far end did. */
	AddressRefused,
	/* What came back is not a playlist. What a caller does about it is play the
	   stream itself rather than a playlist of it. */
	NotAPlaylist,
	/* The playlist came back and could not be turned into one this box can
	   answer: a reference that resolves to nothing, more addresses than this
	   holds, or a range of bytes, which is the one thing in the format this
	   does not carry across and refuses rather than gets subtly wrong. */
	PlaylistUnreadable,

	/* Nothing was wrong with the request and nothing is wrong here: the box is
	   already carrying as many of these at once as it will. The one refusal a
	   caller answers by coming back rather than by changing anything. */
	TooManyStreams,
	/* The same answer about a different thing, and its own code because the two
	   run out separately: converting a channel's sound for a browser costs the
	   box a running program per listener, and it will run far fewer of those
	   than it will carry event streams. */
	TooManyConversions,
	/* Nothing this box can do makes this channel playable in a browser. Not a
	   plain refusal, because a caller answers it by offering the raw stream to
	   a player outside the browser. */
	NotPlayableInBrowser,
	/* Too much is being asked of the one thing that answers before a caller has
	   shown who it is. One code for both causes, too little time since this
	   caller's last wrong guess and too many being answered at once: a caller
	   comes back either way, and telling them apart would say whether the box
	   is loaded to somebody who has presented nothing. */
	TooManyAttempts,

	// The box already has it.
	TimerExists,
	NameTaken,
	/* What carries that name is not a plain file, and a plain file was to go in
	   its place. Not the code above: that one is answered by asking again and
	   saying to replace what is there, and a directory, a link and a device are
	   each something a caller has to deal with as itself first. */
	NotAPlainFile,
	/* A file is already being written beside that name. Writing goes to a name
	   of its own and is put in the file's place at the end, so something at
	   that second name is either another write of the same file still running
	   or the remains of one that was cut off. A caller waits or clears the
	   remains away rather than renaming or overwriting. */
	SideFileInTheWay,
	NotEmpty,
	/* The box is already keeping a shift, where one was to be started. Refused
	   rather than granted quietly, because the record manager would start a
	   second one, and two shifts of one channel hold two tuners to write the
	   same stream twice. */
	TimeshiftRunning,

	// A table this layer wrote is wrong, which is a fault here and not at the caller.
	BadScript,
	BadTable,
	NoTimeout,
	NoOutputRoom,
	NoValues,
	NoRoot,
	NoRootToResolve,

	// The box was asked and did not do it.
	CommandNotSent,
	/* There is no session, where the credential was right and one was meant to
	   be opened. Several things inside can leave no session, the pool the token
	   is drawn from answering nothing among them, and none of them is anything
	   a caller acts on differently. */
	SessionNotOpened,
	CommandNotPosted,
	TimerNotCreated,
	TimerNotChanged,
	TimerNotRemoved,
	TimerStillThere,
	/* The daemon that ends a recording would not take the request. Not the code
	   for a timer that was not removed: nothing was removed. */
	RecordingNotStopped,
	/* This server is set not to hand channel pictures over. Not the code for a
	   channel that has no picture, although both carry the same status on
	   purpose: a page draws the same thing for either, and whoever is looking
	   for a picture that used to be there wants to know it is a line of a file
	   and not a file that has gone. */
	LogosNotOffered,
	/* The box would not put its tuners through a reset. The reset is what
	   somebody reaches for when the picture has already gone, so being told the
	   box did not even take the request is the difference between a box to
	   restart and a signal to look at. */
	TunerNotReset,
	/* The file holding the network filesystems could not be written, whether it
	   could not be opened or the write did not land. One code, because it is
	   written whole or not at all. */
	NetfsTableNotWritten,
	// One code for the one thing a caller can do about it. Which call the
	// filesystem turned down says nothing a caller acts on differently.
	ChangeRefused,
	/* The value did not land, whether the store turned the write down or the
	   save after it did. The two are one act here: the value reaches the
	   program only if both go through. */
	SettingNotWritten,
	/* The bouquet is not what was asked for, whether the box turned the change
	   down, would not write the list out afterwards, or would not read it back.
	   The change reaches the box only if all three go through.

	   What it cannot promise is that nothing happened. A change made and not
	   written out is one the box carries until it restarts. */
	BouquetNotChanged,
	/* A plugin the box has and whose own file did not take the change. The flag
	   that keeps a plugin out of the box's menus lives in that file and nowhere
	   else, so a write that did not land is a plugin still shown. */
	PluginNotChanged,
	// The key was one the box has and the node it goes to did not take it.
	KeyNotSent,
	/* The box was asked for a picture of its screen and there is no file to
	   hand on. Not the code below it: the screen is there and the reading of it
	   did not come off, so a caller asks again in a moment. */
	ScreenNotCaptured,
	// The same for the display on the front of the box. Its own code because
	// most boxes have none, so a caller learns to stop asking rather than to
	// come back.
	DisplayNotCaptured,

	// Something below could not be read at all.
	ChannelListUnavailable,
	/* The server a channel is played from did not answer, answered a refusal of
	   its own, or went quiet part way through. One code, because a caller comes
	   back or picks another channel either way; which it was is in the words
	   beside it. */
	UpstreamUnreachable,
	BouquetListUnavailable,
	ModeUnavailable,
	CurrentChannelUnresolved,
	EpgUnavailable,
	VolumeUnavailable,
	MuteUnavailable,
	SettingUnreadable,
	BoxUnreadable,
	/* How the box is descrambling the running channel could not be read. Not
	   the same as nothing descrambling it: that is a state this reports as
	   such, and this is the absence of a reading at all. */
	DecryptionUnreadable,
	TimerListUnavailable,
	ClockUnavailable,
	/* The file holding the network filesystems could not be read off the disc
	   at all. Not an empty list: a file naming nothing is a file this answers
	   out of, eight empty slots being what an unconfigured box has. */
	NetfsTableUnreadable,
	FrontendListUnavailable,
	// The frontend carrying the picture is there and what it is measuring could
	// not be taken off it. Not the same as its having nothing to measure: a
	// signal of nought is a reading and this is the absence of one.
	SignalUnavailable,
	RemoteLockUnreadable,
	/* The plugins of the box, where what holds them could not be reached at
	   all. Not an empty list: a box carrying no plugins is one this answers
	   about. */
	PluginListUnavailable,
	/* What the box is recording, where what holds it could not be reached at
	   all. Not an empty list: a box recording nothing is one this answers
	   about. */
	RecordingListUnavailable,
	/* A document the box keeps its own configuration in and that could not be
	   read off the disc. Not a no-such-something: the box states where the file
	   is. */
	ConfigDocumentUnreadable,
	/* The configuration the web server itself runs on did not land, whether a
	   value in it could not be used or the file could not be written. One code,
	   because the file is rewritten whole or not at all: the box runs on what
	   it ran on either way, and the line the box wrote says which value it
	   was. */
	WebserverNotConfigured,

	// A command this layer ran, and what became of it.
	CommandNotStarted,
	CommandOutputUnreadable,
	Timeout,
	OutputTooLarge,
	NotStopped,
	NoSupervision,
	SupervisionLost,
	NoExitStatus,
	DidNotExit,
	Killed,
	ExitStatus,
	/* The values a setting offers could not be told: either it is not a setting
	   that offers a set, or the set is the box's own and nobody has said what
	   it is. Either way the caller has no list to draw from. */
	ChoicesUnavailable,
};

/* The wire form. A code with no case here answers with nothing, and two things
   keep that from happening.

   The compiler is the first. This function is inline in a header, src/httpd
   compiles with an unhandled enumerator as an error, and one translation unit
   there includes this, so a code added without a case below stops that build.
   The error is reported at the switch here and raised from a directory the
   author may not have been editing. No other directory turns that on, so a
   build of the program alone would still pass.

   The case in the tests that walks every enumerator is the second, and it
   answers for what the projection returns rather than for whether it has a case
   at all. Its list is held to this enum by a scan of it. */
inline const char *codeString(ErrorCode c)
{
	switch (c)
	{
		case ErrorCode::NoSuchChannel: return "no-such-channel";
		case ErrorCode::NoSuchBouquet: return "no-such-bouquet";
		case ErrorCode::NoSuchTimer: return "no-such-timer";
		case ErrorCode::NoSuchDaemon: return "no-such-daemon";
		case ErrorCode::NoSuchVerb: return "no-such-verb";
		case ErrorCode::NoSuchCommand: return "no-such-command";
		case ErrorCode::NoSuchRoute: return "no-such-route";
		case ErrorCode::NoSuchMethod: return "no-such-method";
		case ErrorCode::NoSuchParameter: return "no-such-parameter";
		case ErrorCode::NoSuchTimerType: return "no-such-timer-type";
		case ErrorCode::NoSuchName: return "no-such-name";
		case ErrorCode::NoSuchParent: return "no-such-parent";
		case ErrorCode::NoSuchFilesystem: return "no-such-filesystem";
		case ErrorCode::UnknownSetting: return "no-such-setting";
		case ErrorCode::ScriptMissing: return "script-missing";
		case ErrorCode::NoMountTable: return "no-mount-table";
		case ErrorCode::NoSuchKey: return "no-such-key";
		case ErrorCode::NoSuchLogo: return "no-such-logo";
		case ErrorCode::NoSuchEvent: return "no-such-event";
		case ErrorCode::NoSuchRecording: return "no-such-recording";
		case ErrorCode::NoSuchPart: return "no-such-part";
		case ErrorCode::NoRunningChannel: return "no-running-channel";
		case ErrorCode::NoMode: return "no-mode";
		case ErrorCode::NoCurrentEvent: return "no-current-event";
		case ErrorCode::NoTuner: return "no-tuner";
		case ErrorCode::NoTimeshift: return "no-timeshift";
		case ErrorCode::EmptyWindow: return "empty-window";
		case ErrorCode::QueryTooShort: return "query-too-short";
		case ErrorCode::NoRoomForAResult: return "no-room-for-a-result";
		case ErrorCode::LimitTooLarge: return "limit-too-large";
		case ErrorCode::EmptyMessage: return "empty-message";
		case ErrorCode::MessageTooLong: return "message-too-long";
		case ErrorCode::VolumeOutOfRange: return "volume-out-of-range";
		case ErrorCode::EmptyPath: return "empty-path";
		case ErrorCode::RelativePath: return "relative-path";
		case ErrorCode::PathHasZeroByte: return "path-has-zero-byte";
		case ErrorCode::PathTooLong: return "path-too-long";
		case ErrorCode::OutsideRoots: return "outside-roots";
		case ErrorCode::AccessStore: return "access-store";
		case ErrorCode::BadName: return "bad-name";
		case ErrorCode::TimerWithoutId: return "timer-without-id";
		case ErrorCode::TimerWithoutChannel: return "timer-without-channel";
		case ErrorCode::RecordingWithoutDuration: return "recording-without-duration";
		case ErrorCode::TimerInThePast: return "timer-in-the-past";
		case ErrorCode::NotACommand: return "not-a-command";
		case ErrorCode::DuplicateParameter: return "duplicate-parameter";
		case ErrorCode::MissingParameter: return "missing-parameter";
		case ErrorCode::ConflictingParameters: return "conflicting-parameters";
		case ErrorCode::LeadingDash: return "leading-dash";
		case ErrorCode::BadInt: return "bad-int";
		case ErrorCode::BadBool: return "bad-bool";
		case ErrorCode::BadEnum: return "bad-enum";
		case ErrorCode::AlreadyAtTheEnd: return "already-at-the-end";
		case ErrorCode::BadString: return "bad-string";
		case ErrorCode::BadPath: return "bad-path";
		case ErrorCode::ValueHasZeroByte: return "value-has-zero-byte";
		case ErrorCode::ValueTooLong: return "value-too-long";
		case ErrorCode::BodyTooLarge: return "body-too-large";
		case ErrorCode::RangeOutsideFile: return "range-outside-file";
		case ErrorCode::TooManyChannels: return "too-many-channels";
		case ErrorCode::NotANumber: return "not-a-number";
		case ErrorCode::OutOfRange: return "out-of-range";
		case ErrorCode::NotAListedValue: return "not-a-listed-value";
		case ErrorCode::EmptyCredential: return "empty-credential";
		case ErrorCode::NotACredential: return "not-a-credential";
		case ErrorCode::NotPermitted: return "not-permitted";
		case ErrorCode::NoAuthority: return "no-authority";
		case ErrorCode::NotAWebChannel: return "not-a-web-channel";
		case ErrorCode::AddressRefused: return "address-refused";
		case ErrorCode::NotAPlaylist: return "not-a-playlist";
		case ErrorCode::PlaylistUnreadable: return "playlist-unreadable";
		case ErrorCode::TooManyStreams: return "too-many-streams";
		case ErrorCode::TooManyConversions: return "too-many-conversions";
		case ErrorCode::NotPlayableInBrowser: return "not-playable-in-browser";
		case ErrorCode::TooManyAttempts: return "too-many-attempts";
		case ErrorCode::TimerExists: return "timer-exists";
		case ErrorCode::NameTaken: return "name-taken";
		case ErrorCode::NotAPlainFile: return "not-a-plain-file";
		case ErrorCode::SideFileInTheWay: return "side-file-in-the-way";
		case ErrorCode::NotEmpty: return "not-empty";
		case ErrorCode::TimeshiftRunning: return "timeshift-running";
		case ErrorCode::BadScript: return "bad-script";
		case ErrorCode::BadTable: return "bad-table";
		case ErrorCode::NoTimeout: return "no-timeout";
		case ErrorCode::NoOutputRoom: return "no-output-room";
		case ErrorCode::NoValues: return "no-values";
		case ErrorCode::NoRoot: return "no-root";
		case ErrorCode::NoRootToResolve: return "no-root-to-resolve";
		case ErrorCode::CommandNotSent: return "command-not-sent";
		case ErrorCode::SessionNotOpened: return "session-not-opened";
		case ErrorCode::CommandNotPosted: return "command-not-posted";
		case ErrorCode::TimerNotCreated: return "timer-not-created";
		case ErrorCode::TimerNotChanged: return "timer-not-changed";
		case ErrorCode::TimerNotRemoved: return "timer-not-removed";
		case ErrorCode::TimerStillThere: return "timer-still-there";
		case ErrorCode::RecordingNotStopped: return "recording-not-stopped";
		case ErrorCode::LogosNotOffered: return "logos-not-offered";
		case ErrorCode::TunerNotReset: return "tuner-not-reset";
		case ErrorCode::NetfsTableNotWritten: return "netfs-table-not-written";
		case ErrorCode::NetfsTableUnreadable: return "netfs-table-unreadable";
		case ErrorCode::ChangeRefused: return "change-refused";
		case ErrorCode::SettingNotWritten: return "setting-not-written";
		case ErrorCode::BouquetNotChanged: return "bouquet-not-changed";
		case ErrorCode::PluginNotChanged: return "plugin-not-changed";
		case ErrorCode::KeyNotSent: return "key-not-sent";
		case ErrorCode::ScreenNotCaptured: return "screen-not-captured";
		case ErrorCode::DisplayNotCaptured: return "display-not-captured";
		case ErrorCode::ChannelListUnavailable: return "channel-list-unavailable";
		case ErrorCode::UpstreamUnreachable: return "upstream-unreachable";
		case ErrorCode::BouquetListUnavailable: return "bouquet-list-unavailable";
		case ErrorCode::ModeUnavailable: return "mode-unavailable";
		case ErrorCode::CurrentChannelUnresolved: return "current-channel-unresolved";
		case ErrorCode::EpgUnavailable: return "epg-unavailable";
		case ErrorCode::VolumeUnavailable: return "volume-unavailable";
		case ErrorCode::MuteUnavailable: return "mute-unavailable";
		case ErrorCode::SettingUnreadable: return "setting-unreadable";
		case ErrorCode::BoxUnreadable: return "box-unreadable";
		case ErrorCode::DecryptionUnreadable: return "decryption-unreadable";
		case ErrorCode::TimerListUnavailable: return "timer-list-unavailable";
		case ErrorCode::ClockUnavailable: return "clock-unavailable";
		case ErrorCode::FrontendListUnavailable: return "frontend-list-unavailable";
		case ErrorCode::SignalUnavailable: return "signal-unavailable";
		case ErrorCode::RemoteLockUnreadable: return "remote-lock-unreadable";
		case ErrorCode::PluginListUnavailable: return "plugin-list-unavailable";
		case ErrorCode::RecordingListUnavailable: return "recording-list-unavailable";
		case ErrorCode::ConfigDocumentUnreadable: return "config-document-unreadable";
		case ErrorCode::WebserverNotConfigured: return "webserver-not-configured";
		case ErrorCode::CommandNotStarted: return "command-not-started";
		case ErrorCode::CommandOutputUnreadable: return "command-output-unreadable";
		case ErrorCode::Timeout: return "timeout";
		case ErrorCode::OutputTooLarge: return "output-too-large";
		case ErrorCode::NotStopped: return "not-stopped";
		case ErrorCode::NoSupervision: return "no-supervision";
		case ErrorCode::SupervisionLost: return "supervision-lost";
		case ErrorCode::NoExitStatus: return "no-exit-status";
		case ErrorCode::DidNotExit: return "did-not-exit";
		case ErrorCode::Killed: return "killed";
		case ErrorCode::ExitStatus: return "exit-status";
		case ErrorCode::ChoicesUnavailable: return "choices-unavailable";
	}
	return "";
}

} // namespace coreapi

#endif
