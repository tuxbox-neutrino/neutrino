Format of .locale files:
------------------------
character encoding: UTF-8
filename suffix   : .locale

Files must be strictly alphabetically ordered, and must not contain
any empty lines.

Destination of .locale files:
-----------------------------
directory: /var/tuxbox/config/locale or /share/tuxbox/neutrino/locale

Master file:
------------
deutsch.locale is considered the master file.


Verfication of .locale files:
-----------------------------
Use the check.locale.files shell script for detecting
- violations of the sorting order,
- missing translations and
- legacy strings.



How do I add a new locale string?
---------------------------------
1.)
First of all, add the new string to deutsch.locale while preserving
the ordering. Do not add any empty lines. Use `make ordercheck' to for
verification.

2.)
Enter the directory apps/tuxbox/neutrino/data/locale.

3.)
Create new versions of the files apps/tuxbox/neutrino/src/system/locals.h
and apps/tuxbox/neutrino/src/system/locals_intern.h
using the command `make locals.h locals_intern.h'.

4.)
Check the modifications with `make check', or with
diff locals.h ../../src/system/locals.h
diff locals_intern.h ../../src/system/locals_intern.h

5.)
Copy the replacement file to their destination with `make install-locals',
or with
cp -p locals.h ../../src/system/locals.h
cp -p locals_intern.h ../../src/system/locals_intern.h

6.)
To the extent possible, update other locale file. For this, the
Perl-script create-locals-update.pl may be useful. 

7.)
If committing the changes to CVS, commit both the involved
locale-files, apps/tuxbox/neutrino/src/system/locals.h, and
as apps/tuxbox/neutrino/src/system/locals_intern.h.

Useful tools:
-------------
- emacs (add '(file-coding-system-alist (quote (("\\.locale\\'" . utf-8-unix) ("" undecided)))) to .emacs or use "C-x <RET> c utf-8 <RET> C-x C-f deutsch.locale")
- iconv
- sort
- uxterm
