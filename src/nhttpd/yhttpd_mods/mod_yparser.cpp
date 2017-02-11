//=============================================================================
// YHTTPD
// yParser
//=============================================================================
// C
#include <config.h>

#include <cstdlib>
#include <cstring>
#include <stdio.h>
#include <cctype>
// C++
#include <iostream>
#include <fstream>
#include <map>
// system
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
// tuxbox
#include <global.h>
#include <configfile.h>
#include <system/helpers.h>
// yhttpd
#include <yconfig.h>
#include <ytypes_globals.h>
#include <helper.h>
#include <ylogging.h>
#include "mod_yparser.h"
#include <ylanguage.h>

//=============================================================================
// Initialization of static variables
//=============================================================================
pthread_mutex_t CyParser::yParser_mutex = PTHREAD_MUTEX_INITIALIZER;
;
std::string CyParser::HTML_DIRS[HTML_DIR_COUNT];
std::string CyParser::PLUGIN_DIRS[PLUGIN_DIR_COUNT];
std::map<std::string, std::string> CyParser::ycgi_global_vars;
struct stat CyParser::yCached_blocks_attrib;
std::string CyParser::yCached_blocks_filename;
std::string CyParser::yCached_blocks_content;

//=============================================================================
// Constructor & Destructor
//=============================================================================
CyParser::CyParser() {
	yConfig = new CConfigFile(',');
	yCached_blocks_attrib.st_mtime = 0;
}
//-----------------------------------------------------------------------------
CyParser::~CyParser(void) {
	if (yConfig != NULL)
		delete yConfig;
}
//-----------------------------------------------------------------------------
void CyParser::init(CyhookHandler *hh) {
	if (HTML_DIRS[0].empty()) {
		CyParser::HTML_DIRS[0] = hh->WebserverConfigList["WebsiteMain.override_directory"];
		HTML_DIRS[1] = hh->WebserverConfigList["WebsiteMain.directory"];
		PLUGIN_DIRS[0]=PLUGIN_DIRS[1] = HTML_DIRS[0];
		PLUGIN_DIRS[1].append("/scripts");
		PLUGIN_DIRS[2]=PLUGIN_DIRS[3] = HTML_DIRS[1];
		PLUGIN_DIRS[3].append("/scripts");
	}
}
//=============================================================================
// Main Dispatcher / Call definitions for /y/<dispatch>
//=============================================================================
const CyParser::TyCgiCall CyParser::yCgiCallList[] = {
			{ "cgi", 			&CyParser::cgi, 				"text/html; charset=UTF-8" },
#ifdef Y_CONFIG_FEATURE_SHOW_SERVER_CONFIG
			{ "server-config", 	&CyParser::cgi_server_config, 	"text/html"},
#endif
		};
//-----------------------------------------------------------------------------
// HOOK: response_hook Handler
// This is the main dispatcher for this module
//-----------------------------------------------------------------------------
THandleStatus CyParser::Hook_SendResponse(CyhookHandler *hh) {
	hh->status = HANDLED_NONE;

	log_level_printf(4, "yparser hook start url:%s\n",
			hh->UrlData["url"].c_str());
	init(hh);

	CyParser *yP = new CyParser(); // create a Session
	if (hh->UrlData["fileext"] == "yhtm" || hh->UrlData["fileext"] == "yjs" || hh->UrlData["fileext"] == "ysh") // yParser for y*-Files
		yP->ParseAndSendFile(hh);
	else if (hh->UrlData["path"] == "/y/") // /y/<cgi> commands
	{
		yP->Execute(hh);
		if (hh->status == HANDLED_NOT_IMPLEMENTED)
			hh->status = HANDLED_NONE; // y-calls can be implemented anywhere
	}
	delete yP;

	//	log_level_printf(4,"yparser hook end status:%d\n",(int)hh->status);
	//	log_level_printf(5,"yparser hook result:%s\n",hh->yresult.c_str());

	return hh->status;
}

