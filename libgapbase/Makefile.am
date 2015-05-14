## Process this file with automake to produce Makefile.in

libexecdir = $(GAPLIBDIR)

noinst_LIBRARIES = libgapbase.a

AM_CPPFLAGS = \
	-DLOCALEDIR=\""$(LOCALEDIR)"\"

INCLUDES = \
	-I$(top_srcdir)	\
	$(GLIB_CFLAGS)	\
	$(GIMP_CFLAGS)	\
	-I$(includedir)


libgapbase_a_SOURCES = \
	gap_libgapbase.h	\
	gap_base.c		\
	gap_base.h		\
	gap_file_util.c		\
	gap_file_util.h		\
	gap_timm.c		\
	gap_timm.h		\
	gap_val_file.c		\
	gap_val_file.h

