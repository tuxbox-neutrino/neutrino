/*
The contents of this file are subject to the Mozilla Public License
Version 1.0 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
License for the specific language governing rights and limitations
under the License.

The Original Code is expat.

The Initial Developer of the Original Code is James Clark.
Portions created by James Clark are Copyright (C) 1998
James Clark. All Rights Reserved.

Contributor(s):

  Fabian Giesen    - converted this monster to C++ (at least the core)

ChangeLog:

  $Log: xmlparse.cpp,v $
  Revision 1.3  2003/03/14 05:13:04  obi
  compileable with -W -Werror

  Revision 1.2  2003/02/26 22:21:24  obi
  Mismatched free() / delete / delete []

  Revision 1.1  2002/01/18 20:22:39  tmbinc
  initial checkin

  Revision 1.1.1.1  2001/10/07 13:01:18  tmbinc
  Import of ezap2-200110070

  Revision 1.1  1999/01/11 23:01:59  cvs
  Another try to commit this.

*/

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "xmltok/xmldef.h"

#ifdef XML_UNICODE
#define XML_ENCODE_MAX XML_UTF16_ENCODE_MAX
#define XmlConvert XmlUtf16Convert
#define XmlGetInternalEncoding XmlGetUtf16InternalEncoding
#define XmlEncode XmlUtf16Encode
#define MUST_CONVERT(enc, s) (!(enc)->isUtf16 || (((unsigned long)s) & 1))
typedef unsigned short ICHAR;
#else
#define XML_ENCODE_MAX XML_UTF8_ENCODE_MAX
#define XmlConvert XmlUtf8Convert
#define XmlGetInternalEncoding XmlGetUtf8InternalEncoding
#define XmlEncode XmlUtf8Encode
#define MUST_CONVERT(enc, s) (!(enc)->isUtf8)
typedef char ICHAR;
#endif

#ifdef XML_UNICODE_WCHAR_T
#define XML_T(x) L ## x
#else
#define XML_T(x) x
#endif

/* Round up n to be a multiple of sz, where sz is a power of 2. */
#define ROUND_UP(n, sz) (((n) + ((sz) - 1)) & ~((sz) - 1))

#include "xmlparse.h"

static int dtdInit(DTD *);
static void dtdDestroy(DTD *);
static int dtdCopy(DTD *newDtd, const DTD *oldDtd);

static void poolInit(STRING_POOL *);
static void poolClear(STRING_POOL *);
static void poolDestroy(STRING_POOL *);
static XML_Char *poolAppend(STRING_POOL *pool, const ENCODING *enc, const char *ptr, const char *end);
static XML_Char *poolStoreString(STRING_POOL *pool, const ENCODING *enc, const char *ptr, const char *end);
static int poolGrow(STRING_POOL *pool);
static const XML_Char *poolCopyString(STRING_POOL *pool, const XML_Char *s);
static const XML_Char *poolCopyStringN(STRING_POOL *pool, const XML_Char *s, int n);

#define poolStart(pool) ((pool)->start)
#define poolEnd(pool) ((pool)->ptr)
#define poolLength(pool) ((pool)->ptr - (pool)->start)
#define poolChop(pool) ((void)--(pool->ptr))
#define poolLastChar(pool) (((pool)->ptr)[-1])
#define poolDiscard(pool) ((pool)->ptr = (pool)->start)
#define poolFinish(pool) ((pool)->start = (pool)->ptr)
#define poolAppendChar(pool, c) (((pool)->ptr == (pool)->end && !poolGrow(pool)) ? 0 : ((*((pool)->ptr)++ = c), 1))

XML_Parser::XML_Parser(const XML_Char *encodingName)
{
	processor=prologInitProcessor;
	XmlPrologStateInit(&prologState);

	buffer=0;
	bufferPtr=0;
	bufferEnd=0;
	parseEndByteIndex=0;
	parseEndPtr=0;
	bufferLim=0;
	declElementType=0;
	declAttributeId=0;
	declAttributeIsCdata=0;
	declEntity=0;
	declNotationName=0;
	declNotationPublicId=0;

	startElementHandler=0;
	endElementHandler=0;
	characterDataHandler=0;
	processingInstructionHandler=0;
	defaultHandler=0;
	unparsedEntityDeclHandler=0;
	notationDeclHandler=0;
	externalEntityRefHandler=0;
	unknownEncodingHandler=0;

	memset(&position, 0, sizeof(POSITION));

	errorCode=XML_ERROR_NONE;
	eventPtr=0;
	eventEndPtr=0;
	positionPtr=0;
	tagLevel=0;
	tagStack=0;
	freeTagList=0;
	attsSize=INIT_ATTS_SIZE;
	/* must not realloc stuff allocated with new[] */
	atts=(ATTRIBUTE *)malloc(attsSize * sizeof(ATTRIBUTE));
	dataBuf=new XML_Char[INIT_DATA_BUF_SIZE];
	groupSize=0;
	groupConnector=0;
	hadExternalDoctype=0;
	unknownEncodingMem=0;
	unknownEncodingRelease=0;
	unknownEncodingData=0;
	unknownEncodingHandlerData=0;

	poolInit(&tempPool);
	poolInit(&temp2Pool);

	protocolEncodingName=encodingName ? poolCopyString(&tempPool, encodingName) : 0;

	if (!dtdInit(&dtd) || !atts || !dataBuf || (encodingName && !protocolEncodingName))
	{
		poolDestroy(&tempPool);
		poolDestroy(&temp2Pool);

		if (atts) free(atts);
		if (dataBuf) delete[] dataBuf;

		return;
	};

	dataBufEnd=dataBuf+INIT_DATA_BUF_SIZE;
	XmlInitEncoding(&initEncoding, &encoding, 0);
}

XML_Parser *XML_Parser::ExternalEntityParserCreate(const XML_Char *openEntityNames, const XML_Char *encodingName)
{
	XML_Parser *parser;

	parser=new XML_Parser(encodingName);
	if (!parser) return 0;

	if (!dtdCopy(&parser->dtd, &dtd) || !parser->setOpenEntityNames(openEntityNames))
	{
		delete parser;
		return 0;
	};

	parser->processor=externalEntityInitProcessor;
	return parser;
}

XML_Parser::~XML_Parser()
{
	for (;;)
	{
		TAG *p;

		if (!tagStack)
		{
			if (!freeTagList) break;

			tagStack=freeTagList;
			freeTagList=0;
		};

		p=tagStack;
		tagStack=tagStack->parent;

		delete[] p->buf;
		delete p;
	};

	poolDestroy(&tempPool);
	poolDestroy(&temp2Pool);
	dtdDestroy(&dtd);

	free(atts);
	free(groupConnector);
	free(buffer);
	delete[] dataBuf;
	free(unknownEncodingMem);

	if (unknownEncodingRelease)
		unknownEncodingRelease(unknownEncodingData);
}

int XML_Parser::SetBase(const XML_Char *p)
{
	if (p)
	{
		p=poolCopyString(&dtd.pool, p);
		if (!p) return 0;

		dtd.base=p;
	}
	else
		dtd.base=0;

	return 1;
}

const XML_Char *XML_Parser::GetBase()
{
	return dtd.base;
}

int XML_Parser::Parse(const char *s, int len, int isFinal)
{
	if (!len)
	{
		if (!isFinal) return 1;

		errorCode=processor(this, bufferPtr, parseEndPtr=bufferEnd, 0);
		if (errorCode==XML_ERROR_NONE) return 1;

		eventEndPtr=eventPtr;
		return 0;
	}
	else if (bufferPtr==bufferEnd)
	{
		const char *end;
		int nLeftOver;

		parseEndByteIndex+=len;
		positionPtr=s;

		if (isFinal)
		{
			errorCode=processor(this, s, parseEndPtr=s+len, 0);
			if (errorCode==XML_ERROR_NONE) return 1;

			eventEndPtr=eventPtr;
			return 0;
		};

		errorCode=processor(this, s, parseEndPtr=s+len, &end);
		if (errorCode!=XML_ERROR_NONE)
		{
			eventEndPtr=eventPtr;
			return 0;
		};

		XmlUpdatePosition(encoding, positionPtr, end, &position);

		nLeftOver=s+len-end;
		if (nLeftOver)
		{
			if (!buffer || nLeftOver>bufferLim-buffer)
			{
				buffer=buffer==0 ? (char *) malloc(len*2) : (char *) realloc(buffer, len*2);

				if (!buffer)
				{
					errorCode=XML_ERROR_NO_MEMORY;
					eventPtr=eventEndPtr=0;
					return 0;
				};

				bufferLim=buffer+len*2;
			};

			memmove(buffer, end, nLeftOver);

			bufferPtr=buffer;
			bufferEnd=buffer+nLeftOver;
		};

		return 1;
	}
	else
	{
		memmove(GetBuffer(len), s, len);
		return ParseBuffer(len, isFinal);
	};
}

int XML_Parser::ParseBuffer(int len, int isFinal)
{
	const char *start = bufferPtr;

	positionPtr=start;
	bufferEnd+=len;
	parseEndByteIndex+=len;

	errorCode=processor(this, start, parseEndPtr=bufferEnd,
			    isFinal ? (const char **) 0: &bufferPtr);

	if (errorCode==XML_ERROR_NONE)
	{
		if (!isFinal)
			XmlUpdatePosition(encoding, positionPtr, bufferPtr, &position);

		return 1;
	}
	else
	{
		eventEndPtr=eventPtr;
		return 0;
	};
}

