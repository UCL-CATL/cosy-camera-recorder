AM_CPPFLAGS =		\
	-I$(top_srcdir)	\
	$(DEP_CFLAGS)	\
	$(WARN_CFLAGS)

AM_LDFLAGS = $(WARN_LDFLAGS)

bin_PROGRAMS = cosy-camera-recorder

cosy_camera_recorder_SOURCES =	\
	cosy-camera-recorder.c

cosy_camera_recorder_LDADD = $(DEP_LIBS)

-include $(top_srcdir)/git.mk
