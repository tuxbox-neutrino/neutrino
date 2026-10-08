/*
 * ep_osd.cpp - routes for volume, messages and the screen
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

#include "httpd/endpoints.h"

#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/schema.h"
#include "httpd/status.h"

#include "coreapi/base/errors.h"
#include "coreapi/osd.h"
#include "coreapi/base/result.h"

#include "coreapi/base/types.h"

#include <cstddef>
#include <string>
#include <vector>

#include <fcntl.h>

namespace httpd
{

namespace
{

const FieldDesc kVolumeFields[] = {
	HTTPD_MEMBER("percent", FieldType::Int, "how loud the box is, as a percentage from 0 to 100"),
	HTTPD_MEMBER("muted", FieldType::Bool,
		"whether the sound is off, which is kept apart from the level so that turning it back on is the level it was"),
};

const Schema kVolumeSchema = { "volume", HTTPD_FIELDS(kVolumeFields) };

Response getVolume(const Request &)
{
	/* Both readings, because a caller drawing a control needs both and asking for
	   them separately would give it two answers taken at two moments. A box that
	   answers one and not the other is a refusal here rather than half an answer: a
	   level with the mute state missing would be drawn as sound that is on. */
	coreapi::Result<int> level = coreapi::osd::volume();
	if (!level.ok())
		return problemFor(level.error());

	coreapi::Result<bool> off = coreapi::osd::muted();
	if (!off.ok())
		return problemFor(off.error());

	Response out = okJson();
	Json j(out.body, 64);
	j.beginObject();
	j.key("percent");
	j.value(level.value());
	j.key("muted");
	j.value(off.value());
	j.endObject();
	return out;
}

Response setVolume(const Request &r)
{
	/* The percentage is bounded by the row that declares it and again by the layer
	   below. Two bounds and not one copy of a bound: this one refuses the value
	   before a handler is entered, the other before the event is built, and the
	   event carries the number in a single byte, so a value that got past both would
	   arrive as a different volume rather than as the loudest. */
	coreapi::Result<void> done = coreapi::osd::setVolume((int) r.asInt("percent"));
	if (!done.ok())
		return problemFor(done.error());
	return accepted();
}

Response setMuted(const Request &r)
{
	coreapi::Result<void> done = coreapi::osd::setMuted(r.asBool("on"));
	if (!done.ok())
		return problemFor(done.error());
	return accepted();
}

Response showMessage(const Request &r)
{
	/* Absent reads as a hint, which is the one that goes away by itself. A
	   caller that says nothing is not asking for something the person in front
	   of the box has to dismiss. */
	const bool box = r.has("kind") && r.asString("kind") == "box";
	coreapi::Result<void> done =
		coreapi::osd::message(box ? coreapi::osd::MessageKind::Box
		                          : coreapi::osd::MessageKind::Hint,
		                      r.asString("text"));
	if (!done.ok())
		return problemFor(done.error());
	return accepted();
}

const FieldDesc kKeyNameFields[] = {
	HTTPD_LIST_OF_VALUES("items", ElementType::String,
		"every key name the box knows, in the order its own table keeps them, which names a few of them twice"),
};

const Schema kKeyNameSchema = { "keys", HTTPD_FIELDS(kKeyNameFields) };

const FieldDesc kRemoteFields[] = {
	HTTPD_MEMBER("locked", FieldType::Bool, "whether the box is ignoring its remote control"),
};

const Schema kRemoteSchema = { "remote", HTTPD_FIELDS(kRemoteFields) };

Response getKeyNames(const Request &)
{
	coreapi::Result<coreapi::KeyNameList> names = coreapi::osd::keyNames();
	if (!names.ok())
		return problemFor(names.error());

	const coreapi::KeyNameList &list = names.value();
	Response out = okJson();
	// Room for the whole list at once, because the names are short and known
	// and a document grown a name at a time is a hundred reallocations of the
	// same string.
	Json j(out.body, 32 * list.size() + 32);
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < list.size(); ++i)
		j.value(list[i]);
	j.endArray();
	j.endObject();
	return out;
}

Response sendKey(const Request &r)
{
	coreapi::Result<void> done = coreapi::osd::sendKey(r.asString("name"));
	if (!done.ok())
		return problemFor(done.error());
	return accepted();
}

