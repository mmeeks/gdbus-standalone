include $(top_srcdir)/Makefile.decl

NULL =

SUBDIRS=. tests

AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"GLib-DBus-Standalone\"			\
	-I$(top_builddir)					\
	-I$(top_srcdir)						\
	-DG_DISABLE_DEPRECATED					\
	-DG_DBUS_COMPILATION					\
	-DG_DBUS_I_UNDERSTAND_THAT_ABI_AND_API_IS_UNSTABLE	\
	$(NULL)

lib_LTLIBRARIES = libgdbus-standalone.la

marshal_sources = \
        gdbus-marshal.h	\
        gdbus-marshal.c	\
	$(NULL)

glib_genmarshal=glib-genmarshal

gdbus-marshal.h: gdbus-marshal.list
	$(glib_genmarshal) --prefix=_gdbus_marshal $(srcdir)/gdbus-marshal.list --header --internal > $@.tmp && \
	  mv $@.tmp $@

gdbus-marshal.c: gdbus-marshal.h gdbus-marshal.list
	(echo "#include \"gdbus-marshal.h\""; \
	$(glib_genmarshal) --prefix=_gdbus_marshal $(srcdir)/gdbus-marshal.list --body --internal) > $@.tmp && \
	  mv $@.tmp $@

gvariant_sources =			\
	gbitlock.c			\
	gvarianttype.c			\
	gvariant-core.c			\
	gvariant-serialiser.c		\
	gvariant-util.c			\
	gvariant-valist.c		\
	gvarianttypeinfo.c		\
	$(NULL)

gvariant_headers =			\
	gvariant.h			\
	gbitlock.h			\
	gvariant-serialiser.h		\
	gvarianttype.h			\
	gvarianttypeinfo.h		\
	$(NULL)

gdbus_headers =				\
	$(gvariant_headers)		\
	gdbus.h				\
	gdbusenums.h			\
	gdbustypes.h			\
	gdbuserror.h			\
	gdbusconnection.h		\
	gdbusnameowning.h		\
	gdbusnamewatching.h		\
	gdbusproxywatching.h		\
	gdbusproxy.h			\
	gdbusintrospection.h		\
	gdbusmethodinvocation.h		\
	gdbusserver.h			\
	$(NULL)

libgdbus_standalone_la_SOURCES =					\
	$(gvariant_sources)						\
	gdbus.h								\
	gdbustypes.h							\
	gdbusenumtypes.h						\
	gdbus-marshal.h 		gdbus-marshal.c 		\
	gdbusenumtypes.h		gdbusenumtypes.c		\
	gdbuserror.h			gdbuserror.c			\
	gdbusconnection.h		gdbusconnection.c		\
	gdbusnameowning.h		gdbusnameowning.c		\
	gdbusnamewatching.h		gdbusnamewatching.c		\
	gdbusproxywatching.h		gdbusproxywatching.c		\
	gdbusproxy.h			gdbusproxy.c			\
	gdbusprivate.h			gdbusprivate.c			\
	gdbusintrospection.h		gdbusintrospection.c		\
	gdbusconversion.h		gdbusconversion.c		\
	gdbusmethodinvocation.h		gdbusmethodinvocation.c		\
	gdbusserver.h			gdbusserver.c			\
	$(NULL)

$(libgdbus_standalone_la_OBJECTS): $(marshal_sources)

libgdbus_standalone_la_CFLAGS = 			\
	$(GLIB2_CFLAGS)					\
	$(GOBJECT2_CFLAGS)				\
	$(GIO2_CFLAGS)					\
	$(DBUS1_CFLAGS)					\
	$(NULL)

libgdbus_standalone_la_LIBADD = 			\
	$(GLIB2_LIBS)					\
	$(GOBJECT2_LIBS)				\
	$(GIO2_LIBS)					\
	$(DBUS1_LIBS)					\
	$(NULL)
install-def-file:
uninstall-def-file:

export_symbols = -export-symbols-regex '^g_.*'

