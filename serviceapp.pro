# Created by and for Qt Creator. This file was created for editing the project sources only.
# You may attempt to use it for building too, by modifying this file here.

TEMPLATE = lib

CONFIG -= qt
CONFIG -= app_bundle
CONFIG += plugin no_plugin_name_prefix

__i686__ {
CONFIG += link_pkgconfig
PKGCONFIG += python enigma2 uchardet openssl

QMAKE_CXXFLAGS = -include Python.h
DEFINES += OPENPLI_ISERVICE_VERSION=1 DEBUG=1

DESTDIR = ./output_686/
OBJECTS_DIR = ./output_686/obj/
MOC_DIR = ./output_686/moc/
RCC_DIR = ./output_686/rcc/
UI_DIR = ./output_686/ui/

target.path = /usr/local/e2/bin
INSTALLS += target
}

__mips32el__ {
DESTDIR = ./output_mips32el/
OBJECTS_DIR = ./output_mips32el/obj/
MOC_DIR = ./output_mips32el/moc/
RCC_DIR = ./output_mips32el/rcc/
UI_DIR = ./output_mips32el/ui/

DEFINES += OPENPLI_ISERVICE_VERSION=2 DEBUG=1 HAVE_EPG=1

SYSROOT = /home/marko/Projects/Enigma2/openpli-oe-core2/build/tmp/sysroots/vusolose
CROSS_COMPILE           = mipsel-oe-linux-

QMAKE_CC                = $${CROSS_COMPILE}gcc
QMAKE_CFLAGS            = --sysroot=$$SYSROOT -D PLATFORM_MIPSEL -mel -mabi=32 -march=mips32 -fno-rtti -fno-exceptions -Wall
QMAKE_CXX               = $${CROSS_COMPILE}g++
QMAKE_CXXFLAGS          = -include Python.h --sysroot=$$SYSROOT -D PLATFORM_MIPSEL -mel -mabi=32 -march=mips32 -fno-rtti -fno-exceptions -Wall
QMAKE_LINK              = $${QMAKE_CXX}
QMAKE_LFLAGS            = --sysroot=$$SYSROOT -D PLATFORM_MIPSEL -mel -mabi=32 -march=mips32

INCLUDEPATH += \
    $$SYSROOT/usr/include/python2.7 \
    $$SYSROOT/usr/include/enigma2 \
    $$SYSROOT/usr/include/sigc++-2.0 \
    $$SYSROOT/usr/lib/sigc++-2.0/include

LIBS += -luchardet -lssl -lcrypto

target.path = /usr/lib/enigma2/python/Plugins/SystemPlugins/ServiceApp
python.path = /usr/lib/enigma2/python/Plugins/SystemPlugins/ServiceApp
python.files = \
    $$PWD/src/plugin/__init__.py \
    $$PWD/src/plugin/plugin.py \
    $$PWD/src/plugin/serviceapp_client.py \

INSTALLS += target python
}

HEADERS = \
   $$PWD/src/serviceapp/cJSON/cJSON.h \
   $$PWD/src/serviceapp/subtitles/esubtitle2.h \
   $$PWD/src/serviceapp/subtitles/subrip.h \
   $$PWD/src/serviceapp/subtitles/subtitles.h \
   $$PWD/src/serviceapp/common.h \
   $$PWD/src/serviceapp/debug.h \
   $$PWD/src/serviceapp/exteplayer3.h \
   $$PWD/src/serviceapp/extplayer.h \
   $$PWD/src/serviceapp/gstplayer.h \
   $$PWD/src/serviceapp/m3u8.h \
   $$PWD/src/serviceapp/myconsole.h \
   $$PWD/src/serviceapp/serviceapp.h \
   $$PWD/src/serviceapp/wrappers.h \
   $$PWD/configure.ac

SOURCES = \
   $$PWD/src/serviceapp/cJSON/cJSON.c \
   $$PWD/src/serviceapp/subtitles/subrip.cpp \
   $$PWD/src/serviceapp/subtitles/subtitles.cpp \
   $$PWD/src/serviceapp/common.cpp \
   $$PWD/src/serviceapp/exteplayer3.cpp \
   $$PWD/src/serviceapp/extplayer.cpp \
   $$PWD/src/serviceapp/gstplayer.cpp \
   $$PWD/src/serviceapp/m3u8.cpp \
   $$PWD/src/serviceapp/myconsole.cpp \
   $$PWD/src/serviceapp/serviceapp.cpp \
   $$PWD/src/serviceapp/wrappers.cpp \
   #$$PWD/test/explore_m3u8.cpp

INCLUDEPATH += \
    $$PWD/src/serviceapp \
    $$PWD/src/serviceapp/cJSON \
    $$PWD/src/serviceapp/subtitles \
    $$PWD/test \

DISTFILES += \
   $$PWD/src/plugin/__init__.py \
   $$PWD/src/plugin/plugin.py \
   $$PWD/src/plugin/serviceapp_client.py \

CONFIG(release, debug|release) {
    QMAKE_POST_LINK=$(STRIP) $(DESTDIR)$(TARGET)
}
