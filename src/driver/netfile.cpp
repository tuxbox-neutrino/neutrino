/******************************************************************************\
|	Neutrino-GUI  -   DBoxII-Project
|
|	Copyright (C) 2004 by Sanaia <sanaia at freenet dot de>
|	Copyright (C) 2010-2013, 2017 Stefan Seyfried
|
|	netfile - remote file access mapper
|
|
| description:
|	netfile - a URL capable wrapper for the fopen() call
|
| usage:
|	include 'netfile.h' in your code as *LAST* file after all other
|	include files and add netfile.c to your sources. That's it. The
|	include file maps the common fopen() call onto a URL capable
|	version that handles files hosted on http servers like local ones.
|
|
| examples:
|
| automatic redirection example:
|
|	fd = fopen("remotefile.url", "r");
|
|	This is a pretty straight forward implementation that should
|	even work with applications which refuse to accept filenames
|	starting with protocol headers (i.e. 'http://'). If the
|	given file ends with '.url', then the file is opened and read
|	and its content is assumed to be a valid URL which opened then.
|	This works for all protocols provided by this code, e.g. 'http://',
|	'icy://' and 'scast://'
|	All restrictions apply for the protocols themself as follows.
|
|
| http example:
|
|	fd = fopen("http://find.me:666/somewhere/foo.bar", "r");
|
|	This opens the specified file on a webserver (read only).
|	NOTE: the stream itself is bidirectional, you can write
|	into it (e.g. commands for communication with the server),
|	but this is neither supported nor recommended. The result
|	of such an action is rather undefined. You *CAN NOT* make
|	any changes to the file of the webserver !
|
|
| shoutcast example:
|
|	fd = fopen("scast://666", "r");
|
|	This opens a shoutcast sation; all you need to know is the
|	station number. The query of the shoutcast directory and the
|	lookup of a working server is done automatically. The stream
|	is opened read-only.
|	I recommend this for official shoutcast stations.
|
| shoutcast example:
|
|	fd = fopen("icy://find.me:666/funky/station/", "r");
|       or
|       fd = fopen("icy://username:password@find.me:666/station", "r");
|
|	This is a low level mechanism that can be used for all
|	shoutcast servers, but it mainly is intended to be used for
|	private radio stations which are not listed in the official
|	shoutcast directory. The stream is opened read-only.
|       The second format contains a basic authentication string before '@'
|       of username:passwort to be sent to the server.
|
| file access modes, selectable by the fopen 'access type':
|
|	"r"	read only, stream caching enabled
|	"rc"	read only, stream caching disabled (compatibility mode)
|
| NOTE: All network accesses are only possible read only. Although some
|       protocols could allow write access to the remote resource, this
|       is not (and rather unlikely to be anytime) implemented here.
|       All remote accesses are made through a FIFO caching mechanism that
|       uses read-ahead caching (as far as possible). The fopen() call starts
|       a background fetching thread that fills the cache up to the ceiling.
|
|
|	License: GPL
|
|	This program is free software; you can redistribute it and/or modify
|	it under the terms of the GNU General Public License as published by
|	the Free Software Foundation; either version 2 of the License, or
|	(at your option) any later version.
|
|	This program is distributed in the hope that it will be useful,
|	but WITHOUT ANY WARRANTY; without even the implied warranty of
|	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
|	GNU General Public License for more details.
|
|	You should have received a copy of the GNU General Public License
|	along with this program; if not, write to the Free Software
|	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
|
\******************************************************************************/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netfile.h"
#include <global.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <driver/audioplay.h>
#include <system/set_threadname.h>
/*
TODO:
	- ICECAST support
	- follow redirection errors (server error codes 302, 301) (done for HTTP, 2005-05-16 ChakaZulu)
	- support for automatic playlist processing (shoutcast pls files)

known bugs:
	- HTTP POST requests - are implemented, but don't work with shoutcast.com !?
*/

#define STATIC /**/

#ifdef fopen
	#undef fopen
#endif
#ifdef fclose
	#undef fclose
#endif
#ifdef fread
	#undef fread
#endif
#ifdef ftell
	#undef ftell
#endif
#ifdef rewind
	#undef rewind
#endif
#ifdef fseek
	#undef fseek
#endif

/* somewhat safe, 0-terminated sprintf and strcpy */
#define SPRINTF(a, b...) snprintf(a, sizeof(a)-1, b)
#define STRCPY(a, b)     { strncpy(a, b, sizeof(a)-1);     a[sizeof(a)-1] = 0; }



typedef struct
{
	const unsigned char mask[4];
	const unsigned char mode[4];
	const char *type;
} magic_t;

magic_t known_magic[] =
{
	{{0xFF, 0xFF, 0xFF, 0x00}, {'I' , 'D' , '3' , 0x00}, "audio/mpeg"},
	{{0xFF, 0xFF, 0xFF, 0x00}, {'O' , 'g' , 'g' , 0x00}, "audio/ogg" },
	{{0xFF, 0xFE, 0x00, 0x00}, {0xFF, 0xFA, 0x00, 0x00}, "audio/mpeg"},
	{{0xFF, 0xFF, 0xFF, 0x00}, {'F' , 'L' , 'V' , 0x00}, "audio/flv"}
};

#if 0
#warning if a magic is contained in a file (for instance in .cdr) there is no way to play it correctly with neutrino
#warning the third magic is pretty short - hence disabled by default
/* 1111 1111 1111 1010 0000 0000 0000 0000
   AAAA AAAA AAAB BCC
   where A: frame sync
   B: MPEG audio ID (11 = MPEG Version 1)
   C: Layer description (01 = Layer III)
   see http://mpgedit.org/mpgedit/mpeg_format/mpeghdr.htm */

#define known_magic_count (sizeof(known_magic) / sizeof(magic_t))
#else
#define known_magic_count 2
#endif

#define is_redirect(a) ((a == 301) || (a == 302))

static int	meta_interval; /*chunked mode */
static bool	chunked; /*chunked mode */
char err_txt[2048];			/* human readable error message */
char redirect_url[2048];		/* new url if we've been redirected (HTTP 301/302) */
static int debug = 0;			/* print debugging output or not */
static char logfile[256];		/* redirect errors from stderr */
static int retry_num = 2 /*10*/;		/* number of retries for failed connections */
static int enable_metadata = 0;	/* allow shoutcast meta data streaming */
static int got_opts = 0;		/* is set to 1 if getOpts() was executed */
static int cache_size = 196608;	/* default cache size; can be overridden at */
						/* runtime with an option in the options file */

STATIC STREAM_CACHE cache[CACHEENTMAX];
STATIC STREAM_TYPE stream_type[CACHEENTMAX];

static int  ConnectToServer(char *hostname, int port);
static int  parse_response(URL *url, void *, CSTATE*);
static int  request_file(URL *url);
//static void readln(int, char *);
static int  getCacheSlot(FILE *fd);
static int  push(FILE *fd, char *buf, long len);
static int  pop(FILE *fd, char *buf, long len);
static void CacheFillThread(void *url);
static void ShoutCAST_MetaFilter(STREAM_FILTER *);
static void ShoutCAST_DestroyFilter(void *a);
static STREAM_FILTER *ShoutCAST_InitFilter(int);
static void ShoutCAST_ParseMetaData(char *, CSTATE *);

static void getOpts(void);

static void parseURL_url(URL& url);

static int  base64_encode(char *dest, const char *src);

/***************************************/
/* this is a simple options parser     */

void getOpts()
{
	const char *dirs[] = { "/var/etc", ".", NULL };
	char buf[4096], *ptr;
	int i;
	FILE *fd = NULL;

#ifndef RADIOBOX //a not generic thing in a generic library
	/* options which can be set from within neutrino */
	enable_metadata = g_settings.audioplayer_enable_sc_metadata;
#endif /* RADIOBOX */

	if (got_opts) /* prevent reading in options multiple times */
		return;
	else
		got_opts = 1;

	for(i=0; dirs[i] != NULL; i++)
	{
		sprintf(buf, "%s/.netfile", dirs[i]);
		fd = fopen(buf, "r");
		if(fd) break;
	}

	if(!fd)
		return;
	fread(buf, sizeof(char), 4095, fd);
	fclose(fd);

	if(strstr(buf, "debug=1"))
		debug = 1;

	if((ptr = strstr(buf, "cachesize=")))
		cache_size = atoi(strchr(ptr, '=') + 1);

	if((ptr = strstr(buf, "retries=")))
		retry_num = atoi(strchr(ptr, '=') + 1);

	if((ptr = strstr(buf, "logfile=")))
	{
		STRCPY(logfile, strchr(ptr, '=') + 1);
		CRLFCut(logfile);
		freopen(logfile, "w", stderr);
	}
}

