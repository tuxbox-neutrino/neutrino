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


#ifndef __listbox_legacy__
#define __listbox_legacy__

#include "listbox.h"

class CListBoxExt : public CListBox
{
 protected:
	neutrino_locale_t saveBoxCaption;
	const char *      saveBoxText;
    
	void setTitle(const char * const title);

	virtual void hide();
	void setSaveDialogText(const neutrino_locale_t title, const char * const text);
	virtual void onSaveData(){};
    
 public:
	CListBoxExt(void); 
};


#endif
