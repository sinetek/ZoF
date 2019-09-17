include $(top_srcdir)/config/Rules.am

# Unconditionally enable debugging for zdb
AM_CPPFLAGS += -DDEBUG -UNDEBUG

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

sbin_PROGRAMS = zdb

zdb_SOURCES = \
	zdb.c \
	zdb_il.c \
	zdb.h

zdb_LDADD = \
	$(top_builddir)/lib/libnvpair/libnvpair.la \
	$(top_builddir)/lib/libzpool/libzpool.la
