/*
	channellock.h -- who holds the channel line of the graphic display

	Copyright (C) 2026 NI-Team

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, see <http://www.gnu.org/licenses/>.
*/

#ifndef __glcd_channellock_h__
#define __glcd_channellock_h__

/* While the channel line is locked, the display shows the text it was
   locked with instead of the channel that plays. Screens lock it while
   they are open and unlock it when they close. A failed zap locks it too,
   to show the channel name in brackets, but no screen closes after that:
   the lock has to go when the next zap starts or succeeds, and only if it
   is still the failure's own -- a menu or a player that locked the line
   since then keeps it. */
class cGLCDChannelLock
{
	public:
		enum Owner
		{
			NONE = 0,
			SCREEN,
			ZAP_FAILURE
		};

		cGLCDChannelLock() : owner(NONE) {}

		void lock(Owner o) { owner = o; }
		void unlock() { owner = NONE; }

		/* Unlocks only a lock that o holds; says whether it did. */
		bool release(Owner o)
		{
			if (o == NONE || owner != o)
				return false;
			owner = NONE;
			return true;
		}

		bool locked() const { return owner != NONE; }
		Owner holder() const { return owner; }

	private:
		Owner owner;
};

#endif
