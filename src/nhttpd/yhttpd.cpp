//=============================================================================
// YHTTPD
// Main Program
//=============================================================================

// system
#include <csignal>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pwd.h>
#include <grp.h>
#include <syscall.h>
#include <stdio.h>

#include <system/set_threadname.h>
#include <system/helpers.h>
// yhttpd
#include <ylogging.h>
#include <ylanguage.h>
#include <yhook.h>
#include <httpd/nhttpd_bridge.h>
#include <httpd/server_control.h>
#include <httpd/webconfig.h>

#ifdef Y_CONFIG_USE_YPARSER
#include <mod_yparser.h>
static CyParser yParser;
#endif

//-----------------------------------------------------------------------------
// Setting yhttpd Instance
//-----------------------------------------------------------------------------
#include "yhttpd.h"
static Cyhttpd *yhttpd = NULL;
CStringList Cyhttpd::ConfigList;
//=============================================================================
// HOOKS: Definition & Instance for Hooks, attach/detach Hooks
//=============================================================================

#ifdef Y_CONFIG_USE_AUTHHOOK
#include <mod_auth.h>
static CmAuth *auth = NULL;
static CApiBridge *api_bridge = NULL;
#endif

/* Server threads that began, and that finished their teardown, the delete of
   their instance included. Equal when no server thread is alive. A stop
   waits for that, so the next start never overlaps the last one's teardown:
   the instance's delete takes things with it that a new server would already
   be using, the language instance among them. And a start that sees the
   ended count move before its server reported a bound port knows the server
   it started came up and went away without listening. */
static unsigned server_runs_begun = 0;
static unsigned server_runs_ended = 0;

static bool server_threads_gone()
{
	return __atomic_load_n(&server_runs_begun, __ATOMIC_SEQ_CST) ==
	       __atomic_load_n(&server_runs_ended, __ATOMIC_SEQ_CST);
}

#ifdef Y_CONFIG_USE_WEBLOG
#include <mod_weblog.h>
static CmWebLog *weblog = NULL;
#endif

#ifdef Y_CONFIG_USE_SENDFILE
#include <mod_sendfile.h>
static CmodSendfile *mod_sendfile = NULL;
#endif

#ifdef Y_CONFIG_USE_CACHE
#include <mod_cache.h>
static CmodCache mod_cache; // static instance
#endif

//-----------------------------------------------------------------------------

#ifdef CONFIG_SYSTEM_TUXBOX
#include "neutrinoapi.h"
#include <config.h>
static CNeutrinoAPI *NeutrinoAPI;
#endif

//=============================================================================
// Main: Main Entry, Webserver thread
//=============================================================================

void yhttpd_reload_config() {
	if (yhttpd)
		yhttpd->ReadConfig();
}

//-----------------------------------------------------------------------------
// Main Entry
//-----------------------------------------------------------------------------

/* Only on cancellation; nothing cancels this thread today. It ends the run the
   way the normal tail below does, so a stop or a start waiting on the counts
   is not left waiting for good. */
