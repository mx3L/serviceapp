import os

from enigma import eEnv

import serviceapp


ID_SERVICEMP3 = 4097
ID_SERVICEGSTPLAYER = 5001
ID_SERVICEEXTEPLAYER3 = 5002

(OPTIONS_SERVICEMP3_GSTPLAYER,
	OPTIONS_SERVICEMP3_EXTEPLAYER3,
	OPTIONS_SERVICEGSTPLAYER,
	OPTIONS_SERVICEEXTEPLAYER3,
	OPTIONS_USER) = range(5)

_SERVICEMP3_REPLACE_PATH = eEnv.resolve("$sysconfdir/enigma2/serviceapp_replaceservicemp3")


def isExtEplayer3Available():
	return os.path.isfile(eEnv.resolve("$bindir/exteplayer3"))


def isGstPlayerAvailable():
	return os.path.isfile(eEnv.resolve("$bindir/gstplayer_gst-1.0"))


def isServiceMP3Replaced():
	return os.path.isfile(_SERVICEMP3_REPLACE_PATH)


def setServiceMP3Replace(replace):
	if replace:
		open(_SERVICEMP3_REPLACE_PATH, "wb").close()
	else:
		if os.path.isfile(_SERVICEMP3_REPLACE_PATH):
			os.remove(_SERVICEMP3_REPLACE_PATH)


def setServiceMP3GstPlayer():
	serviceapp.servicemp3_gstplayer_enable()


def setServiceMP3ExtEplayer3():
	serviceapp.servicemp3_exteplayer3_enable()


def setUseUserSettings():
	serviceapp.use_user_settings()


def setGstreamerPlayerSettings(settingId, videoSink, audioSink, subtitleEnabled, bufferSize, bufferDuration):
	return serviceapp.gstplayer_set_setting(settingId, videoSink, audioSink, subtitleEnabled, bufferSize, bufferDuration)

def setExtEplayer3Settings(settingId, aacSwDecoding, dtsSwDecoding, wmaSwDecoding, lpcmInjection, downmix):
	return serviceapp.exteplayer3_set_setting(settingId, aacSwDecoding, dtsSwDecoding, wmaSwDecoding, lpcmInjection, downmix)

