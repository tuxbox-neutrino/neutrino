/*
 * shares.cpp - the network shares the box mounts itself, its eight slots
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include <config.h>

#include "coreapi/shares.h"
#include "coreapi/base/errors.h"

#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace coreapi
{
namespace shares
{

Share::Share()
	: automount(false), kind(Kind::Nfs), change_password(false), has_password(false)
{
}

namespace
{

Source *g_source = NULL;

const size_t kMaxValueBytes = 255;

Failure notSupported()
{
	return fail(Status::NotSupported, ErrorCode::CommandNotSent, "this box keeps no network shares of its own");
}

Failure refused(const std::string &why)
{
	return fail(Status::InvalidArgument, ErrorCode::BadString, why);
}

/* What every value has to be to come back the same after the settings file
   was written and read again: no control byte, the file holding one value to
   a line, and no number sign, where that file's reader cuts a value off. */
bool storable(const std::string &v)
{
	if (v.size() > kMaxValueBytes)
		return false;
	for (size_t i = 0; i < v.size(); ++i)
	{
		const unsigned char c = static_cast<unsigned char>(v[i]);
		if (c < 0x20 || c == 0x7f || c == '#')
			return false;
	}
	return true;
}

// Never with a dash in front, which the mount would read as an option of its own.
bool hostOk(const std::string &h)
{
	if (!h.empty() && h[0] == '-')
		return false;
	for (size_t i = 0; i < h.size(); ++i)
	{
		const unsigned char c = static_cast<unsigned char>(h[i]);
		if (!std::isalnum(c) && c != '.' && c != '-' && c != ':' && c != '[' && c != ']')
			return false;
	}
	return true;
}

// Absolute, and without a part that names where it stands or above it.
bool localDirOk(const std::string &d)
{
	if (d.size() < 2 || d[0] != '/')
		return false;
	size_t from = 1;
	while (from <= d.size())
	{
		size_t end = d.find('/', from);
		if (end == std::string::npos)
			end = d.size();
		const std::string part = d.substr(from, end - from);
		if (part == "." || part == ".." || (part.empty() && end != d.size()))
			return false;
		from = end + 1;
	}
	return true;
}

/* The mount takes these as a list split at commas, white space ending it. An
   option carrying the credential would be a second value for what the slot
   holds apart, and one a read hands back in clear. */
bool optionsOk(const std::string &o)
{
	for (size_t i = 0; i < o.size(); ++i)
	{
		const unsigned char c = static_cast<unsigned char>(o[i]);
		if (std::isspace(c) || c == '\'' || c == '"' || c == '\\')
			return false;
	}
	static const char *const credential[] = { "user", "username", "pass", "password", "credentials", "cred" };
	size_t from = 0;
	while (from <= o.size())
	{
		size_t end = o.find(',', from);
		if (end == std::string::npos)
			end = o.size();
		std::string key = o.substr(from, end - from);
		key = key.substr(0, key.find('='));
		for (size_t k = 0; k < key.size(); ++k)
			key[k] = (char) std::tolower(static_cast<unsigned char>(key[k]));
		for (size_t k = 0; k < sizeof(credential) / sizeof(credential[0]); ++k)
			if (key == credential[k])
				return false;
		from = end + 1;
	}
	return true;
}

bool macOk(const std::string &m)
{
	if (m.empty())
		return true;
	if (m.size() != 17)
		return false;
	for (size_t i = 0; i < m.size(); ++i)
	{
		const unsigned char c = static_cast<unsigned char>(m[i]);
		if (i % 3 == 2 ? c != ':' : !std::isxdigit(c))
			return false;
	}
	return true;
}

Result<Slots> readAll()
{
	if (g_source == NULL)
		return notSupported();
	Slots out;
	if (g_source->read(out.items, out.states) != Status::Ok || out.items.size() != kSlots ||
	    out.states.size() != kSlots)
		return fail(Status::Internal, ErrorCode::CommandNotSent, "the network shares could not be read");
	return Result<Slots>::success(out);
}

Failure noSuchSlot()
{
	return fail(Status::NotFound, ErrorCode::NoSuchName, "there are 8 slots, counted from 0 to 7");
}

} // namespace

Result<Slots> read()
{
	Result<Slots> all = readAll();
	if (!all.ok())
		return all;
	Slots out = all.value();
	for (size_t i = 0; i < out.items.size(); ++i)
	{
		out.items[i].has_password = !out.items[i].password.empty();
		out.items[i].password.clear();
		out.items[i].change_password = false;
	}
	return Result<Slots>::success(out);
}

