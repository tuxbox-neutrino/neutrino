/*
 * ESC/control-sequence handlers for the YAFT framebuffer terminal integration
 * in Neutrino.
 * Copyright (C) 2018 Stefan Seyfried
 *
 * Neutrino integration license: GPL-2.0-only.
 *
 * This file contains code derived from upstream yaft/ctrlseq/esc.h
 * (https://github.com/uobikiemukot/yaft), originally:
 * Copyright (c) 2012 haru <uobikiemukot at gmail dot com>
 *
 * Upstream yaft is MIT-licensed; see LICENSE in this directory for the
 * original permission notice and warranty disclaimer. This integration is
 * distributed with Neutrino under the GPL terms above, without warranty.
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
