# Two halves, because the two cost different things to run. unit/ compiles
# the suite and every table it reads; web/ reads the source of the pages and
# starts no compiler at all. Split so that a check of the page is something a
# person runs while writing a screen rather than something they wait out behind
# seventy translation units they did not touch.
SUBDIRS = unit

# The checks against a running box live beside the suite and are run by hand;
# they are shipped with the tree so that the document naming them is true of a
# tarball as well.
EXTRA_DIST = http/check-v1.sh http/check-remotebox.sh