/***************************************/
/* networking functions                */

int ConnectToServer(char *hostname, int port)
{
	struct hostent *host;
	struct sockaddr_in sock;
	int fd, addr;
	struct pollfd pfd;

	dprintf(stderr, "looking up hostname: %s\n", hostname);

	host = gethostbyname(hostname);

	if(host == NULL)
	{
		herror(err_txt);
		return -1;
	}

	addr = htonl(*(int *)host->h_addr);

	dprintf(stderr, "connecting to %s [%d.%d.%d.%d], port %d\n", host->h_name,
			(addr & 0xff000000) >> 24,
			(addr & 0x00ff0000) >> 16,
			(addr & 0x0000ff00) >>  8,
			(addr & 0x000000ff), port);

	fd = socket(AF_INET, SOCK_STREAM, 0);

	if(fd == -1)
	{
		STRCPY(err_txt, strerror(errno));
		return -1;
	}

	memset(&sock, 0, sizeof(sock));
	memmove((char*)&sock.sin_addr, host->h_addr, host->h_length);

	sock.sin_family = AF_INET;
	sock.sin_port = htons(port);

	int flgs = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flgs | O_NONBLOCK);

	if( connect(fd, (struct sockaddr *)&sock, sizeof(sock)) == -1 )
	{
		if(errno != EINPROGRESS) {
			close(fd);
			STRCPY(err_txt, strerror(errno));
			dprintf(stderr, "error connecting to %s: %s\n", hostname, err_txt);
			return -1;
		}
	}
	dprintf(stderr, "connected to %s\n", hostname);
	pfd.fd = fd;
	//pfd.events = POLLIN | POLLPRI;
	pfd.events = POLLOUT | POLLERR | POLLHUP;
	pfd.revents = 0;

	int ret = poll(&pfd, 1, 5000);
	if(ret != 1) {
		STRCPY(err_txt, strerror(errno));
		dprintf(stderr, "error connecting to %s: %s\n", hostname, err_txt);
		close(fd);
		return -1;
	}
	if ((pfd.revents & POLLOUT) == POLLOUT) {
		fcntl(fd, F_SETFL, flgs &~ O_NONBLOCK);
		return fd;
	}

	return fd;
}

/*********************************************/
/* request a file from the HTTP server       */
/* the network stream must be opened already */
/* and the URL structure be initialized      */
/* properly !                                */

int request_file(URL *url)
{
	char str[256], *ptr;
	int slot;
	ID3 id3;
	memset(&id3, 0, sizeof(ID3));

	/* get the cache slot for this stream. A negative return value */
	/* indicates that no cache has been set up for this stream */
	slot = getCacheSlot(url->stream);

	/* if the filename contains a line break, then use everything */
	/* up to the line break as file name and everything beyond as */
	/* entity to be sent with the request */
	ptr = strchr(url->file, '\n');
	if(ptr)
	{
		strcpy(url->entity, ptr + 1);
		*ptr = 0;
	}
	switch(url->proto_version)
	{
		/* send a HTTP/1.0 request */
		case HTTP10:	{
				snprintf(str, sizeof(str)-1, "GET http://%s:%d%s\n", url->host, url->port, url->file);
				dprintf(stderr, "> %s", str);
				send(url->fd, str, strlen(str), 0);
			}
			break;

		/* send a HTTP/1.1 request */
		case HTTP11:	{
				int meta_int;
				CSTATE tmp;

				snprintf(str, sizeof(str)-1, "%s %s HTTP/1.1\r\n", (url->entity[0]) ? "POST" : "GET", url->file);
				dprintf(stderr, "> %s", str);
				send(url->fd, str, strlen(str), 0);

				if(url->port != 80)
					snprintf(str, sizeof(str)-1, "Host: %s:%d\r\n", url->host, url->port);
				else
					snprintf(str, sizeof(str)-1, "Host: %s\r\n", url->host);
				dprintf(stderr, "> %s", str);
				send(url->fd, str, strlen(str), 0);

				snprintf(str, sizeof(str)-1, "User-Agent: WinampMPEG/5.52\r\n");
				dprintf(stderr, "> %s", str);
				send(url->fd, str, strlen(str), 0);

				if (url->logindata[0])
				{
					SPRINTF(str, "Authorization: Basic %s\r\n", url->logindata);
					dprintf(stderr, "> %s", str);
					send(url->fd, str, strlen(str), 0);
				}

				strcpy(str, "Accept: */*\r\n");
				dprintf(stderr, "> %s", str);
				send(url->fd, str, strlen(str), 0);

				if(enable_metadata)
				{
					strcpy(str, "Icy-MetaData: 1\r\n");
					dprintf(stderr, "> %s", str);
					send(url->fd, str, strlen(str), 0);
				}

				/* if we have a entity, announce it to the server */
				if(url->entity[0])
				{
					snprintf(str, sizeof(str)-1, "Content-Length: %d\r\n", (int)strlen(url->entity));
					dprintf(stderr, "> %s", str);
					send(url->fd, str, strlen(str), 0);
				}

				snprintf(str, sizeof(str)-1, "Connection: close\r\n");
				dprintf(stderr, "> %s", str);
				send(url->fd, str, strlen(str), 0);

				// empty line -> end of HTTP request
				snprintf(str, sizeof(str)-1, "\r\n");
				dprintf(stderr, "> %s", str);
				send(url->fd, str, strlen(str), 0);

				if( (meta_int = parse_response(url, &id3, &tmp)) < 0)
				{
					if(meta_int == -301 || meta_int == -302)
						return meta_int;
					else
						return -1;
				}
				if(meta_int)
				{
					if (slot < 0) {
						dprintf(stderr, "error: meta_int != 0 && slot < 0");
					} else {
						/* hook in the filter function if there is meta */
						/* data present in the stream */
						cache[slot].filter_arg = ShoutCAST_InitFilter(meta_int);
						cache[slot].filter = ShoutCAST_MetaFilter;

						/* this is a *really bad* way to pass along the argument, */
						/* since we convert the integer into a pointer instead of */
						/* passing along a pointer/reference !*/
						if(cache[slot].filter_arg->state)
							memmove(cache[slot].filter_arg->state, &tmp, sizeof(CSTATE));
					}
				}

				/* push the created ID3 header into the stream cache */
				push(url->stream, (char*)&id3, id3.len);
#if 0
				rval = parse_response(url, NULL, NULL);
				dprintf(stderr, "server response parser: return value = %d\n", rval);

				/* if the header indicated a zero length document or an */
				/* error, then close the cache, if there is any */
				/* 25.04.05 ChakaZulu: zero length can be a stream so let's try playing */
				if((slot >= 0) && (rval < 0))
					cache[slot].closed = 1;

				/* return on error */
				if( rval < 0 )
					return rval;
#endif
			}
			break;

		/* send a SHOUTCAST request */
		case SHOUTCAST:{
			int meta_int;
			CSTATE tmp;

			SPRINTF(str, "GET %s HTTP/1.0\r\n", url->file);
			dprintf(stderr, "> %s", str);
			send(url->fd, str, strlen(str), 0);
			SPRINTF(str, "Host: %s\r\n", url->host);
			dprintf(stderr, "> %s", str);
			send(url->fd, str, strlen(str), 0);

			if(enable_metadata)
			{
				strcpy(str, "icy-metadata: 1\r\n");
				dprintf(stderr, "> %s", str);
				send(url->fd, str, strlen(str), 0);
			}

			SPRINTF(str, "User-Agent: %s\r\n", "RealPlayer/4.0");
			dprintf(stderr, "> %s", str);
			send(url->fd, str, strlen(str), 0);

			if (url->logindata[0])
			{
				SPRINTF(str, "Authorization: Basic %s\r\n", url->logindata);
				dprintf(stderr, "> %s", str);
				send(url->fd, str, strlen(str), 0);
			}

			strcpy(str, "\r\n"); /* end of headers to send */
			dprintf(stderr, "> %s", str);
			send(url->fd, str, strlen(str), 0);

			if( (meta_int = parse_response(url, &id3, &tmp)) < 0)
			{
				if(meta_int == -301 || meta_int == -302)
					return meta_int;
				else
					return -1;
			}

			if(meta_int)
			{
				/* hook in the filter function if there is meta */
				/* data present in the stream */
				cache[slot].filter_arg = ShoutCAST_InitFilter(meta_int);
				cache[slot].filter = ShoutCAST_MetaFilter;

				/* this is a *really bad* way to pass along the argument, */
				/* since we convert the integer into a pointer instead of */
				/* passing along a pointer/reference !*/
				if(cache[slot].filter_arg->state)
					memmove(cache[slot].filter_arg->state, &tmp, sizeof(CSTATE));
			}

			/* push the created ID3 header into the stream cache */
			push(url->stream, (char*)&id3, id3.len);
		}
		break;
	} /* end switch(url->proto_version) */

	/* now it get's nasty ;) */
	/* create a thread that continously feeds the cache */
	/* with the data it fetches from the network stream */
	/* but *ONLY* if there is a cache slot for this stream at all ! */
	/* HINT: in compatibility mode no cache is configured */

	if(slot >= 0)
	{
		pthread_attr_init(&cache[slot].attr);
		pthread_create(
			&cache[slot].fill_thread,
			&cache[slot].attr,
			(void *(*)(void*))&CacheFillThread, (void*)&cache[slot]);
		dprintf(stderr, "request_file: slot %d fill_thread 0x%x\n", slot, (int)cache[slot].fill_thread);
	}

	/* now we do not care any longer about fetching new data,*/
	/* but we can not be shure that the cache is filled with */
	/* enough stuff ! */
	return 0;
}

