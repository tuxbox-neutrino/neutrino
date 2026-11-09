/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	nhttpd setup menu - Neutrino-GUI
	Copyright (C) 2026, Thilo Graf 'dbt'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
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

#include <driver/captureguard.h>

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include <nhttpd/yconfig.h>
#include <httpd/login_bridge.h>
#include <httpd/webconfig.h>

#include <libconfigfile/configfile.h>
#include <libnet.h>

#include <unistd.h>

extern void yhttpd_reload_config();

static const CMenuOptionChooser::keyval HTTPD_LOGLEVEL_OPTIONS[] =
{
	{ 0, LOCALE_OPTIONS_OFF },
	{ 1, LOCALE_DEBUG_LEVEL_1 },
	{ 2, LOCALE_DEBUG_LEVEL_2 },
	{ 3, LOCALE_DEBUG_LEVEL_3 }
};
#define HTTPD_LOGLEVEL_OPTION_COUNT (sizeof(HTTPD_LOGLEVEL_OPTIONS)/sizeof(CMenuOptionChooser::keyval))

/* The next %s of text from at on replaced by value, and at moved past it, so a
   value that itself holds a %s is not filled in again by the next call. */
static void fillIn(std::string &text, const std::string &value, size_t &at)
{
	const size_t found = text.find("%s", at);
	if (found == std::string::npos)
		return;
	text.replace(found, 2, value);
	at = found + value.size();
}

/* Seconds the message with the first password stands: until OK, and five
   minutes at most, so a start nobody watches goes on to the picture. */
static const int kFirstPasswordShown = 300;

/* Where the web interface answers: http:// and the box's address, its port
   where that is not 80; https:// where the server speaks it, its port where
   that is not 443. The box's name where it has no address yet (the
   network can come up after the start), a dash where it has neither. */
static std::string webAddress()
{
	std::string ip, mask, broadcast;
	netGetIP(g_settings.ifname, ip, mask, broadcast);
	if (ip.empty() || ip == "0.0.0.0")
	{
		char name[256] = "";
		if (gethostname(name, sizeof(name) - 1) != 0 || !name[0])
			return "-";
		ip = name;
	}
	CConfigFile config(',');
	config.loadConfig(HTTPD_CONFIGFILE);
	const int port = config.getInt32("WebsiteMain.port", HTTPD_STANDARD_PORT);
#ifdef Y_CONFIG_USE_OPEN_SSL
	// read as the server reads it (yhttpd.cpp): "true" and nothing else
	const bool ssl = config.getString("WebsiteMain.ssl", "false") == "true";
#else
	const bool ssl = false;
#endif
	const std::string scheme = ssl ? "https://" : "http://";
	return scheme + ip + (port == (ssl ? 443 : 80) ? std::string() : ":" + std::to_string(port));
}

/* The one message with the first password, at the start that drew it, from the
   menu and after a reset: where the web interface answers, the user, the
   password, that the first sign-in asks for one of one's own, and where the
   message is found again. Every copy of the password is wiped afterwards.
   No picture of the screen is taken while the message stands: the exclusion
   is begun before it is drawn and ends once ShowMsg has taken it off again,
   and a capture already reading the screen when it begins keeps nothing. */
static void showFirstPasswordText(std::string &first)
{
	std::string text = g_Locale->getText(LOCALE_NETWORKMENU_HTTPD_FIRST_PASSWORD_SHOWN);
	size_t at = 0;
	fillIn(text, webAddress(), at);
	fillIn(text, login_bridge::user(), at);
	fillIn(text, first, at);
	first.assign(first.size(), '\0');
	{
		captureguard::Exclusion no_capture;
		ShowMsg(LOCALE_NETWORKMENU_HTTPD, text, CMsgBox::mbrOk, CMsgBox::mbOk, NEUTRINO_ICON_INFO, MSGBOX_MIN_WIDTH, kFirstPasswordShown);
	}
	text.assign(text.size(), '\0');
}

CNhttpdSetup::CNhttpdSetup()
	: width(40)
	, port(HTTPD_STANDARD_PORT)
	, auth_enabled(0)
	, threading(0)
	, loglevel(0)
	, ssl_enabled(0)
	, host(HTTPD_DEFAULT_HOST)
	, username(AUTHUSER)
	, password("")
	, no_auth_client("")
	, ssl_pemfile(SSL_PEMFILE)
	, ssl_ca_file(SSL_CA_FILE)
	, pass_fwd(NULL)
	, first_fwd(NULL)
{
}

