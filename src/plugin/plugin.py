import os
import json

from Components.ActionMap import ActionMap
from Components.ConfigList import ConfigListScreen
from Components.Console import Console
from Components.config import config, ConfigSubsection, ConfigSelection, ConfigBoolean, \
	getConfigListEntry, ConfigSubDict, ConfigInteger, ConfigNothing
from Components.Label import Label
from Components.Sources.StaticText import StaticText
from Plugins.Plugin import PluginDescriptor
from Screens.InfoBar import InfoBar, MoviePlayer
from Screens.MessageBox import MessageBox
from Screens.Screen import Screen
from Tools.BoundFunction import boundFunction
from enigma import eEnv, eServiceReference

import serviceapp_client


SINKS_DEFAULT = ("dvbvideosink", "dvbaudiosink")
SINKS_EXPERIMENTAL = ("dvbvideosinkexp", "dvbaudiosinkexp")

sinkChoices = []
if (os.path.isfile(eEnv.resolve("$libdir/gstreamer-1.0/libgstdvbvideosink.so")) and
			os.path.isfile(eEnv.resolve("$libdir/gstreamer-1.0/libgstdvbaudiosink.so"))):
	sinkChoices.append("original")
if (os.path.isfile(eEnv.resolve("$libdir/gstreamer-1.0/libgstdvbvideosinkexp.so")) and
			os.path.isfile(eEnv.resolve("$libdir/gstreamer-1.0/libgstdvbaudiosinkexp.so"))):
	sinkChoices.append("experimental")

playerChoices = ["gstplayer", "exteplayer3"]
GSTPLAYER_VERSION = None
EXTEPLAYER3_VERSION = None

config.plugins.serviceapp = ConfigSubsection()
configServiceApp = config.plugins.serviceapp

configServiceApp.servicemp3 = ConfigSubsection()
configServiceApp.servicemp3.replace = ConfigBoolean(default=False, descriptions={0: "original", 1: "serviceapp"})
configServiceApp.servicemp3.replace.value = serviceapp_client.isServiceMP3Replaced()
configServiceApp.servicemp3.player = ConfigSelection(default="gstplayer", choices=playerChoices)

configServiceApp.gstplayer = ConfigSubDict()
configServiceApp.gstplayer["servicemp3"] = ConfigSubsection()
configServiceApp.gstplayer["servicegstplayer"] = ConfigSubsection()
for key in configServiceApp.gstplayer.keys():
	configServiceApp.gstplayer[key].sink = ConfigSelection(default="original", choices=sinkChoices)
	configServiceApp.gstplayer[key].bufferSize = ConfigInteger(8192, (1024, 1024 * 64))
	configServiceApp.gstplayer[key].bufferDuration = ConfigInteger(0, (0, 100))
	configServiceApp.gstplayer[key].subtitleEnabled = ConfigBoolean(default=True)

configServiceApp.exteplayer3 = ConfigSubDict()
configServiceApp.exteplayer3["servicemp3"] = ConfigSubsection()
configServiceApp.exteplayer3["serviceexteplayer3"] = ConfigSubsection()
for key in configServiceApp.exteplayer3.keys():
	configServiceApp.exteplayer3[key].aacSwDecoding = ConfigBoolean(default=False)
	configServiceApp.exteplayer3[key].dtsSwDecoding = ConfigBoolean(default=False)
	configServiceApp.exteplayer3[key].wmaSwDecoding = ConfigBoolean(default=False)
	configServiceApp.exteplayer3[key].lpcmInjection = ConfigBoolean(default=False)
	configServiceApp.exteplayer3[key].downmix = ConfigBoolean(default=False)


