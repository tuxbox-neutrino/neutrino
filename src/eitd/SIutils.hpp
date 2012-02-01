#ifndef SIUTILS_HPP
#define SIUTILS_HPP
//
// $Id: SIutils.hpp,v 1.5 2006/05/19 21:28:08 houdini Exp $
//
// utility functions for the SI-classes (dbox-II-project)
//
//    Homepage: http://dbox2.elxsi.de
//
//    Copyright (C) 2001 fnbrd (fnbrd@gmx.de)
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
// $Log: SIutils.hpp,v $
// Revision 1.5  2006/05/19 21:28:08  houdini
// - Nirvanas save/restore EPG patch #g
// - automatic update of subchannels for Premiere (disable with <sectionsd -nu>)
// - Fix for ZDF audio option "mono/Hörfilm"
// - improved navigation speed in bouquet/channel list
// - zapit/pzapit new option (-sbo) save bouquets.xml including Bouquet "Andere" which saves me a lot of time :-)
//
// Revision 1.4  2001/07/14 16:38:46  fnbrd
// Mit workaround fuer defektes mktime der glibc
//
// Revision 1.3  2001/06/10 14:55:51  fnbrd
// Kleiner Aenderungen und Ergaenzungen (epgMini).
//
// Revision 1.2  2001/05/19 22:46:50  fnbrd
// Jetzt wellformed xml.
//
// Revision 1.1  2001/05/16 15:23:47  fnbrd
// Alles neu macht der Mai.
//
//
time_t changeUTCtoCtime(const unsigned char *buffer, int local_time=1);

// returns the descriptor type as readable text
const char *decode_descr (unsigned char tag_value);

int saveStringToXMLfile(FILE *out, const char *string, int withControlCodes=0);

// Entfernt die ControlCodes aus dem String (-> String wird evtl. kuerzer)
void removeControlCodes(char *string);

#endif // SIUTILS_HPP