/***************************************/
/* parse the server response (header)  */
/* and translate possible errors into  */
/* local syserror numbers              */

#define getHeaderVal(a,b) { \
  char *_ptr; \
  _ptr = strcasestr(header, a); \
  if(_ptr) \
  { \
    _ptr = strchr(_ptr, ':'); \
    for(; !isalnum(*_ptr); _ptr++) {}; \
    b = atoi(_ptr); \
  } else b = -1; }

#define getHeaderStr(a,b) { \
  char *_ptr; \
  _ptr = strcasestr(header, a); \
  if(_ptr) \
  { \
    unsigned int i; \
    _ptr = strchr(_ptr, ':'); \
    for(_ptr++; isspace(*_ptr); _ptr++) {}; \
    for (i=0; (_ptr[i]!='\n') && (_ptr[i]!='\r') && (_ptr[i]!='\0') && (i<sizeof(b)); i++) b[i] = _ptr[i]; \
    b[i] = 0; \
  } }

/*
void readln(int fd, char *buf)
{
	for(recv(fd, buf, 1, 0); (buf && isalnum(*buf)); recv(fd, ++buf, 1, 0)){};
	if(buf)
		*buf = 0;
}
*/

int parse_response(URL *url, void * /*opt*/, CSTATE *state)
{
	char header[2049], /*str[255]*/ str[2049]; // combined with 2nd local str from id3 part
	char *ptr, chr=0, lastchr=0;
	int hlen = 0, response;
	int rval;
	int fd = url->fd;
	meta_interval = 0;
	chunked = false;

	memset(header, 0, 2049);
	ptr = header;

	/* extract the http header from the stream */
	do
	{
		recv(fd, ptr, 1, 0);

		/* skip all 'CR' symbols */
		if(*ptr != 13)
		{
			lastchr = chr; chr = *ptr++; hlen++;
		}

	} while((hlen < 2048) && ( ! ((chr == 10) && (lastchr == 10))));

	*ptr = 0;

	dprintf(stderr, "----------\n%s----------\n", header);

	/* parse the header fields */
	ptr = strstr(header, "HTTP/1.1");

	if(!ptr)
		ptr = strstr(header, "HTTP/1.0");

	if(!ptr)
		ptr = strstr(header, "ICY");

	/* no valid HTTP/1.1 or SHOUTCAST response */
	if(!ptr)
		return -1;

	response = atoi(strchr(ptr, ' ') + 1);

	switch(response)
	{
		case 200:	errno = 0;
				break;

		case 301:	/* 'file moved' error */
		case 302:	/* 'file not found' error */
				errno = ENOENT;
				STRCPY(err_txt, ptr);
				getHeaderStr("Location", redirect_url);
				return -1 * response;
				break;

		case 404:	/* 'file not found' error */
				errno = ENOENT;
				STRCPY(err_txt, ptr);
				return -1;
				break;

		default:	errno = ENOPROTOOPT;
				dprintf(stderr, "unknown server response code: %d\n", response);
				return -1;
	}

	/* use 'audio/mpeg' as default type, in case we are not told otherwise */
	strcpy(str, "audio/mpeg");
	getHeaderStr("Content-Type:", str);
	f_type(url->stream, str);
	dprintf(stderr, "type %s\n", str);

	/* if we got a content length from the server, i.e. rval >= 0 */
	/* then return now with the content length as return value */
	getHeaderVal("Content-Length:", rval);
	//  dprintf(stderr, "file size: %d\n", rval);
	if(rval >= 0)
		return rval;

	/* no content length indication from the server ? Then treat it as stream */
	getHeaderVal("icy-metaint:", meta_interval);
	if(meta_interval < 0)
		meta_interval = 0;

	/* yet another hack: this is only implemented to be able to fetch */
	/* the playlists from shoutcast */
	if(strstr(header, "Transfer-Encoding: chunked"))
	{
		chunked = true;
	}

	if (state != NULL) {
		getHeaderStr("icy-genre:", state->genre);
		getHeaderStr("icy-name:", state->station);
		getHeaderStr("icy-url:", state->station_url);
		getHeaderVal("icy-br:", state->bitrate);
	}
#if 0
	ID3 *id3 = (ID3*)opt;
	/********************* dirty hack **********************/
	/* we parse the stream header sent by the server and	*/
	/* build based on this information an arteficial id3		*/
	/* header that is pushed into the streamcache before	*/
	/* any data from the stream is fed into the cache. This	*/
	/* makes the stream look like an MP3 and we have the	*/
	/* station information in the display of the player :))	*/

#define SSIZE(a) (\
   (((a) & 0x0000007f) << 0) | (((a) & 0x00003f80) << 1) | \
   (((a) & 0x001fc000) << 2) | (((a) & 0xfe000000) << 3))

#define FRAME(b,c) {\
  strcpy(id3frame.id, (b)); \
  strcpy(id3frame.base, (c)); \
  id3frame.size = strlen(id3frame.base); \
  fcnt = 11 + id3frame.size; }

	if(id3)
	{
		int cnt = 0, fcnt = 0;
		ID3_frame id3frame;
		uint32_t sz;
		char station[2048], desc[2048];

		memmove(id3->magic, "ID3", 3);
		id3->version[0] = 3;
		id3->version[1] = 0;

		ptr = strstr(header, "icy-name:");
		if(ptr)
		{
			ptr = strchr(ptr, ':') + 1;
			for(; ((*ptr == '-') || (*ptr == ' ')); ptr++){};
			strcpy(station, ptr);
			*strchr(station, '\n') = 0;

			ptr = strchr(station, '-');
			if(ptr)
			{
				*ptr = 0;
				for(ptr++; ((*ptr == '-') || (*ptr == ' ')); ptr++){};
				strcpy(desc, ptr);
			}

			FRAME("TPE1", station);
			id3frame.size = SSIZE(id3frame.size + 1);
			memmove(id3->base + cnt, &id3frame, fcnt);
			cnt += fcnt;

			FRAME("TALB", desc);
			id3frame.size = SSIZE(id3frame.size + 1);
			memmove(id3->base + cnt, &id3frame, fcnt);
			cnt += fcnt;
		}

		ptr = strstr(header, "icy-genre:");
		if(ptr)
		{
			ptr = strchr(ptr, ':') + 1;
			for(; ((*ptr == '-') || (*ptr == ' ')); ptr++){};
			strcpy(str, ptr);
			*strchr(str, '\n') = 0;

			FRAME("TIT2", str);
			id3frame.size = SSIZE(id3frame.size + 1);
			memmove(id3->base + cnt, &id3frame, fcnt);
			cnt += fcnt;
		}

		FRAME("COMM", "dbox streamconverter");
		id3frame.size = SSIZE(id3frame.size + 1);
		memmove(id3->base + cnt, &id3frame, fcnt);
		cnt += fcnt;

		sz = 14 + cnt - 10;

		id3->size[0] = (sz & 0xfe000000) >> 21;
		id3->size[1] = (sz & 0x001fc000) >> 14;
		id3->size[2] = (sz & 0x00003f80) >> 7;
		id3->size[3] = (sz & 0x0000007f) >> 0;

		id3->len = 14 + cnt;
	}
#endif

	return meta_interval;
}

/******* wrapper functions for the f*() calls ************************/

