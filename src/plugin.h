
#ifndef __plugin_h__
#define __plugin_h__

// plugin type as defined in plugin's cfg
enum
{
	PLUGIN_TYPE_DISABLED	= 0,
	PLUGIN_TYPE_GAME	= 1,
	PLUGIN_TYPE_TOOL	= 2,
	PLUGIN_TYPE_SCRIPT	= 3,
	PLUGIN_TYPE_LUA		= 4
};

// plugin integration as defined in plugin's cfg
enum
{
	PLUGIN_INTEGRATION_DISABLED		= 0,
	PLUGIN_INTEGRATION_MAIN			= 1,
	PLUGIN_INTEGRATION_MULTIMEDIA		= 2,
	PLUGIN_INTEGRATION_SETTING		= 3,
	PLUGIN_INTEGRATION_SERVICE		= 4,
	PLUGIN_INTEGRATION_INFORMATION		= 5,
	PLUGIN_INTEGRATION_SOFTWARE_MANAGE	= 6
};

#endif // __plugin_h__
