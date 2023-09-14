EXTRA_DIST = \
	backup.sh backup.sh.in \
	restore.sh restore.sh.in \
	backup_flash.sh backup_flash.sh.in \
	restore_flash.sh restore_flash.sh.in


if BOXTYPE_GENERIC
EXTRA_DIST += \
	neutrino.sh neutrino.sh.in
endif

installdir = $(bindir)

install_SCRIPTS = \
	backup.sh \
	restore.sh \
	install.sh 



#install_SCRIPTS += \
#	start_flash.sh
#
# if BOXMODEL_CST_HD2
# install_SCRIPTS += \
# 	backup_flash.sh \
# 	restore_flash.sh
# endif
#
# if BOXTYPE_ARMBOX
# install_SCRIPTS += \
# 	restore_flash.sh
# endif
#
#install_SCRIPTS += \
#	start_flash.sh
#
#if BOXMODEL_CST_HD2
#install_SCRIPTS += \
#	backup_flash.sh \
#	restore_flash.sh
#endif
#
#if BOXTYPE_ARMBOX
#install_SCRIPTS += \
#	restore_flash.sh
#endif
#
#if BOXTYPE_GENERIC
#install_SCRIPTS += \
#	neutrino.sh
#endif

