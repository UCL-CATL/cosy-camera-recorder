AM_CPPFLAGS =				\
	-I$(top_srcdir)			\
	$(DEP_CFLAGS)			\
	$(WARN_CFLAGS)			\
	-Wno-switch-enum		\
	-Wno-deprecated-declarations

cheese_headers =			\
	cheese.h			\
	cheese-camera.h			\
	cheese-camera-device.h		\
	cheese-camera-device-monitor.h	\
	cheese-fileutil.h		\
	cheese-gtk.h			\
	cheese-widget.h			\
	cheese-widget-private.h		\
	totem-aspect-frame.h

cheese_c_files =			\
	cheese.c			\
	cheese-camera.c			\
	cheese-camera-device.c		\
	cheese-camera-device-monitor.c	\
	cheese-fileutil.c		\
	cheese-gtk.c			\
	cheese-widget.c			\
	totem-aspect-frame.c

cheese_built_headers =	\
	cheese-enums.h

cheese_built_c_files =	\
	cheese-enums.c

# Statically-linked helper library
noinst_LTLIBRARIES = libcheese.la

libcheese_la_SOURCES =		\
	$(cheese_c_files)	\
	$(cheese_headers)

nodist_libcheese_la_SOURCES =	\
	$(cheese_built_c_files)	\
	$(cheese_built_headers)

BUILT_SOURCES =			\
	$(cheese_built_c_files)	\
	$(cheese_built_headers)

libcheese_la_LDFLAGS =	\
	-no-undefined	\
	$(WARN_LDFLAGS)

libcheese_la_LIBADD =	\
	$(DEP_LIBS)	\
	$(LIBM)

ENUM_TYPES =		\
	cheese-widget.h

cheese-enums.h: cheese-enums.h.template $(ENUM_TYPES) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template cheese-enums.h.template $(ENUM_TYPES)) > $@

cheese-enums.c: cheese-enums.c.template $(ENUM_TYPES) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template cheese-enums.c.template $(ENUM_TYPES)) > $@

EXTRA_DIST =			\
	cheese-enums.c.template	\
	cheese-enums.h.template

CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
