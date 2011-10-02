#ifndef __LT_DEBUG_H
#define __LT_DEBUG_H

#define TRIPLE_DEBUG_AUDIO	0
#define TRIPLE_DEBUG_VIDEO	1
#define TRIPLE_DEBUG_DEMUX	2
#define TRIPLE_DEBUG_PLAYBACK	3
#define TRIPLE_DEBUG_PWRMNGR	4
#define TRIPLE_DEBUG_INIT	5
#define TRIPLE_DEBUG_CA		6
#define TRIPLE_DEBUG_RECORD	7
#define TRIPLE_DEBUG_ALL	((1<<8)-1)

extern int debuglevel;

void _lt_debug(int facility, const void *, const char *fmt, ...);
void _lt_info(int facility, const void *, const char *fmt, ...);
void lt_debug_init(void);
#endif
