/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2021, Thilo Graf 'dbt'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include "cc_item_picture.h"


void CComponentsPicture::getRealSize(int *dx, int *dy)
{
	ccp->getRealSize(dx, dy);
}

void CComponentsPicture::setPicture(const std::string &name, const int &w, const int &h)
{
	ccp->setPicture(name, w, h);
	init(x, y, name,  ccp->getBodyBGImageTranparencyMode());
}

void CComponentsPicture::setPicture(const char *name, const int &w, const int &h)
{
	setPicture(name, w, h);
}

std::string CComponentsPicture::getPictureName()
{
	return ccp->getPictureName();
}

void CComponentsPicture::SetTransparent(const int &mode)
{
	init(x, y, ccp->getBodyBGImage(), mode);
}

void CComponentsPicture::setWidth(const int &w, bool keep_aspect)
{
	ccp->setWidth(w, keep_aspect);
	CComponentsForm::setWidth(ccp->getWidth());
	CComponentsForm::setHeight(ccp->getHeight());
}

void CComponentsPicture::setHeight(const int &h, bool keep_aspect)
{
	ccp->setHeight(h, keep_aspect);
	CComponentsForm::setWidth(ccp->getWidth());
	CComponentsForm::setHeight(ccp->getHeight());
}

void CComponentsPicture::paint(const bool &do_save_bg)
{
	CComponentsForm::paint(do_save_bg);
};
