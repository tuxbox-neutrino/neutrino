/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2004 Martin Griep 'vivamiga'
	Copyright (C) 2017 Sven Hoefer

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

#ifndef __epgplus__
#define __epgplus__

#include <gui/components/cc.h>
#include "widget/menue.h"

#include <string>

class CFrameBuffer;
class Font;
class CBouquetList;
class ConfigFile;
struct button_label;

class EpgPlus
{
	//// types, inner classes
	public:
		enum SizeSettingID
		{
			EPGPlus_channelentry_width = 0,
			EPGPlus_separationline_thickness,
			NumberOfSizeSettings
		};

		struct SizeSetting
		{
			SizeSettingID	settingID;
			int		size;
		};

		enum TViewMode
		{
			ViewMode_Stretch,
			ViewMode_Scroll
		};

		enum TSwapMode
		{
			SwapMode_ByPage,
			SwapMode_ByBouquet
		};

		class Footer;

		class Header
		{
			//// construction / destruction
			public:
				Header(CFrameBuffer* frameBuffer,
					int x,
					int y,
					int width);

				~Header();

			//// methods
			public:
				static void init();

				void paint(const char * Name = NULL);

				static int getUsedHeight();

			//// attributes
			public:
				CFrameBuffer* frameBuffer;

				int x;
				int y;
				int width;

				static Font* font;
		};


		class TimeLine
		{
			//// construction / destruction
			public:
				TimeLine(CFrameBuffer* frameBuffer,
					int x,
					int y,
					int width,
					int startX,
					int durationX);

				~TimeLine();

			//// methods
			public:
				static void init();

				void paint(time_t startTime, int duration);

				void paintMark(time_t startTime, int duration, int x, int width);

				void paintGrid();

				void clearMark();

				static int getUsedHeight();

			//// attributes
			public:
				CFrameBuffer* frameBuffer;

				int currentDuration;

				int x;
				int y;
				int width;
				static int separationLineThickness;

				static Font* font;

				int startX;
				int durationX;
		};

		class ChannelEventEntry
		{
			//// construction / destruction
			public:
				ChannelEventEntry(const CChannelEvent* channelEvent,
					CFrameBuffer* frameBuffer,
					TimeLine* timeLine,
					Footer* footer,
					int x,
					int y,
					int width);

				~ChannelEventEntry();

			//// methods
			public:
				static void init();

				bool isSelected(time_t selectedTime) const;

				void paint(bool isSelected, bool toggleColor);

				static int getUsedHeight();

			//// attributes
			public:
				CChannelEvent channelEvent;

				CFrameBuffer* frameBuffer;
				TimeLine* timeLine;
				Footer* footer;

				int x;
				int y;
				int width;
				static int separationLineThickness;

				static Font* font;
		};

		typedef std::vector<ChannelEventEntry*> TCChannelEventEntries;

		class ChannelEntry
		{
			//// construction / destruction
			public:
				ChannelEntry(const CZapitChannel* channel,
					int index,
					CFrameBuffer* frameBuffer,
					Footer* footer,
					CBouquetList* bouquetList,
					int x,
					int y,
					int width);

				~ChannelEntry();

			//// methods
			public:
				static void init();

				void paint(bool isSelected, time_t selectedTime);

				static int getUsedHeight();

			//// attributes
			public:
				const CZapitChannel * channel;
				std::string displayName;
				int index;

				CFrameBuffer* frameBuffer;
				Footer* footer;
				CBouquetList* bouquetList;

				int x;
				int y;
				int width;
				static int separationLineThickness;

				static Font* font;

				TCChannelEventEntries channelEventEntries;
				CComponentsDetailsLine *detailsLine;
		};

		typedef std::vector<ChannelEntry*> TChannelEntries;

		class Footer
		{
			//// construction / destruction
			public:
				Footer(CFrameBuffer* frameBuffer,
					int x,
					int y,
					int width,
					int height);

				~Footer();

			//// methods
			public:
				static void init();

				void setBouquetChannelName(const std::string& newBouquetName, const std::string& newChannelName);

				void paintEventDetails(const std::string& description, const std::string& shortDescription);

				void paintButtons(button_label* buttonLabels, int numberOfButtons);

				static int getUsedHeight();

			//// attributes
			public:
				CFrameBuffer* frameBuffer;

				int x;
				int y;
				int width;

				int buttonY;
				int buttonHeight;

				static Font* fontBouquetChannelName;
				static Font* fontEventDescription;
				static Font* fontEventInfo1;

				std::string currentBouquetName;
				std::string currentChannelName;
		};

		class MenuTargetAddReminder : public CMenuTarget
		{
			public:
				MenuTargetAddReminder(EpgPlus* epgPlus);

