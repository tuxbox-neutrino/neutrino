/*
 * $Id: transponder.cpp,v 1.6 2002/10/12 23:14:20 obi Exp $
 *
 * (C) 2002 by Steffen Hehn "McClean" <McClean@tuxbox.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include <zapit/transponder.h>


CTransponder::CTransponder()
{
	frequency = 0;
	modulation = 0;
	symbolrate = 0;
	polarisation = 0;
	innerFec = 0;
	diseqc = 0;
	inversion = 0;
	originalNetworkId = 0;
	transportStreamId = 0;
	updated = 0;
}

unsigned int CTransponder::getFrequency()
{
	return frequency;
}

void CTransponder::setFrequency(unsigned int ifrequency)
{
	frequency = ifrequency;
}

unsigned char CTransponder::getModulation()
{
	return modulation;
}

void CTransponder::setModulation(unsigned char cmodulation)
{
	modulation = cmodulation;
}

unsigned int CTransponder::getSymbolrate()
{
	return symbolrate;
}

void CTransponder::setSymbolrate(unsigned int isymbolrate)
{
	symbolrate = isymbolrate;
}

unsigned char CTransponder::getPolarisation()
{
	return polarisation;
}

void CTransponder::setPolarisation(unsigned char cpolarisation)
{
	polarisation = cpolarisation;
}

unsigned char CTransponder::getDiseqc()
{
	return diseqc;
}

void CTransponder::setDiseqc(unsigned char cdiseqc)
{
	diseqc = cdiseqc;
}

unsigned char CTransponder::getInnerFec()
{
	return innerFec;
}

void CTransponder::setInnerFec(unsigned char cinnerFec)
{
	innerFec = cinnerFec;
}

unsigned char CTransponder::getInversion()
{
	return inversion;
}

void CTransponder::setInversion(unsigned char cinversion)
{
	inversion = cinversion;
}

unsigned short CTransponder::getOriginalNetworkId()
{
	return originalNetworkId;
}

void CTransponder::setOriginalNetworkId(unsigned short soriginalNetworkId)
{
	originalNetworkId = soriginalNetworkId;
}

unsigned short CTransponder::getTransportStreamId()
{
	return transportStreamId;
}

void CTransponder::setTransportStreamId(unsigned short stransportStreamId)
{
	transportStreamId = stransportStreamId;
}

unsigned int CTransponder::getTsidOnid()
{
	return (transportStreamId << 16) | originalNetworkId;
}

