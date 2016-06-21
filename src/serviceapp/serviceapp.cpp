#include "Python.h"

#include <lib/service/service.h>
#include <lib/base/init_num.h>
#include <lib/base/init.h>
#include <lib/base/eenv.h>
#include <lib/base/nconfig.h>
#ifdef HAVE_EPG
#include <lib/dvb/epgcache.h>
#endif
#include <lib/gui/esubtitle.h>

#include "serviceapp.h"
#include "gstplayer.h"
#include "exteplayer3.h"

enum
{
	EXTEPLAYER3,
	GSTPLAYER,
};

enum
{
	OPTIONS_SERVICEMP3_GSTPLAYER,
	OPTIONS_SERVICEMP3_EXTEPLAYER3,
	OPTIONS_SERVICEGSTPLAYER,
	OPTIONS_SERVICEEXTEPLAYER3,
	OPTIONS_USER,
};

static int g_playerServiceMP3 = GSTPLAYER;
static bool g_useUserSettings = false;

static GstPlayerOptions g_GstPlayerOptionsServiceMP3;
static GstPlayerOptions g_GstPlayerOptionsServiceGst;
static GstPlayerOptions g_GstPlayerOptionsUser;

static ExtEplayer3Options g_ExtEplayer3OptionsServiceMP3;
static ExtEplayer3Options g_ExtEplayer3OptionsServiceExt3;
static ExtEplayer3Options g_ExtEplayer3OptionsUser;

static const std::string gReplaceServiceMP3Path = eEnv::resolve("$sysconfdir/enigma2/serviceapp_replaceservicemp3");
static const bool gReplaceServiceMP3 = ( access( gReplaceServiceMP3Path.c_str(), F_OK ) != -1 );

static BasePlayer *createPlayer(const eServiceReference& ref)
{
	if( ref.type == eServiceFactoryApp::idServiceExtEplayer3)
	{
		ExtEplayer3Options& options = g_ExtEplayer3OptionsServiceExt3;
		if (g_useUserSettings)
		{
			options = g_ExtEplayer3OptionsUser;
			g_useUserSettings = false;
		}
		return new ExtEplayer3(options);
	}
	else if ( ref.type == eServiceFactoryApp::idServiceGstPlayer)
	{
		GstPlayerOptions& options = g_GstPlayerOptionsServiceGst;
		if (g_useUserSettings)
		{
			options = g_GstPlayerOptionsUser;
			g_useUserSettings = false;
		}
		return new GstPlayer(options);
	}
	else
	{
		switch(g_playerServiceMP3)
		{
			case EXTEPLAYER3:
			{
				ExtEplayer3Options& options = g_ExtEplayer3OptionsServiceMP3;
				if (g_useUserSettings)
				{
					options = g_ExtEplayer3OptionsUser;
					g_useUserSettings = false;
				}
				return new ExtEplayer3(options);
			}
			case GSTPLAYER:
			default:
			{
				GstPlayerOptions& options = g_GstPlayerOptionsServiceMP3;
				if (g_useUserSettings)
				{
					options = g_GstPlayerOptionsUser;
					g_useUserSettings = false;
				}
				return new GstPlayer(options);
			}
		}
	}
}

DEFINE_REF(eServiceApp);

eServiceApp::eServiceApp(eServiceReference ref):
	m_ref(ref),
	player(0),
	extplayer(0),
	m_paused(false),
	m_framerate(-1),
	m_width(-1),
	m_height(-1),
	m_progressive(-1)
{
	eDebug("eServiceApp");
	extplayer = createPlayer(ref);
	player = new PlayerBackend(extplayer);
	
	m_subtitle_widget = 0;
	m_subtitle_sync_timer = eTimer::create(eApp);
	CONNECT(m_subtitle_sync_timer->timeout, eServiceApp::pushSubtitles);

#ifdef HAVE_EPG
	m_nownext_timer = eTimer::create(eApp);
	CONNECT(m_nownext_timer->timeout, eServiceApp::updateEpgCacheNowNext);
#endif

	CONNECT(player->gotPlayerMessage, eServiceApp::gotExtPlayerMessage);
};

