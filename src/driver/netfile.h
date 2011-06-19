/******************************************************************/
/* netfile - a URL capable wrapper for the fopen() call           */
/*                                                                */
/* usage: include 'netfile.h' in your code as *LAST* file         */
/* after all other include files and add netfile.c to your        */
/* sources. That's it. The include file maps the common           */
/* fopen() call onto a URL capable version that handles           */
/* files hosted on http servers like local ones.                  */
/*                                                                */
/* http example:                                                  */
/*	fd = fopen("http://find.me:666/somewhere/foo.bar", "r");  */
/*                                                                */
/* shoutcast example:                                             */
/*	fd = fopen("icy://find.me:666/funky/station/", "r");      */
/*                                                                */
/* shoutcast example: (opens the shoutcast station 666 directly)  */
/*	fd = fopen("scast://666", "r");                           */
/*                                                                */
/* NOTE: only read access is implemented !                        */
/******************************************************************/
#ifndef NETFILE_H
#define NETFILE_H 1

/* whether we should open an *.url file as plain file or the url */
/* contained in it instead */
//#define DISABLE_URLFILES 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>

#define dprintf if(debug) fprintf

/* file access modes */
#define ACC_RO		0
#define ACC_RW	1
#define ACC_UD		2	/* -> this is used for the 'r+' mode */
#define ACC_WO	3

#define MODE_FILE	0
#define MODE_HTTP	1
#define MODE_SCAST	2	/* pseudo transfer mode; is actually HTTP/SHOUTCAST */
#define MODE_ICAST	3	/* pseudo transfer mode; is actually HTTP/SHOUTCAST */
#define MODE_PLS	4	/* pseudo transfer mode; is actually HTTP/SHOUTCAST */

#define HTTP10		0
#define HTTP11		1
#define SHOUTCAST	2

#define MAX_REDIRECTS 	5  /* follow this amount of redirects */

#define CONNECTING		1	/* not used */
#define BUFFERING		2	/* not used */
#define RUNNING		3

/* map all fopen() calls onto out f_open() function */
#define fopen	f_open
#define fclose	f_close
#define fread	f_read
#define ftell	f_tell
#define rewind	f_rewind
#define fseek	f_seek
#define fstatus	f_status
#define ftype	f_type

extern FILE	*f_open(const char *, const char *);
extern int		f_close(FILE *);
extern size_t	f_read (void *, size_t, size_t, FILE *);
extern long	f_tell(FILE *);
extern void	f_rewind(FILE *);
extern int		f_seek(FILE *, long, int);
extern int		f_status(FILE *, void (*)(void*));
extern const char	*f_type(FILE*, const char*);

extern char err_txt[2048];

#define CACHESIZE	cache_size
#define CACHEENTMAX	20	/* at most 20 caches are available */
#define CACHEBTRANS	1024	/* blocksize for the stream-to-cache transfer */

typedef struct
{
	int	access_mode;	/* access mode; FILE or HTTP */
	int	proto_version;	/* 0= 1.0; 1 = 1.1; 2 = shoutcast */
	char	url[2048];		/* universal resource locator */	
	char	host[2048];
	int	port;
	char	file[2048];
	char	entity[2048];	/* data to send on POST requests */
	int	fd;			/* filedescriptor of the file*/
	FILE	*stream;		/* streamdescriptor */
	char	logindata[2048];	/* base64 encoded auhtentication string of "username:password" */
} URL;

typedef struct
{
	void	(*cb)(void *);		/* user provided callback function */
	void	*user;			/* user date hook point */
	int	state;			/* CONNECTING, BUFFERING, RUNNING */
	int	bitrate;
	int	buffered;			/* "waterlevel" in the cache; 0 ... 65535 */
	char 	station_url[1024];	/*station url */
	char	station[1024];		/* station name */
	char	genre[4096];		/* station genre */
	char	artist[4096];		/* artist currently playing */
	char	title[4096];			/* title currently playing */
} CSTATE;

typedef struct
{
	void	*buf;		/* start of the buffer */
	int	*len;		/* pointer to a variable containing the length of the buffer */
	void	*arg;		/* pointer to some arguments for the filter function */
	void	*user;	/* here the filter function can hook in */
				/* some private data */
	void 	(*destructor)(void*);	/* stream filter destructor */

	CSTATE 	*state;
} STREAM_FILTER;

typedef struct
{
	int		cnt;			/* counter */
	int		len;			/* meta block length */
	int		stored;		/* number of bytes already stored */
	int		meta_int;		/* meta data intervall */
	char	meta_data[4096];	/* meta blocks cam be at most 4096 bytes */
} FILTERDATA;

typedef struct
{
	FILE		*fd;			/* stream ID */

	int 		acc_mode;		/* ACC_RO, ACC_RW, ACC_UD (unused yet) */

	char	*cache;			/* cache buffer */
	char	*ceiling;			/* cache ceiling */
	int		csize;		/* cache size */
	char	*wptr;			/* next write position */
	char	*rptr;				/* next read position */
	long 	filled;
	int   	closed;			/* flag; closed = 1 if supply thread */
	/* has been terminated due to a */
	/* disrupted incoming stream or an EOF */
	long total_bytes_delivered;

	pthread_t fill_thread;
	pthread_attr_t attr;
	pthread_mutex_t cache_lock;
	pthread_mutexattr_t cache_lock_attr;

	pthread_mutex_t readable;
	pthread_mutexattr_t readable_attr;

	pthread_mutex_t writeable;
	pthread_mutexattr_t writeable_attr;

	void (*filter)(STREAM_FILTER*);	/* stream filter function */

	STREAM_FILTER *filter_arg;	/* place to hook in a pointer to the arguments */
} STREAM_CACHE;

typedef struct
{
	FILE	*stream;
	char	type[65];
} STREAM_TYPE;

typedef struct
{
	char	magic[3];	/* "ID3" */
	char version[2];	/* version of the tag */
	char flags;
	char size[4];
	char base[1024]; 
	int len;
} ID3;

typedef  struct
{
	char id[5];
	uint32_t size;
	char flags[3];
	char base[2048];
} ID3_frame;

#define CRLFCut(a) \
{ \
  char *_ptr; \
  _ptr = strchr(a, '\r'); \
  if(_ptr) *_ptr = 0; \
  else { \
  _ptr = strchr(a, '\n'); \
  if(_ptr) *_ptr = 0; } \
}

#ifdef __cplusplus
}
#endif

#endif