void *XML_Parser::GetBuffer(int len)
{
	if (len>bufferLim-bufferEnd)
	{
		int neededSize=len+(bufferEnd-bufferPtr);

		if (neededSize<=bufferLim-buffer)
		{
			memmove(buffer, bufferPtr, bufferEnd-bufferPtr);
			bufferEnd=buffer+(bufferEnd-bufferPtr);
			bufferPtr=buffer;
		}
		else
		{
			char *newBuf;
			int bufferSize=bufferLim-bufferPtr;

			if (!bufferSize)
				bufferSize=INIT_BUFFER_SIZE;

			do
			{
				bufferSize*=2;
			} while (bufferSize<neededSize);

			newBuf=(char *) malloc(bufferSize);

			if (newBuf==0)
			{
				errorCode=XML_ERROR_NO_MEMORY;
				return 0;
			};

			bufferLim=newBuf+bufferSize;

			if (bufferPtr)
			{
				memmove(newBuf, bufferPtr, bufferEnd-bufferPtr);
				free(buffer);
			};

			bufferEnd=newBuf+(bufferEnd-bufferPtr);
			bufferPtr=buffer=newBuf;
		};
	};

	return bufferEnd;
}

enum XML_Error XML_Parser::GetErrorCode()
{
	return errorCode;
}

long XML_Parser::GetCurrentByteIndex()
{
	if (eventPtr)
		return parseEndByteIndex-(parseEndPtr-eventPtr);

	return -1;
}

int XML_Parser::GetCurrentLineNumber()
{
	/* at EOF positionPtr point to input, while eventPtr not updated -- focus */
	if (eventPtr && (positionPtr < eventPtr))
	{
		XmlUpdatePosition(encoding, positionPtr, eventPtr, &position);
		positionPtr=eventPtr;
	};

	return position.lineNumber+1;
}

int XML_Parser::GetCurrentColumnNumber()
{
	if (eventPtr)
	{
		XmlUpdatePosition(encoding, positionPtr, eventPtr, &position);
		positionPtr=eventPtr;
	};

	return position.columnNumber;
}

void XML_Parser::DefaultCurrent()
{
	if (defaultHandler)
		reportDefault(encoding, eventPtr, eventEndPtr);
}

const XML_LChar *XML_Parser::ErrorString(int code)
{
	static const XML_LChar *message[] = {
		0,
		XML_T("out of memory"),
		XML_T("syntax error"),
		XML_T("no element found"),
		XML_T("not well-formed"),
		XML_T("unclosed token"),
		XML_T("unclosed token"),
		XML_T("mismatched tag"),
		XML_T("duplicate attribute"),
		XML_T("junk after document element"),
		XML_T("illegal parameter entity reference"),
		XML_T("undefined entity"),
		XML_T("recursive entity reference"),
		XML_T("asynchronous entity"),
		XML_T("reference to invalid character number"),
		XML_T("reference to binary entity"),
		XML_T("reference to external entity in attribute"),
		XML_T("xml processing instruction not at start of external entity"),
		XML_T("unknown encoding"),
		XML_T("encoding specified in XML declaration is incorrect"),
		XML_T("unclosed CDATA section"),
		XML_T("error in processing external entity reference")
	};

	if (code > 0 && (unsigned)code < sizeof(message)/sizeof(message[0]))
		return message[code];

	return 0;
}

enum XML_Error contentProcessor(void *parser, const char *start, const char *end, const char **endPtr)
{
	XML_Parser *p=(XML_Parser *) parser;

	return p->doContent(0, p->encoding, start, end, endPtr);
}

enum XML_Error externalEntityInitProcessor(void *parser, const char *start, const char *end, const char **endPtr)
{
	XML_Parser *p=(XML_Parser *) parser;

	enum XML_Error result = p->initializeEncoding();
	if (result != XML_ERROR_NONE)
		return result;

	p->processor = externalEntityInitProcessor2;
	return externalEntityInitProcessor2(p, start, end, endPtr);
}

enum XML_Error externalEntityInitProcessor2(void *parser, const char *start, const char *end, const char **endPtr)
{
	XML_Parser *p=(XML_Parser *) parser;
	const char *next;

	int tok=XmlContentTok(p->encoding, start, end, &next);
	switch (tok)
	{
	case XML_TOK_BOM:
		start=next;
		break;

	case XML_TOK_PARTIAL:
		if (endPtr)
		{
			*endPtr=start;
			return XML_ERROR_NONE;
		};

		p->eventPtr=start;
		return XML_ERROR_UNCLOSED_TOKEN;

	case XML_TOK_PARTIAL_CHAR:
		if (endPtr)
		{
			*endPtr=start;
			return XML_ERROR_NONE;
		};

		p->eventPtr=start;
		return XML_ERROR_PARTIAL_CHAR;
	};

	p->processor=externalEntityInitProcessor3;
	return externalEntityInitProcessor3(parser, start, end, endPtr);
}

enum XML_Error externalEntityInitProcessor3(void *parser, const char *start, const char *end, const char **endPtr)
{
	XML_Parser *p=(XML_Parser *) parser;
	const char *next;

	int tok=XmlContentTok(p->encoding, start, end, &next);

	switch (tok) {
	case XML_TOK_XML_DECL:
	{
		enum XML_Error result=p->processXmlDecl(1, start, next);

		if (result!=XML_ERROR_NONE)
			return result;

		start=next;
	};

	break;

	case XML_TOK_PARTIAL:
		if (endPtr)
		{
			*endPtr=start;
			return XML_ERROR_NONE;
		};

		p->eventPtr=start;
		return XML_ERROR_UNCLOSED_TOKEN;

	case XML_TOK_PARTIAL_CHAR:
		if (endPtr)
		{
			*endPtr=start;
			return XML_ERROR_NONE;
		};

		p->eventPtr=start;
		return XML_ERROR_PARTIAL_CHAR;
	}

	p->processor=externalEntityContentProcessor;
	p->tagLevel=1;

	return p->doContent(1, p->encoding, start, end, endPtr);
}

enum XML_Error externalEntityContentProcessor(void *parser, const char *start, const char *end, const char **endPtr)
{
	XML_Parser *p=(XML_Parser *) parser;

	return p->doContent(1, p->encoding, start, end, endPtr);
}

enum XML_Error XML_Parser::doContent(int startTagLevel, const ENCODING *enc, const char *s, const char *end, const char **nextPtr)
{
	const ENCODING *internalEnc=XmlGetInternalEncoding();
	const char *dummy;
	const char **eventPP;
	const char **eventEndPP;

	if (enc==encoding)
	{
		eventPP=&eventPtr;
		*eventPP=s;
		eventEndPP=&eventEndPtr;
	}
	else
		eventPP=eventEndPP=&dummy;