def initServiceAppSettings():
	for key in configServiceApp.gstplayer.keys():
		if key == "servicemp3":
			settingId = serviceapp_client.OPTIONS_SERVICEMP3
		elif key == "servicegstplayer":
			settingId = serviceapp_client.OPTIONS_SERVICEGSTPLAYER
		else:
			continue
		playerCfg = configServiceApp.gstplayer[key]
		if playerCfg.sink.value == "original":
			videoSink, audioSink = SINKS_DEFAULT
		elif playerCfg.sink.value == "experimental":
			videoSink, audioSink = SINKS_EXPERIMENTAL
		else:
			continue
		subtitleEnabled = playerCfg.subtitleEnabled.value
		bufferSize = playerCfg.bufferSize.value
		bufferDuration = playerCfg.bufferDuration.value

		serviceapp_client.setGstreamerPlayerSettings(settingId, videoSink, audioSink, subtitleEnabled, bufferSize, bufferDuration)

	for key in configServiceApp.exteplayer3.keys():
		if key == "servicemp3":
			settingId = serviceapp_client.OPTIONS_SERVICEMP3
		elif key == "serviceexteplayer3":
			settingId = serviceapp_client.OPTIONS_SERVICEEXTEPLAYER3
		else:
			continue
		playerCfg = configServiceApp.exteplayer3[key]
		aacSwDecoding = playerCfg.aacSwDecoding.value
		dtsSwDecoding = playerCfg.dtsSwDecoding.value
		wmaSwDecoding = playerCfg.wmaSwDecoding.value
		lpcmInjection = playerCfg.lpcmInjection.value
		downmix = playerCfg.downmix.value

		serviceapp_client.setExtEplayer3Settings(settingId, aacSwDecoding, dtsSwDecoding, wmaSwDecoding, lpcmInjection, downmix)

	if configServiceApp.servicemp3.player.value == "gstplayer":
		serviceapp_client.setServiceMP3GstPlayer()
	elif configServiceApp.servicemp3.player.value == "exteplayer3":
		serviceapp_client.setServiceMP3ExtEplayer3()

initServiceAppSettings()