//-----------------------------------------------------------------------------
// URL Function Dispatching
//-----------------------------------------------------------------------------
void CyParser::Execute(CyhookHandler *hh) {
	int index = -1;
	std::string filename = hh->UrlData["filename"];

	log_level_printf(4, "yParser.Execute filename%s\n", filename.c_str());
	filename = string_tolower(filename);

	// debugging informations
	if (CLogging::getInstance()->getDebug()) {
		dprintf("Execute CGI : %s\n", filename.c_str());
		for (CStringList::iterator it = hh->ParamList.begin(); it
				!= hh->ParamList.end(); ++it)
			dprintf("  Parameter %s : %s\n", it->first.c_str(),
					it->second.c_str());
	}

	// get function index
	for (unsigned int i = 0; i < (sizeof(yCgiCallList)
			/ sizeof(yCgiCallList[0])); i++)
		if (filename == yCgiCallList[i].func_name) {
			index = i;
			break;
		}
	if (index == -1) // function not found
	{
		hh->SetError(HTTP_NOT_IMPLEMENTED, HANDLED_NOT_IMPLEMENTED);
		return;
	}

	// send header
	if (std::string(yCgiCallList[index].mime_type).empty()) // set by self
		;
	else if (std::string(yCgiCallList[index].mime_type) == "+xml") // Parameter xml?
		if (!hh->ParamList["xml"].empty())
			hh->SetHeader(HTTP_OK, "text/xml");
		else
			hh->SetHeader(HTTP_OK, "text/plain");
	else
		hh->SetHeader(HTTP_OK, yCgiCallList[index].mime_type);
	// response
	hh->status = HANDLED_READY;
	if (hh->Method == M_HEAD) // HEAD or function call
		return;
	else {
		(this->*yCgiCallList[index].pfunc)(hh);
		return;
	}
}

