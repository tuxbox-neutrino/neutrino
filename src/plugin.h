
#ifndef TPLUGIN_H
#define TPLUGIN_H

typedef struct _PluginParam
{
	const char          * id;
	char                * val;
	struct _PluginParam * next;

} PluginParam;

typedef int	(*PluginExec)( PluginParam *par );
/* das dlsym kann auf PluginExec gecastet werden */

/* NOTE : alle Plugins haben uebergangs-weise neue und alte schnittstelle */
/* neues Symbol : plugin_exec */
/* es muessen nur benutzte ids gesetzt werden : nicht genannt = nicht benutzt */

/* fixed ID definitions */
#define	P_ID_FBUFFER	"fd_framebuffer"
#define	P_ID_RCINPUT	"fd_rcinput"
#define	P_ID_LCD		"fd_lcd"
#define	P_ID_NOPIG		"no_pig"		// 1: plugin dont show internal pig
#define P_ID_VTXTPID	"pid_vtxt"
#define P_ID_PROXY		"proxy"			// set proxy for save into highscore
#define P_ID_PROXY_USER	"proxy_user"	// format "user:pass"
#define P_ID_HSCORE		"hscore"		// highscore-server (as url)
#define P_ID_VFORMAT	"video_format"	// videoformat (0 = auto, 1 = 16:9, 2 = 4:3)
#define P_ID_OFF_X		"off_x"			// screen-top-offset x
#define P_ID_OFF_Y		"off_y"			// screen-top-offset y
#define P_ID_END_X		"end_x"			// screen-end-offset x
#define P_ID_END_Y		"end_y"			// screen-end-offset y
#define	P_ID_RCBLK_ANF	"rcblk_anf"		// Key-Repeatblocker Anfang
#define	P_ID_RCBLK_REP	"rcblk_rep"     // Key-Repeatblocker Wiederholung

typedef enum plugin_type
{
	PLUGIN_TYPE_DISABLED = 0,
	PLUGIN_TYPE_GAME     = 1,
	PLUGIN_TYPE_TOOL     = 2,
	PLUGIN_TYPE_SCRIPT   = 3
}
plugin_type_t;

#endif