Response getRemote(const Request &)
{
	coreapi::Result<bool> off = coreapi::osd::locked();
	if (!off.ok())
		return problemFor(off.error());

	Response out = okJson();
	Json j(out.body, 32);
	j.beginObject();
	j.key("locked");
	j.value(off.value());
	j.endObject();
	return out;
}

Response setRemote(const Request &r)
{
	coreapi::Result<void> done = coreapi::osd::setLocked(r.asBool("locked"));
	if (!done.ok())
		return problemFor(done.error());
	return accepted();
}

/* The four, written once. The row that describes the answer and the row that
   declares what a caller may write are the two ends of one set, and two copies
   of it could come to name different halves of the same name. */
const char kInfoIconsValues[] = "static,popup,infoviewer,off";

const char kInfoIconsValueDocs[] =
	"static: the box draws the icons itself, in the plain skin that keeps them visible\n"
	"popup: the box draws the icons itself, in the popup skin\n"
	"infoviewer: the box does not draw the icons; the infobar draws them instead, in its own skin\n"
	"off: nothing draws the icons; if the skin was infoviewer it is moved to static first, "
	"so the infobar does not take the drawing back on its own";

const FieldDesc kInfoIconsFields[] = {
	HTTPD_MEMBER_OF_SET("state", kInfoIconsValues,
		"which of the 4 states the icons that say what the channel is carrying are in",
		kInfoIconsValueDocs),
};

const Schema kInfoIconsSchema = { "infoicons", HTTPD_FIELDS(kInfoIconsFields) };

/* The one naming of the four, read in both directions below. Written as a
   switch and not as a table so that a state added to the enumeration stops this
   build: an unhandled enumerator is an error in this directory, and a name this
   could not give would otherwise be a state the API answers with nothing. */
const char *infoIconsName(coreapi::osd::InfoIcons s)
{
	switch (s)
	{
		case coreapi::osd::InfoIcons::Static:     return "static";
		case coreapi::osd::InfoIcons::Popup:      return "popup";
		case coreapi::osd::InfoIcons::Infoviewer: return "infoviewer";
		case coreapi::osd::InfoIcons::Off:        return "off";
	}
	/* A value cast into the enumeration, which nothing on this side produces. One of
	   the four and not a word outside them, because the row that describes this
	   answer states the four and a name outside them would be the server stepping
	   outside a set it published. The one the write of an unknown name falls back
	   to, so both ends answer the same for a value neither can make. */
	return "off";
}

/* The way back, over the same naming rather than over a second list of the same
   words: two lists of four names are two things that can come apart, and the
   one they would come apart over is which state a caller asked for. */
bool infoIconsFor(const std::string &name, coreapi::osd::InfoIcons &out)
{
	static const coreapi::osd::InfoIcons kAll[] = {
		coreapi::osd::InfoIcons::Static,
		coreapi::osd::InfoIcons::Popup,
		coreapi::osd::InfoIcons::Infoviewer,
		coreapi::osd::InfoIcons::Off,
	};

	for (size_t i = 0; i < sizeof(kAll) / sizeof(kAll[0]); ++i)
	{
		if (name == infoIconsName(kAll[i]))
		{
			out = kAll[i];
			return true;
		}
	}
	return false;
}

Response getInfoIcons(const Request &)
{
	coreapi::Result<coreapi::osd::InfoIcons> state = coreapi::osd::infoIcons();
	if (!state.ok())
		return problemFor(state.error());

	Response out = okJson();
	Json j(out.body, 48);
	j.beginObject();
	j.key("state");
	j.value(infoIconsName(state.value()));
	j.endObject();
	return out;
}

Response setInfoIcons(const Request &r)
{
	coreapi::osd::InfoIcons want = coreapi::osd::InfoIcons::Off;
	if (!infoIconsFor(r.asString("state"), want))
	{
		/* The row below lists these same four names and the router refuses
		   anything else, so this answers only for a row that has come apart
		   from the naming above it. Nothing the caller sent is wrong. */
		return problemResponse(StatusInternalServerError, coreapi::ErrorCode::NotAListedValue,
				       "the box does not know which state that name stands for");
	}

	coreapi::Result<void> done = coreapi::osd::setInfoIcons(want);
	if (!done.ok())
		return problemFor(done.error());

	/* Done and with nothing to say about itself: the value is in the store the moment
	   this answers and the read beside it answers the same. What has not happened yet
	   is the box acting on it, which is true of every setting this server writes. */
	return noContent();
}

