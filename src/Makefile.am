SUBDIRS = libcheese

AM_CPPFLAGS =			\
	-I$(top_srcdir)		\
	-I$(srcdir)/libcheese	\
	-I$(builddir)/libcheese	\
	$(DEP_CFLAGS)		\
	$(WARN_CFLAGS)

AM_LDFLAGS = $(WARN_LDFLAGS)

bin_PROGRAMS = cosy-camera-recorder

cosy_camera_recorder_SOURCES =	\
	cosy-camera-recorder.c

cosy_camera_recorder_LDADD =	\
	libcheese/libcheese.la	\
	$(DEP_LIBS)

-include $(top_srcdir)/git.mk