//=============================================================================
// URL Functions
//=============================================================================
//-----------------------------------------------------------------------------
// mini cgi Engine (Entry for ycgi)
//-----------------------------------------------------------------------------
void CyParser::cgi(CyhookHandler *hh) {
	std::string htmlfilename, yresult, ycmd;

	if ( !hh->ParamList.empty() ) {
		if (!hh->ParamList["tmpl"].empty()) // for GET and POST
			htmlfilename = hh->ParamList["tmpl"];
		else
			htmlfilename = hh->ParamList["1"];
		bool ydebug = false;
		if (!hh->ParamList["debug"].empty()) // switch debug on
			ydebug = true;

		if (!hh->ParamList["execute"].empty()) // execute done first!
		{
			ycmd = hh->ParamList["execute"];
			ycmd = YPARSER_ESCAPE_START + ycmd + YPARSER_ESCAPE_END;
			yresult = cgi_cmd_parsing(hh, ycmd, ydebug); // parsing engine
		}
		// parsing given file
		if (!htmlfilename.empty())
			yresult = cgi_file_parsing(hh, htmlfilename, ydebug);
	} else
		printf("[CyParser] Y-cgi:no parameter given\n");
	if (yresult.empty())
		hh->SetError(HTTP_NOT_IMPLEMENTED, HANDLED_NOT_IMPLEMENTED);
	else
		hh->addResult(yresult, HANDLED_READY);
}
//-----------------------------------------------------------------------------
// URL: server-config
//-----------------------------------------------------------------------------
#ifdef Y_CONFIG_FEATURE_SHOW_SERVER_CONFIG
void CyParser::cgi_server_config(CyhookHandler *hh)
{
	std::string yresult;

	hh->SendHTMLHeader("Webserver Configuration");
	yresult += string_printf("<b>Webserver Config</b><br/>\n");
	yresult += string_printf("Webserver \"%s\"<br/>\n",WEBSERVERNAME);
	yresult += string_printf("<br/><b>CONFIG (compiled)</b><br/>\n");
	yresult += string_printf("Config File: %s<br/>\n", HTTPD_CONFIGFILE);
	yresult += string_printf("Upload File: %s<br/>\n", UPLOAD_TMP_FILE);
	yresult += string_printf("<br/><b>CONFIG (Config File)</b><br/>\n");

	// Cofig file
	yresult += string_printf("<table border=\"1\">\n");
	CStringList::iterator i = (hh->WebserverConfigList).begin();
	for (; i!= (hh->WebserverConfigList).end(); i++ )
	{
		if( ((*i).first) != "mod_auth.username" && ((*i).first) != "mod_auth.password")
		yresult += string_printf("<tr><td>%s</td><td>%s</td></tr>\n", ((*i).first).c_str(), ((*i).second).c_str());
	}
	yresult += string_printf("</table>\n");
	// hook list
	yresult += string_printf("<br/><b>Hooks (Compiled)</b><br/>\n");
	yresult += string_printf("<table border=\"1\">\n");
	THookList::iterator j = hh->HookList.begin();
	for (; j!= hh->HookList.end(); j++ )
	{
		yresult += string_printf("<tr><td>%s</td><td>%s</td></tr>\n", ((*j)->getHookName()).c_str(), ((*j)->getHookVersion()).c_str());
	}
	yresult += string_printf("</table>\n");

	hh->addResult(yresult, HANDLED_READY);
	hh->SendHTMLFooter();
}
#endif
//=============================================================================
// y Parsing and sending .yhtm Files (Main ENTRY)
//=============================================================================
void CyParser::ParseAndSendFile(CyhookHandler *hh) {
	bool ydebug = false;
	std::string yresult, ycmd;
	log_level_printf(3, "yParser.ParseAndSendFile: File: %s\n",
			(hh->UrlData["filename"]).c_str());

	hh->SetHeader(HTTP_OK, "text/html; charset=UTF-8");
	if (hh->Method == M_HEAD)
		return;
	if (!hh->ParamList["debug"].empty()) // switch debug on
		ydebug = true;
	if (!hh->ParamList["execute"].empty()) // execute done first!
	{
		ycmd = hh->ParamList["execute"];
		ycmd = YPARSER_ESCAPE_START + ycmd + YPARSER_ESCAPE_END;
		log_level_printf(3, "<yParser.ParseAndSendFile>: Execute!: %s\n",
				ycmd.c_str());
		yresult = cgi_cmd_parsing(hh, ycmd, ydebug); // parsing engine
	}
	// parsing given file
	yresult += cgi_file_parsing(hh, hh->UrlData["path"]+hh->UrlData["filename"], ydebug);
	if (yresult.empty())
		hh->SetError(HTTP_NOT_IMPLEMENTED, HANDLED_NOT_IMPLEMENTED);
	else {
		hh->addResult(yresult, HANDLED_READY);
		if (!ycgi_vars["cancache"].empty()) {
			hh->HookVarList["CacheCategory"] = ycgi_vars["cancache"];
			hh->HookVarList["CacheMimeType"] = hh->ResponseMimeType;
			hh->status = HANDLED_CONTINUE;
		}
	}
}

//=============================================================================
// y Parsing helpers
//=============================================================================
//-----------------------------------------------------------------------------
// mini cgi Engine (file parsing)
//-----------------------------------------------------------------------------
std::string CyParser::cgi_file_parsing(CyhookHandler *hh,
		std::string htmlfilename, bool ydebug) {
	bool found = false;
	std::string htmlfullfilename, yresult, html_template;

	bool isHosted = false;
#ifdef Y_CONFIG_USE_HOSTEDWEB
	// for hosted webs: search in hosted_directory only
	std::string _hosted=hh->WebserverConfigList["WebsiteMain.hosted_directory"];
	if((hh->UrlData["path"]).compare(0,_hosted.length(),hh->WebserverConfigList["WebsiteMain.hosted_directory"]) == 0) // hosted Web ?
		isHosted = true;
#endif //Y_CONFIG_USE_HOSTEDWEB

	char cwd[255];
	getcwd(cwd, 254);
	for (unsigned int i = 0; i < (isHosted ? 1 : HTML_DIR_COUNT) && !found; i++) {
		htmlfullfilename = (isHosted ? "" : HTML_DIRS[i]) + "/" + htmlfilename;
		std::fstream fin(htmlfullfilename.c_str(), std::fstream::in);
		if (fin.good()) {
			found = true;
			chdir(HTML_DIRS[i].c_str()); // set working dir

			// read whole file into html_template
			std::string ytmp;
			while (!fin.eof()) {
				getline(fin, ytmp);
				html_template = html_template + ytmp + "\r\n";
			}
			yresult += cgi_cmd_parsing(hh, html_template, ydebug); // parsing engine
			fin.close();
		}
	}
	chdir(cwd);
	if (!found) {
		printf("[CyParser] Y-cgi:template %s not found %s\n", htmlfilename.c_str(), isHosted ? "" : "in");
		if (!isHosted)
			for (unsigned int i = 0; i < HTML_DIR_COUNT; i++) {
				printf("%s\n", HTML_DIRS[i].c_str());
			}
	}
	return yresult;
}

