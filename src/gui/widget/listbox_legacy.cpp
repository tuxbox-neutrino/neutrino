/*
  Neutrino-GUI  -   DBoxII-Project

  Copyright (C) 2003 thegoodguy

  License: GPL

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "listbox_legacy.h"

#include <gui/widget/messagebox.h>

CListBoxExt::CListBoxExt(void) : CListBox("")
{
	saveBoxCaption = NONEXISTANT_LOCALE;
	saveBoxText = NULL;
}

void CListBoxExt::setTitle(const char * const title)
{
	caption = title ? title : "";
}

void CListBoxExt::hide()
{
	//want2save?
	if ((modified) && (saveBoxCaption != NONEXISTANT_LOCALE) && (saveBoxText != NULL))
	{
		if (ShowMsgUTF(saveBoxCaption, saveBoxText, CMessageBox::mbrNo, CMessageBox::mbYes | CMessageBox::mbNo) == CMessageBox::mbrYes) // UTF-8
			onSaveData();
	}

	CListBox::hide();
}

void CListBoxExt::setSaveDialogText(const neutrino_locale_t title, const char * const text)
{
	saveBoxCaption = title;
	saveBoxText = text;
}