	for (;;)
	{
		const char *next;
		int tok=XmlContentTok(enc, s, end, &next);
		*eventEndPP = next;

		switch (tok) {
		case XML_TOK_TRAILING_CR:
			if (nextPtr)
			{
				*nextPtr=s;
				return XML_ERROR_NONE;
			}

			*eventEndPP=end;

			if (characterDataHandler)
			{
				XML_Char c=XML_T('\n');
				CharacterDataHandler(&c, 1);
			}
			else if (defaultHandler)
				reportDefault(enc, s, end);

			if (!startTagLevel) return XML_ERROR_NO_ELEMENTS;
			if (tagLevel!=startTagLevel) return XML_ERROR_ASYNC_ENTITY;

			return XML_ERROR_NONE;

		case XML_TOK_NONE:
			if (nextPtr)
			{
				*nextPtr=s;
				return XML_ERROR_NONE;
			};

			if (startTagLevel > 0)
			{
				if (tagLevel!=startTagLevel) return XML_ERROR_ASYNC_ENTITY;

				return XML_ERROR_NONE;
			};

			return XML_ERROR_NO_ELEMENTS;

		case XML_TOK_INVALID:
			*eventPP=next;
			return XML_ERROR_INVALID_TOKEN;

		case XML_TOK_PARTIAL:
			if (nextPtr)
			{
				*nextPtr=s;
				return XML_ERROR_NONE;
			};

			return XML_ERROR_UNCLOSED_TOKEN;

		case XML_TOK_PARTIAL_CHAR:
			if (nextPtr)
			{
				*nextPtr=s;
				return XML_ERROR_NONE;
			};

			return XML_ERROR_PARTIAL_CHAR;

		case XML_TOK_ENTITY_REF:
		{
			const XML_Char *name;
			ENTITY *entity;
			XML_Char ch=XmlPredefinedEntityName(enc,
							    s+enc->minBytesPerChar,
							    next-enc->minBytesPerChar);

			if (ch)
			{
				if (characterDataHandler)
					CharacterDataHandler(&ch, 1);
				else if (defaultHandler)
					reportDefault(enc, s, next);

				break;
			};

			name=poolStoreString(&dtd.pool, enc, s+enc->minBytesPerChar,
					     next-enc->minBytesPerChar);

			if (!name) return XML_ERROR_NO_MEMORY;

			entity=(ENTITY *) lookup(&dtd.generalEntities, name, 0);
			poolDiscard(&dtd.pool);

			if (!entity)
			{
				if (dtd.complete || dtd.standalone)
					return XML_ERROR_UNDEFINED_ENTITY;

				if (defaultHandler)
					reportDefault(enc, s, next);

				break;
			};

			if (entity->open) return XML_ERROR_RECURSIVE_ENTITY_REF;
			if (entity->notation) return XML_ERROR_BINARY_ENTITY_REF;

			if (entity)
			{
				if (entity->textPtr)
				{
					enum XML_Error result;

					if (defaultHandler)
					{
						reportDefault(enc, s, next);
						break;
					};

					/* Protect against the possibility that somebody sets
					   the defaultHandler from inside another handler. */

					*eventEndPP=*eventPP;
					entity->open=1;

					result=doContent(tagLevel, internalEnc,
							 (char *) entity->textPtr,
							 (char *) (entity->textPtr+entity->textLen),
							 0);

					entity->open = 0;

					if (result) return result;
				}
				else if (externalEntityRefHandler)
				{
					const XML_Char *openEntityNames;

					entity->open=1;
					openEntityNames=getOpenEntityNames();
					entity->open=0;

					if (!openEntityNames) return XML_ERROR_NO_MEMORY;
					if (!ExternalEntityRefHandler(openEntityNames, dtd.base, entity->systemId, entity->publicId))
						return XML_ERROR_EXTERNAL_ENTITY_HANDLING;

					poolDiscard(&tempPool);
				}
				else if (defaultHandler)
					reportDefault(enc, s, next);
			};
			break;
		}

		case XML_TOK_START_TAG_WITH_ATTS:
			if (!startElementHandler)
			{
				enum XML_Error result=storeAtts(enc, 0, s);

				if (result) return result;
			};

			// fall through

		case XML_TOK_START_TAG_NO_ATTS:
		{
			TAG *tag;

			if (freeTagList)
			{
				tag=freeTagList;
				freeTagList=freeTagList->parent;
			}
			else
			{
				tag=new TAG;
				if (!tag) return XML_ERROR_NO_MEMORY;

				tag->buf=new char[INIT_TAG_BUF_SIZE];
				if (!tag->buf) {
					delete tag;
					return XML_ERROR_NO_MEMORY;
				}
				tag->bufEnd=tag->buf+INIT_TAG_BUF_SIZE;
			}

			tag->parent=tagStack;
			tagStack=tag;

			tag->rawName=s+enc->minBytesPerChar;
			tag->rawNameLength=XmlNameLength(enc, tag->rawName);

			if (nextPtr)
			{
				if (tag->rawNameLength>tag->bufEnd-tag->buf)
				{
					int bufSize=ROUND_UP(tag->rawNameLength*4, sizeof(XML_Char));

					tag->buf=(char *) realloc(tag->buf, bufSize);
					if (!tag->buf) {
						delete tag;
						return XML_ERROR_NO_MEMORY;
					}
					tag->bufEnd=tag->buf+bufSize;
				};

				memmove(tag->buf, tag->rawName, tag->rawNameLength);
				tag->rawName=tag->buf;
			};

			tagLevel++;

			if (startElementHandler)
			{
				enum XML_Error result;
				XML_Char *toPtr;

				for (;;)
				{
					const char *rawNameEnd=tag->rawName+tag->rawNameLength;
					const char *fromPtr=tag->rawName;
					int bufSize;

					if (nextPtr)
						toPtr=(XML_Char *) (tag->buf+ROUND_UP(tag->rawNameLength, sizeof(XML_Char)));
					else
						toPtr=(XML_Char *) tag->buf;

					tag->name=toPtr;

					XmlConvert(enc, &fromPtr, rawNameEnd,
						   (ICHAR **) &toPtr, (ICHAR *) tag->bufEnd-1);

					if (fromPtr==rawNameEnd) break;

					bufSize=(tag->bufEnd-tag->buf) << 1;
					tag->buf=(char *) realloc(tag->buf, bufSize);

					if (!tag->buf) {
						delete tag;
						return XML_ERROR_NO_MEMORY;
					}
					tag->bufEnd=tag->buf+bufSize;

					if (nextPtr) tag->rawName=tag->buf;
				};

				*toPtr=XML_T('\0');

				result=storeAtts(enc, tag->name, s);
				if (result) return result;

				StartElementHandler(tag->name, (const XML_Char **) atts);
				poolClear(&tempPool);
			}
			else
			{
				tag->name=0;

				if (defaultHandler) reportDefault(enc, s, next);
			};

			break;
		}

		case XML_TOK_EMPTY_ELEMENT_WITH_ATTS:

			if (!startElementHandler)
			{
				enum XML_Error result=storeAtts(enc, 0, s);

				if (result) return result;
			};

			// fall through */

		case XML_TOK_EMPTY_ELEMENT_NO_ATTS:
			if (startElementHandler || endElementHandler)
			{
				const char *rawName=s+enc->minBytesPerChar;
				const XML_Char *name=poolStoreString(&tempPool, enc, rawName,
								     rawName +
								     XmlNameLength(enc, rawName));

				if (!name) return XML_ERROR_NO_MEMORY;

				poolFinish(&tempPool);

				if (startElementHandler)
				{
					enum XML_Error result=storeAtts(enc, name, s);

					if (result) return result;

					StartElementHandler(name, (const XML_Char **) atts);
				};

				if (endElementHandler)
				{
					if (startElementHandler) *eventPP=*eventEndPP;

					EndElementHandler(name);
				};

				poolClear(&tempPool);
			}
			else if (defaultHandler)
				reportDefault(enc, s, next);

			if (!tagLevel)
				return epilogProcessor(this, next, end, nextPtr);

			break;

		case XML_TOK_END_TAG:
			if (tagLevel==startTagLevel)
				return XML_ERROR_ASYNC_ENTITY;
			else
			{
				int len;
				const char *rawName;

				TAG *tag=tagStack;
				tagStack=tag->parent;
				tag->parent=freeTagList;
				freeTagList=tag;

				rawName=s+enc->minBytesPerChar*2;
				len=XmlNameLength(enc, rawName);

				if (len!=tag->rawNameLength || memcmp(tag->rawName, rawName, len)!=0)
				{
					*eventPP = rawName;
					return XML_ERROR_TAG_MISMATCH;
				};

				--tagLevel;

				if (endElementHandler)
				{
					if (tag->name)
						EndElementHandler(tag->name);
					else
					{
						const XML_Char *name=poolStoreString(&tempPool, enc, rawName,
										     rawName+len);

						if (!name) return XML_ERROR_NO_MEMORY;

						EndElementHandler(name);
						poolClear(&tempPool);
					};
				}
				else if (defaultHandler)
					reportDefault(enc, s, next);

				if (!tagLevel)
					return epilogProcessor(this, next, end, nextPtr);
			};

			break;

		case XML_TOK_CHAR_REF:
		{
			int n=XmlCharRefNumber(enc, s);

			if (n<0) return XML_ERROR_BAD_CHAR_REF;

			if (characterDataHandler)
			{
				XML_Char buf[XML_ENCODE_MAX];
				CharacterDataHandler(buf, XmlEncode(n, (ICHAR *) buf));
			}
			else if (defaultHandler)
				reportDefault(enc, s, next);
		}

		break;

		case XML_TOK_XML_DECL:
			return XML_ERROR_MISPLACED_XML_PI;

		case XML_TOK_DATA_NEWLINE:
			if (characterDataHandler)
			{
				XML_Char c=XML_T('\n');
				CharacterDataHandler(&c, 1);
			}
			else if (defaultHandler)
				reportDefault(enc, s, next);

			break;

		case XML_TOK_CDATA_SECT_OPEN:
		{
			enum XML_Error result;

			if (characterDataHandler)
				CharacterDataHandler(dataBuf, 0);
			else if (defaultHandler)
				reportDefault(enc, s, next);

			result=doCdataSection(enc, &next, end, nextPtr);

			if (!next)
			{
				processor=cdataSectionProcessor;
				return result;
			};
		};

		break;

		case XML_TOK_TRAILING_RSQB:
			if (nextPtr)
			{
				*nextPtr=s;
				return XML_ERROR_NONE;
			};

			if (characterDataHandler)
			{
				if (MUST_CONVERT(enc, s))
				{
					ICHAR *dataPtr=(ICHAR *) dataBuf;
					XmlConvert(enc, &s, end, &dataPtr, (ICHAR *) dataBufEnd);

					CharacterDataHandler(dataBuf, dataPtr - (ICHAR *) dataBuf);
				}
				else
					CharacterDataHandler((XML_Char *) s, (XML_Char *) end-(XML_Char *) s);
			}
			else if (defaultHandler)
				reportDefault(enc, s, end);

			if (!startTagLevel)
			{
				*eventPP=end;
				return XML_ERROR_NO_ELEMENTS;
			};

			if (tagLevel!=startTagLevel)
			{
				*eventPP=end;
				return XML_ERROR_ASYNC_ENTITY;
			};

			return XML_ERROR_NONE;

		case XML_TOK_DATA_CHARS:
			if (characterDataHandler)
			{
				if (MUST_CONVERT(enc, s))
				{
					for (;;)
					{
						ICHAR *dataPtr=(ICHAR *) dataBuf;
						XmlConvert(enc, &s, next, &dataPtr, (ICHAR *) dataBufEnd);

						*eventEndPP = s;
						CharacterDataHandler(dataBuf, dataPtr-(ICHAR *) dataBuf);

						if (s==next) break;
						*eventPP = s;
					};
				}
				else
					CharacterDataHandler((XML_Char *) s, (XML_Char *) next-(XML_Char *) s);
			}
			else if (defaultHandler)
				reportDefault(enc, s, next);

			break;

		case XML_TOK_PI:
			if (!reportProcessingInstruction(enc, s, next))
				return XML_ERROR_NO_MEMORY;

			break;

		default:
			if (defaultHandler)
				reportDefault(enc, s, next);

			break;
		};

		*eventPP=s=next;
	};