			public:
				int exec(CMenuTarget* parent, const std::string& actionKey);

			private:
				EpgPlus* epgPlus;

		};

		class MenuTargetAddRecordTimer : public CMenuTarget
		{
			public:
				MenuTargetAddRecordTimer(EpgPlus* epgPlus);

			public:
				int exec(CMenuTarget* parent, const std::string& actionKey);

			private:
				EpgPlus* epgPlus;

		};

		class MenuTargetRefreshEpg : public CMenuTarget
		{
			public:
				MenuTargetRefreshEpg(EpgPlus* epgPlus);

			public:
				int exec(CMenuTarget* parent, const std::string& actionKey);

			private:
				EpgPlus* epgPlus;

		};

		class MenuOptionChooserSwitchSwapMode : public CMenuOptionChooser
		{
			public:
				MenuOptionChooserSwitchSwapMode(EpgPlus* epgPlus);

				virtual ~MenuOptionChooserSwitchSwapMode();

			public:
				int exec(CMenuTarget* parent);

			private:
				EpgPlus* epgPlus;
				int oldTimingMenuSettings;
				TSwapMode oldSwapMode;
		};

		class MenuOptionChooserSwitchViewMode : public CMenuOptionChooser
		{
			public:
				MenuOptionChooserSwitchViewMode(EpgPlus* epgPlus);

				virtual ~MenuOptionChooserSwitchViewMode();

			public:
				int exec(CMenuTarget* parent);

			private:
				int oldTimingMenuSettings;
		};

		class MenuTargetSettings : public CMenuTarget
		{
			public:
				MenuTargetSettings(EpgPlus* epgPlus);

			public:
				int exec(CMenuTarget* parent, const std::string& actionKey);

			private:
				EpgPlus* epgPlus;
		};

		typedef time_t DurationSetting;

		/*
		struct Settings
		{
			Settings(bool doInit = true);

			virtual ~Settings();

			FontSetting*	fontSettings;
			SizeSetting*	sizeSettings;
			DurationSetting	durationSetting;
		};
		typedef std::map<int, Font*> Fonts;
		typedef std::map<int, int> Sizes;
		static Font * fonts[NumberOfFontSettings];
		static int sizes[NumberOfSizeSettings];
		*/

		friend class EpgPlus::ChannelEventEntry;
		friend class EpgPlus::ChannelEntry;
		friend class EpgPlus::MenuOptionChooserSwitchSwapMode;
		friend class EpgPlus::MenuOptionChooserSwitchViewMode;

	//// construction / destruction
	public:
		EpgPlus();
		~EpgPlus();

	//// methods
	public:
		void init();
		void free();

		int exec(CChannelList* channelList, int selectedChannelIndex, CBouquetList* bouquetList);

	private:
		static std::string getTimeString(const time_t& time, const std::string& format);

		TCChannelEventEntries::const_iterator getSelectedEvent() const;

		void createChannelEntries(int selectedChannelEntryIndex);
		void paint();
		void paintChannelEntry(int position);
		void hide();

	//// properties
	private:
		CFrameBuffer*	frameBuffer;

		TChannelEntries	displayedChannelEntries;

		Header*		header;
		TimeLine*	timeLine;

		CChannelList*	channelList;
		CBouquetList*	bouquetList;

		Footer*		footer;

		ChannelEntry*	selectedChannelEntry;
		time_t		selectedTime;

		int		channelListStartIndex;
		int		maxNumberOfDisplayableEntries; // maximal number of displayable entrys

		time_t		startTime;
		time_t		firstStartTime;
		static time_t	duration;

		int		entryHeight;
		static int	entryFontSize;

		TViewMode	currentViewMode;
		TSwapMode	currentSwapMode;

		int		headerX;
		int		headerY;
		int		headerWidth;

		int		usableScreenWidth;
		int		usableScreenHeight;
		int		usableScreenX;
		int		usableScreenY;

		int		timeLineX;
		int		timeLineY;
		int		timeLineWidth;

		int		bodyHeight;

		int		channelsTableX;
		int		channelsTableY;
		static int	channelsTableWidth;
		int		channelsTableHeight;

		int		eventsTableX;
		int		eventsTableY;
		int		eventsTableWidth;
		int		eventsTableHeight;

		int		sliderX;
		int		sliderY;
		static int	sliderWidth;
		int		sliderHeight;

		int		footerX;
		int		footerY;
		int		footerWidth;

		bool		refreshAll;
		bool		refreshFooterButtons;
};

class CEPGplusHandler : public CMenuTarget
{
	public:
		int exec(CMenuTarget* parent, const std::string &actionKey);
};

#endif // __epgplus__
