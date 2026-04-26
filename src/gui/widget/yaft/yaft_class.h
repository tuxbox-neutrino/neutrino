/*
 * Public C++ wrapper for embedding the YAFT framebuffer terminal in
 * Neutrino.
 * Copyright (C) 2018 Stefan Seyfried
 *
 * Neutrino integration license: GPL-2.0-only.
 *
 * This integration is distributed with Neutrino under the GPL terms above,
 * without warranty.
 */

#ifndef __yaft_class__
#define __yaft_class__

#include <string>
#include <sigc++/signal.h>

class YaFT : public sigc::trackable
{
 private:
	int *res;
	bool paint;
 public:
	YaFT(const char * const *argv, int *Res, bool Paint, sigc::signal<void, std::string*, int*, bool*>);
	~YaFT();
	int run(); /* returns exit code */
	sigc::signal<void, std::string*, int*, bool*> OnShellOutputLoop;
};

#endif