void thread_cleanup (void *p)
{
	Cyhttpd *y = (Cyhttpd *)p;
	if (y) {
		Cyhttpd *mine = y;
		__atomic_compare_exchange_n(&yhttpd, &mine, (Cyhttpd *) NULL, false,
		                            __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
		y->stop_webserver();
		delete y;
	}
	__atomic_add_fetch(&server_runs_ended, 1, __ATOMIC_SEQ_CST);
}

void * nhttpd_main_thread(void *) {
	__atomic_add_fetch(&server_runs_begun, 1, __ATOMIC_SEQ_CST);
	set_threadname("yweb:main_thread");
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	aprintf("Webserver %s tid %ld\n", WEBSERVERNAME, syscall(__NR_gettid));
	Cyhttpd *fresh = new Cyhttpd();
	__atomic_store_n(&yhttpd, fresh, __ATOMIC_SEQ_CST);
	//CLogging::getInstance()->setDebug(true);
	//CLogging::getInstance()->LogLevel = 9;
	if (!fresh) {
		aprintf("Error initializing WebServer\n");
		__atomic_add_fetch(&server_runs_ended, 1, __ATOMIC_SEQ_CST);
		return (void *) EXIT_FAILURE;
	}
	/* we pthread_cancel this thread from the main thread, but still want to clean up */
	pthread_cleanup_push(thread_cleanup, fresh);
#ifndef Y_CONFIG_FEATURE_THREADING
	fresh->flag_threading_off = true;
#endif

	fresh->hooks_attach();
	fresh->ReadConfig();
	if (fresh->Configure()) {
		// Start Webserver: fork ist if not in debug mode
		aprintf("Webserver starting...\n");
		dprintf("Start in Debug-Mode\n"); // non forked debugging loop

		fresh->run();
	}
	pthread_cleanup_pop(0);
	/* Only its own instance: with the start guard below two servers never
	   coexist, and this line keeps even a surprised sleeper from deleting
	   a successor's instance under it. */
	Cyhttpd *mine = __atomic_load_n(&yhttpd, __ATOMIC_SEQ_CST);
	if (mine != NULL &&
	    __atomic_compare_exchange_n(&yhttpd, &mine, (Cyhttpd *) NULL, false,
	                                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
		delete mine;
	__atomic_add_fetch(&server_runs_ended, 1, __ATOMIC_SEQ_CST);

	aprintf("Main end\n");
	return (void *) EXIT_SUCCESS;
}
//=============================================================================
// Class yhttpd
//=============================================================================
Cyhttpd::Cyhttpd() {
	webserver = new CWebserver();
	flag_threading_off = false;
}
//-----------------------------------------------------------------------------
Cyhttpd::~Cyhttpd() {
	if (webserver)
		delete webserver;
	CLanguage::deleteInstance();
	webserver = NULL;
}

//-----------------------------------------------------------------------------
// Change to Root
//-----------------------------------------------------------------------------
bool Cyhttpd::Configure() {

	if (!getuid()) // you must be root to do that!
	{
		// Get user and group data
#ifdef Y_CONFIG_FEATURE_HTTPD_USER
		struct passwd *pwd = NULL;
		struct group *grp = NULL;
		std::string username = ConfigList["server.user_name"];
		std::string groupname= ConfigList["server.group_name"];

		// get user data
		if(!username.empty())
		{
			if((pwd = getpwnam(username.c_str())) == NULL)
			{
				dperror("Dont know user to set uid\n");
				return false;
			}
		}
		// get group data
		if(!groupname.empty())
		{
			if((grp = getgrnam(groupname.c_str())) == NULL)
			{
				aprintf("Can not get Group-Information. Group: %s\n", groupname.c_str());
				return false;
			}
		}
#endif
		// change root directory
#ifdef Y_CONFIG_FEATURE_CHROOT
		if(!ConfigList["server.chroot"].empty())
		{
			log_level_printf(2, "do chroot to dir:%s\n", ConfigList["server.chroot"].c_str() );
			// do change Root
			if(chroot(ConfigList["server.chroot"].c_str()) == -1)
			{
				dperror("Change Root failed\n");
				return false;
			}
			// Set Working Dir
			if(chdir("/") == -1)
			{
				dperror("Change Directory to Root failed\n");
				return false;
			}
		}
#endif
#ifdef Y_CONFIG_FEATURE_HTTPD_USER
		if(!username.empty() && pwd != NULL && grp != NULL)
		{
			log_level_printf(2, "set user and groups\n");

			// drop root privileges
			setgid(grp->gr_gid);
			setgroups(0, NULL);
			// set user group
			if(!groupname.empty())
			initgroups(username.c_str(), grp->gr_gid);
			// set user
			if(setuid(pwd->pw_uid) == -1)
			{
				dperror("Change User Context failed\n");
				return false;
			}
		}
#endif
	}
	return true;
}
//-----------------------------------------------------------------------------
// Main Webserver call
//-----------------------------------------------------------------------------
void Cyhttpd::run() {
	set_threadname("yweb:run");
	if (webserver) {
		if (flag_threading_off)
			webserver->is_threading = false;
		// the webserver itself reports the bound port to the API layer's
		// seam, after a successful listen and fallback included
		webserver->run();
		stop_webserver();
	} else
		aprintf("Error initializing WebServer\n");
}

//-----------------------------------------------------------------------------
// Show Version Text and Number
//-----------------------------------------------------------------------------
void Cyhttpd::version(FILE *dest) {
	fprintf(dest, "%s - Webserver v%s\n", HTTPD_NAME, HTTPD_VERSION);
}

//-----------------------------------------------------------------------------
// Stop WebServer
//-----------------------------------------------------------------------------
void Cyhttpd::stop_webserver() {
	aprintf("stop requested......\n");
	if (webserver) {
		// before the hooks go: every open stream is closed and waited for,
		// so no connection thread is inside the bridge when it is deleted
		if (api_bridge)
			CApiBridge::stopEvents();
		webserver->stop();
		/* And every other connection thread as well: the hooks below and the
		   server after them are deleted, and a request still being served
		   walks both. A thread waiting on its peer is woken. One still inside
		   a hook - a script /control/exec runs, say - is waited for as long as
		   it takes: deleting under it would hand it freed hooks and a freed
		   server. Until then this thread does not finish, and a start refuses
		   to put a second server beside it. */
		while (!webserver->waitForConnections(15000))
			aprintf("stop: connection threads still running, waiting on\n");
		hooks_detach();
	}
}
//-----------------------------------------------------------------------------
// Attach hooks (use hook order carefully)
//-----------------------------------------------------------------------------
void Cyhttpd::hooks_attach() {
#ifdef Y_CONFIG_USE_AUTHHOOK
	// First Check Authentication
	auth = new CmAuth();
	CyhookHandler::attach(auth);
#endif

	// The REST layer: behind auth, before every hook that claims a URL by
	// its extension. Attached only when its tables pass their own check;
	// a server that would answer from broken tables is worse than one that
	// says so once at start and serves the rest.
	{
		std::string why;
		if (CApiBridge::routesOk(&why)) {
			CApiBridge::registerPrefixes();
			api_bridge = new CApiBridge();
			CyhookHandler::attach(api_bridge);
			// after the attach, so no stream can open before the hook that
			// carries it exists; refuses streams itself if its thread fails
			CApiBridge::startEvents();
		} else {
			api_bridge = NULL;
			aprintf("yhttpd: the REST layer is not attached: %s\n", why.c_str());
		}
	}

#ifdef CONFIG_SYSTEM_TUXBOX
	NeutrinoAPI = new CNeutrinoAPI();
	CyhookHandler::attach(NeutrinoAPI->NeutrinoYParser);
	CyhookHandler::attach(NeutrinoAPI->ControlAPI);
#else
#ifdef Y_CONFIG_USE_YPARSER
	CyhookHandler::attach(&yParser);
#endif
#endif

#ifdef Y_CONFIG_USE_CACHE
	CyhookHandler::attach(&mod_cache);
#endif

#ifdef Y_CONFIG_USE_SENDFILE
	mod_sendfile = new CmodSendfile();
	CyhookHandler::attach(mod_sendfile);
#endif
#ifdef Y_CONFIG_USE_WEBLOG
	weblog = new CmWebLog();
	CyhookHandler::attach(weblog);
#endif
}

//-----------------------------------------------------------------------------
// Detach hooks & Destroy
//-----------------------------------------------------------------------------
void Cyhttpd::hooks_detach() {
#ifdef Y_CONFIG_USE_AUTHHOOK
	CyhookHandler::detach(auth);
	delete auth;
#endif

	if (api_bridge) {
		CyhookHandler::detach(api_bridge);
		delete api_bridge;
		api_bridge = NULL;
	}

#ifdef CONFIG_SYSTEM_TUXBOX
	CyhookHandler::detach(NeutrinoAPI->NeutrinoYParser);
#else
#ifdef Y_CONFIG_USE_YPARSER
	CyhookHandler::detach(&yParser);
#endif
#endif

#ifdef Y_CONFIG_USE_CACHE
	CyhookHandler::detach(&mod_cache);
#endif

#ifdef Y_CONFIG_USE_SENDFILE
	CyhookHandler::detach(mod_sendfile);
#endif

#ifdef Y_CONFIG_USE_WEBLOG
	CyhookHandler::detach(weblog);
	delete weblog;
#endif
}

//-----------------------------------------------------------------------------
// Read Webserver Configurationfile
// Call "Hooks_ReadConfig" so Hooks can read/write own Configuration Values
//-----------------------------------------------------------------------------
void Cyhttpd::ReadConfig(void) {
	log_level_printf(3, "ReadConfig Start\n");
	/* The whole read, because it writes the file back where it filled in
	   defaults: under the lock every writer of nhttpd.conf in the program
	   holds, so it cannot write back what the menu or the REST route has
	   just changed. */
	httpd::ServerFileLock hold;
	CConfigFile *Config = new CConfigFile(',');
	bool have_config = false;
	if (access(HTTPD_CONFIGFILE, R_OK) == 0)
		have_config = true;
	Config->loadConfig(HTTPD_CONFIGFILE);
	// convert old config files
	if (have_config) {
		if (Config->getInt32("configfile.version", 0) == 0) {
			CConfigFile OrgConfig = *Config;
			Config->clear();

			Config->setInt32("server.log.loglevel", OrgConfig.getInt32("LogLevel", 0));
			Config->setInt32("configfile.version", CONF_VERSION);
			Config->setString("webserver.websites", "WebsiteMain");
			Config->setBool("webserver.threading", OrgConfig.getBool("THREADS", true));
			Config->setInt32("WebsiteMain.port", OrgConfig.getInt32("Port", HTTPD_STANDARD_PORT));
			Config->setString("WebsiteMain.directory", OrgConfig.getString("PrivatDocRoot", PRIVATEDOCUMENTROOT));
			if (!OrgConfig.getString("PublicDocRoot", "").empty())
				Config->setString("WebsiteMain.override_directory", OrgConfig.getString("PublicDocRoot", PRIVATEDOCUMENTROOT));
			// mod_auth
			Config->setString("mod_auth.username", OrgConfig.getString("AuthUser", AUTHUSER));
			Config->setString("mod_auth.password", OrgConfig.getString("AuthPassword", AUTHPASSWORD));
			Config->setString("mod_auth.no_auth_client", OrgConfig.getString("NoAuthClient", ""));
			Config->setString("mod_auth.authenticate", OrgConfig.getString("Authenticate", "false"));

			Config->setString("mod_sendfile.mime_types", HTTPD_SENDFILE_EXT);

			httpd::saveServerFile(HTTPD_CONFIGFILE, Config->getConfigDataMap());

		}
		// Add Defaults for Version 2
		if (Config->getInt32("configfile.version") < 2) {
			Config->setString("mod_sendfile.mime_types", HTTPD_SENDFILE_EXT);
			Config->setInt32("configfile.version", CONF_VERSION);
			Config->setString("mod_sendfile.sendAll", HTTPD_SENDFILE_ALL);
			httpd::saveServerFile(HTTPD_CONFIGFILE, Config->getConfigDataMap());
		}
		// Add Defaults for Version 4
		if (Config->getInt32("configfile.version") < 4) {
			Config->setInt32("configfile.version", CONF_VERSION);
			Config->setString("Language.selected", HTTPD_DEFAULT_LANGUAGE);
			Config->setString("Language.directory", HTTPD_LANGUAGEDIR);
			if (Config->getString("WebsiteMain.hosted_directory", "").empty())
				Config->setString("WebsiteMain.hosted_directory", HOSTEDDOCUMENTROOT);
			httpd::saveServerFile(HTTPD_CONFIGFILE, Config->getConfigDataMap());
		}
		// Add Defaults for Version 5
		if (Config->getInt32("configfile.version") < 5) {
			if (Config->getString("WebsiteMain.host", "").empty())
				Config->setString("WebsiteMain.host", HTTPD_DEFAULT_HOST);
			Config->setInt32("configfile.version", CONF_VERSION);
			httpd::saveServerFile(HTTPD_CONFIGFILE, Config->getConfigDataMap());
		}
	}
	if (Config->getString("WebsiteMain.host", "").empty()) {
		Config->setString("WebsiteMain.host", HTTPD_DEFAULT_HOST);
		have_config = false;
	}
	// configure debugging & logging
	if (CLogging::getInstance()->LogLevel == 0)
		CLogging::getInstance()->LogLevel = Config->getInt32("server.log.loglevel", 0);
	if (CLogging::getInstance()->LogLevel > 0)
		CLogging::getInstance()->setDebug(true);

	// get variables
	const int web_port = Config->getInt32("WebsiteMain.port", HTTPD_STANDARD_PORT);
	const bool use_threading = Config->getBool("webserver.threading", true);
	const std::string web_host = Config->getString("WebsiteMain.host", HTTPD_DEFAULT_HOST);
	webserver->init(web_port, use_threading, web_host);
	// informational use
	ConfigList["WebsiteMain.port"] = itoa(web_port);
	ConfigList["WebsiteMain.host"] = web_host;
	ConfigList["webserver.threading"] = Config->getString("webserver.threading", "true");
	ConfigList["configfile.version"] = Config->getInt32("configfile.version", CONF_VERSION);
	ConfigList["server.log.loglevel"] = itoa(Config->getInt32("server.log.loglevel", 0));
	ConfigList["server.no_keep-alive_ips"] = Config->getString("server.no_keep-alive_ips", "");
	webserver->conf_no_keep_alive_ips = Config->getStringVector("server.no_keep-alive_ips");

	// MainSite
	ConfigList["WebsiteMain.directory"] = Config->getString("WebsiteMain.directory", PRIVATEDOCUMENTROOT);
	ConfigList["WebsiteMain.override_directory"] = Config->getString("WebsiteMain.override_directory", PUBLICDOCUMENTROOT);
	ConfigList["WebsiteMain.hosted_directory"] = Config->getString("WebsiteMain.hosted_directory", HOSTEDDOCUMENTROOT);

	ConfigList["Tuxbox.DisplayLogos"] = Config->getString("Tuxbox.DisplayLogos", "true");
	// Check location of logos
	if (Config->getString("Tuxbox.LogosURL", "").empty()) {
		if (access(ConfigList["WebsiteMain.override_directory"] + "/logos", R_OK) == 0) {
			Config->setString("Tuxbox.LogosURL", ConfigList["WebsiteMain.override_directory"] + "/logos");
			have_config = false; //save config
		}
		else if (access(ConfigList["WebsiteMain.directory"] + "/logos", R_OK) == 0){
			Config->setString("Tuxbox.LogosURL", ConfigList["WebsiteMain.directory"] + "/logos");
			have_config = false; //save config
		}
#ifdef Y_CONFIG_USE_HOSTEDWEB
		else if (access(ConfigList["WebsiteMain.hosted_directory"] + "/logos", R_OK) == 0){
			Config->setString("Tuxbox.LogosURL", ConfigList["WebsiteMain.hosted_directory"] + "/logos");
			have_config = false; //save config
		}
#endif //Y_CONFIG_USE_HOSTEDWEB
	}
	ConfigList["Tuxbox.LogosURL"] = Config->getString("Tuxbox.LogosURL", "");

#ifdef Y_CONFIG_USE_OPEN_SSL
	ConfigList["SSL"] = Config->getString("WebsiteMain.ssl", "false");
	ConfigList["SSL_pemfile"] = Config->getString("WebsiteMain.ssl_pemfile", SSL_PEMFILE);
	ConfigList["SSL_CA_file"] = Config->getString("WebsiteMain.ssl_ca_file", SSL_CA_FILE);

	CySocket::SSL_pemfile = ConfigList["SSL_pemfile"];
	CySocket::SSL_CA_file = ConfigList["SSL_CA_file"];
	if(ConfigList["SSL"] == "true")
	CySocket::initSSL();
#endif
	ConfigList["server.user_name"] = Config->getString("server.user_name", "");
	ConfigList["server.group_name"] = Config->getString("server.group_name", "");
	ConfigList["server.chroot"] = Config->getString("server.chroot", "");

	// language
	ConfigList["Language.directory"] = Config->getString("Language.directory", HTTPD_LANGUAGEDIR);
	ConfigList["Language.selected"] = Config->getString("Language.selected", HTTPD_DEFAULT_LANGUAGE);
	yhttpd->ReadLanguage();

	// Read App specifig settings by Hook
	CyhookHandler::Hooks_ReadConfig(Config, ConfigList);

	// Save if new defaults are set
	if (!have_config)
		httpd::saveServerFile(HTTPD_CONFIGFILE, Config->getConfigDataMap());
	log_level_printf(3, "ReadConfig End\n");
	delete Config;
}
//-----------------------------------------------------------------------------
// Read Webserver Configurationfile for languages
//-----------------------------------------------------------------------------
void Cyhttpd::ReadLanguage(void) {
	// Init Class vars
	CLanguage *lang = CLanguage::getInstance();
	log_level_printf(3, "ReadLanguage:%s\n", ConfigList["Language.selected"].c_str());
	lang->setLanguage(ConfigList["Language.selected"]);
}

//-----------------------------------------------------------------------------
// Ends the accept loop and nothing else (see yhttpd.h).
//-----------------------------------------------------------------------------
void Cyhttpd::stop_run() {
	if (webserver)
		webserver->stop();
}

//=============================================================================
// The two hands of the API layer's server seam (yhttpd.h says why they are
// here). Both are called from the seam's reload runner, never from a
// connection thread of the webserver they drive.
//=============================================================================
extern void *nhttpd_main_thread(void *);

/* port and bind are what the API layer read out of nhttpd.conf, which is
   also what the server reads when it starts (ReadConfig): that file is the
   one place where the server answers, written by the menu at the box and by
   the API layer's save, so nothing is written here. Named in the log only.
   Going back to the old port when nothing comes up on a new one is the API
   layer's (webconfig.cpp, reloadAndRestart), which knows what the old one
   was. */
int nhttpd_server_start(int port, const char *bind) {
	/* Never two: a start beside a living server would put two accept
	   loops on overlapping state (the hooks and this file's globals are
	   shared). The stop before this call is what ends the old one; where
	   it did not finish, the honest answer is that no new server came
	   up - the old one is still answering where it was. */
	if (__atomic_load_n(&yhttpd, __ATOMIC_SEQ_CST) != NULL || !server_threads_gone()) {
		aprintf("nhttpd_server_start: the old server has not finished stopping\n");
		return 0;
	}

	aprintf("nhttpd_server_start: port %d at %s, as %s says\n", port,
	        (bind != NULL) ? bind : "0.0.0.0", HTTPD_CONFIGFILE);

	const unsigned ended_before = __atomic_load_n(&server_runs_ended, __ATOMIC_SEQ_CST);
	pthread_t nhttpd_thread;
	if (pthread_create(&nhttpd_thread, NULL, nhttpd_main_thread, NULL) != 0)
		return 0;
	pthread_detach(nhttpd_thread);

	/* Up means listening, and the one that knows is the server itself: it
	   reports the port that really bound, its fallback included, once its
	   listen succeeded. A connection to the port would also be answered by
	   any other service that holds it while this server failed to bind
	   there. The server's thread ending first means it never listened.
	   Bounded, so a server that neither binds nor ends answers nought
	   instead of holding the reload runner forever. */
	for (int i = 0; i < 100; i++) {
		usleep(100 * 1000);
		const int bound = httpd::boundPort();
		if (bound > 0)
			return bound;
		if (__atomic_load_n(&server_runs_ended, __ATOMIC_SEQ_CST) != ended_before)
			break;
	}
	return 0;
}

bool nhttpd_server_stop(void) {
	Cyhttpd *running = __atomic_load_n(&yhttpd, __ATOMIC_SEQ_CST);
	if (running != NULL)
		running->stop_run();
	/* The thread carries its own teardown (hooks, events, the delete): it
	   nulls the instance first and deletes it after, so a null instance is
	   not yet a server gone - only the counts say when the delete is over.
	   Also waited for when the instance was already null: a thread still
	   inside its delete. The teardown waits for every connection thread, a
	   long script included, so this waits as long as that takes: the start
	   that follows is the only one there will be, and given up on here the
	   box would answer nothing once the script was done. It runs on the
	   reload runner's own thread, which nothing else waits for. */
	for (unsigned i = 1; !server_threads_gone(); i++) {
		if (i % 150 == 0)
			aprintf("nhttpd_server_stop: the old server is still finishing\n");
		usleep(100 * 1000);
	}
	return true;
}
