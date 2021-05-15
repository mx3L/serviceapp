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

from . import _
import serviceapp_client


SINKS_DEFAULT = ("", "")
SINKS_EXPERIMENTAL = ("dvbvideosinkexp", "dvbaudiosinkexp")

sink_choices = []
if (os.path.isfile(eEnv.resolve("$libdir/gstreamer-1.0/libgstdvbvideosink.so")) and
        os.path.isfile(eEnv.resolve("$libdir/gstreamer-1.0/libgstdvbaudiosink.so"))):
    sink_choices.append(("original", _("original")))
if (os.path.isfile(eEnv.resolve("$libdir/gstreamer-1.0/libgstdvbvideosinkexp.so")) and
        os.path.isfile(eEnv.resolve("$libdir/gstreamer-1.0/libgstdvbaudiosinkexp.so"))):
    sink_choices.append(("experimental", _("experimental")))

player_choices = [("gstplayer", _("gstplayer")), ("exteplayer3", _("exteplayer3"))]
GSTPLAYER_VERSION = None
EXTEPLAYER3_VERSION = None

config.plugins.serviceapp = ConfigSubsection()
config_serviceapp = config.plugins.serviceapp

config_serviceapp.servicemp3 = ConfigSubsection()
config_serviceapp.servicemp3.replace = ConfigBoolean(default=False, descriptions={0: _("original"), 1: _("serviceapp")})
config_serviceapp.servicemp3.replace.value = serviceapp_client.isServiceMP3Replaced()
config_serviceapp.servicemp3.player = ConfigSelection(default="gstplayer", choices=player_choices)

config_serviceapp.options = ConfigSubDict()
config_serviceapp.options["servicemp3"] = ConfigSubsection()
config_serviceapp.options["servicegstplayer"] = ConfigSubsection()
config_serviceapp.options["serviceexteplayer3"] = ConfigSubsection()
for key in config_serviceapp.options.keys():
    config_serviceapp.options[key].hls_explorer = ConfigBoolean(default=True, descriptions={False: _("false"), True: _("true")})
    config_serviceapp.options[key].autoselect_stream = ConfigBoolean(default=True, descriptions={False: _("false"), True: _("true")})
    config_serviceapp.options[key].connection_speed_kb = ConfigInteger(9999999, limits=(0, 9999999))
    config_serviceapp.options[key].autoturnon_subtitles = ConfigBoolean(default=True, descriptions={False: _("false"), True: _("true")})

config_serviceapp.gstplayer = ConfigSubDict()
config_serviceapp.gstplayer["servicemp3"] = ConfigSubsection()
config_serviceapp.gstplayer["servicegstplayer"] = ConfigSubsection()
for key in config_serviceapp.gstplayer.keys():
    config_serviceapp.gstplayer[key].sink = ConfigSelection(default="original", choices=sink_choices)
    config_serviceapp.gstplayer[key].buffer_size = ConfigInteger(8192, (1024, 1024 * 64))
    config_serviceapp.gstplayer[key].buffer_duration = ConfigInteger(0, (0, 100))
    config_serviceapp.gstplayer[key].subtitle_enabled = ConfigBoolean(default=True, descriptions={False: _("false"), True: _("true")})

config_serviceapp.exteplayer3 = ConfigSubDict()
config_serviceapp.exteplayer3["servicemp3"] = ConfigSubsection()
config_serviceapp.exteplayer3["serviceexteplayer3"] = ConfigSubsection()
for key in config_serviceapp.exteplayer3.keys():
    config_serviceapp.exteplayer3[key].aac_swdecoding = ConfigSelection(default="0", choices=[("0", _("off")), ("1", _("To AAC ADTS")), ("2", _("To AAC LATM"))])
    config_serviceapp.exteplayer3[key].eac3_swdecoding = ConfigBoolean(default=False, descriptions={False: _("false"), True: _("true")})
    config_serviceapp.exteplayer3[key].ac3_swdecoding = ConfigBoolean(default=False, descriptions={False: _("false"), True: _("true")})
    config_serviceapp.exteplayer3[key].dts_swdecoding = ConfigBoolean(default=False, descriptions={False: _("false"), True: _("true")})
    config_serviceapp.exteplayer3[key].mp3_swdecoding = ConfigBoolean(default=False, descriptions={False: _("false"), True: _("true")})
    config_serviceapp.exteplayer3[key].wma_swdecoding = ConfigBoolean(default=False, descriptions={False: _("false"), True: _("true")})
    config_serviceapp.exteplayer3[key].lpcm_injecion = ConfigBoolean(default=False, descriptions={False: _("false"), True: _("true")})
    config_serviceapp.exteplayer3[key].downmix = ConfigBoolean(default=False, descriptions={False: _("false"), True: _("true")})
    config_serviceapp.exteplayer3[key].rtmp_protocol = ConfigSelection(default="auto", choices=["auto", "ffmpeg", "librtmp"])


