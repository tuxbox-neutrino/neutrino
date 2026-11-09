/*
 * backup.h - the box's settings packed into one archive, and put back
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#ifndef __coreapi_backup_h__
#define __coreapi_backup_h__

#include "coreapi/base/result.h"

#include <stddef.h>

#include <string>
#include <vector>

namespace coreapi
{
namespace backup
{

struct Archive
{
	std::string name;    // settings_<host>_<date>.tar.gz, as backup.sh named it
	std::string bytes;
};

// More than the settings of any box come to; a larger archive is refused.
const size_t kMaxBytes = 64u * 1024u * 1024u;

/* The box's settings packed by its own backup.sh, the script the settings menu
   on the screen runs: the files tobackup.conf names, or the whole configuration
   directory where there is no such list. Packed into a directory of its own
   under the temporary directory, read, and taken away again.

   Internal BackupFailed when the box has no backup.sh, when it failed or left no
   archive, or when the archive is larger than kMaxBytes. */
Result<Archive> make();

/* Puts back an archive the caller has uploaded to the temporary directory and
   restarts the box at once, without the program saving its settings over what
   was put back, as the settings menu does with restore.sh; on a PC built to
   restart only this program, only this program (restartWithoutSaving). The
   restart comes a moment after this returns, so the answer can still go out.
   One restore at a time: Conflict RestoreRefused for one asked for while
   another is under way or waits for its restart.

   Whether the box can restart is asked first, and Internal RestoreRefused where
   it cannot. The upload is then copied into a directory of this program's own
   under a name it chose, no more than kMaxBytes of it, and the upload goes:
   what is checked is what is put back, whatever happens to the upload's name
   afterwards. Every name in the copy is then read and has to land where the box
   backs up: in the configuration directory, or on a path tobackup.conf names (a
   directory, a file or a pattern). Names land below the box's /var/tuxbox where
   any of them begins with config/, and below / otherwise, as restore.sh unpacks
   them for the menu. A name that climbs with .., an absolute name, one with a .
   or an empty part, one beginning with .restore-, a name with a control
   character, a link of either kind or a device in the archive, a link already on
   the box below where a name lands, a file where the box has a directory or the
   other way round, and an archive that holds one name as a file and as a
   directory or puts an entry below one of its files are refused, and so is an
   archive larger than kMaxBytes, one whose headers say it unpacks to more than
   internal::max_unpacked, or one of more than internal::max_members names,
   extended headers counted.

   The archive is then unpacked by this program and not by tar, with the reader
   that checked it, so nothing is unpacked as another name than the one that
   passed. Before that, nothing of the box writes a settings file any more
   (SettingsWrite), the writes under way finished first. Each file is written
   beside its place under a name of this restore's own first and takes its name
   only once every file is whole, with the permissions its entry carries, no
   set-id bits, and the owner the file had on the box where it had one;
   directories on the way, a listed one the box does not have yet included, are
   made where missing and opened without following a link, and a directory entry
   sets its directory's permissions last. A file that cannot be written leaves the
   settings as they were, writes allowed again, and the box is not restarted; one
   that cannot take its place after others did leaves them back in part, without
   a restart and with writes held off until the box is restarted by hand, and the
   detail says so.

   The login stays the box's, whatever the archive carries (storage::restoreOf): the
   access store, the name it had before and the working files of the two stores are
   not put back and stay as the box has them, and nhttpd.conf is put back with the
   lines for the account taken out and the box's own put in. A caller who may restore
   settings may not take the login over by it; the restore of the settings menu at the
   box, through restore.sh, is not this and puts back what its archive holds.

   file is a plain name in the temporary directory, of letters, digits, dots,
   dashes and underscores, ending in .tar.gz, .tgz or .tar. InvalidArgument
   BadName for any other name, NotFound NoSuchName for a name with no file behind
   it, InvalidArgument RestoreRefused for an archive that cannot be read or may
   not be put back, Internal RestoreRefused when the archive could not be taken
   over or unpacked, or the restart could not be started (the settings are back
   then, and the detail says so).

   The names are read from the archive's own headers, GNU long names and pax
   records included, and not from what a tar prints about them; where a header
   carries two names, all of them have to pass. */
Result<void> restore(const std::string &file);