const Param kVolumeParams[] = {
	HTTPD_BODY_REQUIRED_IN("percent", ParamType::Int,
		"the new volume, as a percentage from 0 to 100, carried to the box in a single byte", 0, 100),
};

const Param kMuteParams[] = {
	HTTPD_BODY_REQUIRED("on", ParamType::Bool,
		"true to mute the sound, false to turn it back on at the level it was before"),
};

const Param kMessageParams[] = {
	/* The ceiling is the one the layer below carries and not a second opinion about
	   it: the length becomes an allocation and a read of exactly that many bytes on
	   the thread the box draws on, and it is refused here so a body over it never
	   reaches the code that would allocate for it. Written as that constant. */
	HTTPD_BODY_REQUIRED_TEXT("text", "the words to put on the screen, at most 4096 bytes long and never empty",
		(long) coreapi::osd::MAX_MESSAGE_BYTES),
	HTTPD_BODY_FROM_SET("kind", "which of the 2 ways the box shows the message; see the values below",
		"hint,box",
		"hint: a small box that goes away by itself after the box's own popup message timeout, or as soon as OK is pressed\n"
		"box: a message box that stays on screen until it is dismissed with the back button"),
};

/* What a picture goes out as, from the form it was written in. One place, so
   that the name on the wire and the bytes behind it cannot part: a caller is
   told which of the two it is holding by this header alone, and every decoder
   it hands the answer to reads that header before it reads a byte. */
const char *pictureContentType(coreapi::PictureFormat f)
{
	switch (f)
	{
		case coreapi::PictureFormat::Png:  return "image/png";
		case coreapi::PictureFormat::Jpeg: return "image/jpeg";
	}
	// Unreachable for a value of the enumeration, and this directory refuses
	// to compile a switch that has left one out.
	return "image/png";
}

// What a caller asked for, and the first of them for a request that asked for
// nothing, which is what the route answered before it had the choice.
coreapi::PictureFormat pictureFormatOf(const Request &r)
{
	return (r.has("format") && r.asString("format") == "jpeg")
		? coreapi::PictureFormat::Jpeg
		: coreapi::PictureFormat::Png;
}

/* The file the box has just written, as the answer. Opened here and handed to the
   transport, which sends its bytes from the kernel to the socket and gives the
   descriptor back when the answer is done with.

   The status is written before the file is put behind it and not left to the builder:
   what that one does is state the length off the descriptor, and an answer carrying
   no status at all is one the transport replaces with a fault about this layer. */
Response pictureAt(const std::string &path, coreapi::ErrorCode missing, const char *type)
{
	const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return problemResponse(StatusInternalServerError, missing,
				       "the picture the box took could not be read");

	Response out;
	out.code = StatusOk;
	out.content_type = type;
	// The descriptor is taken over whichever way this answers, so there is
	// nothing left here to give back.
	if (!answerFromDescriptor(out, fd))
		return problemResponse(StatusInternalServerError, missing,
				       "the picture the box took could not be read");
	return out;
}

Response getScreenshot(const Request &r)
{
	/* Both halves unless the caller says otherwise, which is what the copied
	   interface answers a request that names neither. */
	const bool osd = !r.has("osd") || r.asBool("osd");
	const bool video = !r.has("video") || r.asBool("video");
	const coreapi::PictureFormat format = pictureFormatOf(r);

	coreapi::Result<std::string> taken = coreapi::osd::screenshot(osd, video, format);
	if (!taken.ok())
		return problemFor(taken.error());
	/* The form is read from the request and not from the name the layer below
	   answered with, which is the same reading twice over: that name was
	   chosen from this form, and a header taken off a file name would be this
	   layer trusting a string to say what it already knows. */
	return pictureAt(taken.value(), coreapi::ErrorCode::ScreenNotCaptured,
			 pictureContentType(format));
}