eServiceApp::~eServiceApp()
{
	delete player;
	delete extplayer;

	if (m_subtitle_widget) m_subtitle_widget->destroy();
	m_subtitle_widget = 0;
#ifdef HAVE_EPG
	m_nownext_timer->stop();
#endif
	eDebug("~eServiceApp");
};

#ifdef HAVE_EPG
void eServiceApp::updateEpgCacheNowNext()
{
	bool update = false;
	ePtr<eServiceEvent> next = 0;
	ePtr<eServiceEvent> ptr = 0;
	eServiceReference ref(m_ref);
	ref.type = eServiceFactoryApp::idServiceMP3;
	ref.path.clear();
	if (eEPGCache::getInstance() && eEPGCache::getInstance()->lookupEventTime(ref, -1, ptr) >= 0)
	{
		ePtr<eServiceEvent> current = m_event_now;
		if (!current || !ptr || current->getEventId() != ptr->getEventId())
		{
			update = true;
			m_event_now = ptr;
			time_t next_time = ptr->getBeginTime() + ptr->getDuration();
			if (eEPGCache::getInstance()->lookupEventTime(ref, next_time, ptr) >= 0)
			{
				next = ptr;
				m_event_next = ptr;
			}
		}
	}

	int refreshtime = 60;
	if (!next)
	{
		next = m_event_next;
	}
	if (next)
	{
		time_t now = eDVBLocalTimeHandler::getInstance()->nowTime();
		refreshtime = (int)(next->getBeginTime() - now) + 3;
		if (refreshtime <= 0 || refreshtime > 60)
		{
			refreshtime = 60;
		}
	}
	m_nownext_timer->startLongTimer(refreshtime);
	if (update)
	{
		m_event((iPlayableService*)this, evUpdatedEventInfo);
	}
}
#endif

void eServiceApp::pullSubtitles()
{
	std::queue<subtitleMessage> pulled;
	float convert_fps = 1.0;
	int delay = eConfigManager::getConfigIntValue("config.subtitles.pango_subtitles_delay");
	int subtitle_fps = eConfigManager::getConfigIntValue("config.subtitles.pango_subtitles_fps");
	if (subtitle_fps > 1 && m_framerate > 0)
	{
		convert_fps = subtitle_fps / (double)m_framerate;
	}
	player->getSubtitles(pulled);
	eDebug("eServiceApp::pullSubtitles - pulling %d subtitles", pulled.size());
	while (!pulled.empty())
	{
		subtitleMessage sub = pulled.front();
		uint32_t start_ms = sub.start * convert_fps + (delay / 90);
		uint32_t end_ms = start_ms + sub.duration;
		std::string line(sub.text);
		m_subtitle_pages.insert(subtitle_pages_map_pair_t(end_ms, subtitle_page_t(start_ms, end_ms, line)));
		pulled.pop();
	}
	m_subtitle_sync_timer->start(1, true);
}

