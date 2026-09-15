/*
 * osd.cpp - volume, mute, messages, and what the screen shows
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

#include "osd.h"
#include "coreapi/base/errors.h"

#include "coreapi/base/deps.h"

#include <cstdio>
#include <utility>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

/* The names the box's keys travel under and the codes behind them, generated
   from the input layer's own header. The kernel's header first, because the
   generated one only fills in the names that one does not carry. */
#include <linux/input.h>
#include <src/tools/rcsim.h>

namespace coreapi
{
namespace osd
{

Result<void> message(MessageKind kind, const std::string &text)
{
	// The box draws an empty frame for an empty message and waits for it to be
	// dismissed, so nothing is sent for one.
	if (text.empty())
		return fail(Status::InvalidArgument, ErrorCode::EmptyMessage,
			    "a message needs words");
	// The length travels to the other end as the size of an allocation and of
	// the read that follows it, both on the thread the box draws on, so a
	// caller cannot be allowed to name it freely.
	if (text.size() > MAX_MESSAGE_BYTES)
	{
		char bound[32];
		snprintf(bound, sizeof(bound), "%u", (unsigned) MAX_MESSAGE_BYTES);
		return fail(Status::InvalidArgument, ErrorCode::MessageTooLong,
			    std::string("a message is at most ") + bound + " bytes long");
	}

	BoxEvent e = (kind == MessageKind::Hint) ? BoxEvent::Hint : BoxEvent::Message;
	// The terminator travels with the words: the loop hands the block on as a
	// string and nothing else says where it ends.
	return postEvent(e, text.c_str(), text.size() + 1);
}

Result<int> volume()
{
	int out = 0;
	Status s = systemSource().volume(out);
	if (s != Status::Ok)
		return fail(s, ErrorCode::VolumeUnavailable,
			    "the volume could not be read");
	return ok(out);
}

Result<void> setVolume(int percent)
{
	if (percent < 0 || percent > 100)
		return fail(Status::InvalidArgument, ErrorCode::VolumeOutOfRange,
			    "the volume is a percentage");

	char value = (char) percent;
	return postEvent(BoxEvent::SetVolume, &value, sizeof(value));
}

Result<bool> muted()
{
	bool out = false;
	Status s = systemSource().muted(out);
	if (s != Status::Ok)
		return fail(s, ErrorCode::MuteUnavailable,
			    "the mute state could not be read");
	return ok(out);
}

Result<void> setMuted(bool on)
{
	char value = on ? 1 : 0;
	return postEvent(BoxEvent::SetMute, &value, sizeof(value));
}

namespace
{

const size_t KEY_COUNT = sizeof(keyname) / sizeof(keyname[0]);

// The first row of that name, which is the reading the copied interface has:
// one name stands against two codes there, and the row written first is the
// one a press has always reached.
bool codeForName(const std::string &name, unsigned long &out)
{
	for (size_t i = 0; i < KEY_COUNT; i++)
	{
		if (name == keyname[i].name)
		{
			out = keyname[i].code;
			return true;
		}
	}
	return false;
}

} // anonymous namespace

Result<KeyNameList> keyNames()
{
	KeyNameList names;
	names.reserve(KEY_COUNT);
	for (size_t i = 0; i < KEY_COUNT; i++)
		names.push_back(keyname[i].name);
	return ok(std::move(names));
}

Result<void> sendKey(const std::string &name)
{
	/* Bounded before the walk and not inside it, so that a name of a megabyte
	   is one comparison of two lengths rather than a hundred of a megabyte. */
	if (name.size() > MAX_KEY_NAME_BYTES)
		return fail(Status::InvalidArgument, ErrorCode::NoSuchKey,
			    "the remote control has no key of that name");

	unsigned long code = 0;
	if (!codeForName(name, code))
		return fail(Status::InvalidArgument, ErrorCode::NoSuchKey,
			    "the remote control has no key of that name");

	Status s = inputDevice().sendKey(code);
	if (s != Status::Ok)
		return fail(s, ErrorCode::KeyNotSent, "the box did not take the key");
	return ok();
}

Result<bool> locked()
{
	bool out = false;
	Status s = inputDevice().locked(out);
	if (s != Status::Ok)
		return fail(s, ErrorCode::RemoteLockUnreadable,
			    "whether the remote control is locked could not be read");
	return ok(out);
}

Result<void> setLocked(bool on)
{
	return postEvent(on ? BoxEvent::LockRemote : BoxEvent::UnlockRemote);
}

namespace
{

/* Where a picture lands. One name per kind and per form, for the reason the
   header gives. The suffix is the form the file is in and not decoration: a name
   saying one thing while holding the other is the one way this can be wrong that
   nobody looking at the answer would catch. */
const char SCREEN_PICTURE_PNG[] = "/tmp/neutrino-screenshot.png";
const char SCREEN_PICTURE_JPEG[] = "/tmp/neutrino-screenshot.jpg";
const char DISPLAY_PICTURE[] = "/tmp/neutrino-display.png";

const char *screenPictureFor(PictureFormat f)
{
	// No default, so a form added to the enumeration is a warning here rather
	// than a file written under the name of another form.
	switch (f)
	{
		case PictureFormat::Png:  return SCREEN_PICTURE_PNG;
		case PictureFormat::Jpeg: return SCREEN_PICTURE_JPEG;
	}
	// Only a value cast into the enumeration from outside it reaches this, and
	// the form it is answered with is the one every caller of this can read.
	return SCREEN_PICTURE_PNG;
}

/* One capture at a time per kind. Two writers on one name leave a file that is
   neither picture, and the callers here are the web layer's own workers, of which
   there are several. Made on the first ask, so this translation unit still has
   nothing to run at startup. */
OpenThreads::Mutex &screenGuard()
{
	static OpenThreads::Mutex m;
	return m;
}

OpenThreads::Mutex &displayGuard()
{
	static OpenThreads::Mutex m;
	return m;
}

/* Gives back a lock the caller has already taken, however the call leaves.
   ScopedLock takes the lock itself, which is the one thing a caller that must
   not wait for it can let it do. */
class Held
{
	public:
		explicit Held(OpenThreads::Mutex &m) : m_(m) {}
		~Held() { m_.unlock(); }