	// not reached
}

/* If tagName is non-null, build a real list of attributes,
otherwise just check the attributes for well-formedness. */

enum XML_Error XML_Parser::storeAtts(const ENCODING *enc, const XML_Char *tagName, const char *s)
{
	ELEMENT_TYPE *elementType=0;
	int nDefaultAtts=0;
	const XML_Char **appAtts;
	int i;
	int n;

	if (tagName)
	{
		elementType=(ELEMENT_TYPE *) lookup(&dtd.elementTypes, tagName, 0);

		if (elementType)
			nDefaultAtts=elementType->nDefaultAtts;
	}

	n=XmlGetAttributes(enc, s, attsSize, atts);

	if (n+nDefaultAtts>attsSize)
	{
		int oldAttsSize=attsSize;

		attsSize=n+nDefaultAtts+INIT_ATTS_SIZE;

		ATTRIBUTE *newatts = (ATTRIBUTE *) realloc((void *) atts, attsSize*sizeof(ATTRIBUTE));

		if (!newatts) return XML_ERROR_NO_MEMORY;

		atts = newatts;

		if (n>oldAttsSize) XmlGetAttributes(enc, s, n, atts);
	};

	appAtts=(const XML_Char **) atts;

	for (i=0; i<n; i++)
	{
		ATTRIBUTE_ID *attId=getAttributeId(enc, atts[i].name,
						   atts[i].name +
						   XmlNameLength(enc, atts[i].name));

		if (!attId) return XML_ERROR_NO_MEMORY;

		if ((attId->name)[-1])
		{
			if (enc==encoding) eventPtr=atts[i].name;

			return XML_ERROR_DUPLICATE_ATTRIBUTE;
		};

		(attId->name)[-1]=1;

		appAtts[i<<1]=attId->name;

		if (!atts[i].normalized)
		{
			enum XML_Error result;
			int isCdata=1;

			if (attId->maybeTokenized)
			{
				for (int j=0; j<nDefaultAtts; j++)
				{
					if (attId==elementType->defaultAtts[j].id)
					{
						isCdata=elementType->defaultAtts[j].isCdata;
						break;
					};
				};
			};

			result=storeAttributeValue(enc, isCdata, atts[i].valuePtr,
						   atts[i].valueEnd, &tempPool);

			if (result) return result;

			if (tagName)
			{
				appAtts[(i<<1)+1]=poolStart(&tempPool);
				poolFinish(&tempPool);
			}
			else
				poolDiscard(&tempPool);
		}
		else if (tagName)
		{
			appAtts[(i<<1)+1]=poolStoreString(&tempPool, enc, atts[i].valuePtr, atts[i].valueEnd);

			if (appAtts[(i<<1)+1]==0)	return XML_ERROR_NO_MEMORY;
			poolFinish(&tempPool);
		};
	};

	if (tagName)
	{
		for (int j=0; j<nDefaultAtts; j++)
		{
			const DEFAULT_ATTRIBUTE *da=elementType->defaultAtts+j;

			if (!(da->id->name)[-1] && da->value)
			{
				(da->id->name)[-1]=1;
				appAtts[i<<1]=da->id->name;
				appAtts[(i<<1)+1]=da->value;
				i++;
			};
		};
		appAtts[i<<1]=0;
	};

	while (i-->0) ((XML_Char *)appAtts[i<<1])[-1]=0;

	return XML_ERROR_NONE;
}

/* The idea here is to avoid using stack for each CDATA section when
the whole file is parsed with one call. */

enum XML_Error cdataSectionProcessor(void *parser, const char *start, const char *end, const char **endPtr)
{
	XML_Parser *p=(XML_Parser *) parser;

	enum XML_Error result=p->doCdataSection(p->encoding, &start, end, endPtr);

	if (start)
	{
		p->processor=contentProcessor;
		return contentProcessor(parser, start, end, endPtr);
	};

	return result;
}

/* startPtr gets set to non-null is the section is closed, and to null if
the section is not yet closed. */

enum XML_Error XML_Parser::doCdataSection(const ENCODING *enc, const char **startPtr, const char *end, const char **nextPtr)
{
	const char *s=*startPtr;
	const char *dummy;
	const char **eventPP;
	const char **eventEndPP;

	if (enc==encoding)
	{
		eventPP=&eventPtr;
		*eventPP=s;
		eventEndPP=&eventEndPtr;
	}
	else
		eventPP=eventEndPP=&dummy;

	*startPtr=0;

	for (;;)
	{
		const char *next;

		int tok=XmlCdataSectionTok(enc, s, end, &next);

		*eventEndPP = next;

		switch (tok)
		{
		case XML_TOK_CDATA_SECT_CLOSE:
			if (characterDataHandler)
				CharacterDataHandler(dataBuf, 0);
			else if (defaultHandler)
				reportDefault(enc, s, next);

			*startPtr = next;
			return XML_ERROR_NONE;

		case XML_TOK_DATA_NEWLINE:
			if (characterDataHandler)
			{
				XML_Char c=XML_T('\n');
				CharacterDataHandler(&c, 1);
			}
			else if (defaultHandler)
				reportDefault(enc, s, next);

			break;

		case XML_TOK_DATA_CHARS:
			if (characterDataHandler)
			{
				if (MUST_CONVERT(enc, s))
				{
					for (;;)
					{
						ICHAR *dataPtr=(ICHAR *) dataBuf;

						XmlConvert(enc, &s, next, &dataPtr, (ICHAR *) dataBufEnd);

						*eventEndPP=next;
						CharacterDataHandler(dataBuf, dataPtr-(ICHAR *) dataBuf);

						if (s==next) break;
						*eventPP=s;
					};
				}
				else
					CharacterDataHandler((XML_Char *) s, (XML_Char *) next-(XML_Char *) s);
			}
			else if (defaultHandler)
				reportDefault(enc, s, next);

			break;

		case XML_TOK_INVALID:
			*eventPP=next;
			return XML_ERROR_INVALID_TOKEN;

		case XML_TOK_PARTIAL_CHAR:
			if (nextPtr)
			{
				*nextPtr=s;
				return XML_ERROR_NONE;
			};

			return XML_ERROR_PARTIAL_CHAR;

		case XML_TOK_PARTIAL:
		case XML_TOK_NONE:
			if (nextPtr)
			{
				*nextPtr=s;
				return XML_ERROR_NONE;
			}

			return XML_ERROR_UNCLOSED_CDATA_SECTION;
		default:
			abort();
		};

		*eventPP=s=next;
	};

	// not reached
}

enum XML_Error XML_Parser::initializeEncoding()
{
	const char *s;
#ifdef XML_UNICODE
	char encodingBuf[128];

	if (!protocolEncodingName)
		s=0;
	else
	{
		for (int i=0; protocolEncodingName[i]; i++)
		{
			if (i==sizeof(encodingBuf)-1
					|| protocolEncodingName[i]>=0x80
					|| protocolEncodingName[i]<0)
			{
				encodingBuf[0]='\0';
				break;
			};

			encodingBuf[i]=(char) protocolEncodingName[i];
		};

		encodingBuf[i]='\0';
		s=encodingBuf;
	};
#else
	s=protocolEncodingName;
#endif

	if (XmlInitEncoding(&initEncoding, &encoding, s)) return XML_ERROR_NONE;

	return handleUnknownEncoding(protocolEncodingName);
}

enum XML_Error XML_Parser::processXmlDecl(int isGeneralTextEntity, const char *s, const char *next)
{
	const char *encodingName=0;
	const ENCODING *newEncoding=0;
	const char *version;
	int standalone=-1;

	if (!XmlParseXmlDecl(isGeneralTextEntity, encoding, s, next, &eventPtr,
			     &version, &encodingName, &newEncoding, &standalone))
		return XML_ERROR_SYNTAX;

	if (!isGeneralTextEntity && standalone==1) dtd.standalone=1;

	if (defaultHandler) reportDefault(encoding, s, next);

	if (!protocolEncodingName)
	{
		if (newEncoding)
		{
			if (newEncoding->minBytesPerChar!=encoding->minBytesPerChar)
			{
				eventPtr=encodingName;
				return XML_ERROR_INCORRECT_ENCODING;
			};

			encoding=newEncoding;
		}
		else if (encodingName)
		{
			enum XML_Error result;
			const XML_Char *s1=poolStoreString(&tempPool, encoding, encodingName,
							   encodingName +
							   XmlNameLength(encoding, encodingName));

			if (!s1) return XML_ERROR_NO_MEMORY;

			result=handleUnknownEncoding(s1);

			poolDiscard(&tempPool);

			if (result==XML_ERROR_UNKNOWN_ENCODING) eventPtr=encodingName;
			return result;
		};
	};

