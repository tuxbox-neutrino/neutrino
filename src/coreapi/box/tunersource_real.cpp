/*
 * tunersource_real.cpp - tuner readings from the running frontends
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

#include <config.h>

#include "coreapi/base/deps.h"

#include <zapit/client/zapitclient.h>
#include <zapit/femanager.h>
#include <zapit/frontend_c.h>

extern CZapitClient *g_Zapit;

namespace coreapi
{

namespace
{

/* Sixteen bits, because both readings come back from the frontend as a sixteen
   bit relative value whichever of the two driver interfaces answered. Carried up
   with every reading, so a reader dividing by it divides by what this source
   measured against rather than by a number it assumed. */
const uint32_t SIGNAL_FULL_SCALE = 0xFFFF;

// Its own translation unit, so that the frontend stack is only pulled in by a
// binary that installs this.
class RealTunerSource : public TunerSource
{
	public:
		Status frontends(FrontendList &out) const
		{
			CFEManager *m = CFEManager::getInstance();
			const CFrontend *live = m->getLiveFE();
			const int count = m->getFrontendCount();

			out.clear();
			for (int i = 0; i < count; ++i)
			{
				/* A frontend the walk cannot reach is skipped rather than
				   entered as an empty row, which would be a tuner the box does
				   not have. */
				CFrontend *fe = m->getFE(i);
				if (fe == NULL)
					continue;

				FrontendInfo one;
				one.adapter = fe->getAdapter();
				one.number = fe->getNumber();
				one.name = fe->getName();
				one.satellite = fe->hasSat();
				one.cable = fe->hasCable();
				one.terrestrial = fe->hasTerr();
				// Something is holding it, which on a twin tuner box is what a
				// recording looks like from here.
				one.in_use = fe->Locked();
				one.live = (fe == live);
				out.push_back(one);
			}
			return Status::Ok;
		}

		Status liveSignal(SignalInfo &out) const
		{
			CFrontend *fe = CFEManager::getInstance()->getLiveFE();
			// A box with no tuner and one whose tuner has been given nothing
			// to receive both arrive here, and neither is a fault.
			if (fe == NULL)
				return Status::NotSupported;

			out.adapter = fe->getAdapter();
			out.number = fe->getNumber();
			// Raw, as the frontend reports them. What they mean as a
			// percentage is the reader's arithmetic and not this one's.
			out.strength = fe->getSignalStrength();
			out.snr = fe->getSignalNoiseRatio();
			out.full_scale = SIGNAL_FULL_SCALE;
			out.bit_error_rate = fe->getBitErrorRate();
			out.locked = (fe->getStatus() & FE_HAS_LOCK) != 0;
			return Status::Ok;
		}

		/* The three the channel daemon is told, in the order the shell branch
		   this replaces told them: into standby, out of standby, and tune the
		   running channel again. Standby is what closes the frontends and opens
		   them, which is the whole of a reset; leaving it tunes what was
		   playing, and the third is what the branch asked for beside that.

		   THROUGH THE CLIENT AND NOT THROUGH A SCRIPT. The branch started three
		   copies of a command line program that does exactly this, and slept a
		   second between them so one process was gone before the next began.
		   Here there is no process to wait for, and the two standby calls are
		   answered by the daemon before the next one is sent.

		   Nothing asks whether the box is in standby first. The daemon refuses
		   to enter a standby it is already in, so a reset asked for there would
		   leave it; there is no question on this client that could tell them
		   apart, and the branch this replaces had the same hole. */
		Status reset()
		{
			if (g_Zapit == NULL)
				return Status::Internal;

			g_Zapit->setStandby(true);
			g_Zapit->setStandby(false);
			// The only one of the three that says whether it landed, the other
			// two answering nothing at all.
			if (!g_Zapit->Rezap())
				return Status::Internal;
			return Status::Ok;
		}
};

/* No test of what getInstance answers, in either call above: it makes the
   manager on the first ask and never answers nothing. The frontends are found
   once while the box starts and the collection is not resized afterwards, which
   lets a walk of it run here without taking the manager's lock; taking it would
   put a request behind whatever tuning is under way. Which frontend is the live
   one is a pointer another thread may replace while this reads it, and the worst
   that costs is one answer naming the frontend that was live a moment ago. */

RealTunerSource g_real_tuner;

} // anonymous namespace

void installRealTunerSource() { setTunerSource(&g_real_tuner); }

} // namespace coreapi