void eServiceApp::pushSubtitles()
{
	pts_t running_pts = 0;
	int32_t next_timer = 0, decoder_ms, start_ms, end_ms, diff_start_ms, diff_end_ms;
	subtitle_pages_map_t::iterator current;

	if (getPlayPosition(running_pts) < 0)
	{
		next_timer = 50;
		goto exit;
	}
	decoder_ms = running_pts / 90;

	for (current = m_subtitle_pages.lower_bound(decoder_ms); current != m_subtitle_pages.end(); current++)
	{
		start_ms = current->second.start_ms;
		end_ms = current->second.end_ms;
		diff_start_ms = start_ms - decoder_ms;
		diff_end_ms = end_ms - decoder_ms;
		
		//eDebug("eServiceApp::pushSubtitles - next subtitle: decoder: %d, start: %d, end: %d, duration_ms: %d, diff_start: %d, diff_end: %d : %s",
		//	decoder_ms, start_ms, end_ms, end_ms - start_ms, diff_start_ms, diff_end_ms, current->second.text.c_str());

		if (diff_end_ms < 0)
		{
			//eDebug("eServiceApp::pushSubtitles - current sub has already ended, skip: %d", diff_end_ms);
			continue;
		}
		if (diff_start_ms > 50)
		{
			//eDebug("eServiceApp::pushSubtitles - current sub in the future, start timer, %d", diff_start_ms);
			next_timer = diff_start_ms;
			goto exit;
		}
		if (m_subtitle_widget && !m_paused)
		{
			//eDebug("eServiceApp::pushSubtitles - current sub actual, show!");

			ePangoSubtitlePage pango_page;
			gRGB rgbcol(0xD0,0xD0,0xD0);

			pango_page.m_elements.push_back(ePangoSubtitlePageElement(rgbcol, current->second.text.c_str()));
			pango_page.m_show_pts = start_ms * 90; // actually completely unused by widget!
			pango_page.m_timeout = end_ms - decoder_ms; // take late start into account

			m_subtitle_widget->setPage(pango_page);
		}
		//eDebug("eServiceApp::pushSubtitles - no next sub scheduled, check NEXT subtitle");
	}
exit:
	if (next_timer == 0)
	{
		//eDebug("eServiceApp::pushSubtitles - next timer = 0, set default timer!");
		next_timer = 1000;
	}
	m_subtitle_sync_timer->start(next_timer, true);
}

void eServiceApp::gotExtPlayerMessage(int message)
{
	switch (message)
	{
		case PlayerMessage::start:
			eDebug("eServiceApp::gotExtPlayerMessage - start");
			m_event(this, evStart);
#ifdef HAVE_EPG
			updateEpgCacheNowNext();
#endif
			break;
		case PlayerMessage::stop:
			eDebug("eServiceApp::gotExtPlayerMessage - stop");
			// evEOF signals that end of file was reached and we
			// could make operations like seek back or play again, 
			// however when player signals stop, process
			// has already ended, so there is no possibility to do so.
			// This should be fixed on player's side so it doesn't end
			// immediately but waits at the end for our input..
			m_event(this, evEOF);
			break;
		case PlayerMessage::pause:
			eDebug("eServiceApp::gotExtPlayerMessage - pause");
			m_paused = true;
			break;
		case PlayerMessage::resume:
			eDebug("eServiceApp::gotExtPlayerMessage - resume");
			m_paused = false;
			break;
		case PlayerMessage::error:
			eDebug("eServiceApp::gotExtPlayerMessage - error");
			m_event(this, evUser + 12);
			break;
		case PlayerMessage::videoSizeChanged:
		{
			eDebug("eServiceApp::gotExtPlayerMessage - videoSizeChanged");
			videoStream v;
			if (!player->videoGetTrackInfo(v,0))
			{
				m_width = v.width;
				m_height = v.height;
			}
			m_event(this, evVideoSizeChanged);
			break;
		}
		case PlayerMessage::videoFramerateChanged:
		{
			eDebug("eServiceApp::gotExtPlayerMessage - videoFramerateChanged");
			videoStream v;
			if (!player->videoGetTrackInfo(v,0))
			{
				m_framerate = v.framerate;
			}
			m_event(this, evVideoFramerateChanged);
			break;
		}
		case PlayerMessage::videoProgressiveChanged:
		{
			eDebug("eServiceApp::gotExtPlayerMessage - videoProgressiveChanged");
			videoStream v;
			if (!player->videoGetTrackInfo(v,0))
			{
				m_progressive = v.progressive;
			}
			m_event(this, evVideoProgressiveChanged);
			break;
		}
		case PlayerMessage::subtitleAvailable:
			eDebug("eServiceApp::gotExtPlayerMessage - subtitleAvailable");
			pullSubtitles();
			break;
		default:
			eDebug("eServiceApp::gotExtPlayerMessage - unhandled message");
			break;
	}
}


// __iPlayableService
RESULT eServiceApp::connectEvent(const SigC::Slot2< void, iPlayableService*, int >& event, ePtr< eConnection >& connection)
{
	connection = new eConnection((iPlayableService*)this, m_event.connect(event));
	return 0;
}