	return XML_ERROR_NONE;
}

enum XML_Error XML_Parser::handleUnknownEncoding(const XML_Char *encodingName)
{
	if (unknownEncodingHandler)
	{
		XML_Encoding info;

		for (int i=0; i<256; i++) info.map[i]=-1;

		info.convert=0;
		info.data=0;
		info.release=0;

		if (UnknownEncodingHandler(unknownEncodingHandlerData, encodingName, &info))
		{
			ENCODING *enc;
			unknownEncodingMem=malloc(XmlSizeOfUnknownEncoding());
			if (!unknownEncodingMem)
			{
				if (info.release) info.release(info.data);
				return XML_ERROR_NO_MEMORY;
			};

			enc=XmlInitUnknownEncoding(unknownEncodingMem, info.map, info.convert,
						   info.data);

			if (enc)
			{
				unknownEncodingData=info.data;
				unknownEncodingRelease=info.release;
				encoding=enc;
				return XML_ERROR_NONE;
			};
		};

		if (info.release) info.release(info.data);
	};

	return XML_ERROR_UNKNOWN_ENCODING;
}

enum XML_Error prologInitProcessor(void *parser, const char *s, const char *end, const char **nextPtr)
{
	XML_Parser *p=(XML_Parser *) parser;

	enum XML_Error result=p->initializeEncoding();

	if (result!=XML_ERROR_NONE) return result;

	p->processor=prologProcessor;
	return prologProcessor(parser, s, end, nextPtr);
}

enum XML_Error prologProcessor(void *parser, const char *s, const char *end, const char **nextPtr)
{
	XML_Parser *p=(XML_Parser *) parser;

	for (;;)
	{
		const char *next;

		int tok=XmlPrologTok(p->encoding, s, end, &next);

		if (tok<=0)
		{
			if (nextPtr!=0 && tok!=XML_TOK_INVALID)
			{
				*nextPtr=s;
				return XML_ERROR_NONE;
			};

			switch (tok)
			{
			case XML_TOK_INVALID:
				p->eventPtr=next;
				return XML_ERROR_INVALID_TOKEN;

			case XML_TOK_NONE:
				return XML_ERROR_NO_ELEMENTS;

			case XML_TOK_PARTIAL:
				return XML_ERROR_UNCLOSED_TOKEN;

			case XML_TOK_PARTIAL_CHAR:
				return XML_ERROR_PARTIAL_CHAR;

			case XML_TOK_TRAILING_CR:
				p->eventPtr=s+p->encoding->minBytesPerChar;
				return XML_ERROR_NO_ELEMENTS;

			default:
				abort();
			};
		};

		switch (XmlTokenRole(&p->prologState, tok, s, next, p->encoding))
		{
		case XML_ROLE_XML_DECL:
		{
			enum XML_Error result=p->processXmlDecl(0, s, next);

			if (result!=XML_ERROR_NONE) return result;
		}
		break;

		case XML_ROLE_DOCTYPE_SYSTEM_ID:
			p->hadExternalDoctype=1;
			break;

		case XML_ROLE_DOCTYPE_PUBLIC_ID:
		case XML_ROLE_ENTITY_PUBLIC_ID:
			if (!XmlIsPublicId(p->encoding, s, next, &p->eventPtr))
				return XML_ERROR_SYNTAX;

			if (p->declEntity)
			{
				XML_Char *tem=poolStoreString(&p->dtd.pool, p->encoding,
							      s+p->encoding->minBytesPerChar,
							      next-p->encoding->minBytesPerChar);

				if (!tem) return XML_ERROR_NO_MEMORY;

				p->normalizePublicId(tem);
				p->declEntity->publicId=tem;
				poolFinish(&p->dtd.pool);
			}

			break;

		case XML_ROLE_INSTANCE_START:
			p->processor=contentProcessor;

			if (p->hadExternalDoctype) p->dtd.complete=0;

			return contentProcessor(parser, s, end, nextPtr);

		case XML_ROLE_ATTLIST_ELEMENT_NAME:
		{
			const XML_Char *name=poolStoreString(&p->dtd.pool, p->encoding, s, next);

			if (!name) return XML_ERROR_NO_MEMORY;

			p->declElementType=(ELEMENT_TYPE *) lookup(&p->dtd.elementTypes, name, sizeof(ELEMENT_TYPE));

			if (!p->declElementType) return XML_ERROR_NO_MEMORY;

			if (p->declElementType->name!=name)
				poolDiscard(&p->dtd.pool);
			else
				poolFinish(&p->dtd.pool);

			break;
		};

		case XML_ROLE_ATTRIBUTE_NAME:
			p->declAttributeId=p->getAttributeId(p->encoding, s, next);

			if (!p->declAttributeId) return XML_ERROR_NO_MEMORY;

			p->declAttributeIsCdata=0;

			break;

		case XML_ROLE_ATTRIBUTE_TYPE_CDATA:
			p->declAttributeIsCdata=1;
			break;

		case XML_ROLE_IMPLIED_ATTRIBUTE_VALUE:
		case XML_ROLE_REQUIRED_ATTRIBUTE_VALUE:
			if (p->dtd.complete &&
					!p->defineAttribute(p->declElementType, p->declAttributeId, p->declAttributeIsCdata, 0))
				return XML_ERROR_NO_MEMORY;

			break;

		case XML_ROLE_DEFAULT_ATTRIBUTE_VALUE:
		case XML_ROLE_FIXED_ATTRIBUTE_VALUE:
		{
			const XML_Char *attVal;
			enum XML_Error result=p->storeAttributeValue(p->encoding, p->declAttributeIsCdata,
					      s+p->encoding->minBytesPerChar,
					      next-p->encoding->minBytesPerChar, &p->dtd.pool);

			if (result) return result;

			attVal=poolStart(&p->dtd.pool);
			poolFinish(&p->dtd.pool);

			if (p->dtd.complete &&
					!p->defineAttribute(p->declElementType, p->declAttributeId, p->declAttributeIsCdata, attVal))
				return XML_ERROR_NO_MEMORY;

			break;
		};

		case XML_ROLE_ENTITY_VALUE:
		{
			enum XML_Error result=p->storeEntityValue(s, next);
			if (result!=XML_ERROR_NONE) return result;
		};

		break;

		case XML_ROLE_ENTITY_SYSTEM_ID:
			if (p->declEntity)
			{
				p->declEntity->systemId=poolStoreString(&p->dtd.pool, p->encoding,
									s+p->encoding->minBytesPerChar,
									next-p->encoding->minBytesPerChar);

				if (!p->declEntity->systemId) return XML_ERROR_NO_MEMORY;

				p->declEntity->base=p->dtd.base;
				poolFinish(&p->dtd.pool);
			};

			break;

		case XML_ROLE_ENTITY_NOTATION_NAME:
			if (p->declEntity)
			{
				p->declEntity->notation=poolStoreString(&p->dtd.pool, p->encoding, s, next);

				if (!p->declEntity->notation) return XML_ERROR_NO_MEMORY;

				poolFinish(&p->dtd.pool);

				if (p->unparsedEntityDeclHandler)
				{
					p->eventPtr=p->eventEndPtr=s;

					p->UnparsedEntityDeclHandler(p->declEntity->name, p->declEntity->base,
								     p->declEntity->systemId, p->declEntity->publicId,
								     p->declEntity->notation);
				};
			};

			break;

		case XML_ROLE_GENERAL_ENTITY_NAME:
		{
			const XML_Char *name;

			if (XmlPredefinedEntityName(p->encoding, s, next))
			{
				p->declEntity=0;
				break;
			};

			name=poolStoreString(&p->dtd.pool, p->encoding, s, next);

			if (!name) return XML_ERROR_NO_MEMORY;

			if (p->dtd.complete)
			{
				p->declEntity=(ENTITY *) lookup(&p->dtd.generalEntities, name, sizeof(ENTITY));

				if (!p->declEntity) return XML_ERROR_NO_MEMORY;

				if (p->declEntity->name!=name)
				{
					poolDiscard(&p->dtd.pool);
					p->declEntity=0;
				}
				else
					poolFinish(&p->dtd.pool);
			}
			else
			{
				poolDiscard(&p->dtd.pool);
				p->declEntity=0;
			};
		};

		break;

		case XML_ROLE_PARAM_ENTITY_NAME:
			p->declEntity=0;
			break;

		case XML_ROLE_NOTATION_NAME:
			p->declNotationPublicId=0;
			p->declNotationName=0;

			if (p->notationDeclHandler)
			{
				p->declNotationName=poolStoreString(&p->tempPool, p->encoding, s, next);

				if (!p->declNotationName) return XML_ERROR_NO_MEMORY;
				poolFinish(&p->tempPool);
			};

			break;

		case XML_ROLE_NOTATION_PUBLIC_ID:
			if (!XmlIsPublicId(p->encoding, s, next, &p->eventPtr))
				return XML_ERROR_SYNTAX;

			if (p->declNotationName)
			{
				XML_Char *tem=poolStoreString(&p->tempPool, p->encoding,
							      s+p->encoding->minBytesPerChar,
							      next-p->encoding->minBytesPerChar);

				if (!tem) return XML_ERROR_NO_MEMORY;

				p->normalizePublicId(tem);
				p->declNotationPublicId=tem;
				poolFinish(&p->tempPool);
			};

			break;

		case XML_ROLE_NOTATION_SYSTEM_ID:
			if (p->declNotationName && p->notationDeclHandler)
			{
				const XML_Char *systemId=poolStoreString(&p->tempPool, p->encoding,
							 s+p->encoding->minBytesPerChar,
							 next-p->encoding->minBytesPerChar);

				if (!systemId) return XML_ERROR_NO_MEMORY;

				p->eventPtr=p->eventEndPtr=s;

				p->NotationDeclHandler(p->declNotationName, p->dtd.base, systemId,
						       p->declNotationPublicId);
			};

			poolClear(&p->tempPool);
			break;

		case XML_ROLE_NOTATION_NO_SYSTEM_ID:
			if (p->declNotationPublicId && p->notationDeclHandler)
			{
				p->eventPtr=p->eventEndPtr=s;

				p->NotationDeclHandler(p->declNotationName, p->dtd.base, 0,
						       p->declNotationPublicId);
			};

			poolClear(&p->tempPool);
			break;

		case XML_ROLE_ERROR:
			p->eventPtr=s;

			switch (tok)
			{
			case XML_TOK_PARAM_ENTITY_REF:
				return XML_ERROR_PARAM_ENTITY_REF;

			case XML_TOK_XML_DECL:
				return XML_ERROR_MISPLACED_XML_PI;

			default:
				return XML_ERROR_SYNTAX;
			};

		case XML_ROLE_GROUP_OPEN:
			if (p->prologState.level>=p->groupSize)
			{
				if (p->groupSize)
					p->groupConnector=(char *) realloc(p->groupConnector, p->groupSize*=2);
				else
					p->groupConnector=(char *) malloc(p->groupSize=32);

				if (!p->groupConnector) return XML_ERROR_NO_MEMORY;
			};

			p->groupConnector[p->prologState.level]=0;
			break;

		case XML_ROLE_GROUP_SEQUENCE:
			if (p->groupConnector[p->prologState.level]=='|')
			{
				p->eventPtr=s;
				return XML_ERROR_SYNTAX;
			};

			p->groupConnector[p->prologState.level]=',';
			break;

		case XML_ROLE_GROUP_CHOICE:
			if (p->groupConnector[p->prologState.level]==',')
			{
				p->eventPtr=s;
				return XML_ERROR_SYNTAX;
			};

			p->groupConnector[p->prologState.level]='|';
			break;

		case XML_ROLE_PARAM_ENTITY_REF:
			p->dtd.complete=0;
			break;

		case XML_ROLE_NONE:
			switch (tok)
			{
			case XML_TOK_PI:
				p->eventPtr=s;
				p->eventEndPtr=next;

				if (!p->reportProcessingInstruction(p->encoding, s, next))
					return XML_ERROR_NO_MEMORY;

				break;
			};

			break;
		};

		if (p->defaultHandler)
		{
			switch (tok)
			{
			case XML_TOK_PI:
			case XML_TOK_BOM:
			case XML_TOK_XML_DECL:
				break;

			default:
				p->eventPtr=s;
				p->eventEndPtr=next;
				p->reportDefault(p->encoding, s, next);
			};
		};

		s=next;
	};

