AM_CFLAGS = \
	-Wall

AM_CPPFLAGS = \
	@PYTHON_CPPFLAGS@ \
	-include Python.h

AM_CXXFLAGS = \
	-Wall \
	@ENIGMA2_CFLAGS@ \
	@UCHARDET_CFLAGS@

plugindir = $(libdir)/enigma2/python/Plugins/Extensions/ServiceApp

plugin_LTLIBRARIES = serviceapp.la

serviceapp_la_SOURCES = \
	serviceapp.cpp \
	extplayer.cpp \
	myconsole.cpp \
	m3u8.cpp \
	gstplayer.cpp \
	exteplayer3.cpp \
	common.cpp \
	cJSON/cJSON.c \
	subtitles/subtitles.cpp \
	subtitles/subrip.cpp

serviceapp_la_LDFLAGS = \
	@UCHARDET_LIBS@ \
	-avoid-version \
	-module \
	-shared