RESULT eServiceApp::start()
{
	std::string path_str(m_ref.path);
	std::map<std::string, std::string> headers;
	size_t pos = m_ref.path.find('#');
	if (pos != std::string::npos && (m_ref.path.compare(0, 4, "http") == 0 || m_ref.path.compare(0, 4, "rtsp") == 0))
	{
		path_str = m_ref.path.substr(0, pos);
		std::string headers_str = m_ref.path.substr(pos + 1);
		pos = 0;
		while (pos != std::string::npos)
		{
			std::string name, value;
			size_t start = pos;
			size_t len = std::string::npos;
			pos = headers_str.find('=', pos);
			if (pos != std::string::npos)
			{
				len = pos - start;
				pos++;
				name = headers_str.substr(start, len);
				start = pos;
				len = std::string::npos;
				pos = headers_str.find('&', pos);
				if (pos != std::string::npos)
				{
					len = pos - start;
					pos++;
				}
				value = headers_str.substr(start, len);
			}
			if (!name.empty() && !value.empty())
			{
				headers[name] = value;
			}
		}
	}
	player->start(path_str, headers);
	return 0;
}

RESULT eServiceApp::stop()
{
	eDebug("eServiceApp::stop");
	player->stop();
	return 0;
}

RESULT eServiceApp::setTarget(int target)
{
	eDebug("eServiceApp::setTarget %d", target);
	return -1;
}


// __iPausableService
RESULT eServiceApp::pause()
{
	eDebug("eServiceApp::pause");
	player->pause();
	return 0;
}

RESULT eServiceApp::unpause()
{
	eDebug("eServiceApp::unpause");
	player->resume();
	return 0;
}

RESULT eServiceApp::setSlowMotion(int ratio)
{
	eDebug("eServiceApp::setSlowMotion - ratio = %d", ratio);
	return -1;
}

RESULT eServiceApp::setFastForward(int ratio)
{
	eDebug("eServiceApp::setFastForward - ratio = %d", ratio);
	return -1;
}


// __iSeekableService
RESULT eServiceApp::getLength(pts_t& pts)
{
	//eDebug("eServiceApp::getLength");
	int length;
	if (player->getLength(length) < 0)
	{
		return -1;
	}
	pts = length * 90;
	return 0;
}

RESULT eServiceApp::seekTo(pts_t to)
{
	eDebug("eServiceApp::seekTo - position = %lld", to);
	player->seekTo(int(to/90000));
	return 0;
}

RESULT eServiceApp::seekRelative(int direction, pts_t to)
{
	eDebug("eServiceApp::seekRelative - position = %lld", direction*to);
	int length = 0;
	int position, seekto;
	if (player->getPlayPosition(position) < 0)
	{
		return -1;
	}
	player->getLength(length);
	seekto = position + (to / 90 * direction);
	if (length > 0 && seekto > length)
	{
		stop();
		return 0;
	}
	else if (seekto < 0)
	{
		seekto = 0;
	}
	player->seekTo(int(seekto / 1000));
	return 0;
}

RESULT eServiceApp::getPlayPosition(pts_t& pts)
{
	//eDebug("eServiceApp::getPlayPosition");
	int position;
	if (player->getPlayPosition(position) < 0)
	{
		return -1;
	}
	pts = position * 90;
	return 0;
}

RESULT eServiceApp::setTrickmode(int trick)
{
	eDebug("eServiceApp::setTrickmode = %d", trick);
	return -1;
}

RESULT eServiceApp::isCurrentlySeekable()
{
	eDebug("eServiceApp::isCurrentlySeekable");
	return -1;
}


// __iAudioTrackSelection
int eServiceApp::getNumberOfTracks()
{
	eDebug("eServiceApp::getNumberOfTracks");
	return player->audioGetNumberOfTracks(500);
}

RESULT eServiceApp::selectTrack(unsigned int i)
{
	eDebug("eServiceApp::selectTrack = %d", i);
	if (player->audioSelectTrack(i) < 0)
	{
		return -1;
	}
	return 0;
}

