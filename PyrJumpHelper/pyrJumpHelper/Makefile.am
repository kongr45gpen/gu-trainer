lib_LTLIBRARIES = pyrJumpHelper.la

pyrJumpHelper_la_SOURCES = pyrJumpHelper.cpp
pyrJumpHelper_la_CXXFLAGS= -I$(top_srcdir)/include -I$(top_srcdir)/plugins/plugin_utils
pyrJumpHelper_la_LDFLAGS = -module -avoid-version -shared
pyrJumpHelper_la_LIBADD = $(top_builddir)/plugins/plugin_utils/libplugin_utils.la

AM_CPPFLAGS = $(CONF_CPPFLAGS)
AM_CFLAGS = $(CONF_CFLAGS)
AM_CXXFLAGS = $(CONF_CXXFLAGS)

EXTRA_DIST = \
	README.txt \
	pyrJumpHelper.def \
	pyrJumpHelper.sln \
	pyrJumpHelper.vcxproj

MAINTAINERCLEANFILES =	\
	Makefile.in
