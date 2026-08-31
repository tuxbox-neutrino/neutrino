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

#endif
