/*
	lcd4l - Neutrino-GUI

	Copyright (C) 2012 'defans'
	Homepage: http://www.bluepeercrew.us/

	Copyright (C) 2012-2016 'vanhofen'
	Homepage: http://www.neutrino-images.de/

	Modded    (C) 2016 'TangoCash'

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

#ifndef __lcd4l__
#define __lcd4l__

#include <string>

class CLCD4l
{
	public:
		CLCD4l();
		~CLCD4l();

		// Functions
		void	InitLCD4l();
		void	StartLCD4l();
		void	StopLCD4l();
		void	SwitchLCD4l();

		int	CreateFile(const char *file, std::string content = "", bool convert = false);
		int	RemoveFile(const char *file);

		void	ResetParseID() { m_ParseID = 0; }

	private:
		pthread_t	thrLCD4l;
		static void*	LCD4lProc(void *arg);

		struct tm	*tm_struct;

		// Functions
		void		Init();
		void		ParseInfo(uint64_t parseID, bool newID, bool firstRun = false);

		uint64_t	GetParseID();
		bool		CompareParseID(uint64_t &i_ParseID);

		std::string	hexStr(unsigned char data);
		std::string	hexStrA2A(unsigned char data);
		void		strReplace(std::string &orig, const std::string &fstr, const std::string &rstr);
		bool		WriteFile(const char *file, std::string content = "", bool convert = false);

		// Variables
		uint64_t	m_ParseID;
		int		m_Mode;
		int		m_ModeChannel;

		int		m_Brightness;
		int		m_Brightness_standby;
		std::string	m_Resolution;
		std::string	m_AspectRatio;
		int		m_Videotext;
		int		m_Radiotext;
		std::string	m_DolbyDigital;
		int		m_Tuner;
		int		m_Volume;
		int		m_ModeRec;
		int		m_ModeTshift;
		int		m_ModeTimer;
		int		m_ModeEcm;

		std::string	m_Service;
		int		m_ChannelNr;
		std::string	m_Logo;
		int		m_ModeLogo;

		std::string	m_Layout;

		std::string	m_Event;
		std::string	m_Info1;
		std::string	m_Info2;
		int		m_Progress;
		char		m_Duration[15];
		std::string	m_Start;
		std::string	m_End;

		std::string	m_font;
		std::string	m_fgcolor;
		std::string	m_bgcolor;
		std::string	m_fcolor1;
		std::string	m_fcolor2;
		std::string	m_pbcolor;
};

#endif
