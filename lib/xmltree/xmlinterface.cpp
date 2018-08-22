/*
 * $Header: /cvs/tuxbox/apps/misc/libs/libxmltree/xmlinterface.cpp,v 1.3 2009/02/18 17:51:55 seife Exp $
 *
 * xmlinterface for zapit - d-box2 linux project
 *
 * (C) 2002 by thegoodguy <thegoodguy@berlios.de>
 * (C) 2009, 2018 Stefan Seyfried
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


 * those files (xmlinterface.cpp and xmlinterface.h) lived at three different places
   in the tuxbox-cvs before, so look there for history information:
   - apps/dvb/zapit/include/zapit/xmlinterface.h
   - apps/dvb/zapit/src/xmlinterface.cpp
   - apps/tuxbox/neutrino/daemons/sectionsd/xmlinterface.cpp
   - apps/tuxbox/neutrino/src/system/xmlinterface.cpp
   - apps/tuxbox/neutrino/src/system/xmlinterface.h
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "xmlinterface.h"

#ifdef USE_LIBXML
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#else  /* USE_LIBXML */
#include "gzstream.h"
#include "xmltok.h"
#endif /* USE_LIBXML */
#include <fcntl.h>
#include <stdio.h>
#include <zlib.h>

unsigned long xmlGetNumericAttribute(const xmlNodePtr node, const char *name, const int base)
{
	const char *ptr = xmlGetAttribute(node, name);

	if (!ptr)
		return 0;

	return strtoul(ptr, 0, base);
}

long xmlGetSignedNumericAttribute(const xmlNodePtr node, const char *name, const int base)
{
	const char *ptr = xmlGetAttribute(node, name);

	if (!ptr)
		return 0;

	return strtol(ptr, 0, base);
}

xmlNodePtr xmlGetNextOccurence(xmlNodePtr cur, const char * s)
{
	while ((cur != NULL) && (strcmp(xmlGetName(cur), s) != 0))
		cur = xmlNextNode(cur);
	return cur;
}
#if USE_PUGIXML
std::string to_utf8(unsigned int cp)
{
	static const int mask[4] = { 0, 0xc0, 0xe0, 0xf0 };
	std::string result;
	int count;
	if (cp < 0x0080)
		count = 1;
	else if (cp < 0x0800)
		count = 2;
	else if (cp < 0x10000)
		count = 3;
	else if (cp <= 0x10FFFF)
		count = 4;
	else
		return result;
	result.resize(count);
	for (int i = count-1; i > 0; --i)
	{
		result[i] = (char) (0x80 | (cp & 0x3F));
		cp >>= 6;
	}
	cp |= mask[count-1];

	result[0] = (char) cp;
	return result;
}
#endif
std::string Unicode_Character_to_UTF8(const int character)
{
#ifdef USE_LIBXML
	xmlChar buf[5];
	int length = xmlCopyChar(4, buf, character);
	return std::string((char*)buf, length);
#elif  (defined( USE_PUGIXML ) )
	return to_utf8(character);
#else  /* USE_LIBXML */
	char buf[XML_UTF8_ENCODE_MAX];
	int length = XmlUtf8Encode(character, buf);
	return std::string(buf, length);
#endif /* USE_LIBXML */
}

std::string convert_UTF8_To_UTF8_XML(const char* s)
{
	std::string r;

	while ((*s) != 0)
	{
		/* cf.
		 * http://www.w3.org/TR/2004/REC-xml-20040204/#syntax
		 * and
		 * http://www.w3.org/TR/2004/REC-xml-20040204/#sec-predefined-ent
		 */
		switch (*s)
		{
		case '<':
			r += "&lt;";
			break;
		case '>':
			r += "&gt;";
			break;
		case '&':
			r += "&amp;";
			break;
		case '\"':
			r += "&quot;";
			break;
		case '\'':
			r += "&apos;";
			break;
		default:
			if ((unsigned char)*s >= 32)
				r += *s;
		}
		s++;
	}
	return r;
}

#ifdef USE_LIBXML
xmlDocPtr parseXml(const char * data,char *)
{
	xmlDocPtr doc;
	xmlNodePtr cur;

	doc = xmlParseMemory(data, strlen(data));

	if (doc == NULL)
	{
		WARN("Error parsing XML Data");
		return NULL;
	}
	else
	{
		cur = xmlDocGetRootElement(doc);
		if (cur == NULL)
		{
			WARN("Empty document\n");
			xmlFreeDoc(doc);
			return NULL;
		}
		else
			return doc;
	}
}

xmlDocPtr parseXmlFile(const char * filename, bool warning_by_nonexistence /* = true */,char *)
{
	xmlDocPtr doc;
	xmlNodePtr cur;

	doc = xmlParseFile(filename);

	if (doc == NULL)
	{
		fprintf(stderr, "%s: Error parsing \"%s\"", __FUNCTION__, filename);
		return NULL;
	}
	else
	{
		cur = xmlDocGetRootElement(doc);
		if (cur == NULL)
		{
			fprintf(stderr, "%s: Empty document\n", __FUNCTION__);
			xmlFreeDoc(doc);
			return NULL;
		}
		else
			return doc;
	}
}

#elif  (defined( USE_PUGIXML ) )

#include <fstream>

xmlDocPtr parseXml(const char * data,const char* /*encoding*/)
{
	pugi::xml_document* tree_parser = new pugi::xml_document();
	if (!tree_parser->load_string(data))
	{
		delete tree_parser;
		return NULL;
	}

	if (!tree_parser->root())
	{
		printf("Error: No Root Node\n");
		delete tree_parser;
		return NULL;
	}
	return tree_parser;
}

