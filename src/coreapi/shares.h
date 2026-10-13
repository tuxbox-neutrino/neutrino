/*
 * shares.h - the network shares the box mounts itself, its eight slots
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#ifndef __coreapi_shares_h__
#define __coreapi_shares_h__

#include "coreapi/base/result.h"

#include <stddef.h>

#include <string>
#include <vector>

namespace coreapi
{
namespace shares
{

/* The box's own network shares: the eight slots of its settings
   (network_nfs_* in neutrino.conf) that the screen at the box edits under
   Network shares, and that the program mounts itself, at its start for every
   slot set to mount automatically and on request for any. Not the two files of
   the system's own mounts (fstab and auto.net, coreapi::netfs), which the box
   mounts through its start up scripts: those are a model of their own. */
const size_t kSlots = 8;

// What the other end speaks, as the screen at the box offers it.
enum class Kind
{
	Nfs,
	Cifs,
	Lufs
};

struct Share
{
	// Mounted when the program starts.
	bool automount;
	Kind kind;
	// The server, as an address or a host name, and what it offers.
	std::string host;
	std::string remote_dir;
	// Where it is mounted on the box: an absolute path.
	std::string local_dir;
	std::string username;

	/* The password, and never what a read hands back: the settings file keeps
	   it in clear, the only form the mount takes one in. Filled on the way in
	   only, where change_password says a new one was given; left alone, the
	   stored one stays. A read says has_password instead. */
	std::string password;
	bool change_password;
	bool has_password;

	// What the mount is given, in two parts as the screen keeps them.
	std::string options1;
	std::string options2;
	// The server's hardware address, which the screen wakes it with.
	std::string mac;

	Share();
};

/* Whether the slot's share is what is mounted where it lands: Mounted when the
   mount on its local directory is that share, Foreign when something else is
   mounted there, Unmounted when nothing is. */
enum class State
{
	Unmounted,
	Mounted,
	Foreign
};

struct Slots
{
	std::vector<Share> items;   // kSlots of them
	std::vector<State> states;  // one for each
};

/* Every slot, the passwords withheld, with what is mounted where each lands.
   What the program last published (refresh) and, over it, a write it has not
   carried yet, so a reader after a write reads the write. NotSupported where
   the program installed no source. */
Result<Slots> read();

/* Replaces one slot, of the kind NFS or CIFS: a slot of the userspace FTP
   filesystem is kept as the screen at the box sets it, and read. host and
   remote_dir may be empty, a slot nobody uses;
   local_dir is an absolute path without a . or .. part, and neither it nor
   remote_dir holds white space (the table of mounts writes it as an escape,
   and the share would not be recognised there). No value holds a
   control character or a number sign (the settings file's reader cuts a value
   off there), host only letters, digits, dots, dashes, colons and brackets,
   username and password no comma (the mount takes them as options, split at
   commas), and options1, options2 no white space, quote or backslash and none
   of the options that carry the credential (user, username, pass, password,
   credentials, cred), which the slot holds apart and never answers. mac is
   empty or
   six two-digit hexadecimal numbers separated by colons. InvalidArgument
   BadString naming the value for anything else, and nothing is written.

   Carried by the program's own loop, which saves the settings with it, as a
   write of any other setting is. A password left out (change_password false)
   is the one stored, read in the same step that takes the write. Internal or
   Busy where the loop could not be asked; a write the loop carried meanwhile
   is answered Ok. */
Result<void> write(size_t slot, const Share &share);

/* The judgement write makes, on its own. */
Result<void> check(const Share &share);

/* Mounts the slot's share as it reads, as the screen's Mount does: the local
   directory is made where it is missing. Conflict MountPointBusy where
   something is mounted there already (this share among it), Conflict
   ShareNotConfigured for a slot without a server or a remote directory,
   NotSupported FilesystemUnsupported where the box cannot mount that kind
   or the slot is of the userspace FTP filesystem,
   Internal Timeout where the server did not answer in time, Internal
   MountFailed where the mount failed otherwise. */
Result<void> mount(size_t slot);

/* Unmounts the slot's share. Conflict ShareNotMounted where what is mounted on
   top at its local directory is not that share, or nothing is: nothing else is
   taken away from under a slot that names its directory. The look and the
   unmount are one step for every user of the program's mounter. NotSupported
   FilesystemUnsupported for a slot of the userspace FTP filesystem, whose
   mounts cannot be told apart. Internal UnmountFailed where the unmount
   failed. */
Result<void> unmount(size_t slot);

/* Mounts every slot set to mount automatically that is not mounted already,
   as the program does at its start. What became of each, by slot: Ok, or the
   refusal mount() would have answered. */
struct Outcome
{
	size_t slot;
	Result<void> result;
	explicit Outcome(size_t s, const Result<void> &r) : slot(s), result(r) {}
};
Result<std::vector<Outcome> > mountAutomatic();

/* What reaches the program: the slots of its settings and its mounter on a
   box, something else in a case. Called from request threads. */
enum class Mounted
{
	Ok,
	Busy,
	Unsupported,
	Timeout,
	Failed
};

enum class Unmounted
{
	Ok,
	NotOurs,
	Failed
};

struct Source
{
	virtual ~Source() {}
	// Every slot with its password in it, and the states.
	virtual Status read(std::vector<Share> &shares, std::vector<State> &states) const = 0;
	// change_password false keeps the stored password, read in the same step.
	virtual Status write(size_t slot, const Share &share) = 0;
	virtual Mounted mount(const Share &share) = 0;
	// Only where the share is what is mounted on top, looked at in that step.
	virtual Unmounted unmount(const Share &share) = 0;
	// On the loop's thread, for a source that holds writes until then.
	virtual void carry() {}
	// On the loop's thread, after the program changed the slots itself.
	virtual void refresh() {}
};

// NULL takes it away again; nothing installed answers NotSupported.
void setSource(Source *s);

/* Binds the slots to the program's settings and its mounter, and carries what
   was written on the loop's own thread (applyPending), with the save. Defined
   in the part of this layer that reaches the program (box/shares_real.cpp). */
void installRealSource(bool (*save)());

// On the loop's thread: what was written since the last time, then the save.
void applyPending();

/* On the loop's thread, wherever the program changed the slots itself: after
   it read its settings and when the screen of the network shares closes. A
   request reads a copy the loop publishes, never the slots themselves, whose
   numbers the screen writes without a lock. */
void refresh();

} // namespace shares
} // namespace coreapi

#endif
