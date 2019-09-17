include $(top_srcdir)/config/Rules.am

DEFAULT_INCLUDES += \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/lib/libspl/include

if BUILD_LINUX
DEFAULT_INCLUDES += \
	-I$(top_srcdir)/lib/libspl/include/os/linux
endif

if BUILD_FREEBSD
DEFAULT_INCLUDES += \
	-I$(top_srcdir)/lib/libspl/include/os/freebsd
endif

sbin_PROGRAMS = zinject

zinject_SOURCES = \
	translate.c \
	zinject.c \
	zinject.h

zinject_LDADD = \
	$(top_builddir)/lib/libnvpair/libnvpair.la \
	$(top_builddir)/lib/libzfs/libzfs.la