CNhttpdSetup::~CNhttpdSetup()
{
}

int CNhttpdSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	if (actionKey == "reset_password")
	{
		resetPassword();
		return menu_return::RETURN_REPAINT;
	}
	if (actionKey == "first_password")
	{
		showFirstPassword(false);
		return menu_return::RETURN_REPAINT;
	}

	if (parent)
		parent->hide();

	return showSetup();
}

/* The web interface's login put back for an owner who no longer knows theirs:
   user root and a new first password, which this shows and the first login over
   the web asks to replace again. Asked first. */
void CNhttpdSetup::resetPassword()
{
	if (ShowMsg(LOCALE_NETWORKMENU_HTTPD_RESET_PASSWORD, g_Locale->getText(LOCALE_NETWORKMENU_HTTPD_RESET_PASSWORD_ASK),
		    CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo) != CMsgBox::mbrYes)
		return;
	std::string first, why;
	if (!login_bridge::resetToFirstPassword(&first, &why))
	{
		printf("[CNhttpdSetup] %s: the web password was not reset: %s\n", __func__, why.c_str());
		DisplayErrorMessage(g_Locale->getText(LOCALE_NETWORKMENU_HTTPD_RESET_PASSWORD_FAILED));
		return;
	}
	username = login_bridge::user();
	password.clear();
	password_mask = "********";
	// a first password stands now, so the entry showing it again does too
	if (first_fwd)
		first_fwd->setActive(true);
	// the same message as at the first start
	showFirstPasswordText(first);
}

/* At the start only when this run drew the password, so it is shown once and not
   on every start until somebody changes it, and not for longer than five minutes,
   since nobody may be watching. Where the box still opens to a password it
   shipped with, because the first one could not be written, that is said
   instead, without naming any password; asked from the menu where neither
   stands, which is a box whose password was changed since the menu opened, that
   is said. Whatever held the password is wiped afterwards. */
void CNhttpdSetup::showFirstPassword(bool at_start)
{
	const int timeout = at_start ? kFirstPasswordShown : NO_TIMEOUT;
	std::string first = httpd::firstPasswordInEffect();
	if (first.empty())
	{
		const bool owed = login_bridge::passwordOwed();
		if (owed || !at_start)
			ShowMsg(LOCALE_NETWORKMENU_HTTPD,
				g_Locale->getText(owed ? LOCALE_NETWORKMENU_HTTPD_FIRST_PASSWORD_FAILED
						       : LOCALE_NETWORKMENU_HTTPD_FIRST_PASSWORD_NONE),
				CMsgBox::mbrOk, CMsgBox::mbOk, NEUTRINO_ICON_INFO, MSGBOX_MIN_WIDTH, timeout);
		return;
	}
	if (at_start && !httpd::firstPasswordWrittenThisRun())
	{
		first.assign(first.size(), '\0');
		return;
	}

	showFirstPasswordText(first);
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
	// the account lives in nhttpd-access.conf, hashed (httpd/login_bridge.h): the
	// name can be shown, the password cannot, so its field opens empty and
	// an empty field keeps the stored one
	username = login_bridge::user();
	password.clear();
	no_auth_client = config.getString("mod_auth.no_auth_client", "");
	// fixed-width mask so the menu neither shows the password nor its length
	password_mask = login_bridge::passwordSet() ? "********" : "";

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

	/* Read and written under the lock every writer of this file in the program
	   holds, the REST route among them, and released before the account is
	   changed below, whose lock is taken first everywhere else. */
	bool saved = false;
	{
		httpd::ServerFileLock hold;
		CConfigFile config(',');
		config.loadConfig(HTTPD_CONFIGFILE);

		config.setInt32("WebsiteMain.port", port);
		config.setString("WebsiteMain.host", host);
		config.setBool("webserver.threading", threading != 0);
		config.setInt32("server.log.loglevel", loglevel);

		config.setBool("mod_auth.authenticate", auth_enabled != 0);
		config.setString("mod_auth.no_auth_client", no_auth_client);

#ifdef Y_CONFIG_USE_OPEN_SSL
		config.setBool("WebsiteMain.ssl", ssl_enabled != 0);
		config.setString("WebsiteMain.ssl_pemfile", ssl_pemfile);
		config.setString("WebsiteMain.ssl_ca_file", ssl_ca_file);
#endif

		saved = httpd::saveServerFile(HTTPD_CONFIGFILE, config.getConfigDataMap());
	}
	if (!saved) {
		DisplayErrorMessage(g_Locale->getText(LOCALE_NETWORKMENU_HTTPD_SAVE_FAILED));
		return false;
	}

	// in effect for the pages and the REST login as soon as this returns
	std::string why;
	const bool login_saved = login_bridge::change(username, password, &why);
	password.assign(password.size(), '\0');
	password.clear();

	yhttpd_reload_config();
	if (!login_saved) {
		fprintf(stderr, "[nhttpd_setup] the login was not changed: %s\n", why.c_str());
		DisplayErrorMessage(g_Locale->getText(LOCALE_NETWORKMENU_HTTPD_SAVE_FAILED));
		return false;
	}
	return true;
}

