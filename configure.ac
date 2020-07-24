AC_INIT([serviceapp], [0.1], [mx3ldev@gmail.com])
AM_INIT_AUTOMAKE([subdir-objects foreign dist-bzip2 no-define tar-pax])
AC_CONFIG_MACRO_DIR([m4])
LT_INIT

AC_PROG_CC
AC_PROG_CXX
AM_PATH_PYTHON
AX_PYTHON_DEVEL
AC_LANG(C++)

PKG_CHECK_MODULES(ENIGMA2, enigma2)
PKG_CHECK_MODULES(UCHARDET, uchardet)
PKG_CHECK_MODULES(OPENSSL, openssl)

have_mhw=no
AC_CHECK_HEADER([enigma2/lib/dvb/lowlevel/mhw.h], 
                 [have_mhw=yes], [])
if test "x${have_mhw}" = xyes; then
    AC_DEFINE([HAVE_EPG])
else
    echo "-------------------------------------------"
    echo " Unable to find mhw.h                      "
    echo " Building a serviceapp without EPG support "
    echo "-------------------------------------------"
fi

AC_DEFINE([DEBUG])

SAVE_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$PYTHON_CPPFLAGS -include Python.h $ENIGMA2_CFLAGS $CXXFLAGS"
SAVE_LDFLAGS="$LDFLAGS"
LDFLAGS="$PYTHON_LIBS $LDFLAGS -shared -fPIC"

AC_MSG_CHECKING([for openpli iservice.h version(2)])
AC_LINK_IFELSE(
    [AC_LANG_PROGRAM(
[#include <enigma2/lib/service/iservice.h>
 class MyPlayableService: public iPlayableService
 {
    void AddRef(){}
    void Release(){}

#if SIGCXX_MAJOR_VERSION == 2
    RESULT connectEvent(const sigc::slot2<void,iPlayableService*,int> &event, ePtr<eConnection> &connection){return -1;}
#else
    RESULT connectEvent(const Slot2<void,iPlayableService*,int> &event, ePtr<eConnection> &connection){return -1;}
#endif
    RESULT start(){return -1;}
    RESULT stop(){return -1;}
    RESULT setTarget(int target, bool noaudio=false){return -1;}; //only this function was changed
    SWIG_VOID(RESULT) seek(ePtr<iSeekableService> &SWIG_OUTPUT){};
    SWIG_VOID(RESULT) pause(ePtr<iPauseableService> &SWIG_OUTPUT){};
    SWIG_VOID(RESULT) info(ePtr<iServiceInformation> &SWIG_OUTPUT){};
    SWIG_VOID(RESULT) audioTracks(ePtr<iAudioTrackSelection> &SWIG_OUTPUT){};
    SWIG_VOID(RESULT) audioChannel(ePtr<iAudioChannelSelection> &SWIG_OUTPUT){};
    SWIG_VOID(RESULT) subServices(ePtr<iSubserviceList> &SWIG_OUTPUT){};
    SWIG_VOID(RESULT) frontendInfo(ePtr<iFrontendInformation> &SWIG_OUTPUT){};
    SWIG_VOID(RESULT) timeshift(ePtr<iTimeshiftService> &SWIG_OUTPUT){};
    SWIG_VOID(RESULT) tap(ePtr<iTapService> &ptr) { ptr = nullptr; return -1; };
    SWIG_VOID(RESULT) cueSheet(ePtr<iCueSheet> &SWIG_OUTPUT){};
    SWIG_VOID(RESULT) subtitle(ePtr<iSubtitleOutput> &SWIG_OUTPUT){};
    SWIG_VOID(RESULT) audioDelay(ePtr<iAudioDelay> &SWIG_OUTPUT){};
    SWIG_VOID(RESULT) rdsDecoder(ePtr<iRdsDecoder> &SWIG_OUTPUT){};
    SWIG_VOID(RESULT) stream(ePtr<iStreamableService> &SWIG_OUTPUT){};
    SWIG_VOID(RESULT) streamed(ePtr<iStreamedService> &SWIG_OUTPUT){};
    SWIG_VOID(RESULT) keys(ePtr<iServiceKeys> &SWIG_OUTPUT){};
    void setQpipMode(bool value, bool audio){};
};],[MyPlayableService service;])
    ],[
        AC_MSG_RESULT([yes])
        AC_DEFINE(OPENPLI_ISERVICE_VERSION, 2)
    ],[
        AC_MSG_RESULT([no])
        AC_DEFINE(OPENPLI_ISERVICE_VERSION, 1)
    ])

AC_PATH_PROG(MSGFMT, msgfmt, AC_MSG_ERROR(Could not find msgfmt))

CXXFLAGS="$SAVE_CXXFLAGS"
LDFLAGS="$SAVE_LDFLAGS"


AC_CONFIG_FILES([
Makefile
src/Makefile
src/serviceapp/Makefile
src/plugin/Makefile
src/locale/Makefile

])
AC_OUTPUT
