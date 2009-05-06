include $(top_srcdir)/Makefile.decl
NULL =

# The name of the module.
DOC_MODULE=gdbus-standalone

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gdbus-docs.xml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="G_DISABLE_DEPRECATED"

#SCAN_OPTIONS+=--rebuild-sections

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=$(top_srcdir)/gdbus

HFILE_GLOB=$(top_srcdir)/gdbus/*.h
CFILE_GLOB=$(top_srcdir)/gdbus/*.c

# Headers to ignore
IGNORE_HFILES=				\
        gdbus-marshal.h			\
        gdbus.h				\
        gdbusalias.h			\
        gdbusalias.h			\
        gdbusenumtypes.h		\
        giomodule-priv.h		\
        $(NULL)

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
INCLUDES = \
        -I$(srcdir)                     \
        -I$(top_srcdir)                 \
        -I$(top_builddir)               \
	$(GLIB2_CFLAGS)			\
	$(GOBJECT2_CFLAGS)		\
	$(GIO2_CFLAGS)			\
	$(NULL)

GTKDOC_LIBS = \
	$(GLIB2_LIBS)					\
	$(GOBJECT2_LIBS)				\
	$(GIO2_LIBS)					\
        $(top_builddir)/gdbus/libgdbus-standalone.la	\
        $(NULL)

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS = --output-format=xml --sgml-mode --name-space=g

# Images to copy into HTML directory
HTML_IMAGES =			\
        $(NULL)

content_files =                 \
        version.xml		\
        $(NULL)

expand_content_files =		\
        $(NULL)

extra_files =                   \
        version.xml.in		\
        $(NULL)

include $(top_srcdir)/gtk-doc.make

EXTRA_DIST +=				\
        version.xml.in