bool CNhttpdSetup::changeNotify(const neutrino_locale_t OptionName, void * /*Data*/)
{
	if (OptionName == LOCALE_NETWORKMENU_HTTPD_AUTH)
		auth_items.Activate(auth_enabled != 0);
	else if (OptionName == LOCALE_NETWORKMENU_HTTPD_PASS)
	{
		password_mask = (!password.empty() || login_bridge::passwordSet()) ? "********" : "";
		if (pass_fwd)
			pass_fwd->setOption(password_mask);
	}
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

	CMenuWidget *menu = new CMenuWidget(LOCALE_MAINSETTINGS_NETWORK, NEUTRINO_ICON_NETWORK, width, MN_WIDGET_ID_NHTTPDSETUP);
	menu->addIntroItems(LOCALE_NETWORKMENU_HTTPD);

	CMenuOptionNumberChooser *port_chooser = new CMenuOptionNumberChooser(LOCALE_NETWORKMENU_HTTPD_PORT, &port, true, 1, 65535);
	port_chooser->setNumericInput(true);
	port_chooser->setHint("", LOCALE_MENU_HINT_NET_HTTPD_PORT);
	menu->addItem(port_chooser);

	CKeyboardInput host_input(LOCALE_NETWORKMENU_HTTPD_HOST, &host);
	CMenuForwarder *host_fwd = new CMenuForwarder(LOCALE_NETWORKMENU_HTTPD_HOST, true, host, &host_input);
	host_fwd->setHint("", LOCALE_MENU_HINT_NET_HTTPD_HOST);
	menu->addItem(host_fwd);

	menu->addItem(GenericMenuSeparatorLine);

	CMenuOptionChooser *auth_opt = new CMenuOptionChooser(LOCALE_NETWORKMENU_HTTPD_AUTH, &auth_enabled, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);
	auth_opt->setHint("", LOCALE_MENU_HINT_NET_HTTPD_AUTH);
	menu->addItem(auth_opt);

	CKeyboardInput user_input(LOCALE_NETWORKMENU_HTTPD_USER, &username);
	CMenuForwarder *user_fwd = new CMenuForwarder(LOCALE_NETWORKMENU_HTTPD_USER, auth_enabled != 0, username, &user_input);
	user_fwd->setHint("", LOCALE_MENU_HINT_NET_HTTPD_USER);
	auth_items.Add(user_fwd);
	menu->addItem(user_fwd);

	CKeyboardInput pass_input(LOCALE_NETWORKMENU_HTTPD_PASS, &password, 0, this);
	pass_input.setMasked(true);
	// masked copy as option: CMenuForwarder binds empty options by value,
	// so updates go through setOption() in changeNotify()
	pass_fwd = new CMenuForwarder(LOCALE_NETWORKMENU_HTTPD_PASS, auth_enabled != 0, password_mask, &pass_input);
	pass_fwd->setOption(password_mask);
	pass_fwd->setHint("", LOCALE_MENU_HINT_NET_HTTPD_PASS);
	auth_items.Add(pass_fwd);
	menu->addItem(pass_fwd);

	CKeyboardInput noauth_input(LOCALE_NETWORKMENU_HTTPD_NOAUTH, &no_auth_client);
	CMenuForwarder *noauth_fwd = new CMenuForwarder(LOCALE_NETWORKMENU_HTTPD_NOAUTH, auth_enabled != 0, no_auth_client, &noauth_input);
	noauth_fwd->setHint("", LOCALE_MENU_HINT_NET_HTTPD_NOAUTH);
	auth_items.Add(noauth_fwd);
	menu->addItem(noauth_fwd);

	// the web interface's login, whether or not the pages ask for it
	CMenuForwarder *reset_fwd = new CMenuForwarder(LOCALE_NETWORKMENU_HTTPD_RESET_PASSWORD, true, NULL, this, "reset_password");
	reset_fwd->setHint("", LOCALE_MENU_HINT_NET_HTTPD_RESET_PASSWORD);
	menu->addItem(reset_fwd);

	// to be chosen while the box still waits for a password of the owner's own
	first_fwd = new CMenuForwarder(LOCALE_NETWORKMENU_HTTPD_FIRST_PASSWORD, login_bridge::passwordOwed(), NULL, this, "first_password");
	first_fwd->setHint("", LOCALE_MENU_HINT_NET_HTTPD_FIRST_PASSWORD);
	menu->addItem(first_fwd);

	menu->addItem(GenericMenuSeparatorLine);

	CMenuOptionChooser *thread_opt = new CMenuOptionChooser(LOCALE_NETWORKMENU_HTTPD_THREADING, &threading, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	thread_opt->setHint("", LOCALE_MENU_HINT_NET_HTTPD_THREADING);
	menu->addItem(thread_opt);

	CMenuOptionChooser *loglevel_opt = new CMenuOptionChooser(LOCALE_NETWORKMENU_HTTPD_LOGLEVEL, &loglevel, HTTPD_LOGLEVEL_OPTIONS, HTTPD_LOGLEVEL_OPTION_COUNT, true);
	loglevel_opt->setHint("", LOCALE_MENU_HINT_NET_HTTPD_LOGLEVEL);
	menu->addItem(loglevel_opt);

#ifdef Y_CONFIG_USE_OPEN_SSL
	menu->addItem(GenericMenuSeparatorLine);
	CMenuOptionChooser *ssl_opt = new CMenuOptionChooser(LOCALE_NETWORKMENU_HTTPD_SSL, &ssl_enabled, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);
	ssl_opt->setHint("", LOCALE_MENU_HINT_NET_HTTPD_SSL);
	menu->addItem(ssl_opt);

	CKeyboardInput pem_input(LOCALE_NETWORKMENU_HTTPD_SSL_PEMFILE, &ssl_pemfile);
	CMenuForwarder *pem_fwd = new CMenuForwarder(LOCALE_NETWORKMENU_HTTPD_SSL_PEMFILE, ssl_enabled != 0, ssl_pemfile, &pem_input);
	pem_fwd->setHint("", LOCALE_MENU_HINT_NET_HTTPD_SSL_PEMFILE);
	ssl_items.Add(pem_fwd);
	menu->addItem(pem_fwd);

	CKeyboardInput ca_input(LOCALE_NETWORKMENU_HTTPD_SSL_CAFILE, &ssl_ca_file);
	CMenuForwarder *ca_fwd = new CMenuForwarder(LOCALE_NETWORKMENU_HTTPD_SSL_CAFILE, ssl_enabled != 0, ssl_ca_file, &ca_input);
	ca_fwd->setHint("", LOCALE_MENU_HINT_NET_HTTPD_SSL_CAFILE);
	ssl_items.Add(ca_fwd);
	menu->addItem(ca_fwd);
#endif

	auth_items.Activate(auth_enabled != 0);
#ifdef Y_CONFIG_USE_OPEN_SSL
	ssl_items.Activate(ssl_enabled != 0);
#endif

	int res = menu->exec(NULL, "");
	delete menu;
	pass_fwd = NULL;
	first_fwd = NULL;

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

	if (changed) {
		if (ShowMsg(LOCALE_NETWORKMENU_HTTPD, LOCALE_NETWORKMENU_APPLY_SETTINGS_NOW,
				CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo,
				NEUTRINO_ICON_QUESTION, width) == CMsgBox::mbrYes)
			saveConfig();
	}

	return res;
}