class ServiceAppSettings(ConfigListScreen, Screen):
	def __init__(self, session):
		Screen.__init__(self, session)
		self.skinName = ["ServiceAppSettings", "Setup"]
		ConfigListScreen.__init__(self, [], session)
		self.onLayoutFinish.append(self.initConfigList)
		self.onClose.append(self.deInitConfig)
		self["key_red"] = StaticText(_("Cancel"))
		self["key_green"] = StaticText(_("Ok"))
		self["description"] = Label("")
		self["setupActions"] = ActionMap(["SetupActions", "ColorActions"],
			{
				"cancel": self.keyCancel,
				"red": self.keyCancel,
				"ok": self.keyOk,
				"green": self.keyOk,
			}, -2)

	def initConfigList(self):
		configServiceApp.servicemp3.player.addNotifier(self.serviceMP3PlayerChanged, initial_call=False)
		configServiceApp.servicemp3.replace.addNotifier(self.serviceMP3ReplacedChanged, initial_call=False)
		self.buildConfigList()
		self.setTitle(_("ServiceApp"))

	def gstPlayerOptions(self, gstPlayerOptionsCfg):
		configList = [getConfigListEntry("  " + _("GstPlayer"), ConfigSelection([GSTPLAYER_VERSION or "not installed"], GSTPLAYER_VERSION or "not installed"))]
		configList.append(getConfigListEntry("  " + _("Sink"), gstPlayerOptionsCfg.sink, _("Select sink that you want to use.")))
		configList.append(getConfigListEntry("  " + _("Subtitles"), gstPlayerOptionsCfg.subtitleEnabled, _("Turn on the subtitles.")))
		configList.append(getConfigListEntry("  " + _("Buffer size"), gstPlayerOptionsCfg.bufferSize, _("Set buffer size in kilobytes.")))
		configList.append(getConfigListEntry("  " + _("Buffer duration"), gstPlayerOptionsCfg.bufferDuration, _("Set buffer duration in seconds.")))
		return configList

	def extEplayer3Options(self, extEplayer3OptionsCfg):
		configList = [getConfigListEntry("  " + _("ExtEplayer3"), ConfigSelection([EXTEPLAYER3_VERSION or "not installed"], EXTEPLAYER3_VERSION or "not installed"))]
		configList.append(getConfigListEntry("  " + _("AAC software decoding"), extEplayer3OptionsCfg.aacSwDecoding, _("Turn on AAC software decoding.")))
		configList.append(getConfigListEntry("  " + _("DTS software decoding"), extEplayer3OptionsCfg.dtsSwDecoding, _("Turn on DTS software decoding.")))
		configList.append(getConfigListEntry("  " + _("WMA software decoding"), extEplayer3OptionsCfg.wmaSwDecoding, _("Turn on WMA1, WMA2, WMA/PRO software decoding.")))
		configList.append(getConfigListEntry("  " + _("Stereo downmix"), extEplayer3OptionsCfg.downmix, _("Turn on downmix to stereo, when software decoding is in use")))
		configList.append(getConfigListEntry("  " + _("LPCM injection"), extEplayer3OptionsCfg.lpcmInjection, _("Software decoder use LPCM for injection (otherwise wav PCM will be used)")))
		return configList

	def buildConfigList(self):
		configList = [getConfigListEntry(_("Enigma2 playback system"), configServiceApp.servicemp3.replace, _("Select the player who will be used for Enigma2 playback."))]
		if configServiceApp.servicemp3.replace.value:
			configList.append(getConfigListEntry(_("Player"), configServiceApp.servicemp3.player, _("Select the player who will be used in serviceapp for Enigma2 playback.")))
			configListServiceMp3 = [getConfigListEntry("", ConfigNothing())]
			configListServiceMp3.append(getConfigListEntry(_("ServiceMp3 (%s)" % str(serviceapp_client.ID_SERVICEMP3)), ConfigNothing()))
			if configServiceApp.servicemp3.player.value == "gstplayer":
				configList += configListServiceMp3 + self.gstPlayerOptions(configServiceApp.gstplayer["servicemp3"])
			elif configServiceApp.servicemp3.player.value == "exteplayer3":
				configList += configListServiceMp3 + self.extEplayer3Options(configServiceApp.exteplayer3["servicemp3"])
			else:
				configList += configListServiceMp3
		configList.append(getConfigListEntry("", ConfigNothing()))
		configList.append(getConfigListEntry(_("ServiceGstPlayer (%s)" % str(serviceapp_client.ID_SERVICEGSTPLAYER)), ConfigNothing()))
		configList += self.gstPlayerOptions(configServiceApp.gstplayer["servicegstplayer"])
		configList.append(getConfigListEntry("", ConfigNothing()))
		configList.append(getConfigListEntry(_("ServiceExtEplayer3 (%s)" % str(serviceapp_client.ID_SERVICEEXTEPLAYER3)), ConfigNothing()))
		configList += self.extEplayer3Options(configServiceApp.exteplayer3["serviceexteplayer3"])
		self["config"].list = configList
		self["config"].l.setList(configList)

	def serviceMP3ReplacedChanged(self, configElement):
		self.buildConfigList()

	def serviceMP3PlayerChanged(self, configElement):
		self.buildConfigList()

	def deInitConfig(self):
		configServiceApp.servicemp3.player.removeNotifier(self.serviceMP3PlayerChanged)
		configServiceApp.servicemp3.replace.removeNotifier(self.serviceMP3ReplacedChanged)

	def keyOk(self):
		if configServiceApp.servicemp3.replace.isChanged():
			self.session.openWithCallback(self.saveSettingsAndClose, MessageBox, _("Enigma2 Playback System was changed and Enigma2 should be restarted\nDo you want to restart it now?"), type=MessageBox.TYPE_YESNO)
		else:
			self.saveSettingsAndClose()

	def saveSettingsAndClose(self, callback=False):
		initServiceAppSettings()
		if configServiceApp.servicemp3.replace.value:
			serviceapp_client.setServiceMP3Replace(True)
		else:
			serviceapp_client.setServiceMP3Replace(False)
		self.saveAll()
		self.close(callback)


class ServiceAppPlayer(MoviePlayer):
	def __init__(self, session, service):
		MoviePlayer.__init__(self, session, service)
		self.skinName = ["ServiceAppPlayer", "MoviePlayer"]
		self.servicelist = InfoBar.instance and InfoBar.instance.servicelist

	def handleLeave(self, how):
		if how == "ask":
			self.session.openWithCallback(self.leavePlayerConfirmed,
					MessageBox, _("Stop playing this movie?"))
		else:
			self.close()

	def leavePlayerConfirmed(self, answer):
		if answer:
			self.close()


