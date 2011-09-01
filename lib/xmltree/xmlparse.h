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

  $Log: xmlparse.h,v $
  Revision 1.2  2003/03/14 05:13:04  obi
  compileable with -W -Werror

  Revision 1.1  2002/01/18 20:22:39  tmbinc
  initial checkin

  Revision 1.1.1.1  2001/10/07 13:01:16  tmbinc
  Import of ezap2-200110070

  Revision 1.2  1999/01/24 16:00:05  cvs
  First release of the XMLTree API (ryg)

  Revision 1.1  1999/01/11 23:01:58  cvs
  Another try to commit this.

*/

#ifndef XmlParse_INCLUDED
#define XmlParse_INCLUDED 1

#ifndef XMLPARSEAPI
#define XMLPARSEAPI /* as nothing */
#endif

#ifdef XML_UNICODE_WCHAR_T

/* XML_UNICODE_WCHAR_T will work only if sizeof(wchar_t) == 2 and wchar_t
uses Unicode. */
/* Information is UTF-16 encoded as wchar_ts */

#ifndef XML_UNICODE
#define XML_UNICODE
#endif

#include <stddef.h>
typedef wchar_t XML_Char;
typedef wchar_t XML_LChar;

#else /* not XML_UNICODE_WCHAR_T */

#ifdef XML_UNICODE

/* Information is UTF-16 encoded as unsigned shorts */
typedef unsigned short XML_Char;
typedef char XML_LChar;

#else /* not XML_UNICODE */

/* Information is UTF-8 encoded. */
typedef char XML_Char;
typedef char XML_LChar;

#endif /* not XML_UNICODE */

#endif /* not XML_UNICODE_WCHAR_T */

#include "xmltok.h"
#include "xmlrole.h"
#include "hashtab.h"

typedef struct {
  int map[256];
  void *data;
  int (*convert)(void *data, const char *s);
  void (*release)(void *data);
} XML_Encoding;

enum XML_Error {
  XML_ERROR_NONE,
  XML_ERROR_NO_MEMORY,
  XML_ERROR_SYNTAX,
  XML_ERROR_NO_ELEMENTS,
  XML_ERROR_INVALID_TOKEN,
  XML_ERROR_UNCLOSED_TOKEN,
  XML_ERROR_PARTIAL_CHAR,
  XML_ERROR_TAG_MISMATCH,
  XML_ERROR_DUPLICATE_ATTRIBUTE,
  XML_ERROR_JUNK_AFTER_DOC_ELEMENT,
  XML_ERROR_PARAM_ENTITY_REF,
  XML_ERROR_UNDEFINED_ENTITY,
  XML_ERROR_RECURSIVE_ENTITY_REF,
  XML_ERROR_ASYNC_ENTITY,
  XML_ERROR_BAD_CHAR_REF,
  XML_ERROR_BINARY_ENTITY_REF,
  XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF,
  XML_ERROR_MISPLACED_XML_PI,
  XML_ERROR_UNKNOWN_ENCODING,
  XML_ERROR_INCORRECT_ENCODING,
  XML_ERROR_UNCLOSED_CDATA_SECTION,
  XML_ERROR_EXTERNAL_ENTITY_HANDLING
};

#define INIT_TAG_BUF_SIZE 32  /* must be a multiple of sizeof(XML_Char) */
#define INIT_DATA_BUF_SIZE 1024
#define INIT_ATTS_SIZE 16
#define INIT_BLOCK_SIZE 1024
#define INIT_BUFFER_SIZE 1024

typedef struct xml_tag {
  struct xml_tag *parent;
  const char *rawName;
  int rawNameLength;
  const XML_Char *name;
  char *buf;
  char *bufEnd;
} TAG;

typedef struct {
  const XML_Char *name;
  const XML_Char *textPtr;
  int textLen;
  const XML_Char *systemId;
  const XML_Char *base;
  const XML_Char *publicId;
  const XML_Char *notation;
  char open;
} ENTITY;

typedef struct block {
  struct block *next;
  int size;
  XML_Char s[1];
} BLOCK;

typedef struct {
  BLOCK *blocks;
  BLOCK *freeBlocks;
  const XML_Char *end;
  XML_Char *ptr;
  XML_Char *start;
} STRING_POOL;

/* The XML_Char before the name is used to determine whether
an attribute has been specified. */
typedef struct {
  XML_Char *name;
  char maybeTokenized;
} ATTRIBUTE_ID;

typedef struct {
  const ATTRIBUTE_ID *id;
  char isCdata;
  const XML_Char *value;
} DEFAULT_ATTRIBUTE;

typedef struct {
  const XML_Char *name;
  int nDefaultAtts;
  int allocDefaultAtts;
  DEFAULT_ATTRIBUTE *defaultAtts;
} ELEMENT_TYPE;

typedef struct {
  HASH_TABLE generalEntities;
  HASH_TABLE elementTypes;
  HASH_TABLE attributeIds;
  STRING_POOL pool;
  int complete;
  int standalone;
  const XML_Char *base;
} DTD;

typedef enum XML_Error Processor(void *parser,
				 const char *start,
				 const char *end,
				 const char **endPtr);

extern Processor prologProcessor;
extern Processor prologInitProcessor;
extern Processor contentProcessor;
extern Processor cdataSectionProcessor;
extern Processor epilogProcessor;
extern Processor errorProcessor;
extern Processor externalEntityInitProcessor;
extern Processor externalEntityInitProcessor2;
extern Processor externalEntityInitProcessor3;
extern Processor externalEntityContentProcessor;


