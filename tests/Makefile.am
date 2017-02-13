AM_CPPFLAGS = 			\
	-I$(top_srcdir) 	\
	$(WARN_CFLAGS) 		\
	$(DEP_CFLAGS)

AM_LDFLAGS = $(WARN_LDFLAGS)

LDADD = $(DEP_LIBS)

noinst_PROGRAMS = $(TEST_PROGS)
TEST_PROGS =

TEST_PROGS += test-request
test_request_SOURCES =	\
	test-request.c	\
	zhelpers.h

EXTRA_DIST =				\
	ccr-gst-launch-record.sh	\
	ccr-gst-launch-view.sh

-include $(top_srcdir)/git.mk
