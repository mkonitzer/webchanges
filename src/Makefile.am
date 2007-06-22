AUTOMAKE_OPTIONS = gnu
LDADD = $(XML_LIBS)
INCLUDES= $(XML_CPPFLAGS)

bin_PROGRAMS = webchanges evalxpath

BUILT_SOURCES = monfile_dtd.inc
MAINTAINERCLEANFILES = monfile_dtd.inc

webchanges_SOURCES = main.c global.h monfile.c monfile.h monfile_dtd.inc metafile.c metafile.h monitor.c monitor.h vpair.c vpair.h sha.c sha.h
evalxpath_SOURCES = evalxpath.c

monfile_dtd.inc: ../doc/wc1.dtd
	(echo const char monfile_dtd[] = \\ ; (sed 's/^/    \"/' | sed 's/$$/\\n\" \\/') < ../doc/wc1.dtd ; echo \;) > monfile_dtd.inc