/* The restart after settings were put back, by this route or by restore.sh
   from the menu, without the program saving its own settings over them: the
   system, or in a build configured --with-restore-restart=neutrino, the default
   on a generic PC, only this program, started again in its own place from the
   file and with the arguments it was started with. Its process stays, so a
   start script or a service manager that watches it sees nothing end. A moment
   after this returns. Whether the restart could be started. */
bool restartWithoutSaving();

/* The restore of the settings menu, through restore.sh, takes its turn with
   restore(): one restore at a time, and none once settings were put back and
   wait for the restart that reads them. taken() says whether this one may go
   ahead; the turn is given back when it goes. Before the script runs, the menu
   holds the settings writes off (SettingsWrite::freeze); where the script put
   nothing back it lets them go again, and where it put the settings back, in
   whole or in part, it says so with putBack(), which keeps every later restore
   off and leaves the writes held until the restart. Only the holder of the turn
   that froze them lets them go, so a restore that failed cannot let go of the
   hold of one that waits for its restart. */
/* What a restore left in the directory of the access store when the program ended
   before it was done: its working files, the copy of nhttpd.conf with the box's
   account lines among them, which nothing kept across the start. Each is kept as one
   of the files that hold who may reach the box (accessfiles), so a second name it has
   elsewhere stays held back, and removed. At the start, before the web server
   answers anybody. One that cannot be kept stays where it is, and the configuration
   directory then goes out through no gate (accessfiles::closeTheirDirectory); false,
   and why says so. */
bool keepLeftovers(std::string *why);

/* The working files of a restore that are in the directory of the access store now,
   by their full names: for the web server, which holds back what such a name stands
   for, as it does for the store's own names. */
std::vector<std::string> leftovers();

class MenuRestore
{
	public:
		MenuRestore();
		~MenuRestore();
		bool taken() const { return taken_; }
		void putBack();

	private:
		MenuRestore(const MenuRestore &);
		MenuRestore &operator=(const MenuRestore &);
		bool taken_;
};

namespace internal
{
// Where uploads and the packing directory are; a case points it at one of its own.
extern const char *tmp_dir;
// The configuration directory and the list of what is backed up besides it.
extern const char *config_dir;
extern const char *backup_list;
// The directory an archive of the box's own layout, its names under config/,
// is unpacked below.
extern const char *tuxbox_dir;
// Where backup.sh is looked for instead of the search path; NULL for the search
// path.
extern const char *script_dir;
// What an archive may come to, unpacked: more than any box's settings.
extern size_t max_members;
extern unsigned long long max_unpacked;

/* What the shell started after a restore runs where the system restarts: a
   moment's wait, then the restart. It gets this program's file as $0, its
   number as $1 and its arguments after that. A case that lets the real start
   run puts a harmless command here. */
extern const char *restart_command;
/* Whether the restart is this program's own, started again in its place, as
   a build configured --with-restore-restart=neutrino does, rather than the
   system's through that shell. A case runs the shell. */
extern bool restart_in_place;

/* Stands in for starting the restart after a restore, so no case restarts the
   machine it runs on. NULL puts the real start back. */
typedef bool (*Restarter)();
void setRestarterForTest(Restarter r);
/* What the restart would have ended: the restore waiting for it, and the
   settings files left alone until then. */
void restartedForTest();

/* Called once the box's own account lines are in the working copy a restore
   makes of nhttpd.conf, before the rest of the archive is unpacked, with the
   directory it lies in and its name there; NULL, as the program runs, for
   none. A case uses it to look at that copy while it stands. */
typedef void (*AccountLinesSeen)(int dir, const std::string &working);
void setAccountLinesSeenForTest(AccountLinesSeen seen);

/* Called once that working copy is made, before it is kept and while no gate
   asks about a descriptor, with the directory and the name it was made under,
   empty where it was made without one; with_a_name makes it under its name even
   where the filesystem could do without. NULL, as the program runs, for none. */
typedef void (*StagingMade)(int dir, const std::string &name);
void setStagingMadeForTest(StagingMade made, bool with_a_name);
extern StagingMade stagingMade;
extern bool stagingNamed;
} // namespace internal

} // namespace backup
} // namespace coreapi

#endif
