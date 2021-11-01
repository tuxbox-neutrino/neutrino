#!/usr/bin/luaclient

--[[
	nlab - Neutrino Lua API bridge

	Copyright (C) 2021 Sven Hoefer <svenhoefer@svenhoefer.com>
]]

nlab = "Neutrino Lua API bridge"
version = "0.01"

a = {}
r = ""

if arg[2] == nil then
	r = nlab .. " v" .. version
end

for i, v in ipairs(arg) do
	a[i] = v:lower()
end

if a[2] == "get" then
	-- see src/gui/lua/luainstance.cpp

	if a[3] == "apiversion" then
		r = APIVERSION.MAJOR .. "." .. APIVERSION.MINOR
	elseif a[3] == "apiversion.major" then
		r = APIVERSION.MAJOR
	elseif a[3] == "apiversion.minor" then
		r = APIVERSION.MINOR

	elseif a[3] == "dir.configdir" then
		r = DIR.CONFIGDIR
	elseif a[3] == "dir.zapitdir" then
		r = DIR.ZAPITDIR
	elseif a[3] == "dir.datadir" then
		r = DIR.DATADIR
	elseif a[3] == "dir.datadir_var" then
		r = DIR.DATADIR_VAR
	elseif a[3] == "dir.controldir" then
		r = DIR.CONTROLDIR
	elseif a[3] == "dir.controldir_var" then
		r = DIR.CONTROLDIR_VAR
	elseif a[3] == "dir.fontdir" then
		r = DIR.FONTDIR
	elseif a[3] == "dir.fontdir_var" then
		r = DIR.FONTDIR_VAR
	elseif a[3] == "dir.libdir" then
		r = DIR.LIBDIR
	elseif a[3] == "dir.gamesdir" then
		r = DIR.GAMESDIR
	elseif a[3] == "dir.iconsdir" then
		r = DIR.ICONSDIR
	elseif a[3] == "dir.iconsdir_var" then
		r = DIR.ICONSDIR_VAR
	elseif a[3] == "dir.localedir" then
		r = DIR.LOCALEDIR
	elseif a[3] == "dir.localedir_var" then
		r = DIR.LOCALEDIR_VAR
	elseif a[3] == "dir.plugindir" then
		r = DIR.PLUGINDIR
	elseif a[3] == "dir.plugindir_mnt" then
		r = DIR.PLUGINDIR_MNT
	elseif a[3] == "dir.plugindir_var" then
		r = DIR.PLUGINDIR_VAR
	elseif a[3] == "dir.luaplugindir" then
		r = DIR.LUAPLUGINDIR
	elseif a[3] == "dir.luaplugindir_var" then
		r = DIR.LUAPLUGINDIR_VAR
	elseif a[3] == "dir.themesdir" then
		r = DIR.THEMESDIR
	elseif a[3] == "dir.themesdir_var" then
		r = DIR.THEMESDIR_VAR
	elseif a[3] == "dir.webradiodir" then
		r = DIR.WEBRADIODIR
	elseif a[3] == "dir.webradiodir_var" then
		r = DIR.WEBRADIODIR_VAR
	elseif a[3] == "dir.webtvdir" then
		r = DIR.WEBTVDIR
	elseif a[3] == "dir.webtvdir_var" then
		r = DIR.WEBTVDIR_VAR
	elseif a[3] == "dir.logodir" then
		r = DIR.LOGODIR
	elseif a[3] == "dir.logodir_var" then
		r = DIR.LOGODIR_VAR
	elseif a[3] == "dir.private_httpddir" then
		r = DIR.PRIVATE_HTTPDDIR
	elseif a[3] == "dir.public_httpddir" then
		r = DIR.PUBLIC_HTTPDDIR
	elseif a[3] == "dir.hosted_httpddir" then
		r = DIR.HOSTED_HTTPDDIR
	elseif a[3] == "dir.flagdir" then
		r = DIR.FLAGDIR

	elseif a[3] == "screen.off_x" then
		r = SCREEN.OFF_X
	elseif a[3] == "screen.off_y" then
		r = SCREEN.OFF_Y
	elseif a[3] == "screen.end_x" then
		r = SCREEN.END_X
	elseif a[3] == "screen.end_y" then
		r = SCREEN.END_Y
	elseif a[3] == "screen.x_res" then
		r = SCREEN.X_RES
	elseif a[3] == "screen.y_res" then
		r = SCREEN.Y_RES

	end

elseif a[2] == "misc" then
	-- see src/gui/lua/lua_misc.cpp

	misc = misc.new()
	if a[3] == "postmsg.standby_on" then
		r = misc:postMsg(POSTMSG.STANDBY_ON)
	elseif a[3] == "getvolume" then
		r = misc:getVolume()
	elseif a[3] == "setvolume" and a[4] ~= nil then
		r = misc:setVolume(a[4])
	elseif a[3] == "ismuted" then
		r = misc:isMuted()
	elseif a[3] == "getrevision" then
		r = misc:GetRevision()
	end
	misc = nil

elseif a[2] == "video" then
	-- see src/gui/lua/lua_video.cpp

	video = video.new()
	if a[3] == "showpicture" and a[4] ~= nil then
		video:zapitStopPlayBack()
		r = video:ShowPicture(arg[4])
	elseif a[3] == "stoppicture" then
		video:zapitStopPlayBack(false)
		r = video:StopPicture()
	end

end

if r ~= nil and r ~= "" then
	return tostring(r) .. "\n"
end