//-----------------------------------------------------------------------------
// main parsing (nested and recursive)
//-----------------------------------------------------------------------------
std::string CyParser::cgi_cmd_parsing(CyhookHandler *hh,
		std::string html_template, bool ydebug) {
	std::string::size_type start, end;
	unsigned int esc_len = strlen(YPARSER_ESCAPE_START);
	bool is_cmd;
	std::string ycmd, yresult;

	do // replace all {=<cmd>=} nested and recursive
	{
		is_cmd = false;
		if ((end = html_template.find(YPARSER_ESCAPE_END)) != std::string::npos) // 1. find first y-end
		{
			if (ydebug)
				hh->printf("[ycgi debug]: END at:%d following:%s<br/>\n", end,
						(html_template.substr(end, 10)).c_str());
			if ((start = html_template.rfind(YPARSER_ESCAPE_START, end))
					!= std::string::npos) // 2. find next y-start befor
			{
				if (ydebug)
					hh->printf("[ycgi debug]: START at:%d following:%s<br/>\n",
							start,
							(html_template.substr(start + esc_len, 10)).c_str());

				ycmd = html_template.substr(start + esc_len, end - (start
						+ esc_len)); //3. get cmd
				if (ydebug)
					hh->printf("[ycgi debug]: CMD:[%s]<br/>\n", ycmd.c_str());
				yresult = YWeb_cgi_cmd(hh, ycmd); // 4. execute cmd
				log_level_printf(5, "<yLoop>: ycmd...:%s\n", ycmd.c_str());
				log_level_printf(6, "<yLoop>: yresult:%s\n", yresult.c_str());
				if (ydebug)
					hh->printf("[ycgi debug]: RESULT:[%s]<br/>\n",
							yresult.c_str());
				html_template.replace(start, end - start + esc_len, yresult); // 5. replace cmd with output
				is_cmd = true; // one command found

				if (ydebug)
					hh->printf("[ycgi debug]: STEP<br/>\n%s<br/>\n",
							html_template.c_str());
			}
		}
	} while (is_cmd);
	return html_template;
}
//=============================================================================
// ycmd - Dispatching
//=============================================================================

//-----------------------------------------------------------------------------
// ycgi : cmd executing
//	comment:<y-comment>~<html-comment>
//	script:<scriptname without .sh>
//	include:<filename>
//	func:<funcname> (funcname to be implemented in CyParser::YWeb_cgi_func)
//	ini-get:<filename>;<varname>[;<default>][~open|cache]
//	ini-set:<filename>;<varname>;<value>[~open|save|cache]
//	if-empty:<value>~<then>~<else>
//	if-equal:<left_value>~<right_value>~<then>~<else> (left_value == right_value?)
//	if-not-equal:<left_value>~<right_value>~<then>~<else> (left_value == right_value?)
//	if-file-exists:<filename>~<then>~<else>
//	find-exec:<filename>
//	include-block:<filename>;<block-name>[;<default-text>]
//	var-get:<varname>
//	var-set:<varname>=<varvalue>
//	global-var-get:<varname>
//	global-var-set:<varname>=<varvalue>
//	file-action:<filename>;<action=add|addend|delete>[;<content>]
//	L:<translation-id>
//-----------------------------------------------------------------------------