	// not reached
}

enum XML_Error epilogProcessor(void *parser, const char *s, const char *end, const char **nextPtr)
{
	XML_Parser *p=(XML_Parser *) parser;

	p->processor=epilogProcessor;
	p->eventPtr=s;

	for (;;)
	{
		const char *next;

		int tok=XmlPrologTok(p->encoding, s, end, &next);
		p->eventEndPtr=next;

		switch (tok) {
		case XML_TOK_TRAILING_CR:
			if (p->defaultHandler)
			{
				p->eventEndPtr=end;
				p->reportDefault(p->encoding, s, end);
			};

			// fall through

		case XML_TOK_NONE:
			if (nextPtr) *nextPtr=end;
			return XML_ERROR_NONE;

		case XML_TOK_PROLOG_S:
		case XML_TOK_COMMENT:
			if (p->defaultHandler) p->reportDefault(p->encoding, s, next);
			break;

		case XML_TOK_PI:
			if (!p->reportProcessingInstruction(p->encoding, s, next))
				return XML_ERROR_NO_MEMORY;

			break;

		case XML_TOK_INVALID:
			p->eventPtr=next;
			return XML_ERROR_INVALID_TOKEN;

		case XML_TOK_PARTIAL:
			if (nextPtr)
			{
				*nextPtr=s;
				return XML_ERROR_NONE;
			};

			return XML_ERROR_UNCLOSED_TOKEN;

		case XML_TOK_PARTIAL_CHAR:
			if (nextPtr)
			{
				*nextPtr=s;
				return XML_ERROR_NONE;
			};

			return XML_ERROR_PARTIAL_CHAR;

		default:
			return XML_ERROR_JUNK_AFTER_DOC_ELEMENT;
		};

		p->eventPtr=s=next;
	};
}

enum XML_Error errorProcessor(void *parser, const char* /*s*/, const char* /*end*/, const char** /*nextPtr*/)
{
	XML_Parser *p=(XML_Parser *) parser;

	return p->errorCode;
}

enum XML_Error XML_Parser::storeAttributeValue(const ENCODING *enc, int isCdata, const char *ptr, const char *end, STRING_POOL *pool)
{
	enum XML_Error result=appendAttributeValue(enc, isCdata, ptr, end, pool);

	if (result) return result;

	if (!isCdata && poolLength(pool) && poolLastChar(pool) == XML_T(' '))
		poolChop(pool);

	if (!poolAppendChar(pool, XML_T('\0'))) return XML_ERROR_NO_MEMORY;

	return XML_ERROR_NONE;
}

enum XML_Error XML_Parser::appendAttributeValue(const ENCODING *enc, int isCdata, const char *ptr, const char *end, STRING_POOL *pool)
{
	const ENCODING *internalEnc=XmlGetInternalEncoding();

	for (;;)
	{
		const char *next;
		int tok=XmlAttributeValueTok(enc, ptr, end, &next);

		switch (tok)
		{
		case XML_TOK_NONE:
			return XML_ERROR_NONE;

		case XML_TOK_INVALID:
			if (enc==encoding) eventPtr = next;
			return XML_ERROR_INVALID_TOKEN;

		case XML_TOK_PARTIAL:
			if (enc==encoding) eventPtr = ptr;
			return XML_ERROR_INVALID_TOKEN;

		case XML_TOK_CHAR_REF:
		{
			XML_Char buf[XML_ENCODE_MAX];
			int i;
			int n=XmlCharRefNumber(enc, ptr);

			if (n<0)
			{
				if (enc==encoding) eventPtr=ptr;
				return XML_ERROR_BAD_CHAR_REF;
			};

			if (!isCdata && n==0x20 && (poolLength(pool)==0 ||
						    poolLastChar(pool)==XML_T(' '))) break;

			n=XmlEncode(n, (ICHAR *) buf);

			if (!n)
			{
				if (enc==encoding) eventPtr = ptr;
				return XML_ERROR_BAD_CHAR_REF;
			};

			for (i=0; i<n; i++)
			{
				if (!poolAppendChar(pool, buf[i])) return XML_ERROR_NO_MEMORY;
			};
		};

		break;

		case XML_TOK_DATA_CHARS:
			if (!poolAppend(pool, enc, ptr, next)) return XML_ERROR_NO_MEMORY;

			break;

		case XML_TOK_TRAILING_CR:
			next=ptr+enc->minBytesPerChar;
			// fall through

		case XML_TOK_ATTRIBUTE_VALUE_S:
		case XML_TOK_DATA_NEWLINE:
			if (!isCdata && (poolLength(pool)==0 || poolLastChar(pool)==XML_T(' ')))
				break;

			if (!poolAppendChar(pool, XML_T(' '))) return XML_ERROR_NO_MEMORY;
			break;

		case XML_TOK_ENTITY_REF:
		{
			const XML_Char *name;
			ENTITY *entity;
			XML_Char ch=XmlPredefinedEntityName(enc,
							    ptr+enc->minBytesPerChar,
							    next-enc->minBytesPerChar);

			if (ch)
			{
				if (!poolAppendChar(pool, ch)) return XML_ERROR_NO_MEMORY;
				break;
			};

			name=poolStoreString(&temp2Pool, enc,
					     ptr+enc->minBytesPerChar,
					     next-enc->minBytesPerChar);

			if (!name) return XML_ERROR_NO_MEMORY;

			entity=(ENTITY *) lookup(&dtd.generalEntities, name, 0);

			poolDiscard(&temp2Pool);

			if (!entity)
			{
				if (dtd.complete)
				{
					if (enc==encoding) eventPtr=ptr;
					return XML_ERROR_UNDEFINED_ENTITY;
				};
			}
			else if (entity->open)
			{
				if (enc==encoding) eventPtr = ptr;
				return XML_ERROR_RECURSIVE_ENTITY_REF;
			}
			else if (entity->notation)
			{
				if (enc==encoding) eventPtr=ptr;
				return XML_ERROR_BINARY_ENTITY_REF;
			}
			else if (!entity->textPtr)
			{
				if (enc==encoding) eventPtr=ptr;
				return XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF;
			}
			else
			{
				enum XML_Error result;
				const XML_Char *textEnd=entity->textPtr+entity->textLen;

				entity->open=1;
				result=appendAttributeValue(internalEnc, isCdata, (char *) entity->textPtr, (char *) textEnd, pool);
				entity->open = 0;

				if (result) return result;
			};
		};
		break;

		default:
			abort();
		};

		ptr=next;
	};