Response getDisplayScreenshot(const Request &r)
{
	coreapi::Result<std::string> taken = coreapi::osd::displayScreenshot(r.asString("name"));
	if (!taken.ok())
		return problemFor(taken.error());
	/* The display is written by whatever drives it and in one form only, so
	   this route offers no choice rather than offering one it would have to
	   refuse. */
	return pictureAt(taken.value(), coreapi::ErrorCode::DisplayNotCaptured,
			 pictureContentType(coreapi::PictureFormat::Png));
}

// The older route names no display and answers the first one listed.
Response getFirstDisplayScreenshot(const Request &)
{
	coreapi::Result<std::string> taken = coreapi::osd::displayScreenshot();
	if (!taken.ok())
		return problemFor(taken.error());
	return pictureAt(taken.value(), coreapi::ErrorCode::DisplayNotCaptured,
			 pictureContentType(coreapi::PictureFormat::Png));
}

const FieldDesc kDisplayFields[] = {
	HTTPD_MEMBER("name", FieldType::String,
		"the identifier the picture of this display is asked for under, graphlcd or lcd4linux"),
	HTTPD_MEMBER("title", FieldType::String, "what to call the display to a person"),
};

const Schema kDisplaySchema = { "display", HTTPD_FIELDS(kDisplayFields) };

const FieldDesc kDisplayListFields[] = {
	HTTPD_LIST_OF("items", &kDisplaySchema,
		"every display on the front of the box that can be pictured right now, empty on a box with none"),
};

const Schema kDisplayListSchema = { "display-list", HTTPD_FIELDS(kDisplayListFields) };

Response listDisplays(const Request &)
{
	coreapi::Result<std::vector<coreapi::osd::Display> > got = coreapi::osd::displays();
	if (!got.ok())
		return problemFor(got.error());

	const std::vector<coreapi::osd::Display> &all = got.value();
	Response out = okJson();
	Json j(out.body, 32 + 64 * all.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < all.size(); ++i)
	{
		j.beginObject();
		j.key("name");
		j.value(all[i].name);
		j.key("title");
		j.value(all[i].title);
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return out;
}

const Param kKeyParams[] = {
	/* The ceiling is the one the layer below carries rather than a second
	   opinion about it, for the reason the message ceiling is: written as that
	   constant, the two cannot part. */
	HTTPD_BODY_REQUIRED_TEXT("name",
		"the key to send, spelt exactly as GET /api/v1/osd/remote/keys names it, at most 32 bytes long",
		(long) coreapi::osd::MAX_KEY_NAME_BYTES),
};

const Param kRemoteParams[] = {
	HTTPD_BODY_REQUIRED("locked", ParamType::Bool, "whether the box is to ignore its remote control"),
};

const Param kInfoIconsParams[] = {
	HTTPD_BODY_REQUIRED_FROM_SET("state", "which of the 4 states to put the icons in; see the values below",
		kInfoIconsValues, kInfoIconsValueDocs),
};

const Param kPictureParams[] = {
	// Carried in the query, because the method that fetches a picture is
	// written with no body.
	HTTPD_QUERY("osd", ParamType::Bool, "whether the graphics the box draws over the picture are included, true by default"),
	HTTPD_QUERY("video", ParamType::Bool, "whether the live video itself is included in the picture, true by default"),
	/* A set and not a free string, so a form this box cannot write is refused where
	   the request is read instead of being answered with a picture in whichever form
	   the branch below fell through to. The two named here are the two the capture
	   can write for a socket; the size is the box's and is not asked for. */
	HTTPD_QUERY_FROM_SET("format", "which image format the picture is encoded as; see the values below",
		"png,jpeg",
		"png: lossless, the larger file for a picture of moving video\n"
		"jpeg: throws detail away to be smaller, much smaller for a picture of moving video"),
	/* NOT READ, AND HERE BECAUSE A BROWSER LEAVES NO OTHER WAY TO ASK TWICE.
	   A capture is a moment, and asking for the same moment again is asking
	   for a different picture. An image element cannot carry a header, and a
	   browser answers a second element with the same address out of the copy
	   it already has in the page, whatever this says about storing it:
	   measured on Chromium 152, where three presses of the button that fetches
	   a fresh capture put nothing at all on the wire. So the caller says which
	   capture it is asking for, and two captures are two addresses. Any number
	   will do and the box looks at none of them. */
	HTTPD_QUERY("at", ParamType::UInt, "any number, which this does not read: a capture is a moment, and 2 moments asked for under one address are one picture in a browser"),
};

// The same and for the same reason, for the pictures that have nothing else
// to say about themselves.
const Param kDisplayPictureParams[] = {
	HTTPD_SEGMENT_TEXT("name", "the display's name field, as GET /api/v1/osd/displays lists it", 32),
	HTTPD_QUERY("at", ParamType::UInt, "any number, which this does not read: a capture is a moment, and 2 moments asked for under one address are one picture in a browser"),
};

// The older route, which names no display.
const Param kFirstDisplayPictureParams[] = {
	HTTPD_QUERY("at", ParamType::UInt, "any number, which this does not read: a capture is a moment, and 2 moments asked for under one address are one picture in a browser"),
};

const RouteRefusal kGetVolumeRefusals[] = {
	HTTPD_REFUSES(Internal, MuteUnavailable,
		"the mute state could not be read"),
};

const RouteRefusal kSendKeyRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, NoSuchKey,
		"the remote control has no key of that name"),
};

