import os

from enigma import eEnv

import serviceapp


ID_SERVICEMP3 = 4097
ID_SERVICEGSTPLAYER = 5001
ID_SERVICEEXTEPLAYER3 = 5002

OPTIONS_SERVICEMP3_GSTPLAYER = 0
OPTIONS_SERVICEMP3_EXTEPLAYER3 = 0
OPTIONS_SERVICEMP3 = 0
OPTIONS_SERVICEGSTPLAYER = 1
OPTIONS_SERVICEEXTEPLAYER3 = 2
OPTIONS_USER = 3

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


def setServiceAppSettings(settingId, HLSExplorer, autoSelectStream, connectionSpeedInKb, autoTurnOnSubtitles=True):
	return serviceapp.serviceapp_set_setting(settingId,
                HLSExplorer,
                autoSelectStream,
                connectionSpeedInKb,
                autoTurnOnSubtitles)


def setGstreamerPlayerSettings(settingId, videoSink, audioSink, subtitleEnabled, bufferSize, bufferDuration):
	return serviceapp.gstplayer_set_setting(settingId,
                videoSink,
                audioSink,
                subtitleEnabled,
                bufferSize,
                bufferDuration)


def setExtEplayer3Settings(settingId, aacSwDecoding, dtsSwDecoding, wmaSwDecoding, lpcmInjection, downmix, ac3SwDecoding=False, eac3SwDecoding=False, mp3SwDecoding=False, rtmpProtocol=0):
	return serviceapp.exteplayer3_set_setting(settingId,
                aacSwDecoding,
                dtsSwDecoding,
                wmaSwDecoding,
                lpcmInjection,
                downmix,
                ac3SwDecoding,
                eac3SwDecoding,
                mp3SwDecoding,
				rtmpProtocol)
