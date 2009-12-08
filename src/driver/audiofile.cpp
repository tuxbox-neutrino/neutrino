/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: non-nil; c-basic-offset: 4 -*- */
/*
  Neutrino-GUI  -   DBoxII-Project

  Copyright (C) 2001 Steffen Hehn 'McClean'
  Homepage: http://dbox.cyberphoria.org/

  Kommentar:

  Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
  Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
  auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
  Steuerung getrennt. Diese wird dann von Daemons uebernommen.


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

#include <driver/audiofile.h>

/* default constructor */
CAudiofile::CAudiofile()
  : MetaData(), Filename(), FileType( CFile::FILE_UNKNOWN )
{
}

/* constructor */
CAudiofile::CAudiofile( std::string name, CFile::FileType type )
	: MetaData(), Filename( name ), FileType( type )
{
}

/* copy constructor */
CAudiofile::CAudiofile( const CAudiofile& src )
  : MetaData( src.MetaData ), Filename( src.Filename ),
	FileType( src.FileType )
{
}

/* assignment operator */
void CAudiofile::operator=( const CAudiofile& src )
{
	MetaData = src.MetaData;
	Filename = src.Filename;
	FileType = src.FileType;
}

void CAudiofile::clear()
{
	MetaData.clear();
	Filename.clear();
	FileType = CFile::FILE_UNKNOWN;
}