const RouteRefusal kGetScreenshotRefusals[] = {
	HTTPD_REFUSES(NotSupported, ScreenNotCaptured,
		"the box could not take a picture of its screen"),
};

const RouteRefusal kGetDisplayScreenshotRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchDisplay,
		"this box has no display of that name"),
	HTTPD_REFUSES(NotSupported, DisplayNotCaptured,
		"the display is not running, or the box could not take a picture of it"),
	HTTPD_REFUSES(Internal, DisplayNotCaptured,
		"the picture of the display could not be written"),
};

const RouteRefusal kGetFirstDisplayScreenshotRefusals[] = {
	HTTPD_REFUSES(NotSupported, DisplayNotCaptured,
		"no display is drawing, or the box could not take a picture of it"),
	HTTPD_REFUSES(Internal, DisplayNotCaptured,
		"the picture of the display could not be written"),
};

const Endpoint kOsdEndpoints[] = {
	{ Method::Get, "/api/v1/osd/volume", AuthLevel::Read,
	  "how loud the box is and whether the sound is off", "Reads the current volume "
	  "and the mute flag together, both taken at the same moment. The volume is a "
	  "percentage from 0 to 100 and is independent of mute: muting does not change "
	  "it, so turning the sound back on restores the level it had before.\n\n"
	  "**Refusals:**\n"
	  "- `500 mute-unavailable`: the mute state could not be read from the box.",
	  NULL, 0, &kVolumeSchema, &getVolume, false,
	  Answers200, HTTPD_REFUSALS(kGetVolumeRefusals) },
	{ Method::Put, "/api/v1/osd/volume", AuthLevel::Write,
	  "asks the box to change the volume", "Sets the volume to `percent`, a value "
	  "from 0 to 100. `202` means the box has queued the change, not that it has "
	  "been applied: watch for the `volume` event on `GET /api/v1/events`, or read "
	  "`GET /api/v1/osd/volume` again. The level is carried to the box in a single "
	  "byte, which is why it is bounded to 0 to 100 both here and once more before "
	  "it is applied. Changing the volume does not turn mute off, so a muted box "
	  "stays muted at the new level until `PUT /api/v1/osd/mute` is sent.\n\n"
	  "**Related:** `GET /api/v1/osd/volume`, `PUT /api/v1/osd/mute`.",
	  HTTPD_PARAMS(kVolumeParams), NULL, &setVolume, false,
	  Answers202, HTTPD_NO_REFUSALS },
	{ Method::Put, "/api/v1/osd/mute", AuthLevel::Write,
	  "asks the box to turn the sound off or on", "Sets the mute flag to `on`, "
	  "independently of the volume level, which is kept apart so that turning the "
	  "sound back on restores the level it had before. `202` means the box has "
	  "queued the change: watch for the `mute` event on `GET /api/v1/events`, or "
	  "read `GET /api/v1/osd/volume` again.\n\n"
	  "**Related:** `GET /api/v1/osd/volume`.",
	  HTTPD_PARAMS(kMuteParams), NULL, &setMuted, false,
	  Answers202, HTTPD_NO_REFUSALS },
	{ Method::Post, "/api/v1/osd/message", AuthLevel::Write,
	  "asks the box to put words on the screen", "Shows `text` on the screen, as "
	  "either a hint that goes away by itself or a message box that waits to be "
	  "dismissed, depending on `kind`. `202` means the box has queued the message, "
	  "not that it is on screen yet; nothing on this interface reports when it "
	  "goes away.\n\n"
	  "**Preconditions:** the box is not in standby and not showing an AV input; "
	  "in either of those the message is silently dropped and never appears.",
	  HTTPD_PARAMS(kMessageParams), NULL, &showMessage, false,
	  Answers202, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/osd/remote", AuthLevel::Read,
	  "whether the box is ignoring its remote control", "Reads whether the box is "
	  "currently locked against its own remote control, as `PUT /api/v1/osd/remote` "
	  "sets it.\n\n"
	  "**Related:** `PUT /api/v1/osd/remote`.",
	  NULL, 0, &kRemoteSchema, &getRemote, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Put, "/api/v1/osd/remote", AuthLevel::Write,
	  "asks the box to start or stop ignoring its remote control", "Sets whether "
	  "the box ignores key presses from its own remote control. `202` means the "
	  "box has queued the change: read `GET /api/v1/osd/remote` to see it take "
	  "effect. This has no effect on `POST /api/v1/osd/remote/key`, which injects "
	  "a key directly and does not go through the remote control receiver.\n\n"
	  "**Related:** `GET /api/v1/osd/remote`, `POST /api/v1/osd/remote/key`.",
	  HTTPD_PARAMS(kRemoteParams), NULL, &setRemote, false,
	  Answers202, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/osd/remote/keys", AuthLevel::Read,
	  "every key name the box knows", "Lists every key name the box's own input "
	  "table carries, in the order that table keeps them. A few names appear "
	  "twice because the table maps 2 physical codes to the same name; sending "
	  "such a name with `POST /api/v1/osd/remote/key` always reaches the first of "
	  "those 2 codes. These are the only names `POST /api/v1/osd/remote/key` accepts.\n\n"
	  "**Related:** `POST /api/v1/osd/remote/key`.",
	  NULL, 0, &kKeyNameSchema, &getKeyNames, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Post, "/api/v1/osd/remote/key", AuthLevel::Write,
	  "asks the box to act on a key as though it had been pressed", "Injects the "
	  "named key as a press followed immediately by a release, as though it had "
	  "come from the remote control. `202` means the box has queued the key; there "
	  "is no separate event that confirms it was acted on. This writes the key "
	  "directly to the box's input device and ignores the remote control lock set "
	  "by `PUT /api/v1/osd/remote`. There is no repeat and no long press: every "
	  "call injects exactly one press and one release.\n\n"
	  "**Refusals:**\n"
	  "- `400 no-such-key`: `name` is not one of the names `GET /api/v1/osd/remote/keys` "
	  "lists.\n\n"
	  "**Related:** `GET /api/v1/osd/remote/keys`.",
	  HTTPD_PARAMS(kKeyParams), NULL, &sendKey, false,
	  Answers202, HTTPD_REFUSALS_AND_BODY(kSendKeyRefusals, "{\"name\":\"KEY_OK\"}") },
	{ Method::Get, "/api/v1/osd/infoicons", AuthLevel::Read,
	  "which state the icons that say what the channel is carrying are in", "Reads "
	  "which of the 4 states the small icons that say what the current channel is "
	  "carrying (for example scrambled or carrying subtitles) are in. The box "
	  "keeps this as 2 separate settings, and this answers their combination as "
	  "one of the 4 named states; see the values listed for `state` on "
	  "`PUT /api/v1/osd/infoicons`.\n\n"
	  "**Related:** `PUT /api/v1/osd/infoicons`.",
	  NULL, 0, &kInfoIconsSchema, &getInfoIcons, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Put, "/api/v1/osd/infoicons", AuthLevel::Write,
	  "puts those icons in one of the 4 states, which is 2 settings and a rule that is in neither",
	  "Writes both of the settings the icon state is made of in one save, so the "
	  "box is never left with only one of them changed. `204` means both values "
	  "are taken: a read answers with them from then on, and the box saves its "
	  "configuration file on its own loop right after, waiting while a text input "
	  "is open on the box.\n\n"
	  "**Side effects:** takes 2 settings and has the box save its configuration "
	  "file on its own loop.",
	  HTTPD_PARAMS(kInfoIconsParams), NULL, &setInfoIcons, false,
	  Answers204, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/osd/screenshot", AuthLevel::Read,
	  "a picture of what the television is showing", "Takes a picture of the "
	  "live screen and returns it as an image, in the format `format` names. "
	  "`osd` and `video` each default to true and say whether the box's own OSD "
	  "graphics and the live video are each drawn into the picture; leaving both "
	  "at their default gives the same picture the television shows. The `at` "
	  "parameter is never read; it exists only so that 2 requests for a fresh "
	  "picture use 2 different addresses, which keeps a browser from answering a "
	  "second `<img>` out of its own cache.\n\n"
	  "**Preconditions:** only one capture of the screen runs at a time; a request "
	  "that arrives while another is still running is refused with a conflict "
	  "rather than waiting for it.\n\n"
	  "**Refusals:**\n"
	  "- `501 screen-not-captured`: this build or this box has no way of reading "
	  "its own screen.",
	  HTTPD_PARAMS(kPictureParams), NULL, &getScreenshot, false,
	  Answers200 | Answers206, HTTPD_REFUSALS(kGetScreenshotRefusals) },
	{ Method::Get, "/api/v1/osd/display/screenshot", AuthLevel::Read,
	  "a picture of the first display on the front of the box", "Takes a picture of "
	  "the first display `GET /api/v1/osd/displays` lists and returns it as a PNG "
	  "image. Most boxes have no such display; this answers a refusal rather than a "
	  "picture for them, and `format` is not offered because this is written in one "
	  "form only. The `at` parameter is never read; it exists only so that 2 requests "
	  "for a fresh picture use 2 different addresses, which keeps a browser from "
	  "answering a second `<img>` out of its own cache.\n\n"
	  "**Refusals:**\n"
	  "- `501 display-not-captured`: no display is drawing on this box, or this build "
	  "was made without support for it.\n"
	  "- `500 display-not-captured`: the picture could not be copied or written.\n\n"
	  "**Related:** `GET /api/v1/osd/displays/{name}/screenshot` names the display.",
	  HTTPD_PARAMS(kFirstDisplayPictureParams), NULL, &getFirstDisplayScreenshot, false,
	  Answers200 | Answers206, HTTPD_REFUSALS(kGetFirstDisplayScreenshotRefusals) },
	{ Method::Get, "/api/v1/osd/displays", AuthLevel::Read,
	  "the displays on the front of the box that can be pictured", "Lists the displays "
	  "on the front of the box that are drawing right now: `graphlcd` where GraphLCD is "
	  "built and running, `lcd4linux` where LCD4Linux is switched on with its screenshots "
	  "and is writing its picture. Both can be listed at once. A box with none answers "
	  "an empty list rather than a refusal.\n\n"
	  "**Related:** `GET /api/v1/osd/displays/{name}/screenshot`.",
	  NULL, 0, &kDisplayListSchema, &listDisplays, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/osd/displays/{name}/screenshot", AuthLevel::Read,
	  "a picture of one display on the front of the box", "Takes a picture of the "
	  "named display and returns it as a PNG image; `format` is not offered because "
	  "this is written in one form only. The `at` parameter is never read; it exists "
	  "only so that 2 requests for a fresh picture use 2 different addresses, which "
	  "keeps a browser from answering a second `<img>` out of its own cache.\n\n"
	  "**Refusals:**\n"
	  "- `404 no-such-display`: no display has that name. Take names from "
	  "`GET /api/v1/osd/displays`.\n"
	  "- `501 display-not-captured`: this display is not running on this box, or "
	  "this build was made without support for it.\n"
	  "- `500 display-not-captured`: the picture could not be copied or written.\n\n"
	  "**Related:** `GET /api/v1/osd/displays`.",
	  HTTPD_PARAMS(kDisplayPictureParams), NULL, &getDisplayScreenshot, false,
	  Answers200 | Answers206, HTTPD_REFUSALS(kGetDisplayScreenshotRefusals) },
};

} // namespace

extern const RouteTable osdTable = {
	HTTPD_TABLE("osd", kOsdEndpoints)
};

} // namespace httpd