RESULT eServiceApp::getTrackInfo(iAudioTrackInfo& trackInfo, unsigned int n)
{
	eDebug("eServiceApp::getTrackInfo = %d", n);
	audioStream track;
	if (player->audioGetTrackInfo(track, n) < 0)
	{
		return -1;
	}
	trackInfo.m_description = track.description;
	trackInfo.m_language = track.language_code;
	return 0;
}

int eServiceApp::getCurrentTrack()
{
	eDebug("eServiceApp::getCurrentTrack");
	return player->audioGetCurrentTrackNum();
}


// __iAudioChannelSelection
int eServiceApp::getCurrentChannel()
{
	eDebug("eServiceApp::getCurrentChannel");
	return STEREO;
}

RESULT eServiceApp::selectChannel(int i)
{
	eDebug("eServiceApp::selectChannel %d", i);
	return -1;
}


// __iSubtitleOutput
RESULT eServiceApp::enableSubtitles(iSubtitleUser *user, struct SubtitleTrack &track)
{
	eDebug("eServiceApp::enableSubtitles - track = %d", track.pid);
	m_subtitle_sync_timer->stop();
	m_subtitle_pages.clear();
	player->subtitleSelectTrack(track.pid);
	m_subtitle_widget = user;
	return 0;
}

RESULT eServiceApp::disableSubtitles()
{
	eDebug("eServiceApp::disableSubtitles");
	m_subtitle_sync_timer->stop();
	m_subtitle_pages.clear();
	if (m_subtitle_widget) m_subtitle_widget->destroy();
	m_subtitle_widget = 0;
	return 0;
}

RESULT eServiceApp::getCachedSubtitle(struct SubtitleTrack &track)
{
	eDebug("eServiceApp::getCachedSubtitle");
	return -1;
}

RESULT eServiceApp::getSubtitleList(std::vector<struct SubtitleTrack> &subtitlelist)
{
	int trackNum = player->subtitleGetNumberOfTracks(500);
	eDebug("eServiceApp::getSubtitleList - found %d tracks", trackNum);
	for (int i = 0; i < trackNum; i++)
	{
		subtitleStream s;
		if (player->subtitleGetTrackInfo(s, i) < 0)
			continue;
		struct SubtitleTrack track;
		track.type = 2;
		track.pid = i;
		// assume SRT, it really doesn't matter
		track.page_number = 4;
		track.magazine_number = 0;
		track.language_code = s.language_code;
		subtitlelist.push_back(track);
	}
	return 0;
}


// __iServiceInformation
RESULT eServiceApp::getName(std::string& name)
{
	std::string title = m_ref.getName();
	if (title.empty())
	{
		name = m_ref.path;
		size_t n = name.rfind('/');
		if (n != std::string::npos)
			name = name.substr(n + 1);
	}
	else
		name = title;
	return 0;
}

#ifdef HAVE_EPG
RESULT eServiceApp::getEvent(ePtr<eServiceEvent> &evt, int nownext)
{
	evt = nownext ? m_event_next : m_event_now;
	if (!evt)
		return -1;
	return 0;
}
#endif