class ServiceAppDetectPlayers(Screen):
	skin = """
	<screen position="center,center" size="500,340" title="ServiceApp - player check">
		<widget name="text" position="10,10" size="490,325" font="Regular;28" halign="center" valign="center" />
	</screen>
		"""
	def __init__(self, session):
		Screen.__init__(self, session)
		self["text"] = Label()
		self.playersIter = iter(
				[("gstplayer_gst-1.0", _("Detecting gstreamer player ..."), self.detectGstPlayer),
				("exteplayer3", _("Detecting exteplayer3 player ..."), self.detectExtEplayer3)])
		self.onLayoutFinish.append(self.detectNextPlayer)

	def detectNextPlayer(self):
		player = next(self.playersIter, None)
		if player is not None:
			self["text"].setText(player[1])
			self.console = Console()
			self.console.ePopen(player[0], boundFunction(self.detectPlayerCB, player[2]))
		else:
			self.close()

	def detectPlayerCB(self, datafnc, data, retval, extra_args):
		datafnc(data, retval, extra_args)
		self.detectNextPlayer()

	def _getFirstJsonDataFromString(self, data):
		jsondata = None
		for line in data.splitlines():
			try:
				jsondata = json.loads(line)
				break
			except ValueError as e:
				pass
		return jsondata


	def detectGstPlayer(self, data, retval, extra_args):
		global GSTPLAYER_VERSION
		GSTPLAYER_VERSION = None
		jsondata = self._getFirstJsonDataFromString(data)
		if jsondata is None:
			print "[ServiceApp] cannot detect exteplayer3 version(1)!"
			return
		try:
			GSTPLAYER_VERSION = jsondata["GSTPLAYER_EXTENDED"]["version"]
		except KeyError:
			print "[ServiceApp] cannot detect gstplayer version(2)!"
		else:
			print "[ServiceApp] found gstplayer - %d version" % GSTPLAYER_VERSION

	def detectExtEplayer3(self, data, retval, extra_args):
		global EXTEPLAYER3_VERSION
		EXTEPLAYER3_VERSION = None
		jsondata = self._getFirstJsonDataFromString(data)
		if jsondata is None:
			print "[ServiceApp] cannot detect exteplayer3 version(1)!"
			return
		try:
			EXTEPLAYER3_VERSION = jsondata["EPLAYER3_EXTENDED"]["version"]
		except KeyError:
			print "[ServiceApp] cannot detect exteplayer3 version(2)!"
		else:
			print "[ServiceApp] found exteplayer3 - %d version" % EXTEPLAYER3_VERSION


def main(session, **kwargs):
	def restartE2(restart=False):
		if restart:
			from Screens.Standby import TryQuitMainloop
			session.open(TryQuitMainloop, 3)

	def openServiceAppSettings(callback=None):
		session.openWithCallback(restartE2, ServiceAppSettings)

	session.openWithCallback(openServiceAppSettings, ServiceAppDetectPlayers)


def play_exteplayer3(session, service, **kwargs):
	ref = eServiceReference(5002, 0, service.getPath())
	session.open(ServiceAppPlayer, service=ref)


def play_gstplayer(session, service, **kwargs):
	ref = eServiceReference(5001, 0, service.getPath())
	session.open(ServiceAppPlayer, service=ref)


def Plugins(**kwargs):
	return [
		PluginDescriptor(name=_("ServiceApp"), description=_("setup player framework"),
				where=PluginDescriptor.WHERE_PLUGINMENU, needsRestart=False, fnc=main),
		PluginDescriptor(name=_("ServiceApp"), description=_("Play with ServiceExtEplayer3"),
				where=PluginDescriptor.WHERE_MOVIELIST, needsRestart=False, fnc=play_exteplayer3),
		PluginDescriptor(name=_("ServiceApp"), description=_("Play with ServiceGstPlayer"),
				where=PluginDescriptor.WHERE_MOVIELIST, needsRestart=False, fnc=play_gstplayer)
		]
