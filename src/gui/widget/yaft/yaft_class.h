/*
 * definition for embedding the YaFT framebuffer terminal in c++
 * (C) 2018 Stefan Seyfried
 * License: GPL-2.0+
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef __yaft_class__
#define __yaft_class__
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
