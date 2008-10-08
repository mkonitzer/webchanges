AUTOMAKE_OPTIONS = gnu
LDADD = $(XML_LIBS)
INCLUDES = $(XML_CPPFLAGS)

bin_PROGRAMS = webchanges evalxpath

BUILT_SOURCES = monfile_dtd.inc
MAINTAINERCLEANFILES = monfile_dtd.inc gmain-res.rc
EXTRA_DIST = gmain16.xpm gmain32.xpm gmain48.xpm gmain.ico

if COMPILE_GUI
bin_PROGRAMS += gwebchanges 
gwebchanges_SOURCES = gmain.cc gmain.h basedir.c basedir.h global.h monfile.c monfile.h monfile_dtd.inc metafile.c metafile.h monitor.c monitor.h vpair.c vpair.h sha1.c sha1.h
gwebchanges_CFLAGS = -x c++ $(WX_CFLAGS_ONLY)
gwebchanges_CXXFLAGS = $(WX_CXXFLAGS_ONLY)
gwebchanges_CPPFLAGS = $(WX_CPPFLAGS)
gwebchanges_LDADD = $(WX_LIBS) $(LDADD)
if COMPILE_GUI_RESOURCE
gwebchanges_SOURCES += gmain-res.rc
.rc.o:
	$(WINDRES) $(WINDRESFLAGS) $^ -o $@
endif
endif

webchanges_SOURCES = main.c basedir.c basedir.h global.h monfile.c monfile.h monfile_dtd.inc metafile.c metafile.h monitor.c monitor.h vpair.c vpair.h sha1.c sha1.h
evalxpath_SOURCES = evalxpath.c

monfile_dtd.inc: ../doc/wc1.dtd
	(echo const char monfile_dtd[] = \\ ; (sed 's/^/    \"/' | sed 's/$$/\\n\" \\/') < ../doc/wc1.dtd ; echo \;) > monfile_dtd.inc