	// not reached
}

enum XML_Error XML_Parser::storeEntityValue(const char *entityTextPtr, const char *entityTextEnd)
{
//  const ENCODING *internalEnc=XmlGetInternalEncoding();
	STRING_POOL *pool=&(dtd.pool);

	entityTextPtr+=encoding->minBytesPerChar;
	entityTextEnd-=encoding->minBytesPerChar;

	for (;;)
	{
		const char *next;

		int tok=XmlEntityValueTok(encoding, entityTextPtr, entityTextEnd, &next);

		switch (tok)
		{
		case XML_TOK_PARAM_ENTITY_REF:
			eventPtr=entityTextPtr;
			return XML_ERROR_SYNTAX;

		case XML_TOK_NONE:
			if (declEntity)
			{
				declEntity->textPtr=pool->start;
				declEntity->textLen=pool->ptr-pool->start;
				poolFinish(pool);
			}
			else
				poolDiscard(pool);

			return XML_ERROR_NONE;

		case XML_TOK_ENTITY_REF:
		case XML_TOK_DATA_CHARS:
			if (!poolAppend(pool, encoding, entityTextPtr, next))
				return XML_ERROR_NO_MEMORY;
			break;

		case XML_TOK_TRAILING_CR:
			next=entityTextPtr+encoding->minBytesPerChar;
			// fall through

		case XML_TOK_DATA_NEWLINE:
			if (pool->end==pool->ptr && !poolGrow(pool))
				return XML_ERROR_NO_MEMORY;

			*(pool->ptr)++=XML_T('\n');
			break;

		case XML_TOK_CHAR_REF:
		{
			XML_Char buf[XML_ENCODE_MAX];
			int i;
			int n=XmlCharRefNumber(encoding, entityTextPtr);

			if (n<0)
			{
				eventPtr=entityTextPtr;
				return XML_ERROR_BAD_CHAR_REF;
			};

			n=XmlEncode(n, (ICHAR *) buf);

			if (!n)
			{
				eventPtr=entityTextPtr;
				return XML_ERROR_BAD_CHAR_REF;
			};

			for (i=0; i<n; i++)
			{
				if (pool->end==pool->ptr && !poolGrow(pool))
					return XML_ERROR_NO_MEMORY;

				*(pool->ptr)++=buf[i];
			};
		};

		break;

		case XML_TOK_PARTIAL:
			eventPtr=entityTextPtr;
			return XML_ERROR_INVALID_TOKEN;

		case XML_TOK_INVALID:
			eventPtr=next;
			return XML_ERROR_INVALID_TOKEN;

		default:
			abort();
		};

		entityTextPtr=next;
	};

	// not reached
}

static void normalizeLines(XML_Char *s)
{
	XML_Char *p;

	for (;; s++)
	{
		if (*s==XML_T('\0')) return;
		if (*s==XML_T('\r')) break;
	};

	p=s;

	do
	{
		if (*s==XML_T('\r'))
		{
			*p++=XML_T('\n');
			if (*++s==XML_T('\n')) s++;
		}
		else
			*p++ = *s++;
	} while (*s);

	*p=XML_T('\0');
}

int XML_Parser::reportProcessingInstruction(const ENCODING *enc, const char *start, const char *end)
{
	const XML_Char *target;
	XML_Char *data;
	const char *tem;

	if (!processingInstructionHandler)
	{
		if (defaultHandler) reportDefault(enc, start, end);
		return 1;
	};

	start+=enc->minBytesPerChar*2;
	tem=start+XmlNameLength(enc, start);

	target=poolStoreString(&tempPool, enc, start, tem);
	if (!target) return 0;

	poolFinish(&tempPool);

	data=poolStoreString(&tempPool, enc, XmlSkipS(enc, tem), end-enc->minBytesPerChar*2);
	if (!data) return 0;

	normalizeLines(data);

	ProcessingInstructionHandler(target, data);

	poolClear(&tempPool);

	return 1;
}

void XML_Parser::reportDefault(const ENCODING *enc, const char *s, const char *end)
{
	if (MUST_CONVERT(enc, s))
	{
		for (;;)
		{
			ICHAR *dataPtr=(ICHAR *) dataBuf;
			XmlConvert(enc, &s, end, &dataPtr, (ICHAR *) dataBufEnd);

			if (s==end)
			{
				DefaultHandler(dataBuf, dataPtr-(ICHAR *) dataBuf);
				break;
			};

			if (enc==encoding)
			{
				eventEndPtr=s;
				DefaultHandler(dataBuf, dataPtr-(ICHAR *) dataBuf);
				eventPtr=s;
			}
			else
				DefaultHandler(dataBuf, dataPtr-(ICHAR *) dataBuf);
		};
	}
	else
		DefaultHandler((XML_Char *) s, (XML_Char *) end-(XML_Char *) s);
}

int XML_Parser::defineAttribute(ELEMENT_TYPE *type, ATTRIBUTE_ID *attId, int isCdata, const XML_Char *value)
{
	DEFAULT_ATTRIBUTE *att;

	if (type->nDefaultAtts==type->allocDefaultAtts)
	{
		if (type->allocDefaultAtts==0)
		{
			type->allocDefaultAtts=8;
			type->defaultAtts=new DEFAULT_ATTRIBUTE[type->allocDefaultAtts];
		}
		else
		{
			type->allocDefaultAtts*=2;
			type->defaultAtts=(DEFAULT_ATTRIBUTE *) realloc(type->defaultAtts, type->allocDefaultAtts*sizeof(DEFAULT_ATTRIBUTE));
		};

		if (!type->defaultAtts) return 0;
	};

	att=type->defaultAtts+type->nDefaultAtts;

	att->id=attId;
	att->value=value;
	att->isCdata=isCdata;

	if (!isCdata) attId->maybeTokenized=1;

	type->nDefaultAtts+=1;

	return 1;
}

ATTRIBUTE_ID *XML_Parser::getAttributeId(const ENCODING *enc, const char *start, const char *end)
{
	ATTRIBUTE_ID *id;
	const XML_Char *name;

	if (!poolAppendChar(&dtd.pool, XML_T('\0'))) return 0;

	name=poolStoreString(&dtd.pool, enc, start, end);
	if (!name) return 0;
	++name;

	id=(ATTRIBUTE_ID *) lookup(&dtd.attributeIds, name, sizeof(ATTRIBUTE_ID));
	if (!id) return 0;

	if (id->name!=name)
		poolDiscard(&dtd.pool);
	else
		poolFinish(&dtd.pool);

	return id;
}

const XML_Char *XML_Parser::getOpenEntityNames()
{
	HASH_TABLE_ITER iter;

	hashTableIterInit(&iter, &(dtd.generalEntities));

	for (;;)
	{
		const XML_Char *s;
		ENTITY *e=(ENTITY *) hashTableIterNext(&iter);

		if (!e) break;
		if (!e->open) continue;

		if (poolLength(&tempPool)>0 && !poolAppendChar(&tempPool, XML_T(' ')))
			return 0;

		for (s = e->name; *s; s++)
			if (!poolAppendChar(&tempPool, *s))
				return 0;
	};

	if (!poolAppendChar(&tempPool, XML_T('\0'))) return 0;

	return tempPool.start;
}

int XML_Parser::setOpenEntityNames(const XML_Char *openEntityNames)
{
	const XML_Char *s=openEntityNames;

	while (*openEntityNames!=XML_T('\0'))
	{
		if (*s==XML_T(' ') || *s==XML_T('\0'))
		{
			ENTITY *e;

			if (!poolAppendChar(&tempPool, XML_T('\0'))) return 0;

			e=(ENTITY *)lookup(&dtd.generalEntities, poolStart(&tempPool), 0);

			if (e) e->open = 1;
			if (*s == XML_T(' ')) s++;

			openEntityNames=s;
			poolDiscard(&tempPool);
		}
		else
		{
			if (!poolAppendChar(&tempPool, *s)) return 0;
			s++;
		};
	};

	return 1;
}


void XML_Parser::normalizePublicId(XML_Char *publicId)
{
	XML_Char *p=publicId;
	XML_Char *s;

	for (s=publicId; *s; s++)
	{
		switch (*s)
		{
		case XML_T(' '):
		case XML_T('\r'):
		case XML_T('\n'):
			if (p!=publicId && p[-1]!=XML_T(' ')) *p++=XML_T(' ');
			break;

		default:
			*p++=*s;
		};
	};

	if (p!=publicId && p[-1]==XML_T(' ')) --p;
	*p=XML_T('\0');
}

static int dtdInit(DTD *p)
{
	poolInit(&(p->pool));

	hashTableInit(&(p->generalEntities));
	hashTableInit(&(p->elementTypes));
	hashTableInit(&(p->attributeIds));

	p->complete=1;
	p->standalone=0;
	p->base=0;

	return 1;
}

static void dtdDestroy(DTD *p)
{
	HASH_TABLE_ITER iter;
	hashTableIterInit(&iter, &(p->elementTypes));

	for (;;)
	{
		ELEMENT_TYPE *e=(ELEMENT_TYPE *) hashTableIterNext(&iter);

		if (!e) break;
		if (e->allocDefaultAtts!=0) free(e->defaultAtts);
	};

	hashTableDestroy(&(p->generalEntities));
	hashTableDestroy(&(p->elementTypes));
	hashTableDestroy(&(p->attributeIds));

	poolDestroy(&(p->pool));
}