#define transport(a,b,c) \
    if(strstr(url.url, a)) \
    { \
      url.access_mode = b; \
      url.proto_version = c; \
    }

FILE *f_open(const char *filename, const char *acctype)
{
	URL url;
	FILE *fd;
	int /*i,*/ compatibility_mode = 0;
	char *ptr = NULL, buf[4096] = {0}, type[10] = {0};

	if(acctype)
		strcpy(type, acctype);

	/* read some options from the options-file */
	getOpts();

	/* test if compatibility mode has been requested. */
	/* e.g. "rbc" = read, binary, without stream caching */
	if(strchr(type, 'c'))
	{
		compatibility_mode = 1;
		*strchr(type, 'c') = 0;
	}

	dprintf(stderr, "f_open: %s %s\n", (compatibility_mode) ? "(compatibility mode)" : "", filename);

	/* set default protocol and port */
	memset(&url, 0, sizeof(URL));
	url.proto_version = HTTP11;
	url.port = 80;
	strcpy(url.url, filename);

	/* remove leading spaces */
	for (ptr = url.url; (ptr != NULL) && ((*ptr == ' ') || (*ptr == '	')); ptr++){} ;

	if(ptr != url.url)
		strcpy(url.url, ptr);

	/* did we get an URL file as argument ? If so, then open */
	/* this file, read out the url and open the url instead */
#ifndef DISABLE_URLFILES
	if( strstr(filename, ".url") || strstr(filename, ".imu"))
	{
		fd = fopen(filename, "r");

		if(fd)
		{
			fread(buf, sizeof(char), 4095, fd);
			fclose(fd);

			ptr = strstr(buf, "://");

			if(!ptr)
			{
				dprintf(stderr, "Ups! File doesn't seem to contain any URL !\nbuffer:%s\n", buf);
				return NULL;
			}

			ptr = strchr(buf, '\n');
			if(ptr)
				*ptr = 0;

			STRCPY(url.url, buf);
		}
		else
			return NULL;
	}
#endif

	/* now lets see what we have ... */
	parseURL_url(url);

	dprintf(stderr, "URL  to open: %s, access mode %s%s\n",
		url.url,
		(url.access_mode == MODE_HTTP)  ? 	"HTTP" :
		(url.access_mode == MODE_SCAST) ? 	"SHOUTCAST" :
		(url.access_mode == MODE_ICAST) ? 	"ICECAST" :
		(url.access_mode == MODE_PLS)   ? 	"PLAYLIST" :
									"FILE",
		(url.access_mode != MODE_FILE)   ? (
		(url.proto_version == HTTP10)	 ? 	"/1.0" :
		(url.proto_version == HTTP11)	 ? 	"/1.1" :
		(url.proto_version == SHOUTCAST) ? 	"/SHOUTCAST" :
									"") : "" );

	dprintf(stderr, "FILE to open: %s, access mode: %d\n", url.file, url.access_mode);

	switch(url.access_mode)
	{
		case MODE_HTTP:	{
			int follow_url = 1; // used for redirects
			int redirects = 0;
			*redirect_url = '\0';
			while (follow_url)
			{

  			 	int retries = retry_num;

				do
				{
					url.fd = ConnectToServer(url.host, url.port);
					retries--;
				} while( url.fd < 0 && retries > 0);

				/* if the stream could not be opened, then indicate */
				/* an 'No such device or address' error */
 				if(url.fd < 0)
				{
					fd = NULL;
					errno = ENXIO;
					printf("netfile: could not connect to server %s:%d\n",url.host, url.port);
					return fd;
				}
				else
				{
					fd = fdopen(url.fd, "r+");
					url.stream = fd;

					if(!fd)
					{
						 perror(err_txt);
					}
					else
					{
						/* in compatibility mode we must not use our own stream cache */
						/* because the application makes use of their own f*() calls */
						/* which we can not replace by our own functions and thus they'll */
						/* interfere with each other. All we can do is to open the stream */
						/* and return a valid stream descriptor */
						if(!compatibility_mode)
						{
 							int i;
							/* look for a free cache slot */
							for(i=0; ((i<CACHEENTMAX) && (cache[i].cache != NULL)); i++){};

							/* no free cache slot ? return an error */
							if(i == CACHEENTMAX)
							{
								strcpy(err_txt, "no more free cache slots. Too many open files.\n");
								return NULL;
							}

							dprintf(stderr, "f_open: adding stream %p to cache[%d]\n", fd, i);

							cache[i].fd       = fd;
							cache[i].csize    = CACHESIZE;
							cache[i].cache    = (char*)malloc(cache[i].csize);
							cache[i].ceiling  = cache[i].cache + CACHESIZE;
							cache[i].wptr     = cache[i].cache;
							cache[i].rptr     = cache[i].cache;
							cache[i].closed   = 0;
							cache[i].total_bytes_delivered = 0;
							cache[i].filter   = NULL;

							/* create the readable/writeable mutex locks */
							dprintf(stderr, "f_open: creating mutexes\n");
							pthread_mutex_init(&cache[i].cache_lock, &cache[i].cache_lock_attr);
							pthread_mutex_init(&cache[i].readable,   &cache[i].readable_attr);
							pthread_mutex_init(&cache[i].writeable,  &cache[i].writeable_attr);

							/* and set the empty cache to 'unreadable' */
							dprintf(stderr, "f_open: locking read direction\n");
							pthread_mutex_lock( &cache[i].readable );

							/* but writeable. */
//							dprintf(stderr, "f_open: unlocking write direction\n");
// mutex is not locked after init
//							pthread_mutex_unlock( &cache[i].writeable );
						}

						/* send the file request and check it'S revurn value */
						/* if it failed, then close the stream */
						int request_res = request_file(&url);
						if(request_res < 0)
						{
							/* we need out own f_close() function here, because everything */
							/* already has been set up and f_close() deinitializes it all correctly */
							f_close(url.stream);
							fd = NULL;
						}
						follow_url = 0;
						if (is_redirect(-1*request_res)) {
							redirects++;
							dprintf(stderr,"redirected to %s\n",redirect_url);
							strcpy(url.url,redirect_url);
							parseURL_url(url);
							if (redirects <= MAX_REDIRECTS)
								follow_url = 1;
							else
								dprintf(stderr, "too many redirects: %d (max: %d)\n",redirects,MAX_REDIRECTS);
						}
					}
				}
			}
		} /* endcase MODE_HTTP */
		break;

		case MODE_ICAST:
		case MODE_SCAST:
			/* pseude transport mode; create the url to fetch the shoutcast */
			/* directory playlist, fetch it, parse it and try to open the   */
			/* stream url in it until we find one that works. The we open   */
			/* this one and return with the opened stream.                  */
			/* CAUTION: this is a nasty nested thing, because we call       */
			/* f_open() several times recursively - so the function should  */
			/* better be free from any bugs ;)                              */

			/* if we didn't get a station number, query the shoutcast database */
			/* and look in the reply for the station number */
			char *endptr;
			strtol(url.host, &endptr, 10);
			if((endptr-url.host) < (int)strlen(url.host))
			{
				char buf2[32768];
				FILE *_fd;

				/* convert into shoutcast query format */
				for(ptr=url.host; *ptr; ptr++)
					*ptr = (*ptr != 32) ? *ptr : '+';

				CRLFCut(url.host);

				/* create either a shoutcast or an icecast query */
				if(url.access_mode == MODE_SCAST)
					SPRINTF(buf2, "http://classic.shoutcast.com/directory/?orderby=listeners&s=%s", url.host);
				else
					SPRINTF(buf2, "http://www.icecast.org/streamlist.php?search=%s", url.host);

				//findme
				// ICECAST: it ain't that simple. Icecast doesn't work yet */

				_fd = f_open(buf2, "rc");

				if(!_fd)
				{
					SPRINTF(err_txt, "%s database query failed\nfailed action: %s",
						((url.access_mode == MODE_SCAST) ? "shoutcast" : "icecast"), buf2);
					return NULL;
				}

				fread(buf2, 1, 32768, _fd);
				f_close(_fd);

				ptr = strstr(buf2, "rn=");

				if(!ptr)
				{
					strcpy(err_txt, "failed to find station number");
					dprintf(stderr, "%s\n", buf2);
					return NULL;
				}

				SPRINTF(url.host, "%d", atoi(ptr + 3));
			}

			/* create the correct url from the station number */
			CRLFCut(url.host);
			SPRINTF(url.url, "http://classic.shoutcast.com/sbin/shoutcast-playlist.pls?rn=%s&file=filename.pls", url.host);
			/* fall through */

		case MODE_PLS:	{
			char *ptr2, /*buf[4096], use local buf from function */ servers[25][1024];
			int /*rval,*/ retries = retry_num;
			ptr = NULL;

			/* fetch the playlist from the shoutcast directory with our own */
			/* url-capable f_open() call. We need the compatibility mode for */
			/* this because we will read the data with the standard OS fread() call */
			do
			{
				fd = f_open(url.url, "rc");

				if(fd)
				{
					/* read the playlist (we use the standard fopen() call from the */
					/* operating system because we don't need/want stream caching for */
					/* this operation */

					/*rval =*/ fread(buf, sizeof(char), 4096, fd);
					f_close(fd);

					ptr = strstr(buf, "http://");
				}

				retries--;

			} while(!ptr && retries > 0);


			/* extract the servers from the received playlist */
			if(!ptr)
			{
				dprintf(stderr, "Ups! Playlist doesn't seem to contain any URL !\nbuffer:%s\n", buf);
				strcpy(err_txt, "Ups! Playlist doesn't seem to contain any URL !");
				return NULL;
			}

			for(int i=0; ((ptr != NULL) && (i<25)); ptr = strstr(ptr, "http://") )
			{
				strncpy(servers[i], ptr, 1023);
				servers[i][1023] = '\0';
				ptr2 = strchr(servers[i], '\n');
				if(ptr2) *ptr2 = 0;
				// change ptr so that next strstr searches in buf and not in servers[i]
				ptr = strchr(ptr,'\n');
				if (ptr != NULL)
					ptr++;
				dprintf(stderr, "server[%d]: %s\n", i, servers[i]);
				i++;
			}

			/* try to connect to all servers until we find one that */
			/* is willing to serve us */
			{
				int i;
				for(i=0, fd = NULL; ((fd == NULL) && (i<25)); i++)
				{
					const char* const chptr = strstr(servers[i], "://");
					if(chptr)
					{
						snprintf(url.url, sizeof(url.url)-1, "icy%s", chptr);
						fd = f_open(url.url, "r");
					}
				}
			}
		}
		break;

		case MODE_FILE:
		default:
			{
				unsigned char magic[4] = {0, 0, 0, 0};

				fd = fopen(url.file, type);
				if (fd == NULL)
					return NULL;
				fread(&magic, 4, 1, fd);
				rewind(fd);

				/* first stage: try to determine the filetype from the file */
				/* magic, if there is any */
				for (int i = 0; i < known_magic_count; i++)
				{
					if (((*(unsigned char *)&(magic[0])) & *(unsigned char *)&(known_magic[i].mask[0])) == *(unsigned char *)&(known_magic[i].mode[0]))
					{
						f_type(fd, known_magic[i].type);
						goto magic_found;
					}
				}

				/* stage two: try to determine the filetype from the file name */
				/* a smarter solution would be to get this info from /etc/mime.types */

				ptr = strrchr(url.file , '.');
				//#FIXME warning what about filenames without dots?

				if (ptr++)
				{
					if (strcasecmp(ptr, "cdr") == 0)	f_type(fd, "audio/cdr");
					if (strcasecmp(ptr, "wav") == 0)	f_type(fd, "audio/wave");
					if (strcasecmp(ptr, "aif") == 0)	f_type(fd, "audio/aifc");
					if (strcasecmp(ptr, "snd") == 0)	f_type(fd, "audio/snd");

					/* they should be obsolete now due to the file magic detection */
					if (strcasecmp(ptr, "ogg") == 0)	f_type(fd, "audio/ogg");
					if (strcasecmp(ptr, "mp3") == 0)	f_type(fd, "audio/mpeg");
					if (strcasecmp(ptr, "mp2") == 0)	f_type(fd, "audio/mpeg");
					if (strcasecmp(ptr, "mpa") == 0)	f_type(fd, "audio/mpeg");
				}
magic_found:
			;
			}
		break;
	} /* endswitch */

	return fd;
}

