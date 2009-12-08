/*
 * $Id: ci.h,v 1.6 2003/01/30 17:21:16 obi Exp $
 *
 * (C) 2002 by Andreas Oberritter <obi@tuxbox.org>
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

#ifndef __ci_h__
#define __ci_h__

#include <vector>

class CCaDescriptor
{
	private:
		unsigned descriptor_tag		: 8;
		unsigned descriptor_length	: 8;
		unsigned CA_system_ID		: 16;
		unsigned reserved1		: 3;
		unsigned CA_PID			: 13;
		std::vector <unsigned char> private_data_byte;

	public:
		CCaDescriptor(const unsigned char * const buffer);
		unsigned writeToBuffer(unsigned char * const buffer);
		unsigned getLength(void)	{ return descriptor_length + 2; }
};

/*
 * children of this class need to delete all
 * CCaDescriptors in their destructors
 */
class CCaTable
{
	private:
		std::vector <CCaDescriptor *> ca_descriptor;

	protected:
		CCaTable(void)			{ info_length = 0; };
		~CCaTable(void);
		unsigned getLength(void)	{ return info_length + 2; }
		unsigned writeToBuffer(unsigned char * const buffer);

	public:
		unsigned reserved2		: 4;
		unsigned info_length		: 12;
		void addCaDescriptor(const unsigned char * const buffer);
};

class CEsInfo : public CCaTable
{
	protected:
		unsigned getLength(void)	{ return CCaTable::getLength() + 3; }
		unsigned writeToBuffer(unsigned char * const buffer);

	public:
		unsigned stream_type		: 8;
		unsigned reserved1		: 3;
		unsigned elementary_PID		: 13;

	friend class CCaPmt;
};

class CCaPmt : public CCaTable
{
	protected:
		unsigned ca_pmt_length;

	public:
		~CCaPmt(void);
		unsigned getLength(void);
		unsigned writeToBuffer(unsigned char * const buffer, int demux = 1, int camask = 3);

		unsigned ca_pmt_list_management	: 8;
		unsigned program_number		: 16;
		unsigned reserved1		: 2;
		unsigned version_number		: 5;
		unsigned current_next_indicator	: 1;

		std::vector<CEsInfo *> es_info;
};

#endif /* __ci_h__ */
