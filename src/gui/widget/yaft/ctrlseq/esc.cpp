/*
 * yaft framebuffer terminal as C++ class for embedding in neutrino-MP
 * (C) 2018 Stefan Seyfried
 * License: GPL-2.0
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * derived from yaft/ctrlseq/esc.h
 * original code
 * Copyright (c) 2012 haru <uobikiemukot at gmail dot com>
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 */

#include "yaft_priv.h"

/* function for control character */
void YaFT_p::bs(void)
{
	move_cursor(0, -1);
}

void YaFT_p::tab(void)
{
	int i;
	txt.back().append(" ");
	for (i = cursor.x + 1; i < cols; i++) {
		if (tabstop[i]) {
			set_cursor(cursor.y, i);
			return;
		}
	}
	set_cursor(cursor.y, cols - 1);
}

void YaFT_p::nl(void)
{
	txt.push("");
	move_cursor(1, 0);
}

void YaFT_p::cr(void)
{
	set_cursor(cursor.y, 0);
}

void YaFT_p::enter_esc(void)
{
	esc.state = STATE_ESC;
}

/* function for escape sequence */
void YaFT_p::save_state(void)
{
	state.mode = mode & MODE_ORIGIN;
	state.cursor = cursor;
	state.attribute = attribute;
}

void YaFT_p::restore_state(void)
{
	/* restore state */
	if (state.mode & MODE_ORIGIN)
		mode |= MODE_ORIGIN;
	else
		mode &= ~MODE_ORIGIN;
	cursor    = state.cursor;
	attribute = state.attribute;
}

void YaFT_p::crnl(void)
{
	cr();
	nl();
}

void YaFT_p::set_tabstop(void)
{
	tabstop[cursor.x] = true;
}

void YaFT_p::reverse_nl(void)
{
	move_cursor(-1, 0);
}

void YaFT_p::identify(void)
{
	ssize_t ignored __attribute__((unused)) = write(fd, "\033[?6c", 5); /* "I am a VT102" */
}

void YaFT_p::enter_csi(void)
{
	esc.state = STATE_CSI;
}

void YaFT_p::enter_osc(void)
{
	esc.state = STATE_OSC;
}

void YaFT_p::ris(void)
{
	reset();
}
