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
#endif /* HAVE_CONFIG_H */

#include <driver/file.h>
#include <cstring>
#include <cstdlib>

struct file_ext_s {
	const char *ext;
	const CFile::FileType type;
};

// ATTENTION: the array MUST BE SORTED ASCENDING (cf. sort, man bsearch) - otherwise bsearch will not work correctly!
static const file_ext_s file_ext[] =
{
	{ "aac",	CFile::FILE_AAC		},
	{ "asf",	CFile::FILE_ASF		},
	{ "avi",	CFile::FILE_AVI		},
	{ "bin",	CFile::FILE_BIN_PACKAGE	},
	{ "bmp",	CFile::FILE_PICTURE	},
	{ "cdr",	CFile::FILE_CDR		},
	{ "crw",	CFile::FILE_PICTURE	},
	{ "dts",	CFile::FILE_WAV		},
	{ "flac",	CFile::FILE_FLAC	},
	{ "flv",	CFile::FILE_MPG		},
	{ "gif",	CFile::FILE_PICTURE	},
	{ "imu",	CFile::STREAM_PICTURE	},
	{ "ipk",	CFile::FILE_PKG_PACKAGE	},
	{ "iso",	CFile::FILE_ISO		},
	{ "jpeg",	CFile::FILE_PICTURE	},
	{ "jpg",	CFile::FILE_PICTURE	},
	{ "m2a",	CFile::FILE_MP3		},
	{ "m3u",	CFile::FILE_PLAYLIST	},
	{ "m3u8",	CFile::FILE_PLAYLIST	},
	{ "m4a",	CFile::FILE_AAC		},
	{ "mkv",	CFile::FILE_MKV		},
	{ "mp2",	CFile::FILE_MP3		},
	{ "mp3",	CFile::FILE_MP3		},
	{ "mp4",	CFile::FILE_MPG		},
	{ "mpa",	CFile::FILE_MP3		},
	{ "mpeg",	CFile::FILE_MPG		},
	{ "mpg",	CFile::FILE_MPG		},
	{ "ogg",	CFile::FILE_OGG		},
	{ "opk",	CFile::FILE_PKG_PACKAGE	},
	{ "pls",	CFile::FILE_PLAYLIST	},
	{ "png",	CFile::FILE_PICTURE	},
	{ "sh", 	CFile::FILE_TEXT	},
	{ "tgz",	CFile::FILE_TGZ_PACKAGE	},
	{ "ts", 	CFile::FILE_TS		},
	{ "txt",	CFile::FILE_TEXT	},
	{ "url",	CFile::STREAM_AUDIO	},
	{ "vob",	CFile::FILE_VOB		},
	{ "wav",	CFile::FILE_WAV		},
	{ "xml",	CFile::FILE_XML		},
	{ "zip",	CFile::FILE_ZIP_PACKAGE	}
};

int mycasecmp(const void * a, const void * b)
{
	return strcasecmp(((file_ext_s *)a)->ext, ((file_ext_s *)b)->ext);
}

CFile::CFile() : Size( 0 ), Mode( 0 ), Marked( false ), Time( 0 )
{
}

CFile::FileType CFile::getType(void) const
{
	if(S_ISDIR(Mode))
		return FILE_DIR;

	std::string::size_type ext_pos = Name.rfind('.');

	if (ext_pos != std::string::npos) {
		const char * key = &(Name.c_str()[ext_pos + 1]);
		void * result = ::bsearch(&key, file_ext, sizeof(file_ext) / sizeof(file_ext_s), sizeof(file_ext_s), mycasecmp);
		if (result)
			return ((file_ext_s *)result)->type;
	}
	return FILE_UNKNOWN;
}

std::string CFile::getFileName(void) const  // return name.extension or folder name without trailing /
{
	std::string::size_type namepos = Name.rfind('/');

	return (namepos == std::string::npos) ? Name : Name.substr(namepos + 1);
}

std::string CFile::getPath(void) const      // return complete path including trailing /
{
	int pos = 0;

	return ((pos = Name.rfind('/')) > 1) ? Name.substr(0, pos + 1) : "/";
}