	private:
		OpenThreads::Mutex &m_;
		Held(const Held &);
		Held &operator=(const Held &);
};

} // anonymous namespace

Result<std::string> screenshot(bool osd, bool video, PictureFormat format)
{
	const std::string path = screenPictureFor(format);

	/* One lock for both names and not one each. What two captures at once
	   contend for is the screen and not the file: the layer below reads one
	   video decoder and one framebuffer.

	   Tried and not waited for. The capture below reads the box's framebuffer
	   through a driver call that has no deadline of its own, so a caller that
	   queued here would hand this server's next worker to the same stuck read;
	   there are four of them, and the page asks for a picture on every key.
	   Refusing costs one worker, waiting costs the channel list and the guide
	   with it. */
	if (screenGuard().trylock() != 0)
		return fail(Status::Busy, ErrorCode::ScreenNotCaptured,
			    "the box is already taking a picture of its screen");
	Held held(screenGuard());
	const Status s = screenshotSource().captureScreen(osd, video, format, path);
	if (s != Status::Ok)
		return fail(s, ErrorCode::ScreenNotCaptured,
			    "the box could not take a picture of its screen");
	return ok(path);
}

Result<std::string> displayScreenshot()
{
	const std::string path = DISPLAY_PICTURE;

	/* Waited for, unlike the one above. What is under this lock is an encode of
	   a bitmap this process already holds, bounded by its own size, with no
	   device read in it; and nothing asks for this picture but a press of the
	   button beside it, so two at once means two people pressing at the same
	   moment. A refusal would cost that press a picture and buy nothing. */
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(displayGuard());
	const Status s = screenshotSource().captureDisplay(path);
	if (s != Status::Ok)
		return fail(s, ErrorCode::DisplayNotCaptured,
			    "the box could not take a picture of its display");
	return ok(path);
}

namespace
{

/* The two settings the state is made of, and what the box writes in the second.
   The numbers are the ones the drawing code names, src/gui/infoicons.h, written
   out here because that header is the graphics stack, which this layer may not
   reach into. The row that declares the skin lists the same three. */
const char *const kModeKey = "mode_icons";
const char *const kSkinKey = "mode_icons_skin";

const long kSkinStatic     = 0;
const long kSkinInfoviewer = 1;
const long kSkinPopup      = 2;

/* The pair a state is written as. Off is the one that reads the skin the box is
   in, because it only moves it off infoviewer and leaves every other skin
   standing. False for a value that is none of the four, because either key taken
   from such a value would be a number nobody meant. */
bool infoIconsPair(InfoIcons state, long skin_now, long &mode, long &skin)
{
	switch (state)
	{
		case InfoIcons::Static:
			mode = 1;
			skin = kSkinStatic;
			return true;
		case InfoIcons::Popup:
			mode = 1;
			skin = kSkinPopup;
			return true;
		case InfoIcons::Infoviewer:
			mode = 0;
			skin = kSkinInfoviewer;
			return true;
		case InfoIcons::Off:
			mode = 0;
			skin = (skin_now == kSkinInfoviewer) ? kSkinStatic : skin_now;
			return true;
	}
	return false;
}

} // namespace

Result<InfoIcons> infoIcons()
{
	long mode = 0;
	Status s = settingsSource().readInt(kModeKey, mode);
	if (s != Status::Ok)
		return fail(s, ErrorCode::SettingUnreadable,
			    "whether the box draws the icons could not be read");

	long skin = 0;
	s = settingsSource().readInt(kSkinKey, skin);
	if (s != Status::Ok)
		return fail(s, ErrorCode::SettingUnreadable,
			    "which skin the icons are drawn in could not be read");

	/* The skin the infobar draws is not a skin this draws in, so with the
	   drawing on it reads as the plain one: what the box does with that pairing
	   is draw the plain icons. */
	if (mode != 0)
		return ok(skin == kSkinPopup ? InfoIcons::Popup : InfoIcons::Static);

	return ok(skin == kSkinInfoviewer ? InfoIcons::Infoviewer : InfoIcons::Off);
}

Result<void> setInfoIcons(InfoIcons state)
{
	long skin_now = 0;
	Status s = settingsSource().readInt(kSkinKey, skin_now);
	if (s != Status::Ok)
		return fail(s, ErrorCode::SettingUnreadable,
			    "which skin the icons are drawn in could not be read");

	long mode = 0;
	long skin = 0;
	if (!infoIconsPair(state, skin_now, mode, skin))
		return fail(Status::InvalidArgument, ErrorCode::NotAListedValue,
			    "that is not a state the icons can be put in");

	s = settingsSource().writeInt(kModeKey, mode);
	if (s == Status::Ok)
		s = settingsSource().writeInt(kSkinKey, skin);
	if (s != Status::Ok)
		return fail(s, ErrorCode::SettingNotWritten,
			    "the state of the icons could not be written");

	/* One save for the two, because the pair is one state. What this asks for is
	   that both of them reach the box together. */
	s = settingsSource().persist();
	if (s != Status::Ok)
		return fail(s, ErrorCode::SettingNotWritten,
			    "the state of the icons was taken and not saved");
	return ok();
}

} // namespace osd
} // namespace coreapi