std::string CyParser::YWeb_cgi_cmd(CyhookHandler *hh, std::string ycmd) {
	std::string ycmd_type, ycmd_name, yresult;

	if (ySplitString(ycmd, ":", ycmd_type, ycmd_name)) {
		if (ycmd_type == "L")
			yresult = CLanguage::getInstance()->getTranslation(ycmd_name);
		else if (ycmd_type == "comment") {
			std::string comment_y, comment_html;
			if (ySplitString(ycmd_name, "~", comment_y, comment_html)) {
				if (!comment_html.empty())
					yresult = "<!-- " + comment_html + " -->";
			}
		} else if (ycmd_type == "script")
			yresult = yExecuteScript(ycmd_name);
		else if (ycmd_type == "if-empty") {
			std::string if_value, if_then, if_else;
			if (ySplitString(ycmd_name, "~", if_value, if_then)) {
				ySplitString(if_then, "~", if_then, if_else);
				yresult = (if_value.empty()) ? if_then : if_else;
			}
		} else if (ycmd_type == "if-equal") {
			std::string if_left_value, if_right_value, if_then, if_else;
			if (ySplitString(ycmd_name, "~", if_left_value, if_right_value)) {
				if (ySplitString(if_right_value, "~", if_right_value, if_then)) {
					ySplitString(if_then, "~", if_then, if_else);
					yresult = (if_left_value == if_right_value) ? if_then
							: if_else;
				}
			}
		} else if (ycmd_type == "if-not-equal") {
			std::string if_left_value, if_right_value, if_then, if_else;
			if (ySplitString(ycmd_name, "~", if_left_value, if_right_value)) {
				if (ySplitString(if_right_value, "~", if_right_value, if_then)) {
					ySplitString(if_then, "~", if_then, if_else);
					yresult = (if_left_value != if_right_value) ? if_then
							: if_else;
				}
			}
		} else if (ycmd_type == "if-file-exists") {
			std::string if_value, if_then, if_else;
			if (ySplitString(ycmd_name, "~", if_value, if_then)) {
				ySplitString(if_then, "~", if_then, if_else);
				yresult = (access(if_value, R_OK) == 0) ? if_then
						: if_else;
			}
		} else if (ycmd_type == "find-exec") {
			yresult = find_executable(ycmd_name.c_str());
		} else if (ycmd_type == "include") {
			std::string ytmp;
			std::fstream fin(ycmd_name.c_str(), std::fstream::in);
			if (fin.good()) {
				while (!fin.eof()) {
					getline(fin, ytmp);
					yresult += ytmp + "\n";
				}
				fin.close();
			}
		} else if (ycmd_type == "include-block") {
			std::string filename, blockname, tmp, ydefault;
			if (ySplitString(ycmd_name, ";", filename, tmp)) {
				ySplitString(tmp, ";", blockname, ydefault);
				yresult = YWeb_cgi_include_block(filename, blockname, ydefault);
			}
		} else if (ycmd_type == "func")
			yresult = this->YWeb_cgi_func(hh, ycmd_name);
		else if (ycmd_type == "define-get") {
			     if (ycmd_name.compare("CONFIGDIR"))	yresult = CONFIGDIR;
			else if (ycmd_name.compare("DATADIR"))		yresult = DATADIR;
			else if (ycmd_name.compare("FONTDIR"))		yresult = FONTDIR;
			else if (ycmd_name.compare("LIBDIR"))		yresult = LIBDIR;
			else if (ycmd_name.compare("GAMESDIR"))		yresult = GAMESDIR;
			else if (ycmd_name.compare("PLUGINDIR"))	yresult = PLUGINDIR;
			else if (ycmd_name.compare("PLUGINDIR_VAR"))	yresult = PLUGINDIR_VAR;
			else if (ycmd_name.compare("WEBTVDIR_VAR"))	yresult = WEBTVDIR_VAR;
			else if (ycmd_name.compare("LUAPLUGINDIR"))	yresult = LUAPLUGINDIR;
			else if (ycmd_name.compare("LOCALEDIR"))	yresult = LOCALEDIR;
			else if (ycmd_name.compare("LOCALEDIR_VAR"))	yresult = LOCALEDIR_VAR;
			else if (ycmd_name.compare("THEMESDIR"))	yresult = THEMESDIR;
			else if (ycmd_name.compare("THEMESDIR_VAR"))	yresult = THEMESDIR_VAR;
			else if (ycmd_name.compare("ICONSDIR"))		yresult = ICONSDIR;
			else if (ycmd_name.compare("ICONSDIR_VAR"))	yresult = ICONSDIR_VAR;
			else if (ycmd_name.compare("LOGODIR"))		yresult = LOGODIR;
			else if (ycmd_name.compare("LOGODIR_VAR"))	yresult = LOGODIR_VAR;
			else if (ycmd_name.compare("PRIVATE_HTTPDDIR"))	yresult = PRIVATE_HTTPDDIR;
			else if (ycmd_name.compare("PUBLIC_HTTPDDIR"))	yresult = PUBLIC_HTTPDDIR;
			else if (ycmd_name.compare("HOSTED_HTTPDDIR"))	yresult = HOSTED_HTTPDDIR;
			else						yresult = "";
		}
		else if (ycmd_type == "ini-get") {
			std::string filename, varname, tmp, ydefault, yaccess;
			ySplitString(ycmd_name, "~", filename, yaccess);
			if (ySplitString(filename, ";", filename, tmp)) {
				ySplitString(tmp, ";", varname, ydefault);
				yresult = YWeb_cgi_get_ini(hh, filename, varname, yaccess);
				if (yresult.empty() && !ydefault.empty())
					yresult = ydefault;
			} else
				yresult = "ycgi: ini-get: no ; found";
		} else if (ycmd_type == "ini-set") {
			std::string filename, varname, varvalue, tmp, yaccess;
			ySplitString(ycmd_name, "~", filename, yaccess);
			if (ySplitString(filename, ";", filename, tmp)) {
				if (ySplitString(tmp, ";", varname, varvalue))
					YWeb_cgi_set_ini(hh, filename, varname, varvalue, yaccess);
			} else
				yresult = "ycgi: ini-get: no ; found";
		} else if (ycmd_type == "var-get") {
			yresult = ycgi_vars[ycmd_name];
		} else if (ycmd_type == "var-set") {
			std::string varname, varvalue;
			if (ySplitString(ycmd_name, "=", varname, varvalue))
				ycgi_vars[varname] = varvalue;
		} else if (ycmd_type == "global-var-get") {
			pthread_mutex_lock(&yParser_mutex);
			yresult = ycgi_global_vars[ycmd_name];
			pthread_mutex_unlock(&yParser_mutex);
		} else if (ycmd_type == "global-var-set") {
			std::string varname, varvalue;
			if (ySplitString(ycmd_name, "=", varname, varvalue)) {
				pthread_mutex_lock(&yParser_mutex);
				ycgi_global_vars[varname] = varvalue;
				pthread_mutex_unlock(&yParser_mutex);
			}
		} else if (ycmd_type == "file-action") {
			std::string filename, actionname, content, tmp;
			if (ySplitString(ycmd_name, ";", filename, tmp)) {
				ySplitString(tmp, ";", actionname, content);
				replace(content, "\r\n", "\n");
				if (actionname == "add") {
					std::fstream fout(filename.c_str(), std::fstream::out
							| std::fstream::binary);
					fout << content;
					fout.close();
				} else if (actionname == "append") {
					std::fstream fout(filename.c_str(), std::fstream::app
							| std::fstream::binary);
					fout << content;
					fout.close();
				} else if (actionname == "delete") {
					remove(filename.c_str());
				}
			}
		} else
			yresult = "ycgi-type unknown";
	} else if (!hh->ParamList[ycmd].empty()) {
#if 0
		if ((hh->ParamList[ycmd]).find("script") == std::string::npos)
			yresult = hh->ParamList[ycmd];
		else
			yresult = "<!--Not Allowed script in " + ycmd + " -->";
#else
		yresult = hh->ParamList[ycmd];
#endif
	}

	return yresult;
}

