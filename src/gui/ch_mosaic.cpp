
/*
	Neutrino-GUI  -   DBoxII-Project


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

#include <unistd.h>
#include <config.h>
#include <global.h>
#include <neutrino.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <gui/color.h>
#include <gui/eventlist.h>
#include <gui/channellist.h>

#include "ch_mosaic.h"

#warning "experimental..."


/*
   -- Channel/Service Mosaic
   -- Display multiple channel images on screen
   -- capture used from outdoor (tmbinc)
   -- 2002-11   rasc
 */




//
//  -- init Channel Mosaic Handler Class
//  -- to be used for calls from Menue
// 

int CChMosaicHandler::exec(CMenuTarget* parent, const std::string &actionkey)
{
	int       res = menu_return::RETURN_EXIT_ALL;
	CChMosaic * mosaic;


	if (parent) {
		parent->hide();
	}

	mosaic = new CChMosaic;
	mosaic->doMosaic ();
	delete mosaic;

	return res;
}




#define SCREEN_X	720
#define SCREEN_Y	572
#define RATIO(y)	(((y)*100)/126)		// w/h PAL ratio


//
//  -- Channel Mosaic Class
//  -- do Mosaic
// 

CChMosaic::CChMosaic()
{
	pig = new CPIG (0);
	capture = new CCAPTURE (0);
	current_pig_pos = 0;

  	channellist = CNeutrinoApp::getInstance()->channelList;
  	frameBuffer = CFrameBuffer::getInstance();
}


CChMosaic::~CChMosaic()
{
	delete pig;

}


void CChMosaic::doMosaic()
{
#define W 	(150)
#define H	(120)
  struct PIG_COORD  coord[] = {
	  	{ 40, 10, W,H },
		{200, 10, W,H },
		{360, 10, W,H },
		{520, 10, W,H },
	  	{ 40,150, W,H },
		{200,150, W,H },
		{360,150, W,H },
		{520,150, W,H },
	  	{ 40,290, W,H },
		{200,290, W,H },
		{360,290, W,H },
		{520,290, W,H },
	  	{ 40,430, W,H },
		{200,430, W,H },
		{360,430, W,H },
		{520,430, W,H }
	};

  int    	channel;
  int 		i;



  channellist = CNeutrinoApp::getInstance()->channelList;
  channel     = channellist->getActiveChannelNumber();

  frameBuffer = CFrameBuffer::getInstance();




  //  $$$ mute


  // -- paint background and - windows
  paintBackground();
  for  (i=0; i < (int)(sizeof(coord)/sizeof(coord[0])); i++) {
	paintMiniTVBackground(coord[i].x,coord[i].y, coord[i].w, coord[i].h);
  }




   // experimental
  for  (i=0; i < (int)(sizeof(coord)/sizeof(coord[0])); i++) {

	u_char frame_buf[SCREEN_X*SCREEN_Y*2];
	int j;


	// -- adjust pig and zap to channel, set capture size
	printf ("pig: %d \n",i);
	pig->show (coord[i].x,coord[i].y, coord[i].w, coord[i].h);
	channellist->zapTo(channel);
	capture->set_coord (coord[i].x, coord[i].y, coord[i].w, coord[i].h);
//	capture->set_output_size (coord[i].w, coord[i].h);

	sleep (2);


	// -- inner loop
	// -- try 4 times (4 * 0,5 sec) to get a captured frame 
	for (j=0; j < 4; j++) {

		neutrino_msg_t      msg;
		neutrino_msg_data_t data;

		unsigned long long timeoutEnd = CRCInput::calcTimeoutEnd(500);
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );
		printf ("pig inner loop: %d - %d \n",i,j);


			// $$$ TEST
			capture->readframe (frame_buf);
			//if (frame_buf[10] == 0x00)  continue;

			int a;
			for (a=0; a<0x10; a++) {
				printf("%02x ",frame_buf[a]);
			}
			printf ("\n");

		if (msg == CRCInput::RC_timeout) {
			printf ("pig inner loop timeout: \n");
			i = i;
		}


		// -- push other events
		if ( msg >  CRCInput::RC_MaxRC ) {
			CNeutrinoApp::getInstance()->handleMsg( msg, data ); 
		}
		


		// zap, sleep 0.5 sec
		// capture frame
	
		// loop 4 times frame "empty"?
		// --> wait 0,5 sec, re-capture

	}
	
	// display frame

	// display add info (sendername, epg info)
	//


	channel ++;
  }




  //  -- clear
  clearTV();

  //  $$$ unmute

}





//
// -- some paint abstraction functions in this class
// 

void CChMosaic::clearTV()
{
  frameBuffer->paintBackgroundBoxRel(0,0,SCREEN_X-1,SCREEN_Y-1);
}

void CChMosaic::paintBackground()
{
  fb_pixel_t col = 254;
  frameBuffer->paintBoxRel(0, 0, SCREEN_X-1, SCREEN_Y-1, col);
  //frameBuffer->paintBoxRel(0, 0, SCREEN_X-1, SCREEN_Y-1, frameBuffer->realcolor[0xFe]);
}

void CChMosaic::paintMiniTVBackground(int x, int y, int w, int h)
{
  fb_pixel_t col = 240;
  frameBuffer->paintBoxRel(x,y,w,h, col);
  //frameBuffer->paintBoxRel(x,y,w,h, frameBuffer->realcolor[0xF0]);
}


