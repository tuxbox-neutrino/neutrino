/*
 * osd.h - volume, mute, messages, and what the screen shows
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

#ifndef __coreapi_osd_h__
#define __coreapi_osd_h__

#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include <stddef.h>

#include <string>

namespace coreapi
{
namespace osd
{

// A hint goes away by itself, a box waits to be dismissed, and they are two
// events rather than one event with a flag.
enum class MessageKind
{
	Hint,
	Box
};

// The screen holds a few hundred characters at any legible size, so nothing a
// reader could take in is cut; what the bound is for is the other end, where the
// length becomes an allocation and a read of exactly that many bytes on the
// thread the box draws on.
const size_t MAX_MESSAGE_BYTES = 4096;

// Sent and forgotten, and bounded like every other command here: at most
// COMMAND_SEND_TIMEOUT_MS of the calling thread, after which the message is lost
// rather than the caller held.
Result<void> message(MessageKind kind, const std::string &text);

Result<int> volume();

// A percentage. Anything outside it is refused rather than folded into range,
// because the event carries the number in a single byte and a value that does
// not fit one arrives as a different volume rather than as the loudest.
Result<void> setVolume(int percent);

// Called where the level is applied, whoever asked for it.
void announceVolume(int percent);

Result<bool> muted();
Result<void> setMuted(bool on);

// Called where the mute flag changes, whoever asked for it.
void announceMute(bool on);

/* Above every name the box's own table carries, with room for the ones it gains
   later, and here rather than at the route so that a longer name is turned away
   before anything walks the table against it. A case holds it to that table. */
const size_t MAX_KEY_NAME_BYTES = 32;

/* In the order the box's own table keeps them. That table is read and not
   copied, so a key the input layer gains is a key this answers with and every
   name here is one sendKey below takes. It carries a few names twice, one of
   them against two different codes, and this hands it over as it stands: a rule
   applied here and not at the send would be a list that disagrees with what the
   box does. */
Result<KeyNameList> keyNames();

/* A name the table does not carry is InvalidArgument and nothing is sent. A name
   the table carries twice reaches the first of the two, which is the reading the
   copied interface has always had. */
Result<void> sendKey(const std::string &name);

// Whether the box is ignoring its remote control.
Result<bool> locked();

// A command, bounded like the ones above: what holds the flag reads this off the
// loop and nothing answers, which is why the reading beside it does not travel
// the same way.
Result<void> setLocked(bool on);

/* A picture of what the television is showing, answering where it was written.

   No name is taken from the caller: the copied interface builds its path out of
   one the caller sends, which is a caller writing wherever it likes the moment
   that name carries a slash.

   One name per format. So two captures at once are one file, and a reader that
   asked while somebody else was asking may be given the picture taken for them:
   between two readers who asked for different forms that would be a file in one
   form handed over as the other. What it buys is a box whose free space does not
   fall by a picture for every request.

   The size is the box's and there is no argument for it: the layer that reads
   the screen reports the size it read back over whatever it was asked for,
   before either encoder sees a pixel, on every board this builds for but the one
   family whose capture shells out to a program. Asking for fewer bytes is what
   the format is for instead: the second throws detail away, which makes a
   picture of moving video a fraction of the size and a screen of flat colour
   larger than the first. */
Result<std::string> screenshot(bool osd, bool video, PictureFormat format);

// The same for the small display on the front of the box. NotSupported where
// there is none, which is most of them and every build made without it.
Result<std::string> displayScreenshot();

/* What the box draws in the corner to say what the channel is carrying. Four
   states and not two, because the box keeps this in two settings and only some
   pairings of them are a state anybody meant: whether the icons are drawn by
   this, and which skin they are drawn in, where one of the skins says the
   infobar draws them instead. A caller writing the two by hand can reach a
   pairing the box's own screen cannot offer. */
enum class InfoIcons
{
	Static,
	Popup,
	Infoviewer,
	Off
};

Result<InfoIcons> infoIcons();

/* Writes the pair as one state and asks for one save, because a box that saved
   between them would have a moment in which it is in neither.

   Off carries a rule the settings themselves do not: it moves the skin off
   infoviewer as well. The skin says who draws the icons, so a box left in it with
   the drawing turned off answers infoviewer for ever after and cannot be turned
   off at all. */
Result<void> setInfoIcons(InfoIcons state);

} // namespace osd
} // namespace coreapi

#endif