int eServiceApp::getInfo(int w)
{
	switch (w)
	{
	case sServiceref: return m_ref;
	case sVideoHeight: return m_height;
	case sVideoWidth: return m_width;
	case sFrameRate: return m_framerate;
	case sProgressive: return m_progressive;
	case sAspect: 
	{
		if (m_height <= 0 || m_width <= 0)
		{
			return -1;
		}
		float aspect = m_width/float(m_height);
		// according to wikipedia, widescreen is when width to height is greater then 1.37:1
		if (aspect > 1.37)
		{
			// WIDESCREEN values from ServiceInfo.py: 3, 4, 7, 8, 0xB, 0xC, 0xF, 0x10
			return 3;
		}
		else
		{
			// 4:3 values from ServiceInfo.py: 1, 2, 5, 6, 9, 0xA, 0xD, 0xE
			return 1;
		}
		return 2;
	}
	case sTagTitle:
	case sTagArtist:
	case sTagAlbum:
	case sTagTitleSortname:
	case sTagArtistSortname:
	case sTagAlbumSortname:
	case sTagDate:
	case sTagComposer:
	case sTagGenre:
	case sTagComment:
	case sTagExtendedComment:
	case sTagLocation:
	case sTagHomepage:
	case sTagDescription:
	case sTagVersion:
	case sTagISRC:
	case sTagOrganization:
	case sTagCopyright:
	case sTagCopyrightURI:
	case sTagContact:
	case sTagLicense:
	case sTagLicenseURI:
	case sTagCodec:
	case sTagAudioCodec:
	case sTagVideoCodec:
	case sTagEncoder:
	case sTagLanguageCode:
	case sTagKeywords:
	case sTagChannelMode:
	case sUser+12:
		return resIsString;
	case sTagTrackGain:
	case sTagTrackPeak:
	case sTagAlbumGain:
	case sTagAlbumPeak:
	case sTagReferenceLevel:
	case sTagBeatsPerMinute:
	case sTagImage:
	case sTagPreviewImage:
	case sTagAttachment:
		return resIsPyObject;
	case sTagTrackNumber:
	case sTagTrackCount:
	case sTagAlbumVolumeNumber:
	case sTagAlbumVolumeCount:
	case sTagBitrate:
	case sTagNominalBitrate:
	case sTagMinimumBitrate:
	case sTagMaximumBitrate:
	case sTagSerial:
	case sTagEncoderVersion:
	case sTagCRC:
	case sBuffer:
	default:
		return resNA;
	}
	return 0;
}

std::string eServiceApp::getInfoString(int w)
{
	if ( strstr(m_ref.path.c_str(), "://") )
	{
		switch (w)
		{
		case sProvider:
			return "IPTV";
		case sServiceref:
		{
			eServiceReference ref(m_ref);
			ref.type = m_ref.type;
			ref.path.clear();
			return ref.toString();
		}
		default:
			break;
		}
	}
	if (w < sUser && w > 26 )
		return "";
	switch(w)
	{
	case sUser+12:
	{
		errorMessage e;
		if (!player->getErrorMessage(e))
			return e.message;
		return "";
	}
	default:
		return "";
	}
	return "";
}




DEFINE_REF(eStaticServiceAppInfo);

RESULT eStaticServiceAppInfo::getName(const eServiceReference &ref, std::string &name)
{
	if ( ref.name.length() )
		name = ref.name;
	else
	{
		size_t last = ref.path.rfind('/');
		if (last != std::string::npos)
			name = ref.path.substr(last+1);
		else
			name = ref.path;
	}
	return 0;
}

int eStaticServiceAppInfo::getLength(const eServiceReference &ref)
{
	return -1;
}

int eStaticServiceAppInfo::getInfo(const eServiceReference &ref, int w)
{
	switch (w)
	{
	case iServiceInformation::sTimeCreate:
		{
			struct stat s;
			if (stat(ref.path.c_str(), &s) == 0)
			{
				return s.st_mtime;
			}
		}
		break;
	case iServiceInformation::sFileSize:
		{
			struct stat s;
			if (stat(ref.path.c_str(), &s) == 0)
			{
				return s.st_size;
			}
		}
		break;
	}
	return iServiceInformation::resNA;
}

long long eStaticServiceAppInfo::getFileSize(const eServiceReference &ref)
{
	struct stat s;
	if (stat(ref.path.c_str(), &s) == 0)
	{
		return s.st_size;
	}
	return 0;
}

RESULT eStaticServiceAppInfo::getEvent(const eServiceReference &ref, ePtr<eServiceEvent> &evt, time_t start_time)
{
#ifdef HAVE_EPG
	if (ref.path.find("://") != std::string::npos)
	{
		eServiceReference equivalentref(ref);
		equivalentref.type = eServiceFactoryApp::idServiceMP3;
		equivalentref.path.clear();
		return eEPGCache::getInstance()->lookupEventTime(equivalentref, start_time, evt);
	}
	evt = 0;
#endif
	return -1;
}


DEFINE_REF(eServiceFactoryApp)