def key_to_setting_id(key):
    setting_id = None
    if key == "servicemp3":
        setting_id = serviceapp_client.OPTIONS_SERVICEMP3
    elif key == "serviceexteplayer3":
        setting_id = serviceapp_client.OPTIONS_SERVICEEXTEPLAYER3
    elif key == "servicegstplayer":
        setting_id = serviceapp_client.OPTIONS_SERVICEGSTPLAYER
    return setting_id


def init_serviceapp_settings():
    for key in config_serviceapp.options.keys():
        setting_id = key_to_setting_id(key)
        serviceapp_cfg = config_serviceapp.options[key]

        serviceapp_client.setServiceAppSettings(setting_id,
                serviceapp_cfg.hls_explorer.value,
                serviceapp_cfg.autoselect_stream.value,
                serviceapp_cfg.connection_speed_kb.value,
                serviceapp_cfg.autoturnon_subtitles.value)

    for key in config_serviceapp.gstplayer.keys():
        setting_id = key_to_setting_id(key)
        player_cfg = config_serviceapp.gstplayer[key]

        if player_cfg.sink.value == "original":
            video_sink, audio_sink = SINKS_DEFAULT
        elif player_cfg.sink.value == "experimental":
            video_sink, audio_sink = SINKS_EXPERIMENTAL
        else:
            continue

        serviceapp_client.setGstreamerPlayerSettings(setting_id,
                video_sink,
                audio_sink,
                player_cfg.subtitle_enabled.value,
                player_cfg.buffer_size.value,
                player_cfg.buffer_duration.value)

    for key in config_serviceapp.exteplayer3.keys():
        setting_id = key_to_setting_id(key)
        player_cfg = config_serviceapp.exteplayer3[key]

        rtmp_proto_val = 0
        rtmp_proto_cfg_val = player_cfg.rtmp_protocol.value
        if rtmp_proto_cfg_val == "librtmp":
            rtmp_proto_val = 2
        elif rtmp_proto_cfg_val == "ffmpeg":
            rtmp_proto_val = 1
        else:
            rtmp_proto_val = 0
        serviceapp_client.setExtEplayer3Settings(setting_id,
                int(player_cfg.aac_swdecoding.value),
                player_cfg.dts_swdecoding.value,
                player_cfg.wma_swdecoding.value,
                player_cfg.lpcm_injecion.value,
                player_cfg.downmix.value,
                player_cfg.ac3_swdecoding.value,
                player_cfg.eac3_swdecoding.value,
                player_cfg.mp3_swdecoding.value,
                rtmp_proto_val)

    if config_serviceapp.servicemp3.player.value == "gstplayer":
        serviceapp_client.setServiceMP3GstPlayer()
    elif config_serviceapp.servicemp3.player.value == "exteplayer3":
        serviceapp_client.setServiceMP3ExtEplayer3()


init_serviceapp_settings()


