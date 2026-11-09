/*
 * captureguard.h - no picture of the screen while it shows a password
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
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
 * Code that draws a password on the screen holds an Exclusion from before
 * the text is drawn until it has been taken off again. Every capture of the
 * screen takes a Capture before it reads a pixel and asks it again after the
 * reading and before anything read is written out or handed on:
 *
 * - while an Exclusion stands, a Capture is not admitted, and the capture
 *   reads nothing;
 * - an Exclusion begun after a capture was admitted, standing or already
 *   ended, leaves that Capture unclean, and the capture keeps nothing of
 *   what it read, because it may have read the password.
 *
 * Nobody waits: a capture is refused rather than held, and the thread that
 * draws the password does not wait for a capture to end, since reading the
 * screen goes through driver calls that have no deadline of their own. The
 * counters live under one lock, so an Exclusion begun after a Capture's
 * second question is begun after everything that capture read.
 *
 * What a capture read is not reachable before the second question either:
 * a capture that writes a file writes it into a Staging file no name
 * reaches, and a picture others read while it is drawn again is drawn,
 * checked and cleared under the lock its readers take (drawShared).
 *
 * Plain code over a lock and its counters, so that it links on its own.
 */

#ifndef __captureguard_h__
#define __captureguard_h__

#include <string>

namespace captureguard
{

class Exclusion
{
	public:
		Exclusion();
		~Exclusion();

	private:
		Exclusion(const Exclusion &);
		Exclusion &operator=(const Exclusion &);
};

class Capture
{
	public:
		Capture();

		// No Exclusion stood when this was taken: the capture may read.
		bool admitted() const { return admitted_; }
		/* Admitted, and no Exclusion has begun since: what was read holds
		   no password, and the capture may write it out. */
		bool clean() const;

	private:
		bool admitted_;
		unsigned long begun_;

		Capture(const Capture &);
		Capture &operator=(const Capture &);
};

/* Where a picture captureguard has agreed to is written before it gets the
   name path: in the same directory, so that a rename puts it in place,
   under a short name of its own, so that a name as long as a directory entry
   may be still has room beside it, and a new one on every call, so that two
   captures at once never write into one file. */
std::string sideName(const std::string &path);

/* Where a capture writes its picture before captureguard has agreed to it:
   a file in /tmp without a name, made without one or with its name removed
   before any gate may ask about it, so no reader, no listing and no file
   route can reach what it holds. It is written
   through path(), the name this process has for its own descriptor, which a
   program the capture starts can open as well, and given a name only by
   publish(). Closed, and gone, with this object. */
/* Called once the file is made, with the name it was made under, empty where
   it was made without one, and while no gate asks about a descriptor;
   with_a_name makes it under a name even where /tmp could do without.
   NULL, as the program runs, for none. */
void setStagingMadeForTest(void (*made)(const std::string &name), bool with_a_name);

class Staging
{
	public:
		Staging();
		~Staging();

		// The file was made.
		bool ok() const { return fd_ >= 0; }
		const std::string &path() const { return path_; }
		// Something was written into it.
		bool written() const;
		/* What was written, put at target: copied beside it first
		   (sideName) and renamed into place, so target holds the whole
		   picture or the one it held before. */
		bool publish(const std::string &target) const;

	private:
		int fd_;
		std::string path_;

		Staging(const Staging &);
		Staging &operator=(const Staging &);
};

/* Draws a picture of the screen that others read while it is drawn again,
   such as a display mirroring the OSD: with lock held, drawn only where
   captureguard admits it, and cleared before the lock is given back where
   captureguard does not agree to what was drawn. Every reader takes the same
   lock, so none of them sees a drawing before it was checked; a reader's own
   Capture cannot tell, since the drawing may have been admitted before the
   reader was. lock offers lock() and unlock(); draw answers whether it drew,
   and that is answered, false where it was not called. admitted, where
   given, is set to whether captureguard admitted the drawing. */
template <typename Lock, typename Draw, typename Clear>
bool drawShared(Lock &lock, Draw draw, Clear clear, bool *admitted = 0)
{
	lock.lock();
	Capture capture;
	const bool in = capture.admitted();
	const bool drawn = in && draw();
	if (!capture.clean())
		clear();
	lock.unlock();
	if (admitted)
		*admitted = in;
	return drawn;
}

} // namespace captureguard

#endif