eServiceFactoryApp::eServiceFactoryApp()
{
	ePtr<eServiceCenter> sc;

	eServiceCenter::getPrivInstance(sc);
	if (sc)
	{
		std::list<std::string> extensions;
		extensions.push_back("dts");
		extensions.push_back("mp2");
		extensions.push_back("mp3");
		extensions.push_back("ogg");
		extensions.push_back("ogm");
		extensions.push_back("ogv");
		extensions.push_back("mpg");
		extensions.push_back("vob");
		extensions.push_back("wav");
		extensions.push_back("wave");
		extensions.push_back("m4v");
		extensions.push_back("mkv");
		extensions.push_back("avi");
		extensions.push_back("divx");
		extensions.push_back("dat");
		extensions.push_back("flac");
		extensions.push_back("flv");
		extensions.push_back("mp4");
		extensions.push_back("mov");
		extensions.push_back("m4a");
		extensions.push_back("3gp");
		extensions.push_back("3g2");
		extensions.push_back("asf");
		extensions.push_back("wmv");
		extensions.push_back("wma");
		extensions.push_back("stream");
		if (gReplaceServiceMP3)
		{
			sc->removeServiceFactory(eServiceFactoryApp::idServiceMP3);
			sc->addServiceFactory(eServiceFactoryApp::idServiceMP3, this, extensions);
		}
		extensions.clear();
		sc->addServiceFactory(eServiceFactoryApp::idServiceGstPlayer, this, extensions);
		sc->addServiceFactory(eServiceFactoryApp::idServiceExtEplayer3, this, extensions);
		
	}
	m_service_info = new eStaticServiceAppInfo();
}

eServiceFactoryApp::~eServiceFactoryApp()
{
	ePtr<eServiceCenter> sc;

	eServiceCenter::getPrivInstance(sc);
	if (sc)
	{
		if (gReplaceServiceMP3)
		{
			sc->removeServiceFactory(eServiceFactoryApp::idServiceMP3);
		}
		sc->removeServiceFactory(eServiceFactoryApp::idServiceGstPlayer);
		sc->removeServiceFactory(eServiceFactoryApp::idServiceExtEplayer3);
	}
	
}


eAutoInitPtr<eServiceFactoryApp> init_eServiceFactoryApp(eAutoInitNumbers::service+1, "eServiceFactoryApp");


static PyObject *
use_user_settings(PyObject *self, PyObject *args)
{
	g_useUserSettings = true;
	Py_RETURN_NONE;
}

static PyObject *
servicemp3_exteplayer3_enable(PyObject *self, PyObject *args)
{
	g_playerServiceMP3 = EXTEPLAYER3;
	Py_RETURN_NONE;
}

static PyObject *
servicemp3_gstplayer_enable(PyObject *self, PyObject *args)
{
	g_playerServiceMP3 = GSTPLAYER;
	Py_RETURN_NONE;
}


static PyObject *
gstplayer_set_setting(PyObject *self, PyObject *args)
{
	bool ret = true;

	int settingId;
	char *audioSink, *videoSink;
	bool subtitlesEnable; 
	long bufferSize, bufferDuration;

	if (!PyArg_ParseTuple(args, "issbll", &settingId, &videoSink, &audioSink, &subtitlesEnable, &bufferSize, &bufferDuration))
		return NULL;
	
	GstPlayerOptions *options = NULL;
	switch (settingId)
	{
		case OPTIONS_SERVICEGSTPLAYER:
			options = &g_GstPlayerOptionsServiceGst;
			eDebug("[gstplayer_set_setting] setting servicegstplayer options");
			break;
		case OPTIONS_SERVICEMP3_GSTPLAYER:
			options = &g_GstPlayerOptionsServiceMP3;
			eDebug("[gstplayer_set_setting] setting servicemp3 options");
			break;
		case OPTIONS_USER:
			options = &g_GstPlayerOptionsUser;
			eDebug("[gstplayer_set_setting] setting user options");
			break;
		default:
			eWarning("[gstplayer_set_setting] option '%d' is not known, cannot be set!", settingId);
			ret = false;
			break;
	}
	if (options != NULL)
	{
		options->videoSink = videoSink;
		options->audioSink = audioSink;
		options->subtitleEnabled = subtitlesEnable;
		options->bufferSize = bufferSize;
		options->bufferDuration = bufferDuration;
	}
	return Py_BuildValue("b", ret);
}

