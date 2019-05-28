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

#ifndef __FILE_H__
#define __FILE_H__

#include <config.h>
#if 0
/* this is already done by AC_SYS_LARGEFILE */
#include <features.h> /* make sure off_t has size 8
						 in __USE_FILE_OFFSET64 mode */

#ifndef __USE_FILE_OFFSET64
#error not using 64 bit file offsets
#endif /* __USE_FILE__OFFSET64 */
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <string>
#include <vector>

class CFile
{
	public:
		enum FileType
		{
			FILE_UNKNOWN = 0,
			FILE_AAC,
			FILE_ASF,
			FILE_AVI,
			FILE_BIN_PACKAGE,
			FILE_CDR,
			FILE_DIR,
			FILE_FLAC,
			FILE_FLV,
			FILE_ISO,
			FILE_MKV,
			FILE_MP3,
			FILE_MPG,
			FILE_OGG,
			FILE_PICTURE,
			FILE_PKG_PACKAGE,
			FILE_PLAYLIST,
			FILE_TEXT,
			FILE_TGZ_PACKAGE,
			FILE_TS,
			FILE_VOB,
			FILE_WAV,
			FILE_XML,
			FILE_ZIP_PACKAGE,
			STREAM_AUDIO,
			STREAM_PICTURE
		};

		FileType	getType(void) const;
		std::string	getFileName(void) const;
		std::string	getPath(void) const;
		bool		isDir(void) { return S_ISDIR(Mode); };

		CFile();
		off_t Size;
		std::string Name;
		std::string Url;
		mode_t Mode;
		bool Marked;
		time_t Time;
};

typedef std::vector<CFile> CFileList;

#endif /* __FILE_H__ */