//-------------------------------------------------------------------------
// Get Value from ini/conf-file (filename) for var (varname)
// yaccess = open | cache
//-------------------------------------------------------------------------
std::string CyParser::YWeb_cgi_get_ini(CyhookHandler *, std::string filename,
		std::string varname, std::string yaccess) {
	std::string result;
	if ((yaccess == "open") || (yaccess.empty())) {
		yConfig->clear();
		yConfig->loadConfig(filename);
	}
	result = yConfig->getString(varname, "");
	return result;
}

//-------------------------------------------------------------------------
// set Value from ini/conf-file (filename) for var (varname)
// yaccess = open | cache | save
//-------------------------------------------------------------------------
void CyParser::YWeb_cgi_set_ini(CyhookHandler *, std::string filename,
		std::string varname, std::string varvalue, std::string yaccess) {
	if ((yaccess == "open") || (yaccess.empty())) {
		yConfig->clear();
		yConfig->loadConfig(filename);
	}
	yConfig->setString(varname, varvalue);
	if ((yaccess == "save") || (yaccess.empty()))
		yConfig->saveConfig(filename);
}

//-------------------------------------------------------------------------
// Get text block named <blockname> from file <filename>
// The textblock starts with "start-block~<blockname>" and ends with
// "end-block~<blockname>"
//-------------------------------------------------------------------------
std::string CyParser::YWeb_cgi_include_block(std::string filename,
		std::string blockname, std::string ydefault) {
	std::string ytmp, yfile, yresult;
	struct stat attrib;
	yresult = ydefault;

	stat(filename.c_str(), &attrib);

	pthread_mutex_lock(&yParser_mutex);
	if ((attrib.st_mtime == yCached_blocks_attrib.st_mtime) && (filename
			== yCached_blocks_filename)) {
		log_level_printf(6, "include-block: (%s) from cache\n",
				blockname.c_str());
		yfile = yCached_blocks_content;
	} else {
		bool found = false;
		for (unsigned int i = 0; i < HTML_DIR_COUNT && !found; i++) {
			std::string ifilename = HTML_DIRS[i] + "/" + filename;
			std::fstream fin(ifilename.c_str(), std::fstream::in);
			if (fin.good()) {
				found = true;
				while (!fin.eof()) // read whole file into yfile
				{
					getline(fin, ytmp);
					yfile += ytmp + "\n";
				}
				fin.close();
			}
		}
		yCached_blocks_content = yfile;
		yCached_blocks_attrib = attrib;
		yCached_blocks_filename = filename;
		log_level_printf(6, "include-block: (%s) from file\n",
				blockname.c_str());
	}
	pthread_mutex_unlock(&yParser_mutex);
	if (!yfile.empty()) {
		std::string t = "start-block~" + blockname;
		std::string::size_type start, end;
		if ((start = yfile.find(t)) != std::string::npos) {
			if ((end = yfile.find("end-block~" + blockname, start + t.length()))
					!= std::string::npos) {
				yresult = yfile.substr(start + t.length(), end - (start
						+ t.length()));
				log_level_printf(7, "include-block: (%s) yresult:(%s)\n",
						blockname.c_str(), yresult.c_str());
			} else
				aprintf(
						"include-blocks: Block END not found:%s Blockname:%s\n",
						filename.c_str(), blockname.c_str());
		} else
			aprintf("include-blocks: Block START not found:%s Blockname:%s\n",
					filename.c_str(), blockname.c_str());
	} else
		aprintf("include-blocks: file not found:%s Blockname:%s\n",
				filename.c_str(), blockname.c_str());

	return yresult;
}

