#ifndef __teletext_h__
#define __teletext_h__

extern int tuxtxt_init();
extern void tuxtxt_close();
extern void tuxtxt_start(int tpid);  // Start caching
extern int  tuxtxt_stop(); // Stop caching
extern int tuxtx_main(int _rc, int pid, int page = 0);
void tuxtx_stop_subtitle();
int tuxtx_subtitle_running(int pid, int page, int *running);
void tuxtx_pause_subtitle(bool pause = 1);

#endif
