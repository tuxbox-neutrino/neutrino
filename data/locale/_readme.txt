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
the ordering. Do not add any empty lines.

2.)
Enter the directory build_tmp/neutrino-hd/data/locale.

3.)
Use for sorting (deutsch.locale,english.locale) 'make sort-locals', use 'make ordercheck' to for
verification.

4.)
Create new versions of the files src/system/locals.h and src/system/locals_intern.h
using the command 'make locals.h locals_intern.h'.

5.)
Check the modifications with 'make check'

6.)
Copy the replacement file to their destination with 'make install-locals'

7.)
Or use for item 3-6 'make locals'

8.)
To the extent possible, update other locale file. For this, the
Perl-script create-locals-update.pl may be useful. 

9.)
If committing the changes to Git, commit both the involved
locale-files, src/system/locals.h, and src/system/locals_intern.h.

Useful tools:
-------------
- emacs (add '(file-coding-system-alist (quote (("\\.locale\\'" . utf-8-unix) ("" undecided)))) to .emacs or use "C-x <RET> c utf-8 <RET> C-x C-f deutsch.locale")
- iconv
- sort
- uxterm