class XML_Parser
{
  friend Processor prologProcessor;
  friend Processor prologInitProcessor;
  friend Processor contentProcessor;
  friend Processor cdataSectionProcessor;
  friend Processor epilogProcessor;
  friend Processor errorProcessor;
  friend Processor externalEntityInitProcessor;
  friend Processor externalEntityInitProcessor2;
  friend Processor externalEntityInitProcessor3;
  friend Processor externalEntityContentProcessor;

  protected:
    char *buffer;
    /* first character to be parsed */
    const char *bufferPtr;
    /* past last character to be parsed */
    char *bufferEnd;
    /* allocated end of buffer */
    const char *bufferLim;
    long parseEndByteIndex;
    const char *parseEndPtr;
    XML_Char *dataBuf;
    XML_Char *dataBufEnd;
    const ENCODING *encoding;
    INIT_ENCODING initEncoding;
    const XML_Char *protocolEncodingName;
    void *unknownEncodingMem;
    void *unknownEncodingData;
    void *unknownEncodingHandlerData;
    void (*unknownEncodingRelease)(void *);
    PROLOG_STATE prologState;
    Processor *processor;
    enum XML_Error errorCode;
    const char *eventPtr;
    const char *eventEndPtr;
    const char *positionPtr;
    int tagLevel;
    ENTITY *declEntity;
    const XML_Char *declNotationName;
    const XML_Char *declNotationPublicId;
    ELEMENT_TYPE *declElementType;
    ATTRIBUTE_ID *declAttributeId;
    char declAttributeIsCdata;
    DTD dtd;
    TAG *tagStack;
    TAG *freeTagList;
    int attsSize;
    ATTRIBUTE *atts;
    POSITION position;
    STRING_POOL tempPool;
    STRING_POOL temp2Pool;
    char *groupConnector;
    unsigned groupSize;
    int hadExternalDoctype;

    int startElementHandler, endElementHandler, characterDataHandler;
    int processingInstructionHandler, defaultHandler;
    int unparsedEntityDeclHandler, notationDeclHandler;
    int externalEntityRefHandler, unknownEncodingHandler;

    virtual enum XML_Error handleUnknownEncoding(const XML_Char *encodingName);
    virtual enum XML_Error processXmlDecl(int isGeneralTextEntity, const char *, const char *);
    virtual enum XML_Error initializeEncoding();
    virtual enum XML_Error doContent(int startTagLevel, const ENCODING *enc, const char *start, const char *end, const char **endPtr);
    virtual enum XML_Error doCdataSection(const ENCODING *, const char **startPtr, const char *end, const char **nextPtr);
    virtual enum XML_Error storeAtts(const ENCODING *, const XML_Char *tagName, const char *s);
    virtual int defineAttribute(ELEMENT_TYPE *type, ATTRIBUTE_ID *, int isCdata, const XML_Char *dfltValue);
    virtual enum XML_Error storeAttributeValue(const ENCODING *, int isCdata, const char *, const char *, STRING_POOL *);
    virtual enum XML_Error appendAttributeValue(const ENCODING *, int isCdata, const char *, const char *, STRING_POOL *);
    virtual ATTRIBUTE_ID *getAttributeId(const ENCODING *enc, const char *start, const char *end);
    virtual enum XML_Error storeEntityValue(const char *start, const char *end);
    virtual int reportProcessingInstruction(const ENCODING *enc, const char *start, const char *end);
    virtual void reportDefault(const ENCODING *enc, const char *start, const char *end);

    virtual const XML_Char *getOpenEntityNames();
    virtual int setOpenEntityNames(const XML_Char *openEntityNames);
    virtual void normalizePublicId(XML_Char *s);

    virtual void StartElementHandler(const XML_Char* /*name*/, const XML_Char** /*atts*/) {};
    virtual void EndElementHandler(const XML_Char* /*name*/) {};
    virtual void CharacterDataHandler(const XML_Char* /*s*/, int /*len*/) {};
    virtual void ProcessingInstructionHandler(const XML_Char* /*target*/, const XML_Char* /*data*/) {};
    virtual void DefaultHandler(const XML_Char* /*s*/, int /*len*/) {};
    virtual void UnparsedEntityDeclHandler(const XML_Char* /*entityName*/, const XML_Char* /*base*/, const XML_Char* /*systemId*/, const XML_Char* /*publicId*/, const XML_Char* /*notationName*/) {};
    virtual void NotationDeclHandler(const XML_Char* /*notationName*/, const XML_Char* /*base*/, const XML_Char* /*systemId*/, const XML_Char* /*publicId*/) {};
    virtual int ExternalEntityRefHandler(const XML_Char* /*openEntityNames*/, const XML_Char* /*base*/, const XML_Char* /*systemId*/, const XML_Char* /*publicId*/) {return 0;};
    virtual int UnknownEncodingHandler(void* /*encodingHandlerData*/, const XML_Char* /*name*/, XML_Encoding* /*info*/) {return 0;};

    virtual void DefaultCurrent();

  public:
    XML_Parser(const XML_Char *encoding);
    virtual ~XML_Parser();

    virtual int SetBase(const XML_Char *base);
    virtual const XML_Char *GetBase();

    virtual int Parse(const char *s, int len, int isFinal);
    virtual void *GetBuffer(int len);
    virtual int ParseBuffer(int len, int isFinal);
    virtual XML_Parser *ExternalEntityParserCreate(const XML_Char *openEntityNames, const XML_Char *encoding);

    virtual enum XML_Error GetErrorCode();

    virtual int GetCurrentLineNumber();
    virtual int GetCurrentColumnNumber();
    virtual long GetCurrentByteIndex();

    virtual const XML_LChar *ErrorString(int code);
};

#endif /* not XmlParse_INCLUDED */