static PyObject *
exteplayer3_set_setting(PyObject *self, PyObject *args)
{
	bool ret = true;

	int settingId;
	bool aacSwDecoding;
	bool dtsSwDecoding;
	bool wmaSwDecoding;
	bool downmix;
	bool lpcmInjection;

	if (!PyArg_ParseTuple(args, "ibbbbb", &settingId, &aacSwDecoding, &dtsSwDecoding, &wmaSwDecoding, &lpcmInjection, &downmix))
		return NULL;

	ExtEplayer3Options *options = NULL;
	switch (settingId)
	{
		case OPTIONS_SERVICEEXTEPLAYER3:
			options = &g_ExtEplayer3OptionsServiceExt3;
			eDebug("[exteplayer3_set_setting] setting serviceextplayer3 options");
			break;
		case OPTIONS_SERVICEMP3_EXTEPLAYER3:
			options = &g_ExtEplayer3OptionsServiceMP3;
			eDebug("[exteplayer3_set_setting] setting servicemp3 options");
			break;
		case OPTIONS_USER:
			options = &g_ExtEplayer3OptionsUser;
			eDebug("[exteplayer3_set_setting] setting user options");
			break;
		default:
			eWarning("[exteplayer3_set_setting] option '%d' is not known, cannot be set!", settingId);
			ret = false;
			break;
	}
	if (options != NULL)
	{
		options->aacSwDecoding = aacSwDecoding;
		options->dtsSwDecoding = dtsSwDecoding;
		options->wmaSwDecoding = wmaSwDecoding;
		options->lpcmInjection = lpcmInjection;
		options->downmix = downmix;
	}
	return Py_BuildValue("b", ret);
}


static PyMethodDef serviceappMethods[] = {
	{"use_user_settings", use_user_settings, METH_NOARGS,
	 "user settings will be used for creation of player"},
	{"servicemp3_exteplayer3_enable", servicemp3_exteplayer3_enable, METH_NOARGS,
	 "use ffmpeg based extplayer3, when servicemp3 is replaced by serviceapp"},
	{"servicemp3_gstplayer_enable", servicemp3_gstplayer_enable, METH_NOARGS,
	 "use gstreamer based player, when servicemp3 is replaced by serviceapp"},
	{"gstplayer_set_setting", gstplayer_set_setting, METH_VARARGS,
	 "set gstreamer player settings (setting_id, videoSink, audioSink, subtitlesEnabled, bufferSize, bufferDuration\n\n"
	 " setting_id - (0 - servicemp3_gst, 1 - servicemp3_extep3, 2 - servicegst, 3 - serviceextep3, 4 - user)\n"
	 " videoSink - (dvbvideosink, dvbvideosinkexp, ...)\n"
	 " audioSink - (dvbaudiosink, dvbaudiosinkexp, ...)\n"
	 " subtitleEnable - (True, False)\n"
	 " bufferSize - in kilobytes\n"
	 " bufferDuration - in seconds\n"
	},
	{"exteplayer3_set_setting", exteplayer3_set_setting, METH_VARARGS,
	 "set exteplayer3 settings (setting_id, aacSwDecoding, dtsSwDecoding, wmaSwDecoding, lpcmInjection, downmix\n\n"
	 " setting_id - (0 - servicemp3_gst, 1 - servicemp3_extep3, 2 - servicegst, 3 - serviceextep3, 4 - user)\n"
	 " aacSwDecoding - (True, False)\n"
	 " dtsSwDecoding - (True, False)\n"
	 " wmaSwDecoding - (True, False)\n"
	 " lpcmInjection - (True, False)\n"
	 " downmix - (True, False)\n"
	},
	 {NULL,NULL,0,NULL}
};

PyMODINIT_FUNC
initserviceapp(void)
{
	Py_InitModule("serviceapp", serviceappMethods);
}
