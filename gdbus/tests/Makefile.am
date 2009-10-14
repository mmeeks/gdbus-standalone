include $(top_srcdir)/Makefile.decl

NULL =

INCLUDES =                      \
        -g                      \
        -I$(top_srcdir)         \
	$(GLIB2_CFLAGS)		\
	$(GOBJECT2_CFLAGS)	\
	$(GIO2_CFLAGS)		\
	$(GTHREAD2_CFLAGS)	\
	$(NULL)

noinst_PROGRAMS = $(TEST_PROGS)
progs_ldadd     =                                       \
	$(GLIB2_LIBS)					\
	$(GOBJECT2_LIBS)				\
	$(GIO2_LIBS)					\
	$(GTHREAD2_LIBS)				\
        $(top_builddir)/gdbus/libgdbus-standalone.la	\
	$(NULL)

TEST_PROGS += connection
TEST_PROGS += names
TEST_PROGS += proxy
TEST_PROGS += introspection
TEST_PROGS += threading
TEST_PROGS += export

connection_SOURCES = connection.c sessionbus.c sessionbus.h tests.h tests.c
connection_LDADD = $(progs_ldadd)

names_SOURCES = names.c sessionbus.c sessionbus.h tests.h tests.c
names_LDADD = $(progs_ldadd)

proxy_SOURCES = proxy.c sessionbus.c sessionbus.h tests.h tests.c
proxy_LDADD = $(progs_ldadd)

introspection_SOURCES = introspection.c sessionbus.c sessionbus.h tests.h tests.c
introspection_LDADD = $(progs_ldadd)

threading_SOURCES = threading.c sessionbus.c sessionbus.h tests.h tests.c
threading_LDADD = $(progs_ldadd)

export_SOURCES = export.c sessionbus.c sessionbus.h tests.h tests.c
export_CFLAGS = $(DBUS1_CFLAGS)
export_LDADD = $(progs_ldadd)