int f_close(FILE *stream)
{
	int i, rval;

//	dprintf(stderr, "f_close: stream 0x%x\n", stream);
	/* at first, lookup the stream in the stream type table and remove it */
	for(i=0 ; (i<CACHEENTMAX) && (stream_type[i].stream != stream); i++){};

	if(i < CACHEENTMAX)
		stream_type[i].stream = NULL;

	/* lookup the stream ID in the cache table */
	i = getCacheSlot(stream);

	/* no associated cache slot ? Simply close the stream */
	if(i < 0)
		return( fclose(stream) );

	if(cache[i].fd == stream)
	{
		dprintf(stderr, "f_close: removing stream %p from cache[%d]\n", stream, i);

		cache[i].closed = 1;		/* indicate that the cache is closed */

		/* remove the cache looks */
		pthread_mutex_unlock( &cache[i].writeable );
		pthread_mutex_unlock( &cache[i].readable );
		pthread_mutex_unlock( &cache[i].cache_lock );

		/* wait for the fill thread to finish */
		dprintf(stderr, "f_close: waiting for fill tread to finish, slot %d fill_thread 0x%x\n", i, (int) cache[i].fill_thread);
		if(cache[i].fill_thread)
			pthread_join(cache[i].fill_thread, NULL);

		cache[i].fill_thread = 0;

		dprintf(stderr, "f_close: closing cache\n");
		rval = fclose(cache[i].fd);	/* close the stream */
		free(cache[i].cache);		/* free the cache */

		/* if this stream has a streamfilter, call it's destructor */
		if(cache[i].filter_arg)
			if(cache[i].filter_arg->destructor)
			{
				dprintf(stderr, "f_close: calling stream filter destructor\n");
				cache[i].filter_arg->destructor(cache[i].filter_arg);
				free(cache[i].filter_arg);
			}

		dprintf(stderr, "f_close: destroying mutexes\n");
		pthread_mutex_destroy(&cache[i].cache_lock);
		pthread_mutex_destroy(&cache[i].readable);
		pthread_mutex_destroy(&cache[i].writeable);

		/* completely blank out all data */
		memset(&cache[i], 0, sizeof(STREAM_CACHE));
	}
	else
		rval = fclose(stream);

	return rval;
}

long f_tell(FILE *stream)
{
	int i;
	long rval;

	/* lookup the stream ID in the cache table */
	i = getCacheSlot(stream);

	if(i < 0)
		return( ftell(stream) );

	if(cache[i].fd == stream)
		rval = cache[i].total_bytes_delivered;
	else
		rval = ftell(stream);

	return rval;
}

void f_rewind(FILE *stream)
{
	int i;

	/* lookup the stream ID in the cache table */
	i = getCacheSlot(stream);

	if(i < 0)
	{
		rewind(stream);
		return;
	}

	if(cache[i].fd == stream)
	{
		/* nothing to do */
	}
	else
		rewind(stream);
}

int f_seek(FILE *stream, long offset, int whence)
{
	int i, rval;

	/* lookup the stream ID in the cache table */
	i = getCacheSlot(stream);

	if(i < 0)
		return( fseek(stream, offset, whence) );

	if(cache[i].fd == stream)
	{
		dprintf(stderr, "WARNING: program tries to seek on a stream !! offset %d whence %d\n", (int) offset, whence);
		rval = -1;
	}
	else
		rval = fseek(stream, offset, whence);

	return rval;
}

size_t f_read (void *ptr, size_t size, size_t nitems, FILE *stream)
{
	int i, rval;

	/* lookup the stream ID in the cache table */
	i = getCacheSlot(stream);

	if(i < 0)
		return( fread(ptr, size, nitems, stream) );

	if(cache[i].fd == stream) {
		rval = pop(stream, (char*)ptr, size * nitems);
	}
	else
		rval = fread(ptr, size, nitems, stream);

	return rval;
}

const char *f_type(FILE *stream, const char *type)
{
	int i;

	/* lookup the stream in the stream type table */
	for(i=0 ; (i<CACHEENTMAX) && (stream_type[i].stream != stream); i++){};

	/* if the stream could not be found, look for a free slot ... */
	if(i == CACHEENTMAX)
	{
	dprintf(stderr, "stream %p not in type table, ", stream);

	for(i=0 ; (i<CACHEENTMAX) && (stream_type[i].stream != NULL); i++){};

	/* ... and copy the supplied type into the table */
	if(i < CACHEENTMAX)
	{
		if(type)
		{
			stream_type[i].stream = stream;
			strncpy(stream_type[i].type, type, 64);
			stream_type[i].type[64] = '\0';
			dprintf(stderr, "added entry (%s) for %p\n", type, stream);
		}
		return type;
	}
	else
		dprintf(stderr, "failed to add entry (%s)\n", type);

	}

	/* the stream is already in the table */
	else
	{
		dprintf(stderr, "stream %p lookup in type table succeded\n", stream);

		if(!type)
			return stream_type[i].type;
		else
			if(strstr(stream_type[i].type, type))
				return stream_type[i].type;
	}

	return NULL;
}