//=============================================================================
// y-func : Dispatching
// TODO: new functions for
//	- Compiler-Time Settings like *httpd.conf etc
//	- Versions of httpd, yParser, Hooks,
//=============================================================================

const CyParser::TyFuncCall CyParser::yFuncCallList[] = {
		{ "get_request_data", 			&CyParser::func_get_request_data },
		{ "get_header_data", 			&CyParser::func_get_header_data },
		{ "get_config_data", 			&CyParser::func_get_config_data },
		{ "do_reload_httpd_config", 	&CyParser::func_do_reload_httpd_config },
		{ "httpd_change", 				&CyParser::func_change_httpd },
		{ "get_languages_as_dropdown",	&CyParser::func_get_languages_as_dropdown },
		{ "set_language",				&CyParser::func_set_language },
};

//-------------------------------------------------------------------------
// y-func : dispatching and executing
//-------------------------------------------------------------------------
std::string CyParser::YWeb_cgi_func(CyhookHandler *hh, std::string ycmd) {
	std::string func, para, yresult = "ycgi func not found";
	//bool found = false;
	ySplitString(ycmd, " ", func, para);

	for (unsigned int i = 0; i < (sizeof(yFuncCallList)
			/ sizeof(yFuncCallList[0])); i++)
		if (func == yFuncCallList[i].func_name) {
			yresult = (this->*yFuncCallList[i].pfunc)(hh, para);
			//found = true;
			break;
		}
	log_level_printf(8, "<yparser: func>:%s Result:%s\n", func.c_str(),
			yresult.c_str());
	return yresult;
}

