//=============================================================================
// YHTTPD
// Global System Configuration
//=============================================================================

//-----------------------------------------------------------------------------
#ifndef __yconfig_h__
#define __yconfig_h__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
// c++
#include <string>
#include <map>
#include <vector>

//=============================================================================
// Base Configuration
//=============================================================================
//-----------------------------------------------------------------------------
// System Choice <configure!> ONE choice
//-----------------------------------------------------------------------------
#ifndef CONFIG_SYSTEM_BY_COMPILER			// use Compiler directive to set CONFIG_SYSTEM
#define CONFIG_SYSTEM_TUXBOX	y			// Tuxbox project
#endif
//-----------------------------------------------------------------------------
// General central Definitions <configure!>
//-----------------------------------------------------------------------------
/* The one version the server reports. The yhttpd core it grew from was last
   versioned 1.3.2 and has changed a great deal here since, so it is no line of
   its own any more. */
#define HTTPD_VERSION 			"4.0.0"		// Webserver version  (can be overloaded)
#define IADDR_LOCAL			"127.0.0.1"	// local IP
#define HTTPD_NAME			"yhttpd"	// Webserver name (can be overloaded)
#define AUTH_NAME_MSG			"yhttpd"	// Name in Authentication Dialogue
#define CONF_VERSION			7		// Version of yhttpd-conf file
#define HTTPD_KEEPALIVE_TIMEOUT		500000		// Timeout for Keep-Alive in mircoseconds
//=============================================================================
// Features wanted <configure!>
//=============================================================================
//-----------------------------------------------------------------------------
// modules
//-----------------------------------------------------------------------------
#define Y_CONFIG_USE_AUTHHOOK y				// Add mod: "Authentication"
#define Y_CONFIG_USE_WEBLOG y				// Add mod: "WebLogging"
#define Y_CONFIG_USE_CACHE y				// Add mod: Can cache production pages
#define Y_CONFIG_USE_SENDFILE y				// Add mod: can send static files (mandantory)
//-----------------------------------------------------------------------------
// Features & Build
//-----------------------------------------------------------------------------
#define Y_CONFIG_FEATURE_CHECK_PORT_AUTORITY y		// System: Port < 1024 need Admin-Privileges-Check
#define Y_CONFIG_HAVE_SENDFILE y			// System: Have *IX SendFile
#define Y_CONFIG_USE_HOSTEDWEB y			// Add Feature: Use HOSTED Web
//#define Y_CONFIG_USE_OPEN_SSL y			// Add Feature: use openSSL
//#define Y_CONFIG_FEATURE_KEEP_ALIVE y			// Add Feature: Keep-alive //FIXME: does not work correctly now
#define Y_CONFIG_FEATUE_SENDFILE_CAN_ACCESS_ALL y	// Add Feature: files outside the web folders, where the program lets them (ystatic::setRawPlaceTest)
//#define Y_CONFIG_FEATURE_CHROOT y			// Add Feature: Use Change Root for Security
//#define Y_CONFIG_FEATURE_HTTPD_USER y			// Add Feature: Set User for yhttpd-Process
#define Y_CONFIG_FEATURE_THREADING y			// Build with possibility for multi threading
//-----------------------------------------------------------------------------
// Define/Undefine Features forced by CONFIG_SYSTEM_xxx
// Dependencies
//-----------------------------------------------------------------------------
//#ifdef Y_CONFIG_USE_OPEN_SSL
//#undef Y_CONFIG_HAVE_SENDFILE				// Sendfile does not work for SSL, but we'll fallback to send
//#endif

#ifdef CONFIG_SYSTEM_TUXBOX
#define Y_CONFIG_USE_AUTHHOOK y
#endif

