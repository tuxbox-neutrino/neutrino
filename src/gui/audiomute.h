/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	audioMute - Neutrino-GUI
	Copyright (C) 2013 M. Liebmann (micha-bbg)

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/


#ifndef __CAUDIOMUTE__
#define __CAUDIOMUTE__

#include <gui/components/cc.h>

class CAudioMute : public CComponentsPicture
{
	private:
		int y_old;
		bool do_paint_mute_icon;

	public:

		CAudioMute();
// 		~CAudioMute();
		static CAudioMute* getInstance();

		void AudioMute(int newValue, bool isEvent= false);
		void doPaintMuteIcon(bool mode) { do_paint_mute_icon = mode; }
		void enableMuteIcon(bool enable);

		bool getStatus(void) { return do_paint_mute_icon; }

		/**The mute icon deliberately does NOT reserve its area against other
		 * painters, and this is a trade-off, not a claim that it is safe.
		 *
		 * Reserving it would be worse: the icon sits in the top right corner and
		 * stays there for as long as the box is muted, so it could permanently
		 * stop the channel list from drawing its EPG infozone - an area that is
		 * never given back, which the deferred-repaint path cannot recover from
		 * either.
		 *
		 * Not reserving it leaves the icon on the older mechanism,
		 * CFrameBuffer::checkFbArea(), which takes it down before a foreign
		 * paint and puts it back afterwards. That mechanism guards itself with a
		 * plain fb_no_check flag rather than a lock, so with two threads painting
		 * it can still miss a paint and restore a stale icon background. That
		 * race predates this protocol, but note it does get a wider window:
		 * fb_no_check stays set across CAudioMute::hide(), and that call can now
		 * wait for overlay_paint_mutex - for as long as another thread is
		 * painting under it. While it waits, other painters skip the icon
		 * handling. The result is a stale mute icon, nothing more.
		 * @see		CFrameBuffer::addOverlay()
		*/
		bool claimsBackgroundArea(){return false;}
};

#endif // __CAUDIOMUTE__