xmlDocPtr parseXmlFile(const char * filename, bool,const char* encoding)
{
	pugi::xml_encoding enc = pugi::encoding_auto;
	std::string fn = filename;
	igzstream inz;
	std::ifstream in;
	bool zipped = (fn.substr(fn.find_last_of(".") + 1) == "gz");

	if(encoding==NULL)
	{
		if (zipped)
		{
			inz.open(filename);
			if (inz.is_open())
			{
				std::string line;
				getline(inz, line);
				for (std::string::iterator it = line.begin(); it != line.end(); ++ it)
					*it = toupper(*it);
				if (line.find("ISO-8859-1",0)!= std::string::npos)
				{
					enc = pugi::encoding_latin1;
				}
				inz.close();
			}
		}
		else
		{
			in.open(filename);
			if (in.is_open())
			{
				std::string line;
				getline(in, line);
				for (std::string::iterator it = line.begin(); it != line.end(); ++ it)
					*it = toupper(*it);
				if (line.find("ISO-8859-1",0)!= std::string::npos)
				{
					enc = pugi::encoding_latin1;
				}
				in.close();
			}
		}
	}
	pugi::xml_document* tree_parser = new pugi::xml_document();


	if (zipped)
	{
        int fd = open(filename, O_RDONLY);

		uint32_t gzsize = 0;
		lseek(fd, -4, SEEK_END);
		read(fd, &gzsize, 4);
		lseek(fd, 0, SEEK_SET);

		gzFile xmlgz_file = gzdopen(fd,"rb");

		if (xmlgz_file == NULL)
		{
			delete tree_parser;
			return NULL;
		}

		gzbuffer(xmlgz_file, 64*1024);

		void* buffer = pugi::get_memory_allocation_function()(gzsize);

		if (!buffer)
			{
				gzclose(xmlgz_file);
				delete tree_parser;
				return NULL;
			}

		size_t read_size = gzread(xmlgz_file,buffer,gzsize);

		if (read_size != gzsize)
		{
			gzclose(xmlgz_file);
			delete tree_parser;
			return NULL;
		}

		gzclose(xmlgz_file);

		const pugi::xml_parse_result result = tree_parser->load_buffer_inplace_own(buffer,gzsize, pugi::parse_default, enc);
		if (result.status != pugi::xml_parse_status::status_ok)
			{
				printf("Error: Loading %s (%d)\n", filename, result.status);
				delete tree_parser;
				return NULL;
			}
	}
	else
	{
		if (!tree_parser->load_file(filename, pugi::parse_default, enc))
		{
			delete tree_parser;
			return NULL;
		}
	}

	if (!tree_parser->root())
	{
		printf("Error: No Root Node\n");
		delete tree_parser;
		return NULL;
	}
	return tree_parser;
}

#else /* USE_LIBXML */
xmlDocPtr parseXml(const char * data,const char *encoding)
{
	XMLTreeParser* tree_parser;

	tree_parser = new XMLTreeParser(encoding);

	if (!tree_parser->Parse(data, strlen(data), true))
	{
		printf("Error parsing XML Data: %s at line %d\n",
		       tree_parser->ErrorString(tree_parser->GetErrorCode()),
		       tree_parser->GetCurrentLineNumber());

		delete tree_parser;
		return NULL;
	}

	if (!tree_parser->RootNode())
	{
		printf("Error: No Root Node\n");
		delete tree_parser;
		return NULL;
	}
	return tree_parser;
}

xmlDocPtr parseXmlFile(const char * filename, bool warning_by_nonexistence /* = true */,const char *encoding)
{
	char buffer[2048];
	XMLTreeParser* tree_parser;
	size_t done;
	size_t length;
	FILE* xml_file;
	gzFile xmlgz_file;
	std::string fn = filename;
	bool zipped = (fn.substr(fn.find_last_of(".") + 1) == "gz");

	if (zipped)
	{
		xmlgz_file = gzopen(filename,"r");
		if (xmlgz_file == NULL)
		{
			if (warning_by_nonexistence)
				perror(filename);
			return NULL;
		}
		gzbuffer(xmlgz_file, 64*1024);
	}
	else
	{
		xml_file = fopen(filename, "r");
		if (xml_file == NULL)
		{
			if (warning_by_nonexistence)
				perror(filename);
			return NULL;
		}
	}
	tree_parser = new XMLTreeParser(encoding);

	do
	{
		if (zipped)
			length = gzread(xmlgz_file, buffer, sizeof(buffer));
		else
			length = fread(buffer, 1, sizeof(buffer), xml_file);
		done = length < sizeof(buffer);

		if (!tree_parser->Parse(buffer, length, done))
		{
			fprintf(stderr, "%s: Error parsing \"%s\": %s at line %d\n",
				__FUNCTION__,
				filename,
				tree_parser->ErrorString(tree_parser->GetErrorCode()),
				tree_parser->GetCurrentLineNumber());

			if (zipped)
				gzclose(xmlgz_file);
			else
				fclose(xml_file);
			delete tree_parser;
			return NULL;
		}
	}
	while (!done);

	if (!zipped)
		if (posix_fadvise(fileno(xml_file), 0, 0, POSIX_FADV_DONTNEED) != 0)
			perror("posix_fadvise FAILED!");

	if (zipped)
		gzclose(xmlgz_file);
	else
		fclose(xml_file);

	if (!tree_parser->RootNode())
	{
		delete tree_parser;
		return NULL;
	}
	return tree_parser;
}
#endif /* USE_LIBXML */
