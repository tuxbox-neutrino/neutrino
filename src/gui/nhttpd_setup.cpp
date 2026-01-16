/*
	Neutrino-GUI  -   DBoxII-Project

	nhttpd setup menu - Neutrino-GUI

	License: GPL
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "nhttpd_setup.h"

#include <gui/widget/icons.h>
#include <gui/widget/keyboard_input.h>
#include <gui/widget/menue_options.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput_ext.h>

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include <nhttpd/yconfig.h>

#include <libconfigfile/configfile.h>

extern void yhttpd_reload_config();

static const CMenuOptionChooser::keyval HTTPD_LOGLEVEL_OPTIONS[] =
{
	{ 0, LOCALE_OPTIONS_OFF },
	{ 1, LOCALE_DEBUG_LEVEL_1 },
	{ 2, LOCALE_DEBUG_LEVEL_2 },
	{ 3, LOCALE_DEBUG_LEVEL_3 }
};
#define HTTPD_LOGLEVEL_OPTION_COUNT (sizeof(HTTPD_LOGLEVEL_OPTIONS)/sizeof(CMenuOptionChooser::keyval))

CNhttpdSetup::CNhttpdSetup()
	: width(40)
	, port(HTTPD_STANDARD_PORT)
	, auth_enabled(0)
	, threading(0)
	, loglevel(0)
	, ssl_enabled(0)
	, host(HTTPD_DEFAULT_HOST)
	, username(AUTHUSER)
	, password(AUTHPASSWORD)
	, no_auth_client("")
	, ssl_pemfile(SSL_PEMFILE)
	, ssl_ca_file(SSL_CA_FILE)
{
}

CNhttpdSetup::~CNhttpdSetup()
{
}

int CNhttpdSetup::exec(CMenuTarget* parent, const std::string &/*actionKey*/)
{
	if (parent)
		parent->hide();

	return showSetup();
}

void CNhttpdSetup::loadConfig()
{
	CConfigFile config(',');
	config.loadConfig(HTTPD_CONFIGFILE);

	host = config.getString("WebsiteMain.host", HTTPD_DEFAULT_HOST);
	port = config.getInt32("WebsiteMain.port", HTTPD_STANDARD_PORT);
	threading = config.getBool("webserver.threading", false) ? 1 : 0;
	loglevel = config.getInt32("server.log.loglevel", 0);
	if (loglevel < 0 || loglevel > 3)
		loglevel = 0;

	auth_enabled = config.getBool("mod_auth.authenticate", false) ? 1 : 0;
	username = config.getString("mod_auth.username", AUTHUSER);
	password = config.getString("mod_auth.password", AUTHPASSWORD);
	no_auth_client = config.getString("mod_auth.no_auth_client", "");

#ifdef Y_CONFIG_USE_OPEN_SSL
	ssl_enabled = config.getBool("WebsiteMain.ssl", false) ? 1 : 0;
	ssl_pemfile = config.getString("WebsiteMain.ssl_pemfile", SSL_PEMFILE);
	ssl_ca_file = config.getString("WebsiteMain.ssl_ca_file", SSL_CA_FILE);
#else
	ssl_enabled = 0;
	ssl_pemfile.clear();
	ssl_ca_file.clear();
#endif
}

bool CNhttpdSetup::saveConfig()
{
	if (port < 1 || port > 65535) {
		DisplayErrorMessage(g_Locale->getText(LOCALE_NETWORKMENU_HTTPD_PORT_INVALID));
		return false;
	}

	CConfigFile config(',');
	config.loadConfig(HTTPD_CONFIGFILE);

	config.setInt32("WebsiteMain.port", port);
	config.setString("WebsiteMain.host", host);
	config.setBool("webserver.threading", threading != 0);
	config.setInt32("server.log.loglevel", loglevel);

	config.setBool("mod_auth.authenticate", auth_enabled != 0);
	config.setString("mod_auth.username", username);
	config.setString("mod_auth.password", password);
	config.setString("mod_auth.no_auth_client", no_auth_client);

#ifdef Y_CONFIG_USE_OPEN_SSL
	config.setBool("WebsiteMain.ssl", ssl_enabled != 0);
	config.setString("WebsiteMain.ssl_pemfile", ssl_pemfile);
	config.setString("WebsiteMain.ssl_ca_file", ssl_ca_file);
#endif

	if (!config.saveConfig(HTTPD_CONFIGFILE)) {
		DisplayErrorMessage(g_Locale->getText(LOCALE_NETWORKMENU_HTTPD_SAVE_FAILED));
		return false;
	}

	yhttpd_reload_config();
	return true;
}

bool CNhttpdSetup::changeNotify(const neutrino_locale_t OptionName, void * /*Data*/)
{
	if (OptionName == LOCALE_NETWORKMENU_HTTPD_AUTH)
		auth_items.Activate(auth_enabled != 0);
#ifdef Y_CONFIG_USE_OPEN_SSL
	else if (OptionName == LOCALE_NETWORKMENU_HTTPD_SSL)
		ssl_items.Activate(ssl_enabled != 0);
#endif
	return false;
}