#ifdef Y_CONFIG_FEATURE_KEEP_ALIVE
#error "Y_CONFIG_FEATURE_KEEP_ALIVE: a POST body the reader leaves unread would be read as the next request"
#define HTTP_PROTOCOL "HTTP/1.1"
#else
#define HTTP_PROTOCOL "HTTP/1.0"
#endif
//=============================================================================
// Configurations for systems/OSs <configure!>
//=============================================================================
//-----------------------------------------------------------------------------
// Configurations for LINUX (Tuxbox)
//-----------------------------------------------------------------------------
#undef HTTPD_NAME
#define HTTPD_NAME			"nhttpd"
#define HTTPD_STANDARD_PORT		80
#define HTTPD_DEFAULT_HOST		"0.0.0.0"
#define HTTPD_FALLBACK_PORT		8080
#define HTTPD_MAX_CONNECTIONS		50
#define HTTPD_REQUEST_LOG		"/tmp/httpd_log"
#define LOG_FILE			"/tmp/yhttpd.log"
#define LOG_FORMAT			""
#define CACHE_DIR			"/tmp/.cache"
// The longest form body (application/x-www-form-urlencoded) a POST outside the
// API may declare; it is read into memory before any hook looks at the
// request. A longer one is refused unread. As much as the API takes:
// yrequest.cpp does not build where this and its API_BODY_LIMIT differ.
#define POST_FORM_MAX_LENGTH		(1024 * 1024)
#define HTTPD_SENDFILE_EXT		"htm:text/html,html:text/html,xml:application/xml,txt:text/plain,jpg:image/jpeg,jpeg:image/jpeg,gif:image/gif,png:image/png,bmp:image/x-ms-bmp,css:text/css,js:application/javascript,mjs:application/javascript,json:application/json,svg:image/svg+xml,webp:image/webp,woff:font/woff,woff2:font/woff2,webmanifest:application/manifest+json,yjs:text/plain,img:application/octet-stream,ico:image/x-icon,m3u:audio/x-mpegURL,m3u8:application/x-mpegURL,tar:application/x-tar,gz:application/gzip,ts:video/MP2T,mkv:video/x-matroska,avi:video/x-msvideo,mp3:audio/mpeg,ogg:audio/ogg"
#define HTTPD_SENDFILE_ALL		"false"
#define HTTPD_LANGUAGEDIR		"languages"
#define HTTPD_DEFAULT_LANGUAGE 		"English"

#define AUTHUSER			"root"
#define AUTHPASSWORD			WEB_DEFAULT_PASSWORD
#define AUTH_SESSION_TIMEOUT		86400	// browser session lifetime in seconds (sliding)

#define HTTPD_CONFIGDIR			CONFIGDIR
#define HTTPD_CONFIGFILE		HTTPD_CONFIGDIR"/nhttpd.conf"
#define WEBUI_CONFIGFILE		HTTPD_CONFIGDIR"/webui.conf"
#define NEUTRINO_CONFIGFILE		CONFIGDIR"/neutrino.conf"
#define MOVIEBROWSER_CONFIGFILE		CONFIGDIR"/moviebrowser.conf"

#define PRIVATEDOCUMENTROOT		PRIVATE_HTTPDDIR
#define PUBLICDOCUMENTROOT		PUBLIC_HTTPDDIR
#define HOSTEDDOCUMENTROOT		HOSTED_HTTPDDIR
#define HOSTEDDOCUMENTURL		"/hosted/"
#define EXTRASDOCUMENTROOT		HOSTED_HTTPDDIR"/extras"
#define EXTRASDOCUMENTURL		"/hosted/extras"

#define TUXBOX_LOGOS_URL		LOGODIR

#define SSL_PEMFILE			HTTPD_CONFIGDIR "/server.pem"
#define SSL_CA_FILE			HTTPD_CONFIGDIR "/cacert.pem"
//-----------------------------------------------------------------------------
// Aggregated definitions
//-----------------------------------------------------------------------------
#define WEBSERVERNAME HTTPD_NAME "/" HTTPD_VERSION

#endif // __yconfig_h__