/* Do a deep copy of the DTD.  Return 0 for out of memory; non-zero otherwise.
   The new DTD has already been initialized. */

static int dtdCopy(DTD *newDtd, const DTD *oldDtd)
{
	HASH_TABLE_ITER iter;

	if (oldDtd->base)
	{
		const XML_Char *tem=poolCopyString(&(newDtd->pool), oldDtd->base);
		if (!tem) return 0;
		newDtd->base=tem;
	};

	hashTableIterInit(&iter, &(oldDtd->attributeIds));

	// Copy the attribute id table.

	for (;;)
	{
		ATTRIBUTE_ID *newA;
		const XML_Char *name;
		const ATTRIBUTE_ID *oldA=(ATTRIBUTE_ID *) hashTableIterNext(&iter);

		if (!oldA) break;

		// Remember to allocate the scratch byte before the name.

		if (!poolAppendChar(&(newDtd->pool), XML_T('\0'))) return 0;

		name=poolCopyString(&(newDtd->pool), oldA->name);

		if (!name) return 0;
		++name;

		newA=(ATTRIBUTE_ID *) lookup(&(newDtd->attributeIds), name, sizeof(ATTRIBUTE_ID));

		if (!newA) return 0;
		newA->maybeTokenized=oldA->maybeTokenized;
	}

	// Copy the element type table.

	hashTableIterInit(&iter, &(oldDtd->elementTypes));

	for (;;)
	{
		int i;
		ELEMENT_TYPE *newE;
		const XML_Char *name;
		const ELEMENT_TYPE *oldE=(ELEMENT_TYPE *)hashTableIterNext(&iter);

		if (!oldE) break;
		name=poolCopyString(&(newDtd->pool), oldE->name);
		if (!name) return 0;

		newE=(ELEMENT_TYPE *) lookup(&(newDtd->elementTypes), name, sizeof(ELEMENT_TYPE));
		if (!newE) return 0;

		newE->defaultAtts=new DEFAULT_ATTRIBUTE[oldE->nDefaultAtts];
		if (!newE->defaultAtts) return 0;

		newE->allocDefaultAtts=newE->nDefaultAtts=oldE->nDefaultAtts;

		for (i=0; i<newE->nDefaultAtts; i++)
		{
			newE->defaultAtts[i].id=(ATTRIBUTE_ID *) lookup(&(newDtd->attributeIds), oldE->defaultAtts[i].id->name, 0);
			newE->defaultAtts[i].isCdata=oldE->defaultAtts[i].isCdata;

			if (oldE->defaultAtts[i].value)
			{
				newE->defaultAtts[i].value=poolCopyString(&(newDtd->pool), oldE->defaultAtts[i].value);
				if (!newE->defaultAtts[i].value) return 0;
			}
			else
				newE->defaultAtts[i].value=0;
		};
	};

	// Copy the entity table.

	hashTableIterInit(&iter, &(oldDtd->generalEntities));

	for (;;)
	{
		ENTITY *newE;
		const XML_Char *name;
		const ENTITY *oldE=(ENTITY *)hashTableIterNext(&iter);

		if (!oldE) break;

		name=poolCopyString(&(newDtd->pool), oldE->name);
		if (!name) return 0;

		newE=(ENTITY *) lookup(&(newDtd->generalEntities), name, sizeof(ENTITY));
		if (!newE) return 0;

		if (oldE->systemId)
		{
			const XML_Char *tem=poolCopyString(&(newDtd->pool), oldE->systemId);
			if (!tem) return 0;
			newE->systemId = tem;

			if (oldE->base)
			{
				if (oldE->base==oldDtd->base) newE->base=newDtd->base;

				tem=poolCopyString(&(newDtd->pool), oldE->base);
				if (!tem) return 0;

				newE->base=tem;
			};
		}
		else
		{
			const XML_Char *tem=poolCopyStringN(&(newDtd->pool), oldE->textPtr, oldE->textLen);
			if (!tem)	return 0;
			newE->textPtr=tem;
			newE->textLen=oldE->textLen;
		};

		if (oldE->notation)
		{
			const XML_Char *tem=poolCopyString(&(newDtd->pool), oldE->notation);
			if (!tem)	return 0;
			newE->notation=tem;
		};
	};

	newDtd->complete=oldDtd->complete;
	newDtd->standalone=oldDtd->standalone;

	return 1;
}

static void poolInit(STRING_POOL *pool)
{
	pool->blocks=0;
	pool->freeBlocks=0;
	pool->start=0;
	pool->ptr=0;
	pool->end=0;
}

static void poolClear(STRING_POOL *pool)
{
	if (!pool->freeBlocks)
		pool->freeBlocks = pool->blocks;
	else
	{
		BLOCK *p=pool->blocks;
		while (p)
		{
			BLOCK *tem=p->next;
			p->next=pool->freeBlocks;
			pool->freeBlocks=p;
			p=tem;
		};
	};

	pool->blocks=0;
	pool->start=0;
	pool->ptr=0;
	pool->end=0;
}

void poolDestroy(STRING_POOL *pool)
{
	BLOCK *p=pool->blocks;

	while (p)
	{
		BLOCK *tem=p->next;
		free(p);
		p=tem;
	};

	pool->blocks=0;
	p=pool->freeBlocks;

	while (p)
	{
		BLOCK *tem=p->next;
		free(p);
		p=tem;
	};

	pool->freeBlocks=0;
	pool->ptr=0;
	pool->start=0;
	pool->end=0;
}

static XML_Char *poolAppend(STRING_POOL *pool, const ENCODING *enc, const char *ptr, const char *end)
{
	if (!pool->ptr && !poolGrow(pool)) return 0;

	for (;;)
	{
		XmlConvert(enc, &ptr, end, (ICHAR **) &(pool->ptr), (ICHAR *) pool->end);

		if (ptr == end) break;
		if (!poolGrow(pool)) return 0;
	};

	return pool->start;
}

static const XML_Char *poolCopyString(STRING_POOL *pool, const XML_Char *s)
{
	do
	{
		if (!poolAppendChar(pool, *s)) return 0;
	} while (*s++);

	s=pool->start;
	poolFinish(pool);

	return s;
}

static const XML_Char *poolCopyStringN(STRING_POOL *pool, const XML_Char *s, int n)
{
	if (!pool->ptr && !poolGrow(pool)) return 0;

	for (; n > 0; --n, s++)
	{
		if (!poolAppendChar(pool, *s)) return 0;
	};

	s=pool->start;
	poolFinish(pool);
	return s;
}

static XML_Char *poolStoreString(STRING_POOL *pool, const ENCODING *enc, const char *ptr, const char *end)
{
	if (!poolAppend(pool, enc, ptr, end)) return 0;

	if (pool->ptr==pool->end && !poolGrow(pool)) return 0;
	*(pool->ptr)++=0;

	return pool->start;
}

static int poolGrow(STRING_POOL *pool)
{
	if (pool->freeBlocks)
	{
		if (pool->start==0)
		{
			pool->blocks=pool->freeBlocks;
			pool->freeBlocks=pool->freeBlocks->next;
			pool->blocks->next=0;
			pool->start=pool->blocks->s;
			pool->end=pool->start+pool->blocks->size;
			pool->ptr=pool->start;

			return 1;
		};

		if (pool->end-pool->start<pool->freeBlocks->size)
		{
			BLOCK *tem=pool->freeBlocks->next;

			pool->freeBlocks->next=pool->blocks;
			pool->blocks=pool->freeBlocks;
			pool->freeBlocks=tem;

			memmove(pool->blocks->s, pool->start, (pool->end-pool->start)*sizeof(XML_Char));

			pool->ptr=pool->blocks->s+(pool->ptr-pool->start);
			pool->start=pool->blocks->s;
			pool->end=pool->start+pool->blocks->size;

			return 1;
		};
	};

	if (pool->blocks && pool->start==pool->blocks->s)
	{
		int blockSize=(pool->end-pool->start)*2;

		pool->blocks=(BLOCK *) realloc(pool->blocks, offsetof(BLOCK, s)+blockSize*sizeof(XML_Char));

		if (!pool->blocks) return 0;

		pool->blocks->size=blockSize;
		pool->ptr=pool->blocks->s+(pool->ptr-pool->start);
		pool->start=pool->blocks->s;
		pool->end=pool->start+blockSize;
	}
	else
	{
		BLOCK *tem;

		int blockSize=pool->end-pool->start;

		if (blockSize<INIT_BLOCK_SIZE)
			blockSize=INIT_BLOCK_SIZE;
		else
			blockSize*=2;

		tem=(BLOCK *) malloc(offsetof(BLOCK, s)+blockSize*sizeof(XML_Char));
		if (!tem) return 0;

		tem->size=blockSize;
		tem->next=pool->blocks;

		pool->blocks=tem;

		memmove(tem->s, pool->start, (pool->ptr-pool->start)*sizeof(XML_Char));

		pool->ptr=tem->s+(pool->ptr-pool->start);
		pool->start=tem->s;
		pool->end=tem->s+blockSize;
	};

	return 1;
}