//-------------------------------------------------------------------------
// y-func : get_request_data
//-------------------------------------------------------------------------
std::string CyParser::func_get_request_data(CyhookHandler *hh, std::string para) {
	std::string yresult;
	if (para == "client_addr")
		yresult = hh->UrlData["clientaddr"]; //compatibility TODO in yhtms
	else
		yresult = hh->UrlData[para];
	return yresult;
}
//-------------------------------------------------------------------------
// y-func : get_header_data
//-------------------------------------------------------------------------
std::string CyParser::func_get_header_data(CyhookHandler *hh, std::string para) {
	return hh->HeaderList[para];
}
//-------------------------------------------------------------------------
// y-func : get_header_data
//-------------------------------------------------------------------------
std::string CyParser::func_get_config_data(CyhookHandler *hh, std::string para) {
	if (para != "mod_auth.username" && para != "mod_auth.password")
		return hh->WebserverConfigList[para];
	else
		return "empty";
}
//-------------------------------------------------------------------------
// y-func : Reload the httpd.conf
//-------------------------------------------------------------------------
extern void yhttpd_reload_config();
std::string CyParser::func_do_reload_httpd_config(CyhookHandler *, std::string) {
	log_level_printf(1, "func_do_reload_httpd_config: raise USR1 !!!\n");
	//raise(SIGUSR1); // Send HUP-Signal to Reload Settings
	yhttpd_reload_config();
	return "";
}

//-------------------------------------------------------------------------
// y-func : Change httpd (process image) on the fly
//-------------------------------------------------------------------------
std::string CyParser::func_change_httpd(CyhookHandler *hh, std::string para) {
	if (!para.empty() && access(para, R_OK) == 0) {
		hh->status = HANDLED_ABORT;
		char * argv[2] = { (char *)para.c_str(), NULL };
		int err = execvp(argv[0], argv); // no return if successful
		return "ERROR [change_httpd]: execvp returns error code: " + err;
	} else
		return "ERROR [change_httpd]: para has not path to a file";
}
//-------------------------------------------------------------------------
// y-func : get_header_data
//-------------------------------------------------------------------------
std::string CyParser::func_get_languages_as_dropdown(CyhookHandler *,
		std::string para) {
	std::string yresult, sel;
	DIR *d;

	std::string act_language = CLanguage::getInstance()->language;
	d = opendir((CLanguage::getInstance()->language_dir).c_str());
	if (d != NULL) {
		struct dirent *dir;
		while ((dir = readdir(d))) {
			if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
				continue;
			if (dir->d_type != DT_DIR) {
				sel = (act_language == std::string(dir->d_name)) ? "selected=\"selected\"" : "";
				yresult += string_printf("<option value=%s %s>%s</option>",
						dir->d_name, sel.c_str(), (encodeString(std::string(dir->d_name))).c_str());
				if(para != "nonl")
					yresult += "\n";
			}
		}
		closedir(d);
	}
	return yresult;
}
//-------------------------------------------------------------------------
// y-func : get_header_data
//-------------------------------------------------------------------------
std::string CyParser::func_set_language(CyhookHandler *, std::string para) {
	if (!para.empty()) {
		CConfigFile *Config = new CConfigFile(',');
		Config->loadConfig(HTTPD_CONFIGFILE);
		Config->setString("Language.selected", para);
		Config->saveConfig(HTTPD_CONFIGFILE);
		yhttpd_reload_config();
	}
	return "";
}
