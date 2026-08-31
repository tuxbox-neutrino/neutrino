/*
	Neutrino-GUI

	Copyright (C) 2026

	License: GPL
*/

#ifndef __frontend_label__
#define __frontend_label__

#include <string>

/* The label the tuner list shows for a frontend, e.g.
   "Tuner 02: [B] Satellit TBS 5580 CI USB2.0 DVB-S/S2/S2X".

   `index` is the CFEManager frontend index -- the same one the list numbers
   by. Anything that points a user at a tuner has to use this, otherwise the
   name in the message and the entry in the list drift apart. */
std::string getFrontendLabel(int index);

/* Same label for a frontend named by its device coordinates, which is how the
   zap failure analyzer reports one. Returns an empty string when no frontend
   matches -- the caller then has nothing to name and must say so differently. */
std::string getFrontendLabelForDevice(int adapter, int number);

/* The name used for a frontend that is *not* in the frontend manager's map --
   a busy one, held by another process. It cannot be numbered like the list
   above, because that numbering comes from the map this frontend is missing
   from; it says adapter and frontend instead. Kept apart from
   getFrontendLabel() on purpose: the two number differently, and pretending
   otherwise would put two different tuners under one number. */
std::string getBusyFrontendName(int adapter, int number);

/* The letter a tuner is shown with, "" when the index is beyond the alphabet
   in use. Exposed so the menus that compose their own headings do not each
   keep a copy of the table; they index it differently from the tuner list
   (by frontend number, not by the manager's index), which is why they cannot
   simply call getFrontendLabel(). */
const char *getFrontendLetter(int index);

#endif
