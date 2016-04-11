AM_CFLAGS = \
	-Wall

AM_CPPFLAGS = \
	@PYTHON_CPPFLAGS@ \
	-include Python.h

AM_CXXFLAGS = \
	-Wall \
	@ENIGMA2_CFLAGS@

plugindir = $(libdir)/enigma2/python/Plugins/Extensions/ServiceApp

plugin_LTLIBRARIES = serviceapp.la

serviceapp_la_SOURCES = serviceapp.cpp extplayer.cpp myconsole.cpp gstplayer.cpp exteplayer3.cpp cJSON/cJSON.c
serviceapp_la_LDFLAGS = -avoid-version -module -shared
