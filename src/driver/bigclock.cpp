/*
	LCD-Daemon  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/



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


#include "bigclock.h"


void doppelpunkt(CLCDDisplay* display, int pos) 
{
	int i,j;

	for (i=1;i<=4;i++) 
		for (j=1;j<=4;j++) 
			display->draw_point(i+10+pos*24-3,j+23,CLCDDisplay::PIXEL_ON);
	for (i=1;i<=4;i++) 
		for (j=1;j<=4;j++) 
			display->draw_point(i+10+pos*24-3,j+37,CLCDDisplay::PIXEL_ON);
}


void balken(CLCDDisplay* display, int pos, int balken)
{
	int i,j;
	
	if (balken==3||balken==6||balken==7)
	{
		//horizontaler balken
		if (balken == 6) for (i=1;i<=14;i++) for (j=1;j<=4;j++) display->draw_point(i+5+pos*24-3,j+14,CLCDDisplay::PIXEL_ON);
		if (balken == 7) for (i=1;i<=8;i++) for (j=1;j<=4;j++) display->draw_point(i+8+pos*24-3,j+30,CLCDDisplay::PIXEL_ON);
	}	if (balken == 3) for (i=1;i<=14;i++) for (j=1;j<=4;j++) display->draw_point(i+5+pos*24-3,j+46,CLCDDisplay::PIXEL_ON);
	else
	{
		//vertikaler balken
		if (balken == 1) for (i=1;i<=4;i++) for (j=1;j<=13;j++) display->draw_point(i+19+pos*24-3,j+18,CLCDDisplay::PIXEL_ON);
		if (balken == 2) for (i=1;i<=4;i++) for (j=1;j<=13;j++) display->draw_point(i+19+pos*24-3,j+33,CLCDDisplay::PIXEL_ON);
		if (balken == 5) for (i=1;i<=4;i++) for (j=1;j<=13;j++) display->draw_point(i+1+pos*24-3,j+18,CLCDDisplay::PIXEL_ON);
		if (balken == 4) for (i=1;i<=4;i++) for (j=1;j<=13;j++) display->draw_point(i+1+pos*24-3,j+33,CLCDDisplay::PIXEL_ON);

	}
}

void showBigClock(CLCDDisplay* display, int h, int m)
{

	//stunden einer
		if (h==0||h==10||h==20) 
		{ balken(display, 1,1); balken(display, 1,2); balken(display, 1,3); balken(display, 1,4); balken(display, 1,5); balken(display, 1,6);}
		if (h==1||h==11||h==21) 
		{ balken(display, 1,1); balken(display, 1,2);}
		if (h==2||h==12||h==22) 
		{ balken(display, 1,1); balken(display, 1,7); balken(display, 1,3); balken(display, 1,4); balken(display, 1,6);}
		if (h==3||h==13||h==23) 
		{ balken(display, 1,1); balken(display, 1,2); balken(display, 1,3); balken(display, 1,7); balken(display, 1,6);}
		if (h==4||h==14) 
		{ balken(display, 1,1); balken(display, 1,2); balken(display, 1,7); balken(display, 1,5);}
		if (h==5||h==15) 
		{ balken(display, 1,2); balken(display, 1,3); balken(display, 1,5); balken(display, 1,6); balken(display, 1,7);}
		if (h==6||h==16) 
		{ balken(display, 1,7); balken(display, 1,2); balken(display, 1,3); balken(display, 1,4); balken(display, 1,5); balken(display, 1,6);}
		if (h==7||h==17) 
		{ balken(display, 1,1); balken(display, 1,2); balken(display, 1,6);}
		if (h==8||h==18) 
		{ balken(display, 1,1); balken(display, 1,2); balken(display, 1,3); balken(display, 1,4); balken(display, 1,5); balken(display, 1,6); balken(display, 1,7);}
		if (h==9||h==19) 
		{ balken(display, 1,1); balken(display, 1,2); balken(display, 1,3); balken(display, 1,7); balken(display, 1,5); balken(display, 1,6);}
	//stunden zehner
		if (h>9&&h<20){ balken(display, 0,1); balken(display, 0,2);}
		if (h>19) 	{ balken(display, 0,1); balken(display, 0,7); balken(display, 0,3); balken(display, 0,4); balken(display, 0,6);}


	//minuten einer 
		if (m==0||m==10||m==20||m==30||m==40||m==50) 
		{ balken(display, 4,1); balken(display, 4,2); balken(display, 4,3); balken(display, 4,4); balken(display, 4,5); balken(display, 4,6);}
		if (m==1||m==11||m==21||m==31||m==41||m==51) 
		{ balken(display, 4,1); balken(display, 4,2);}
		if (m==2||m==12||m==22||m==32||m==42||m==52) 
		{ balken(display, 4,1); balken(display, 4,7); balken(display, 4,3); balken(display, 4,4); balken(display, 4,6);}
		if (m==3||m==13||m==23||m==33||m==43||m==53) 
		{ balken(display, 4,1); balken(display, 4,2); balken(display, 4,3); balken(display, 4,7); balken(display, 4,6);}
		if (m==4||m==14||m==24||m==34||m==44||m==54) 
		{ balken(display, 4,1); balken(display, 4,2); balken(display, 4,7); balken(display, 4,5);}
		if (m==5||m==15||m==25||m==35||m==45||m==55) 
		{ balken(display, 4,2); balken(display, 4,3); balken(display, 4,5); balken(display, 4,6); balken(display, 4,7);}
		if (m==6||m==16||m==26||m==36||m==46||m==56) 
		{ balken(display, 4,7); balken(display, 4,2); balken(display, 4,3); balken(display, 4,4); balken(display, 4,5); balken(display, 4,6);}
		if (m==7||m==17||m==27||m==37||m==47||m==57) 
		{ balken(display, 4,1); balken(display, 4,2); balken(display, 4,6);}
		if (m==8||m==18||m==28||m==38||m==48||m==58) 
		{ balken(display, 4,1); balken(display, 4,2); balken(display, 4,3); balken(display, 4,4); balken(display, 4,5); balken(display, 4,6); balken(display, 4,7);}
		if (m==9||m==19||m==29||m==39||m==49||m==59) 
		{ balken(display, 4,1); balken(display, 4,2); balken(display, 4,3); balken(display, 4,7); balken(display, 4,5); balken(display, 4,6);}

		//minuten zehner
		if (m<10)
		{ balken(display, 3,1); balken(display, 3,2); balken(display, 3,3); balken(display, 3,4); balken(display, 3,5); balken(display, 3,6);}
		if (m>9&&m<20){ balken(display, 3,1); balken(display, 3,2);}
		if (m>19&&m<30){ balken(display, 3,1); balken(display, 3,7); balken(display, 3,3); balken(display, 3,4); balken(display, 3,6);}
		if (m>29&&m<40){ balken(display, 3,1); balken(display, 3,2); balken(display, 3,3); balken(display, 3,7); balken(display, 3,6);}
		if (m>39&&m<50){ balken(display, 3,1); balken(display, 3,2); balken(display, 3,7); balken(display, 3,5);}
		if (m>=50)	 { balken(display, 3,2); balken(display, 3,3); balken(display, 3,5); balken(display, 3,6); balken(display, 3,7);}

		doppelpunkt(display, 2);
 }
