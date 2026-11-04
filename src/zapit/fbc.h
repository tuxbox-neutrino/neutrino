/*
 * Copyright (C) 2025 TangoCash
 *
 * License: GPLv2
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

#ifndef __fbc_h__
#define __fbc_h__

#include <map>
#include <string>

/*
 * Full Band Capture tuners
 *
 * An FBC frontend is one input feeding several demodulators: the tuners of a
 * set share the inputs of the card, and a tuner that is not the top of its
 * set has no input of its own until it is tied to one that has. What the box
 * says about a slot, and what it is told, is in
 *
 *	/proc/stb/frontend/<slot>/fbc_set_id           the set it belongs to
 *	/proc/stb/frontend/<slot>/fbc_connect_choices  which positions of the
 *	                                               set may be its top
 *	/proc/stb/frontend/<slot>/fbc_connect          the position it is tied to
 *	/proc/stb/frontend/<slot>/fbc_link             whether it is tied to one
 *
 * A box without FBC tuners has none of these, and every call here answers as
 * if there were no FBC tuner in the slot. This is what enigma2's
 * eFBCTunerManager (lib/dvb/fbc.cpp) does, in the shape this tree keeps its
 * frontends in: a slot is the frontend's number, and the root of a set is the
 * master a link tuner is looped from.
 */

typedef struct fbc_tuner {
	int		set_id;		/* the set the slot belongs to, -1 if not FBC */
	int		position;	/* where the slot sits within its set */
	bool		is_root;	/* whether it may carry an input of its own */
	int		connected_to;	/* the position it is tied to, -1 if none */
	bool		linked;		/* whether fbc_link says it is tied to it */
} fbc_tuner_t;

typedef std::map<int, fbc_tuner_t> fbc_tuner_map_t;
typedef fbc_tuner_map_t::iterator fbc_tuner_map_iterator_t;
typedef fbc_tuner_map_t::const_iterator fbc_tuner_map_const_iterator_t;

class CFBCTunerManager
{
	private:
		static CFBCTunerManager *manager;

		fbc_tuner_map_t	tuners;
		bool		have_fbc;
		std::string	procdir;

		CFBCTunerManager();
		~CFBCTunerManager();

		int  ReadProcInt(int slot, const char *entry) const;
		bool WriteProcInt(int slot, const char *entry, int value) const;
		int  LoadConnectChoices(int slot) const;

	public:
		static CFBCTunerManager *getInstance();

		/* Where the box keeps what it says about its slots; only a test
		 * points this anywhere but /proc/stb/frontend. */
		void SetProcDir(const std::string &dir) { procdir = dir; }

		/* Forgets every slot. The positions of a set are counted by the
		 * slots already known, so a second round of Init() without this
		 * would count every slot twice and find no root at all. */
		void Clear();

		/* Reads what the box says about a slot, in the order the slots of a
		 * set are met, and answers whether the slot is an FBC tuner. A slot
		 * the box says nothing about is left out of the map, so every query
		 * about it answers as a box without FBC would. */
		bool Init(int slot, bool dvbs);

		bool IsFBCTuner(int slot) const;
		bool IsRoot(int slot) const;
		bool IsFBCLink(int slot) const;
		bool IsSameSet(int slot_a, int slot_b) const;

		int  GetSetID(int slot) const;
		int  GetPosition(int slot) const;
		int  GetConnectedTo(int slot) const;

		/* Whether the slot is a link tied to root_slot and marked as linked,
		 * which is what a link tuning from that root's input is. */
		bool IsLinkedTo(int slot, int root_slot) const;

		/* The root of the set the slot is in, which is the slot a link
		 * tuner of that set is tied to. Answers -1 where there is none. */
		int  GetRootOfSet(int slot) const;

		/* Ties a link tuner to the root it tunes from: the master it is
		 * looped from where that is a root of its set, and the first root
		 * of the set where it is not. Answers the slot it was tied to, or
		 * -1 where the slot is not an FBC link. */
		int  ConnectToMaster(int slot, int master_slot);

		/* Ties a link tuner to a root of its own set, so that the input the
		 * root has is the one the link tunes from, and unties it again.
		 * With link, the link leaves the LNB to the root (loop); without,
		 * it only listens to that input and keeps its own control, which is
		 * what a unicable twin does. A slot that is not an FBC link is left
		 * as it is. */
		bool Connect(int slot, int root_slot, bool link = true);
		bool Disconnect(int slot);

		/* Ties every link tuner to the root of its set, and every root to
		 * itself, which is where the box is left when nothing is recording.
		 */
		void ConnectAll();

		void PrintLinks() const;
};

#endif /* __fbc_h__ */
