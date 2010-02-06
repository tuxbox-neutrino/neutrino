/*
 * $Header: /cvs/tuxbox/apps/misc/libs/libxmltree/xmlinterface.cpp,v 1.3 2009/02/18 17:51:55 seife Exp $
 *
 * xmlinterface for zapit - d-box2 linux project
 *
 * (C) 2002 by thegoodguy <thegoodguy@berlios.de>
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
#include <xmltok.h>
#endif /* USE_LIBXML */


unsigned long xmlGetNumericAttribute(const xmlNodePtr node, const char *name, const int base)
{
	char *ptr = xmlGetAttribute(node, name);

	if (!ptr)
		return 0;

	return strtoul(ptr, 0, base);
}

long xmlGetSignedNumericAttribute(const xmlNodePtr node, const char *name, const int base)
{
	char *ptr = xmlGetAttribute(node, name);

	if (!ptr)
		return 0;

	return strtol(ptr, 0, base);
}

xmlNodePtr xmlGetNextOccurence(xmlNodePtr cur, const char * s)
{
	while ((cur != NULL) && (strcmp(xmlGetName(cur), s) != 0))
		cur = cur->xmlNextNode;
	return cur;
}


std::string Unicode_Character_to_UTF8(const int character)
{
#ifdef USE_LIBXML
	xmlChar buf[5];
	int length = xmlCopyChar(4, buf, character);
	return std::string((char*)buf, length);
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
			r += *s;
		}
		s++;
	}
	return r;
}

#ifdef USE_LIBXML
xmlDocPtr parseXml(const char * data)
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

xmlDocPtr parseXmlFile(const char * filename, bool warning_by_nonexistence /* = true */)
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
#else /* USE_LIBXML */
xmlDocPtr parseXml(const char * data)
{
	XMLTreeParser* tree_parser;

	tree_parser = new XMLTreeParser(NULL);

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

xmlDocPtr parseXmlFile(const char * filename, bool warning_by_nonexistence /* = true */)
{
	char buffer[2048];
	XMLTreeParser* tree_parser;
	size_t done;
	size_t length;
	FILE* xml_file;

	xml_file = fopen(filename, "r");

	if (xml_file == NULL)
	{
		if (warning_by_nonexistence)
			perror(filename);
		return NULL;
	}

	tree_parser = new XMLTreeParser(NULL);

	do
	{
		length = fread(buffer, 1, sizeof(buffer), xml_file);
		done = length < sizeof(buffer);

		if (!tree_parser->Parse(buffer, length, done))
		{
			fprintf(stderr, "%s: Error parsing \"%s\": %s at line %d\n",
				__FUNCTION__,
				filename,
				tree_parser->ErrorString(tree_parser->GetErrorCode()),
				tree_parser->GetCurrentLineNumber());

			fclose(xml_file);
			delete tree_parser;
			return NULL;
		}
	}
	while (!done);

	fclose(xml_file);

	if (!tree_parser->RootNode())
	{
		delete tree_parser;
		return NULL;
	}
	return tree_parser;
}
#endif /* USE_LIBXML */