/************************************************************************/
/* stream cache functions                                               */
/*                                                                      */
/* this section implements some functions to cache data from a          */
/* stream in a cache associated to the stream number. The cache         */
/* is created in the f_open() call and destroyed in the f_close()       */
/* call is the object to be opened is a network resource and not        */
/* a local file.                                                        */
/*                                                                      */
/* functions:                                                           */
/*            push(FILE *fd, char *buf, long len)                       */
/*                                  write the buffer pointed to by *buf */
/*                                  with len bytes into the cache for   */
/*                                  stream fd                           */
/*                                                                      */
/*            pop(FILE *fd, char *buf, long len)                        */
/*                                  fill the buffer pointed to by *buf  */
/*                                  with len bytes from the cache for   */
/*                                  stream fd                           */
/*                                                                      */
/*            getCacheSlot(FILE *fd)                                    */
/*                                                                      */
/*            CacheFillThread(void *c)                                  */
/*                                  feeds the cache with data from the  */
/*                                  stream                              */
/************************************************************************/

int getCacheSlot(FILE *fd)
{
	int i;

	for(i=0; ( (i<CACHEENTMAX) && ((cache[i].fd != fd) || (!cache[i].cache))
		); i++){};

	return (i == CACHEENTMAX) ? -1 : i;
}

/* push a block of data into the stream cache */
int push(FILE *fd, char *buf, long len)
{
	int rval = 0, i, j;
	int blen = 0;

	i = getCacheSlot(fd);

	if(i < 0)
		return -1;

	//	dprintf(stderr, "push: %d bytes to store [filled: %d of %d], stream: %x\n", len, cache[i].filled, CACHESIZE, fd);

	if(cache[i].fd != fd) {
		dprintf(stderr, "push: no cache present for stream %p\n", fd);
		return -1;
	}
	do
	{
		if(cache[i].closed)
			return -1;

		/* try to obtain write permissions for the cache */
		/* this will block if the cache is full */
		pthread_mutex_lock( &cache[i].writeable );

		if((cache[i].csize - cache[i].filled))
		{
			int amt[2];

			/* prevent any modification to the cache by other threads, */
			/* mainly by the pop() function, while we write data to it */
			pthread_mutex_lock( &cache[i].cache_lock );

			/* has the cache been closed while we were waiting for the */
			/* lock to open ? */
			if(cache[i].closed)
				return -1;

			/* block transfer length: get either what's there or */
			/* only as much as we need */
			blen = ((len - rval) > (cache[i].csize - cache[i].filled)) ? (cache[i].csize - cache[i].filled) : (len - rval);

			if(cache[i].wptr < cache[i].rptr)
			{
				amt[0] = cache[i].rptr - cache[i].wptr;
				amt[1] = 0;
			}
			else
			{
				amt[0] = cache[i].ceiling - cache[i].wptr;
				amt[1] = cache[i].rptr - cache[i].cache;
			}

			for(j=0; j<2; j++)
			{
				if(amt[j] > blen)
					amt[j] = blen;

				if(amt[j] > 0)
				{
					memmove(cache[i].wptr, buf, amt[j]);
					cache[i].wptr = cache[i].cache +
						(((int)(cache[i].wptr - cache[i].cache) + amt[j]) % cache[i].csize);

					buf += amt[j];	/* adjust the target buffer pointer */
					rval += amt[j];	/* increase the 'total bytes' counter */
					blen -= amt[j];	/* decrease the block length counter */
					cache[i].filled += amt[j];
				}
			}

			//	dprintf(stderr, "push: %d/%d bytes written [filled: %d of %d], stream: %x\n", amt[0] + amt[1], rval, cache[i].filled, CACHESIZE, fd);

			pthread_mutex_unlock( &cache[i].cache_lock );

			/* unlock the cache for read access, if it */
			/* contains some data */
			if(cache[i].filled){
				pthread_mutex_unlock( &cache[i].readable );
			}
		}

		/* if there is still space in the cache, unlock */
		/* it again for further writing by anyone */
		if(cache[i].csize - cache[i].filled){
			pthread_mutex_unlock( & cache[i].writeable );
		}
		else
			dprintf(stderr, "push: buffer overrun; cache full - leaving cache locked\n");

	} while(rval < len);

	dprintf(stderr, "push: exitstate: [filled: %3.1f %%], stream: %p\r", 100.0 * (float)cache[i].filled / (float)cache[i].csize, fd);

	return rval;
}

int pop(FILE *fd, char *buf, long len)
{
	int rval = 0, i, j;
	int blen = 0;

	i = getCacheSlot(fd);

	if(i < 0)
		return -1;

	dprintf(stderr, "pop: %d bytes requested [filled: %d of %d], stream: %p buf %p\n",
		(int) len, (int) cache[i].filled, (int) CACHESIZE, fd, buf);

	if(cache[i].fd == fd)
	{
		do
		{
			if(cache[i].closed && (!cache[i].filled) )
				return 0;

			if((!cache[i].filled) && feof(fd)){
				return 0;
			}

			/* try to obtain read permissions for the cache */
			/* this will block if the cache is empty */
#if 0 //FIXME no way to stop if connection dead ?
			pthread_mutex_lock( & cache[i].readable );
#else
			while(true) {
				int lret = pthread_mutex_trylock(&cache[i].readable);
				if((lret == 0) || (CAudioPlayer::getInstance()->getState() == CBaseDec::STOP_REQ))
					break;
				usleep(100);
			}
#endif
			if(cache[i].filled)
			{
				int amt[2];

				/* prevent any modification to the cache by other threads, */
				/* mainly by the push() function, while we read data from it */
				pthread_mutex_lock( &cache[i].cache_lock );

				/* block transfer length: get either what's there or */
				/* only as much as we need */
				blen = ((len - rval) > cache[i].filled) ? cache[i].filled : (len - rval);

				if(cache[i].rptr < cache[i].wptr)
				{
					amt[0] = cache[i].wptr - cache[i].rptr;
					amt[1] = 0;
				}
				else
				{
					amt[0] = cache[i].ceiling - cache[i].rptr;
					amt[1] = cache[i].wptr - cache[i].cache;
				}

				for(j=0; j<2; j++)
				{
					if(amt[j] > blen) amt[j] = blen;

					if(amt[j])
					{
						dprintf(stderr, "pop(): rptr: %p, buf: %p, amt[%d]=%d, blen=%d, len=%d, rval=%d\n",
							cache[i].rptr, buf, j, amt[j], blen, (int) len, rval);

						memmove(buf, cache[i].rptr, amt[j]);

						cache[i].rptr = cache[i].cache +
							(((int)(cache[i].rptr - cache[i].cache) + amt[j]) % cache[i].csize);

						buf += amt[j];	/* adjust the target buffer pointer */
						rval += amt[j];	/* increase the 'total bytes' counter */
						blen -= amt[j];	/* decrease the block length counter */
						cache[i].filled -= amt[j];
					}
				}

				dprintf(stderr, "pop: %d/%d/%d bytes read [filled: %d of %d], stream: %p\n", amt[0] + amt[1], rval, (int) len, (int) cache[i].filled, (int) CACHESIZE, fd);

				/* if the cache is closed and empty, then */
				/* force the end condition to be met */
				if(cache[i].closed && (! cache[i].filled))
					break;//len = rval;

				/* allow write access again */
				pthread_mutex_unlock( &cache[i].cache_lock );

				/* unlock the cache for write access, if it */
				/* has some free space */
				if(cache[i].csize - cache[i].filled){
					pthread_mutex_unlock( &cache[i].writeable );
				}
			}

			/* if there is still data in the cache, unlock */
			/* it again for further reading by anyone */
			if(cache[i].filled)
			{
				pthread_mutex_unlock( & cache[i].readable );
			}
			else
				dprintf(stderr, "pop: buffer underrun; cache empty - leaving cache locked\n");
		} while((rval < len) && (CAudioPlayer::getInstance()->getState() != CBaseDec::STOP_REQ));
	}
	else
	{
		dprintf(stderr, "pop: no cache present for stream %p\n", fd);
		rval = -1;
	}

//  dprintf(stderr, "pop:  exitstate: [filled: %3.1f %%], stream: %x\n", 100.0 * (float)cache[i].filled / (float)cache[i].csize, fd);

	cache[i].total_bytes_delivered += rval;

	if(cache[i].filter_arg)
		if(cache[i].filter_arg->state)
			cache[i].filter_arg->state->buffered = 65536L * (int64_t)cache[i].filled / (int64_t)CACHESIZE;

	return rval;
}
static int http_read_stream_all(STREAM_CACHE *scache, char *buf, int size)
{
	int pos = 0;
	while (!scache->closed && pos < size) {
		int len = read(fileno(scache->fd), buf + pos, size - pos);
		if (len <= 0)
		{
			scache->closed = 1;
			return 0;
		}
		pos += len;
	}
	return pos;
}