int CNhttpdSetup::showSetup()
{
	loadConfig();

	int orig_port = port;
	int orig_auth_enabled = auth_enabled;
	int orig_threading = threading;
	int orig_loglevel = loglevel;
	int orig_ssl_enabled = ssl_enabled;
	std::string orig_host = host;
	std::string orig_username = username;
	std::string orig_password = password;
	std::string orig_no_auth_client = no_auth_client;
	std::string orig_ssl_pemfile = ssl_pemfile;
	std::string orig_ssl_ca_file = ssl_ca_file;

	CMenuWidget *menu = new CMenuWidget(LOCALE_NETWORKMENU_HTTPD, NEUTRINO_ICON_NETWORK, width, MN_WIDGET_ID_NHTTPDSETUP);
	menu->addIntroItems(LOCALE_NETWORKMENU_HTTPD);

	CMenuOptionNumberChooser *port_chooser = new CMenuOptionNumberChooser(LOCALE_NETWORKMENU_HTTPD_PORT, &port, true, 1, 65535);
	port_chooser->setNumericInput(true);
	menu->addItem(port_chooser);

	CKeyboardInput host_input(LOCALE_NETWORKMENU_HTTPD_HOST, &host);
	menu->addItem(new CMenuForwarder(LOCALE_NETWORKMENU_HTTPD_HOST, true, host, &host_input));

	menu->addItem(GenericMenuSeparatorLine);

	CMenuOptionChooser *auth_opt = new CMenuOptionChooser(LOCALE_NETWORKMENU_HTTPD_AUTH, &auth_enabled, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);
	menu->addItem(auth_opt);

	CKeyboardInput user_input(LOCALE_NETWORKMENU_HTTPD_USER, &username);
	CMenuForwarder *user_fwd = new CMenuForwarder(LOCALE_NETWORKMENU_HTTPD_USER, auth_enabled != 0, username, &user_input);
	auth_items.Add(user_fwd);
	menu->addItem(user_fwd);

	CKeyboardInput pass_input(LOCALE_NETWORKMENU_HTTPD_PASS, &password);
	CMenuForwarder *pass_fwd = new CMenuForwarder(LOCALE_NETWORKMENU_HTTPD_PASS, auth_enabled != 0, password, &pass_input);
	auth_items.Add(pass_fwd);
	menu->addItem(pass_fwd);

	CKeyboardInput noauth_input(LOCALE_NETWORKMENU_HTTPD_NOAUTH, &no_auth_client);
	CMenuForwarder *noauth_fwd = new CMenuForwarder(LOCALE_NETWORKMENU_HTTPD_NOAUTH, auth_enabled != 0, no_auth_client, &noauth_input);
	auth_items.Add(noauth_fwd);
	menu->addItem(noauth_fwd);

	menu->addItem(GenericMenuSeparatorLine);

	CMenuOptionChooser *thread_opt = new CMenuOptionChooser(LOCALE_NETWORKMENU_HTTPD_THREADING, &threading, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	menu->addItem(thread_opt);

	CMenuOptionChooser *loglevel_opt = new CMenuOptionChooser(LOCALE_NETWORKMENU_HTTPD_LOGLEVEL, &loglevel, HTTPD_LOGLEVEL_OPTIONS, HTTPD_LOGLEVEL_OPTION_COUNT, true);
	menu->addItem(loglevel_opt);

#ifdef Y_CONFIG_USE_OPEN_SSL
	menu->addItem(GenericMenuSeparatorLine);
	CMenuOptionChooser *ssl_opt = new CMenuOptionChooser(LOCALE_NETWORKMENU_HTTPD_SSL, &ssl_enabled, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);
	menu->addItem(ssl_opt);

	CKeyboardInput pem_input(LOCALE_NETWORKMENU_HTTPD_SSL_PEMFILE, &ssl_pemfile);
	CMenuForwarder *pem_fwd = new CMenuForwarder(LOCALE_NETWORKMENU_HTTPD_SSL_PEMFILE, ssl_enabled != 0, ssl_pemfile, &pem_input);
	ssl_items.Add(pem_fwd);
	menu->addItem(pem_fwd);

	CKeyboardInput ca_input(LOCALE_NETWORKMENU_HTTPD_SSL_CA_FILE, &ssl_ca_file);
	CMenuForwarder *ca_fwd = new CMenuForwarder(LOCALE_NETWORKMENU_HTTPD_SSL_CA_FILE, ssl_enabled != 0, ssl_ca_file, &ca_input);
	ssl_items.Add(ca_fwd);
	menu->addItem(ca_fwd);
#endif

	auth_items.Activate(auth_enabled != 0);
#ifdef Y_CONFIG_USE_OPEN_SSL
	ssl_items.Activate(ssl_enabled != 0);
#endif

	int res = menu->exec(NULL, "");
	delete menu;

	auth_items.Clear();
	ssl_items.Clear();

	bool changed = (orig_port != port)
		|| (orig_auth_enabled != auth_enabled)
		|| (orig_threading != threading)
		|| (orig_loglevel != loglevel)
		|| (orig_ssl_enabled != ssl_enabled)
		|| (orig_host != host)
		|| (orig_username != username)
		|| (orig_password != password)
		|| (orig_no_auth_client != no_auth_client)
		|| (orig_ssl_pemfile != ssl_pemfile)
		|| (orig_ssl_ca_file != ssl_ca_file);

	if (changed)
		saveConfig();

	return res;
}