class ServiceAppSettings(ConfigListScreen, Screen):
    def __init__(self, session):
        Screen.__init__(self, session)
        self.skinName = ["ServiceAppSettings", "Setup"]
        ConfigListScreen.__init__(self, [], session)
        self.setup_title = _("ServiceApp")
        self.onLayoutFinish.append(self.init_configlist)
        self.onClose.append(self.deinit_config)
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

    def init_configlist(self):
        config_serviceapp.servicemp3.player.addNotifier(
                lambda x: self.build_configlist(), initial_call=False)
        config_serviceapp.servicemp3.replace.addNotifier(
                lambda x: self.build_configlist(), initial_call=False)
        self.build_configlist()

    def deinit_config(self):
        del config_serviceapp.servicemp3.player.notifiers[:]
        del config_serviceapp.servicemp3.replace.notifiers[:]

    def gstplayer_options(self, gstplayer_options_cfg):
        config_list = []
        config_list.append(getConfigListEntry("  " + _("Sink"),
            gstplayer_options_cfg.sink, _("Select sink which you want to use.")))
        config_list.append(getConfigListEntry("  " + _("Embedded subtitles"),
            gstplayer_options_cfg.subtitle_enabled, _("Turn on the embedded subtitles support.")))
        config_list.append(getConfigListEntry("  " + _("Buffer size"),
            gstplayer_options_cfg.buffer_size, _("Set buffer size in kilobytes.")))
        config_list.append(getConfigListEntry("  " + _("Buffer duration"),
            gstplayer_options_cfg.buffer_duration, _("Set buffer duration in seconds.")))
        return config_list

    def exteplayer3_options(self, exteplayer3_options_cfg):
        config_list = []
        config_list.append(getConfigListEntry("  " + _("AAC software decoding"),
            exteplayer3_options_cfg.aac_swdecoding, _("Turn on AAC software decoding.")))
        config_list.append(getConfigListEntry("  " + _("EAC3 software decoding"),
            exteplayer3_options_cfg.eac3_swdecoding, _("Turn on EAC3 software decoding.")))
        config_list.append(getConfigListEntry("  " + _("AC3 software decoding"),
            exteplayer3_options_cfg.ac3_swdecoding, _("Turn on AC3 software decoding.")))
        config_list.append(getConfigListEntry("  " + _("DTS software decoding"),
            exteplayer3_options_cfg.dts_swdecoding, _("Turn on DTS software decoding.")))
        config_list.append(getConfigListEntry("  " + _("MP3 software decoding"),
            exteplayer3_options_cfg.dts_swdecoding, _("Turn on MP3 software decoding.")))
        config_list.append(getConfigListEntry("  " + _("WMA software decoding"),
            exteplayer3_options_cfg.wma_swdecoding, _("Turn on WMA1, WMA2, WMA/PRO software decoding.")))
        config_list.append(getConfigListEntry("  " + _("Stereo downmix"),
            exteplayer3_options_cfg.downmix, _("Turn on downmix to stereo, when software decoding is in use")))
        config_list.append(getConfigListEntry("  " + _("LPCM injection"),
            exteplayer3_options_cfg.lpcm_injecion, _("Software decoder use LPCM for injection (otherwise wav PCM will be used)")))
        config_list.append(getConfigListEntry("  " + _("RTMP protocol implementation"),
            exteplayer3_options_cfg.rtmp_protocol, _("Set which RTMP protocol implementation will be used for playback of RTMP streams")))
        return config_list

    def serviceapp_options(self, serviceapp_options_cfg):
        config_list = []
        config_list.append(getConfigListEntry("  " + _("Auto turn on subtitles"),
            serviceapp_options_cfg.autoturnon_subtitles, _("Automatically turn on subtitles if available.")))
        config_list.append(getConfigListEntry("  " + _("HLS Explorer"),
            serviceapp_options_cfg.hls_explorer, _("Turn on explorer to retrieve different quality streams from HLS variant playlist and select them via subservices.")))
        config_list.append(getConfigListEntry("  " + _("Auto select stream"),
            serviceapp_options_cfg.autoselect_stream, _("Turn on auto-selection of streams according to set Connection speed.")))
        config_list.append(getConfigListEntry("  " + _("Connection speed"),
            serviceapp_options_cfg.connection_speed_kb, _("Set connection speed in kb/s, according to which you want to have streams auto-selected")))
        return config_list

    def player_options(self, player_type, service_type):
        config_list = []
        player_cfg = getattr(config_serviceapp, player_type)[service_type]
        serviceapp_cfg = config_serviceapp.options[service_type]
        if player_type == "exteplayer3":
            config_list.append(getConfigListEntry("  " + _("ExtEplayer3"),
                ConfigSelection([EXTEPLAYER3_VERSION or "not installed"], EXTEPLAYER3_VERSION or _("not installed"))))
            if EXTEPLAYER3_VERSION:
                config_list += self.exteplayer3_options(player_cfg)
                config_list += self.serviceapp_options(serviceapp_cfg)
        if player_type == "gstplayer":
            config_list.append(getConfigListEntry("  " + _("GstPlayer"),
                ConfigSelection([GSTPLAYER_VERSION or "not installed"], GSTPLAYER_VERSION or _("not installed"))))
            if GSTPLAYER_VERSION:
                config_list += self.gstplayer_options(player_cfg)
                config_list += self.serviceapp_options(serviceapp_cfg)
        return config_list

    def build_configlist(self):
        config_list = [getConfigListEntry(_("Enigma2 playback system"),
            config_serviceapp.servicemp3.replace, _("Select the player which will be used for Enigma2 playback."))]
        if config_serviceapp.servicemp3.replace.value:
            config_list.append(getConfigListEntry(_("Player"),
                config_serviceapp.servicemp3.player, _("Select the player which will be used in serviceapp for Enigma2 playback.")))
            configlist_servicemp3 = [getConfigListEntry("", ConfigNothing())]
            configlist_servicemp3.append(getConfigListEntry(_("ServiceMp3 (%s)" % str(serviceapp_client.ID_SERVICEMP3)), ConfigNothing()))
            if config_serviceapp.servicemp3.player.value == "gstplayer":
                config_list += configlist_servicemp3 + self.player_options("gstplayer", "servicemp3")
            elif config_serviceapp.servicemp3.player.value == "exteplayer3":
                config_list += configlist_servicemp3 + self.player_options("exteplayer3", "servicemp3")
            else:
                config_list += configlist_servicemp3
        config_list.append(getConfigListEntry("", ConfigNothing()))
        config_list.append(getConfigListEntry(_("ServiceGstPlayer (%s)" % str(serviceapp_client.ID_SERVICEGSTPLAYER)), ConfigNothing()))
        config_list += self.player_options("gstplayer", "servicegstplayer")
        config_list.append(getConfigListEntry("", ConfigNothing()))
        config_list.append(getConfigListEntry(_("ServiceExtEplayer3 (%s)" % str(serviceapp_client.ID_SERVICEEXTEPLAYER3)), ConfigNothing()))
        config_list += self.player_options("exteplayer3", "serviceexteplayer3")
        self["config"].list = config_list
        self["config"].l.setList(config_list)

    def keyOk(self):
        if config_serviceapp.servicemp3.replace.isChanged():
            self.session.openWithCallback(self.save_settings_and_close,
                    MessageBox, _("Enigma2 playback system was changed and Enigma2 should be restarted\n\nDo you want to restart it now?"),
                    type=MessageBox.TYPE_YESNO)
        else:
            self.save_settings_and_close()

    def save_settings_and_close(self, callback=False):
        init_serviceapp_settings()
        if config_serviceapp.servicemp3.replace.value:
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
        self.players_iter = iter(
                [("gstplayer_gst-1.0",
                    _("Detecting gstreamer player ..."),
                    self.detect_gstplayer),
                 ("exteplayer3",
                     _("Detecting exteplayer3 player ..."),
                     self.detect_exteplayer3)
                 ])
        self.onLayoutFinish.append(self.detect_next_player)

    def detect_next_player(self):
        player = next(self.players_iter, None)
        if player is not None:
            self["text"].setText(player[1])
            self.console = Console()
            self.console.ePopen(player[0], boundFunction(self.detect_player_cb, player[2]))
        else:
            self.close()

    def detect_player_cb(self, datafnc, data, retval, extra_args):
        datafnc(data, retval, extra_args)
        self.detect_next_player()

    def _get_first_json_data_from_string(self, data):
        jsondata = None
        for line in data.splitlines():
            try:
                jsondata = json.loads(line)
                break
            except ValueError as e:
                pass
        return jsondata

    def detect_gstplayer(self, data, retval, extra_args):
        global GSTPLAYER_VERSION
        GSTPLAYER_VERSION = None
        jsondata = self._get_first_json_data_from_string(data)
        if jsondata is None:
            print "[ServiceApp] cannot detect gstplayer version(1)!"
            return
        try:
            GSTPLAYER_VERSION = jsondata["GSTPLAYER_EXTENDED"]["version"]
        except KeyError:
            print "[ServiceApp] cannot detect gstplayer version(2)!"
        else:
            print "[ServiceApp] found gstplayer - %d version" % GSTPLAYER_VERSION

    def detect_exteplayer3(self, data, retval, extra_args):
        global EXTEPLAYER3_VERSION
        EXTEPLAYER3_VERSION = None
        jsondata = self._get_first_json_data_from_string(data)
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

    def restart_enigma2(restart=False):
        if restart:
            from Screens.Standby import TryQuitMainloop
            session.open(TryQuitMainloop, 3)

    def open_serviceapp_settings(callback=None):
        session.openWithCallback(restart_enigma2, ServiceAppSettings)

    session.openWithCallback(open_serviceapp_settings, ServiceAppDetectPlayers)


def menu(menuid, **kwargs):
    if menuid == "system":
        return [(_("ServiceApp"), main, "serviceapp_setup", None)]
    return []


def play_exteplayer3(session, service, **kwargs):
    ref = eServiceReference(5002, 0, service.getPath())
    session.open(ServiceAppPlayer, service=ref)


def play_gstplayer(session, service, **kwargs):
    ref = eServiceReference(5001, 0, service.getPath())
    session.open(ServiceAppPlayer, service=ref)


def Plugins(**kwargs):
    return [
            PluginDescriptor(name=_("ServiceApp"), description=_("setup player framework"),
                where=PluginDescriptor.WHERE_MENU, needsRestart=False, fnc=menu),
            PluginDescriptor(name=_("ServiceApp"), description=_("Play with ServiceExtEplayer3"),
                where=PluginDescriptor.WHERE_MOVIELIST, needsRestart=False, fnc=play_exteplayer3),
            PluginDescriptor(name=_("ServiceApp"), description=_("Play with ServiceGstPlayer"),
                where=PluginDescriptor.WHERE_MOVIELIST, needsRestart=False, fnc=play_gstplayer)
            ]