static bool getChunkSizeLine(STREAM_CACHE *scache,char *line,int size)
{
	char c = 0;
	int pos = 0,rn = 0,rnc = 4;
	int len = 0;
	while((len = read(fileno(scache->fd), &c, 1)) != 0)
	{
		if(pos >= size)
			break;
		if(c == '\n' || c == '\r')
		{
			rn++;
			if(rn == rnc)
				break;
			continue;
		}
		if(isxdigit(c)){
			line[pos++] = c;
			if(!rn)
				rnc = 2;
		}
		else
		{
			break;
		}
	}
	if (len <= 0)
	{
		scache->closed = 1;
		return false;
	}
	if(c == '\n' || rn == 2)
	{
		line[pos++] = 0;
		return true;
	}

	return false;
}

void CacheFillThread(void *c)
{
	STREAM_CACHE *scache = (STREAM_CACHE*)c;

	if(scache->closed)
		return;

	set_threadname("netfile:cache");
	dprintf(stderr, "CacheFillThread: thread started, using stream %p\n", scache->fd);
	int bufSize = CACHEBTRANS;
	if(chunked && meta_interval > 0)
		bufSize = meta_interval;

	char *buf = (char*)malloc(bufSize);
	if(!buf)
	{
		dprintf(stderr, "CacheFillThread: fatal error ! Could not allocate memory. Terminating.\n");
		exit(-1);
	}
	int chunkSize = 0;
	int rest = 0;
	int rval = 0;
	int datalen = 0;

	/* endless loop; read a block of data from the stream */
	/* and push it into the cache */
	do
	{
		struct pollfd pfd;

		/* has a f_close() call in an other thread already closed the cache ? */
		datalen = bufSize;

		pfd.fd = fileno(scache->fd);
		pfd.events = POLLIN | POLLPRI;
		pfd.revents = 0;

		int ret = poll(&pfd, 1, 1000);

		if (ret > 0 && (pfd.revents & POLLIN) == POLLIN) {
			if(chunked)
			{
				if(chunkSize <= 0)
				{
					chunkSize = 0;
					char line[32];
					if(getChunkSizeLine(scache,line,sizeof(line)))
						chunkSize = strtol(line, NULL, 16);

					if(!chunkSize)
					{
						dprintf(stderr, "CacheFillThread: chunkSize 0\n");
						break;
					}
				}
				datalen = bufSize;
				if(chunkSize && chunkSize < datalen)
				{
					datalen = chunkSize ;
				}
				if(rest)
				{
					datalen = rest;
					rest = 0;
				}
			}

			rval = http_read_stream_all(scache, buf, datalen);
			if (rval <= 0)
				break; /* exit cache fill thread if eof and nothing to push */
			/* if there is a filter function set up for this stream, then */
			/* we need to call it with the propper arguments */
			if(!chunked && scache->filter)
			{
				scache->filter_arg->buf = buf;
				scache->filter_arg->len = &rval;
				scache->filter(scache->filter_arg);
				datalen = rval;
			}

			if( push(scache->fd, buf, rval) < 0)
				break;

			if(chunked)
			{
				chunkSize -= rval;
				if(chunkSize <= 0)
				{
					rest = bufSize - rval;
				}
			}
			if(chunked && scache->filter)
			{
				if(chunkSize > 0)
				{
					rval = read(fileno(scache->fd), buf, 1);
					if (rval <= 0)
						break;
					chunkSize -= rval;
					if(rval)
					{
						char ch = buf[0];
						if(ch > 0)
						{
							size_t icybufsize = 256*16+1;
							char icybuf[icybufsize];
							memset(icybuf,0, icybufsize);
							int len = (ch * 16);
							rval = http_read_stream_all(scache, icybuf,len );
							if (rval <= 0)
								break;
							chunkSize -= rval;
							ShoutCAST_ParseMetaData(icybuf, scache->filter_arg->state);
							if(scache->filter_arg->state->cb)
								scache->filter_arg->state->cb(scache->filter_arg->state);
						}
					}
				}
			}
		}
	}
	while(!scache->closed);
	//while( (rval == datalen) && (!scache->closed) );

	/* close the cache if the stream disrupted */
	scache->closed = 1;
	pthread_mutex_unlock( &scache->writeable );
	pthread_mutex_unlock( &scache->readable );

	/* ... and exit this thread. */
	dprintf(stderr, "CacheFillThread: thread exited, stream %p  \n", scache->fd);

	free(buf);
	pthread_exit(0);
}

/**************************** stream filter ******************************/

int f_status(FILE *stream, void (*cb)(void*))
{
	int i, rval = -1;

	if(!stream)
	{
		strcpy(err_txt, "NULL pointer as stream id\n");
		return -1;
	}

	/* lookup the stream ID in the cache table */
	i = getCacheSlot(stream);
	if (i < 0)
		return -1;

	if(cache[i].fd == stream)
	{
		/* hook the users function into the steam filter */
		if(cache[i].filter_arg)
		{
			if(cache[i].filter_arg->state)
			{
				cache[i].filter_arg->state->cb = cb;
				rval = 0;
			}
			else
				strcpy(err_txt, "no cache[].filter_arg->state hook\n");
		}
		else
			strcpy(err_txt, "no cache[].filter_arg hook\n");
	}

	return rval;
}

/* parse the meta data block and copy all relevant */
/* information into the CSTATE structure */
void ShoutCAST_ParseMetaData(char *md, CSTATE *state)
{
	/* abort if we were submitted a NULL pointer */
	if((!md) || (!state))
		return;

	dprintf(stderr, "ShoutCAST_ParseMetaData(%p : %s, %p)\n", md, md, state);

	char *ptr = strstr(md, "StreamTitle=");

	if(ptr)
	{
		/* look if there is a dash or a comma that separates artist and title */
		ptr = strstr(md, " - ");

		if(!ptr)
			ptr = strstr(md, ", ");

		const int bufsize = 4095;
		/* no separator, simply copy everything into the 'title' field */
		if(!ptr)
		{
			ptr = strchr(md, '=');
			strncpy(state->title, ptr + 2, bufsize);
			state->title[bufsize] = '\0';
			ptr = strchr(state->title, ';');
			if(ptr)
				*(ptr - 1) = 0;
			state->artist[0] = 0;
		}
		else
		{
			//SKIP()
			for(int i = 0;(ptr && i < bufsize && *ptr && !isalnum(*ptr)); ++ptr,i++){};

			strncpy(state->title, ptr,bufsize);
			ptr = strchr(state->title, ';');
			if(ptr)
				*(ptr - 1) = 0;

			ptr = strstr(md, "StreamTitle=");
			ptr = strchr(ptr, '\'');
			strncpy(state->artist, ptr + 1, bufsize);
			state->artist[bufsize] = '\0';
			ptr = strstr(state->artist, " - ");
			if(!ptr)
				ptr = strstr(state->artist, ", ");

			if(ptr)
				*ptr = 0;
		}
		state->state = RUNNING;
	}
}

void ShoutCAST_DestroyFilter(void *a)
{
	STREAM_FILTER *arg = (STREAM_FILTER*)a;

	if(arg->state)
		free(arg->state), arg->state = NULL;
	if(arg->user)
		free(arg->user),  arg->user  = NULL;
}

STREAM_FILTER *ShoutCAST_InitFilter(int meta_int)
{
	STREAM_FILTER *arg;

	arg = (STREAM_FILTER*)calloc(1, sizeof(STREAM_FILTER));

	/* allocate our private data space, hook it into the */
	/* stream filter structure and initialize the variables */
	if(!arg->user)
	{
		arg->user = calloc(1, sizeof(FILTERDATA));
		arg->state = (CSTATE*)calloc(1, sizeof(CSTATE));
		((FILTERDATA*)arg->user)->meta_int = meta_int;
		arg->destructor = ShoutCAST_DestroyFilter;
	}

	return arg;
}