LT_CURRENT=0
LT_REVISION=0
LT_AGE=0

libgdbus_standalone_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic $(no_undefined) $(export_symbols)

libgdbus_standalone_la_DEPENDENCIES = $(gdbus_def)

gdbusincludedir=$(includedir)/gdbus-standalone/gdbus
gdbusinclude_HEADERS = 			\
	$(gdbus_headers)		\
	gdbusenumtypes.h		\
	$(NULL)

# these sources (also mentioned above) are generated.
BUILT_SOURCES = 			\
	gdbus-marshal.h 		\
	gdbus-marshal.c 		\
	gdbusenumtypes.h		\
	gdbusenumtypes.c		\
	$(NULL)

EXTRA_DIST += 				\
	gdbus-marshal.list		\
	gdbusenumtypes.h.template	\
	gdbusenumtypes.c.template	\
	$(NULL)

CLEANFILES = 			\
	$(marshal_sources)	\
	$(NULL)

gdbusenumtypes.h: $(gdbus_headers) gdbusenumtypes.h.template Makefile.am
	( top_builddir=`cd $(top_builddir) && pwd`; \
	  cd $(srcdir) && glib-mkenums --template gdbusenumtypes.h.template $(gdbus_headers) ) > \
	    gdbusenumtypes.h.tmp && \
	    mv gdbusenumtypes.h.tmp gdbusenumtypes.h

gdbusenumtypes.c: $(gdbus_headers) gdbusenumtypes.c.template Makefile.am
	( top_builddir=`cd $(top_builddir) && pwd`; \
	  cd $(srcdir) && glib-mkenums --template gdbusenumtypes.c.template $(gdbus_headers) ) > \
	    gdbusenumtypes.c.tmp && mv gdbusenumtypes.c.tmp gdbusenumtypes.c

noinst_PROGRAMS =
noinst_PROGRAMS += example-own-name
noinst_PROGRAMS += example-watch-name
noinst_PROGRAMS += example-watch-proxy
noinst_PROGRAMS += example-server
noinst_PROGRAMS += example-subtree

example_watch_name_SOURCES = example-watch-name.c
example_watch_name_CFLAGS  = $(GLIB2_CFLAGS) $(GOBJECT2_CFLAGS) $(GIO2_CFLAGS)
example_watch_name_LDADD   = libgdbus-standalone.la  $(GLIB2_LIBS) $(GOBJECT2_LIBS) $(GIO2_LIBS)

example_watch_proxy_SOURCES = example-watch-proxy.c
example_watch_proxy_CFLAGS  = $(GLIB2_CFLAGS) $(GOBJECT2_CFLAGS) $(GIO2_CFLAGS)
example_watch_proxy_LDADD   = libgdbus-standalone.la  $(GLIB2_LIBS) $(GOBJECT2_LIBS) $(GIO2_LIBS)

example_own_name_SOURCES = example-own-name.c
example_own_name_CFLAGS  = $(GLIB2_CFLAGS) $(GOBJECT2_CFLAGS) $(GIO2_CFLAGS)
example_own_name_LDADD   = libgdbus-standalone.la $(GLIB2_LIBS) $(GOBJECT2_LIBS) $(GIO2_LIBS)

example_server_SOURCES = example-server.c
example_server_CFLAGS  = $(GLIB2_CFLAGS) $(GOBJECT2_CFLAGS) $(GIO2_CFLAGS)
example_server_LDADD   = libgdbus-standalone.la $(GLIB2_LIBS) $(GOBJECT2_LIBS) $(GIO2_LIBS)

example_subtree_SOURCES = example-subtree.c
example_subtree_CFLAGS  = $(GLIB2_CFLAGS) $(GOBJECT2_CFLAGS) $(GIO2_CFLAGS)
example_subtree_LDADD   = libgdbus-standalone.la $(GLIB2_LIBS) $(GOBJECT2_LIBS) $(GIO2_LIBS)

clean-local :
	rm -f *~
