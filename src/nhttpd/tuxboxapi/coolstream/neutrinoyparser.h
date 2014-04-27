//=============================================================================
// NHTTPD
// Neutrino yParser Extenstion
//=============================================================================

#ifndef __nhttpd_neutrinoyparser_h__
#define __nhttpd_neutrinoyparser_h__

// c++
#include <string>
// yhttpd
#include <yhttpd.h>
#include <ytypes_globals.h>
#include <mod_yparser.h>

// forward declaration
class CNeutrinoAPI;
//-----------------------------------------------------------------------------
class CNeutrinoYParser : public CyParser
{
private:
	// yParser funcs for Tuxbox
	typedef std::string (CNeutrinoYParser::*TyFunc)(CyhookHandler *hh, std::string para);
	typedef struct
	{
		const char *func_name;
		TyFunc pfunc;
	} TyFuncCall;
	const static TyFuncCall yFuncCallList[];

	// func TUXBOX
	std::string func_mount_get_list(CyhookHandler *hh, std::string para);
	std::string func_mount_set_values(CyhookHandler *hh, std::string para);
	std::string func_get_bouquets_as_dropdown(CyhookHandler *hh, std::string para);
	std::string func_get_bouquets_as_templatelist(CyhookHandler *hh, std::string para);
	std::string func_get_actual_bouquet_number(CyhookHandler *hh, std::string para);
	std::string func_get_channels_as_dropdown(CyhookHandler *hh, std::string para);
	std::string func_get_actual_channel_id(CyhookHandler *hh, std::string para);
	std::string func_get_bouquets_with_epg(CyhookHandler *hh, std::string para);
	std::string func_get_mode(CyhookHandler *hh, std::string para);
	std::string func_get_video_pids(CyhookHandler *hh, std::string para);
	std::string func_get_radio_pid(CyhookHandler *hh, std::string para);
	std::string func_get_audio_pids_as_dropdown(CyhookHandler *hh, std::string para);
	std::string func_unmount_get_list(CyhookHandler *hh, std::string para);
	std::string func_get_partition_list(CyhookHandler *hh, std::string para);
	std::string func_get_current_stream_info(CyhookHandler *hh, std::string para);
	std::string func_get_timer_list(CyhookHandler *hh, std::string para);
	std::string func_set_timer_form(CyhookHandler *hh, std::string para);
	std::string func_bouquet_editor_main(CyhookHandler *hh, std::string para);
	std::string func_set_bouquet_edit_form(CyhookHandler *hh, std::string para);

protected:
	CNeutrinoAPI	*NeutrinoAPI;

public:
	// constructor & deconstructor
	CNeutrinoYParser(CNeutrinoAPI	*_NeutrinoAPI);
	virtual ~CNeutrinoYParser(void);

	// virtual functions for BaseClass
	virtual std::string 	YWeb_cgi_func(CyhookHandler *hh, std::string ycmd);
	// virtual functions for HookHandler/Hook
	virtual std::string 	getHookName(void) {return std::string("mod_NeutrinoYParser-Coolstream");}
	virtual std::string 	getHookVersion(void) {return std::string("$Revision$");}
	virtual THandleStatus	Hook_SendResponse(CyhookHandler *hh);
	virtual THandleStatus 	Hook_ReadConfig(CConfigFile *Config, CStringList &ConfigList);

	// func TUXBOX
	std::string func_get_boxtype(CyhookHandler *hh, std::string para);
	std::string func_get_boxmodel(CyhookHandler *hh, std::string para);
};

#endif /*__nhttpd_neutrinoyparser_h__*/