void ShoutCAST_MetaFilter(STREAM_FILTER *arg)
{

	/* bug trap */
	if(!arg)
		return;

	FILTERDATA *filterdata = (FILTERDATA*)arg->user;
	int meta_int = filterdata->meta_int;
	int len = *arg->len;
	if(len < 0){
		dprintf(stderr, "[%s] : error ---> len %i < 0\n",__func__, len);
		return;
	}
	char*buf = (char*)arg->buf;
	int meta_start;

#if 0
	dprintf(stderr, "filter : cnt      : %d\n", filterdata->cnt);
	dprintf(stderr, "filter : len      : %d\n", filterdata->len);
	dprintf(stderr, "filter : stored   : %d\n", filterdata->stored);
	dprintf(stderr, "filter : cnt + len: %d\n", filterdata->cnt + len);
	dprintf(stderr, "filter : meta_int : %d\n", filterdata->meta_int);
#endif
	/* not yet all meta data has been processed */
	if(filterdata->stored < filterdata->len)
	{
		int bsize = (filterdata->len + 1) - filterdata->stored;
		dprintf(stderr, "filterdata->len %i bsize %i len %i\n",filterdata->len,bsize,len);
			/*check overload size*/
			if(bsize > len){
				dprintf(stderr, "[%s] : error ---> bsize %i > len %i\n",__func__,bsize, len);
				return;
			}

		/* if there is some meta data, extract it */
		/* there can be zero size blocks too */
		if(filterdata->len)
		{
			dprintf(stderr, "filter : ********* partitioned metadata block part 2 ******\n");

			memmove(filterdata->meta_data + filterdata->stored, buf, bsize);

			ShoutCAST_ParseMetaData(filterdata->meta_data, arg->state);

			/* call the users callback function */
			if(arg->state->cb)
				arg->state->cb(arg->state);

			//dprintf(stderr, "filter : metadata : \n\n\n----------\n%s\n----------\n\n\n", filterdata->meta_data);

			/* remove the metadata and it's size indicator from the buffer */
			memmove(buf, buf + bsize, len - bsize);

			*arg->len -= bsize;
			filterdata->cnt = len - bsize;
			filterdata->stored = 0;
			filterdata->len = 0;
		}
		return;
	}

	if((filterdata->cnt < meta_int) && ((filterdata->cnt + len) <= meta_int))
	{
		/* do nothing; leave the data block and the length variable */
		/* untouched, update our private counter and return */
		filterdata->cnt += len;
		return;
	}

	/* does a meta data block start in the current block ? */
	if((filterdata->cnt <= meta_int) && ((filterdata->cnt + len) > meta_int))
	{
		meta_start = meta_int - filterdata->cnt;

		/* the first byte of the meta data block tells us how long it is */
		filterdata->len = 16 * (int)buf[meta_start];

		dprintf(stderr, "filter : ---> metadata %d bytes @ %d\n", filterdata->len, meta_start);

		/****************************************************************/
		/* case A: the meta data is completely within the current block */
		/****************************************************************/
		if((meta_start + filterdata->len) <= len)
		{
			int b = meta_start + filterdata->len + 1;

			/* if there is some meta data, extract it; */
			/* there can be zero size blocks too */
			if(filterdata->len)
			{
				memset(filterdata->meta_data, 0, 4096);
				memmove(filterdata->meta_data, buf + meta_start, filterdata->len);

				ShoutCAST_ParseMetaData(filterdata->meta_data, arg->state);

				/* call the users callback function */
				if(arg->state->cb)
					arg->state->cb(arg->state);

				//dprintf(stderr, "filter : metadata : \n\n\n----------\n%s\n----------\n\n\n", filterdata->meta_data);
			}
			/*check negative size*/
			if(len - b < 0)
			{
				dprintf(stderr, "[%s] : error ---> len - b %i\n",__func__,len-b);
				return;
			}
			/* remove the metadata and it's size indicator from the buffer */
			memmove(buf + meta_start, buf + b, len - b );

			/* adjust the buffersize */
			*arg->len -= filterdata->len + 1;
			filterdata->cnt = len - b;
			filterdata->stored = 0;
			filterdata->len = 0;
		}

		/************************************************************************/
		/* case B: the meta data is partitioned and continues in the next block */
		/************************************************************************/
		else
		{
			dprintf(stderr, "filter : ********* partitioned metadata block part 1 ******\n");

			/* if there is some meta data, extract it */
			/* there can be zero size blocks too */
			if(filterdata->len)
			{
				memset(filterdata->meta_data, 0, 4096);
				filterdata->stored = len - meta_start;
				memmove(filterdata->meta_data, buf + meta_start, filterdata->stored);
			}

			*arg->len = meta_start;
			filterdata->cnt = 0;
		}
	}
}

/**************************** utility functions ******************************/
void parseURL_url(URL& url) {
	/* now lets see what we have ... */
	char buffer[2048];
//	printf("parseURL_url: %s\n", url.url);
	char *ptr = strstr(url.url, "://");
	if (!ptr)
	{
		url.access_mode = MODE_FILE;
		strcpy(url.file, url.url);
		url.host[0] = 0;
		url.port = 0;
		url.logindata[0] = 0;
	}
	else
	{
		strcpy(url.host, ptr + 3);

		/* select the appropriate transport modes */
		transport("http",  MODE_HTTP, HTTP11);
		transport("icy",   MODE_HTTP, SHOUTCAST);
		transport("scast", MODE_SCAST, SHOUTCAST);
		//findme
		transport("icast", MODE_ICAST, SHOUTCAST);

		/* if we fetch a playlist file, then set the access mode */
		/* that it will be parsed and processed automatically. If */
		/* it does not fail, then the call returns with an opened stream */

/* this currently results in an endless loop due to recursive calls of f_open()
    if((url.access_mode == HTTP11) && (strstr(url.url, ".pls")))
    {
      url.access_mode = MODE_PLS;
      url.proto_version = SHOUTCAST;
    }
*/

		/* extract the file path from the url */
		ptr = strchr(ptr + 3, '/');
		if(ptr)
			strcpy(url.file, ptr);
		else
			strcpy(url.file, "/");

		/* extract the host part from the url */
		ptr = strchr(url.host, '/');
		if(ptr)
			*ptr = 0;

		if ((ptr = strchr(url.host, '@')))
		{
			*ptr = 0;
			base64_encode(buffer, url.host);
			strcpy(url.logindata, buffer);

			strcpy(buffer, ptr + 1);
			strcpy(url.host, buffer);
		}
		else
			url.logindata[0] = 0;

		ptr = strrchr(url.host, ':');

		if(ptr)
		{
			url.port = atoi(ptr + 1);
			*ptr = 0;
		}
	}
}

int base64_encode(char *dest, const char *src)
{
	int retval = 1;
	int src_len, src_pos;
	char symbols[65];
	char mypart[3];
	char mybits[24];
	int part_pos;
	int null_bytes = 0;

	if (dest != NULL && src != NULL && (src_len = strlen(src)) > 0)
	{
		strcpy(symbols,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
		char buffer[src_len*2];
		char *buf_ptr = buffer;

		for (src_pos = 0; src_pos < src_len; src_pos +=3)
		{
			for (part_pos = 0; part_pos < 3; part_pos++)
			{
				if (src[src_pos+part_pos] != 0)
					mypart[part_pos] = src[src_pos+part_pos];
				else
				{
					null_bytes = 3 - part_pos;
					mypart[part_pos] = 0;
					if (part_pos < 2) /* here: part_pos == 1, cannot be 0 */
						mypart[part_pos+1] = 0;
					break;
				}
			}
			for (part_pos = 0; part_pos < 24; part_pos++)
				mybits[part_pos] = ( mypart[part_pos/8] >> (7 - (part_pos%8)) ) & 0x1;
			for (part_pos = 0; part_pos < 24; part_pos+=6)
			{
				*buf_ptr = (mybits[part_pos] << 5) |
					(mybits[part_pos+1] << 4) | (mybits[part_pos+2] << 3) | (mybits[part_pos+3] << 2) |
					(mybits[part_pos+4] << 1) | mybits[part_pos+5];
				buf_ptr++;
			}
		}
		for (part_pos = 0 ; part_pos < buf_ptr - buffer - null_bytes; part_pos++)
			dest[part_pos] = symbols[(int)buffer[part_pos]];
		for (part_pos = buf_ptr - buffer - null_bytes ; part_pos < buf_ptr - buffer; part_pos++)
			dest[part_pos] = '=';
	}
	else
		retval = 0;

	return retval;
}