Result<void> check(const Share &s)
{
	const struct
	{
		const char *name;
		const std::string *value;
	} values[] = {
		{ "host", &s.host }, { "remote_dir", &s.remote_dir }, { "local_dir", &s.local_dir },
		{ "user", &s.username }, { "password", &s.password }, { "options1", &s.options1 },
		{ "options2", &s.options2 }, { "mac", &s.mac },
	};
	for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
	{
		if (!storable(*values[i].value))
			return refused(std::string(values[i].name) + " holds a control character or a number sign, "
				       "or more than 255 bytes");
	}
	if (!hostOk(s.host))
		return refused("host is an address or a host name: letters, digits, dots, dashes, colons and "
			       "brackets, not beginning with a dash");
	if (!localDirOk(s.local_dir))
		return refused("local_dir is an absolute path without a . or .. part");
	/* The table of mounts writes a space in a path as an escape, so a share
	   mounted on such a path or from one would not be recognised as the
	   slot's own, and could not be unmounted from here. */
	if (s.local_dir.find_first_of(" \t") != std::string::npos ||
	    s.remote_dir.find_first_of(" \t") != std::string::npos)
		return refused("local_dir and remote_dir hold no white space");
	if (s.username.find(',') != std::string::npos || s.password.find(',') != std::string::npos)
		return refused("user and password hold no comma: the mount takes them as options, split at commas");
	if (!optionsOk(s.options1) || !optionsOk(s.options2))
		return refused("options1 and options2 hold no white space, quote or backslash, and none of the "
			       "options that carry the credential");
	if (!macOk(s.mac))
		return refused("mac is empty or six two-digit hexadecimal numbers separated by colons");
	return ok();
}

Result<void> write(size_t slot, const Share &share)
{
	if (slot >= kSlots)
		return noSuchSlot();
	if (share.kind == Kind::Lufs)
		return refused("type is nfs or cifs: a slot of the userspace FTP filesystem is kept as the screen "
			       "at the box sets it");
	const Result<void> passes = check(share);
	if (!passes.ok())
		return passes;
	if (g_source == NULL)
		return notSupported();

	// a password left out is the stored one, which the source reads as it takes the write
	Share put = share;
	if (!put.change_password)
		put.password.clear();
	const Status s = g_source->write(slot, put);
	if (s != Status::Ok)
		return fail(s, ErrorCode::CommandNotPosted, "the box could not be asked to take the share over");
	return ok();
}

Result<void> mount(size_t slot)
{
	if (slot >= kSlots)
		return noSuchSlot();
	Result<Slots> all = readAll();
	if (!all.ok())
		return fail(all.error());
	const Share &s = all.value().items[slot];
	if (s.kind == Kind::Lufs)
		return fail(Status::NotSupported, ErrorCode::FilesystemUnsupported,
			    "a share of the userspace FTP filesystem is mounted from the screen at the box");
	if (s.host.empty() || s.remote_dir.empty())
		return fail(Status::Conflict, ErrorCode::ShareNotConfigured,
			    "the slot names no server or no remote directory");
	if (all.value().states[slot] != State::Unmounted)
		return fail(Status::Conflict, ErrorCode::MountPointBusy,
			    "something is mounted on " + s.local_dir + " already");
	switch (g_source->mount(s))
	{
		case Mounted::Ok:
			return ok();
		case Mounted::Busy:
			return fail(Status::Conflict, ErrorCode::MountPointBusy,
				    "something is mounted on " + s.local_dir + " already");
		case Mounted::Unsupported:
			return fail(Status::NotSupported, ErrorCode::FilesystemUnsupported,
				    "this box cannot mount a share of that kind");
		case Mounted::Timeout:
			return fail(Status::Internal, ErrorCode::Timeout, "the server did not answer in time");
		case Mounted::Failed:
			break;
	}
	return fail(Status::Internal, ErrorCode::MountFailed,
		    "the share could not be mounted; the box's log says why");
}

Result<void> unmount(size_t slot)
{
	if (slot >= kSlots)
		return noSuchSlot();
	Result<Slots> all = readAll();
	if (!all.ok())
		return fail(all.error());
	const Share &s = all.value().items[slot];
	if (s.kind == Kind::Lufs)
		return fail(Status::NotSupported, ErrorCode::FilesystemUnsupported,
			    "a share of the userspace FTP filesystem is unmounted from the screen at the box");
	// the state is a first answer; the source decides again as it unmounts
	switch (all.value().states[slot] != State::Mounted ? Unmounted::NotOurs : g_source->unmount(s))
	{
		case Unmounted::Ok:
			return ok();
		case Unmounted::NotOurs:
			return fail(Status::Conflict, ErrorCode::ShareNotMounted,
				    "the slot's share is not what is mounted on " + s.local_dir);
		case Unmounted::Failed:
			break;
	}
	return fail(Status::Internal, ErrorCode::UnmountFailed,
		    "the share could not be unmounted; something on the box may still be using it");
}

Result<std::vector<Outcome> > mountAutomatic()
{
	Result<Slots> all = readAll();
	if (!all.ok())
		return fail(all.error());
	std::vector<Outcome> out;
	for (size_t i = 0; i < kSlots; ++i)
	{
		if (!all.value().items[i].automount || all.value().states[i] == State::Mounted)
			continue;
		out.push_back(Outcome(i, mount(i)));
	}
	return Result<std::vector<Outcome> >::success(out);
}

void setSource(Source *s)
{
	g_source = s;
}

void applyPending()
{
	if (g_source != NULL)
		g_source->carry();
}

void refresh()
{
	if (g_source != NULL)
		g_source->refresh();
}

} // namespace shares
} // namespace coreapi
