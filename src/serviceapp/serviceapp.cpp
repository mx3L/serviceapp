#include "Python.h"
#include <sstream>
#include <algorithm>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <lib/service/service.h>
#include <lib/components/file_eraser.h>
#include <lib/base/init_num.h>
#include <lib/base/init.h>
#include <lib/base/eenv.h>
#include <lib/base/nconfig.h>
#ifdef HAVE_EPG
#include <lib/dvb/epgcache.h>
#endif
#include <lib/gui/esubtitle.h>
#include <lib/dvb/idvb.h>

#include "serviceapp.h"
#include "gstplayer.h"
#include "exteplayer3.h"

enum
{
	SUBSERVICES_INDEX_START = 1,
	SUBSERVICES_INDEX_END = 0xFF,
	SUBSERVICES_BITRATEKB_START = 0x100
};

enum
{
	EXTEPLAYER3,
	GSTPLAYER,
};

enum
{
	OPTIONS_SERVICEMP3,
	OPTIONS_SERVICEGSTPLAYER,
	OPTIONS_SERVICEEXTEPLAYER3,
	OPTIONS_USER,
};

static int g_playerServiceMP3 = GSTPLAYER;
static bool g_useUserSettings = false;

static GstPlayerOptions *g_GstPlayerOptionsServiceMP3;
static GstPlayerOptions *g_GstPlayerOptionsServiceGst;
static GstPlayerOptions *g_GstPlayerOptionsUser;

static ExtEplayer3Options *g_ExtEplayer3OptionsServiceMP3;
static ExtEplayer3Options *g_ExtEplayer3OptionsServiceExt3;
static ExtEplayer3Options *g_ExtEplayer3OptionsUser;

static eServiceAppOptions *g_ServiceAppOptionsServiceMP3;
static eServiceAppOptions *g_ServiceAppOptionsServiceExt3;
static eServiceAppOptions *g_ServiceAppOptionsServiceGst;
static eServiceAppOptions *g_ServiceAppOptionsUser;

static const std::string gReplaceServiceMP3Path = eEnv::resolve("$sysconfdir/enigma2/serviceapp_replaceservicemp3");
static const bool gReplaceServiceMP3 = ( access( gReplaceServiceMP3Path.c_str(), F_OK ) != -1 );

static HeaderMap getHttpHeaders(const std::string& path)
{
	HeaderMap headers = getHeaders(path);
	for (HeaderMap::iterator it(headers.begin()); it != headers.end();)
	{
		if (it->first.find("sapp_") == 0)
			headers.erase(it++);
		else
			it++;
	}
	return headers;
}

static void updatePlayerOptions(IOption &options, const HeaderMap &headers)
{
	for (HeaderMap::const_iterator it(headers.begin()); it != headers.end(); it++)
	{
		if (it->first.find("sapp_") == 0)
		{
			options.update(it->first.substr(5), it->second);
		}
	}
}

static BasePlayer *createPlayer(const eServiceReference& ref, const HeaderMap &headers)
{
	BasePlayer *player = NULL;
	if (ref.type == eServiceFactoryApp::idServiceExtEplayer3 || (ref.type == eServiceFactoryApp::idServiceMP3 && g_playerServiceMP3 == EXTEPLAYER3) )
	{
		ExtEplayer3Options options;
		if (g_useUserSettings)
			options = *g_ExtEplayer3OptionsUser;
		else if (ref.type == eServiceFactoryApp::idServiceExtEplayer3)
			options = *g_ExtEplayer3OptionsServiceExt3;
		else
			options = *g_ExtEplayer3OptionsServiceMP3;
		updatePlayerOptions(options, headers);
		player = new ExtEplayer3(options);
	}
	else if (ref.type == eServiceFactoryApp::idServiceGstPlayer || (ref.type == eServiceFactoryApp::idServiceMP3 && g_playerServiceMP3 == GSTPLAYER) )
	{
		GstPlayerOptions options;
		if (g_useUserSettings)
			options = *g_GstPlayerOptionsUser;
		else if (ref.type == eServiceFactoryApp::idServiceGstPlayer)
			options = *g_GstPlayerOptionsServiceGst;
		else
			options = *g_GstPlayerOptionsServiceMP3;
		updatePlayerOptions(options, headers);
		player = new GstPlayer(options);
	}
	return player;
}

static eServiceAppOptions *createOptions(const eServiceReference& ref)
{
	eServiceAppOptions *options = NULL;
	switch(ref.type)
	{
		case eServiceFactoryApp::idServiceMP3:
			options = g_ServiceAppOptionsServiceMP3;
			break;
		case eServiceFactoryApp::idServiceExtEplayer3:
			options = g_ServiceAppOptionsServiceExt3;
			break;
		case eServiceFactoryApp::idServiceGstPlayer:
			options = g_ServiceAppOptionsServiceGst;
			break;
		default:
			break;
	}
	if(g_useUserSettings)
	{
		options = g_ServiceAppOptionsUser;
	}
	return new eServiceAppOptions(*options);
}


class eServiceOfflineOperations: public iServiceOfflineOperations
{
	DECLARE_REF(eServiceOfflineOperations);
	eServiceReference m_ref;
public:
	eServiceOfflineOperations(const eServiceReference &ref);

	RESULT deleteFromDisk(int simulate);
	RESULT getListOfFilenames(std::list<std::string> &);
	RESULT reindex();
};

DEFINE_REF(eServiceOfflineOperations);

eServiceOfflineOperations::eServiceOfflineOperations(const eServiceReference &ref): m_ref((const eServiceReference&)ref)
{
}

RESULT eServiceOfflineOperations::deleteFromDisk(int simulate)
{
	if (!simulate)
	{
		std::list<std::string> res;
		if (getListOfFilenames(res))
			return -1;

		eBackgroundFileEraser *eraser = eBackgroundFileEraser::getInstance();
		if (!eraser)
			eDebug("[eServiceOfflineOperations] FATAL !! can't get background file eraser");

		for (std::list<std::string>::iterator i(res.begin()); i != res.end(); ++i)
		{
			eDebug("[eServiceOfflineOperations] Removing %s...", i->c_str());
			if (eraser)
				eraser->erase(i->c_str());
			else
				::unlink(i->c_str());
		}
	}
	return 0;
}

RESULT eServiceOfflineOperations::getListOfFilenames(std::list<std::string> &res)
{
	res.clear();
	res.push_back(m_ref.path);
	return 0;
}

RESULT eServiceOfflineOperations::reindex()
{
	return -1;
}


RESULT eServiceFactoryApp::offlineOperations(const eServiceReference &ref, ePtr<iServiceOfflineOperations> &ptr)
{
	ptr = new eServiceOfflineOperations(ref);
	return 0;
}


DEFINE_REF(eServiceApp);

eServiceApp::eServiceApp(eServiceReference ref):
	m_ref(ref),
	m_subservices_checked(false),
	player(0),
	extplayer(0),
	m_resolver(0),
	m_resolve_uri("resolve://"),
	m_event_started(false),
	m_paused(false),
	m_framerate(-1),
	m_width(-1),
	m_height(-1),
	m_progressive(-1),
	m_subtitle_pages(0),
	m_selected_subtitle_track(0),
	m_prev_subtitle_message(0),
	m_prev_subtitle_fps(1),
	m_prev_decoder_time(-1),
	m_decoder_time_valid_state(0)
{
	options = createOptions(ref);
	extplayer = createPlayer(ref, getHeaders(ref.path));
	player = new PlayerBackend(extplayer);

	m_subtitle_widget = 0;
	m_subtitle_sync_timer = eTimer::create(eApp);
	CONNECT(m_subtitle_sync_timer->timeout, eServiceApp::pushSubtitles);
	m_event_updated_info_timer = eTimer::create(eApp);
	CONNECT(m_event_updated_info_timer->timeout, eServiceApp::signalEventUpdatedInfo);

#ifdef HAVE_EPG
	m_nownext_timer = eTimer::create(eApp);
	CONNECT(m_nownext_timer->timeout, eServiceApp::updateEpgCacheNowNext);
#endif
	CONNECT(player->gotPlayerMessage, eServiceApp::gotExtPlayerMessage);
};

eServiceApp::~eServiceApp()
{
	delete options;
	delete player;
	delete extplayer;
	delete m_resolver;

	if (m_subtitle_widget) m_subtitle_widget->destroy();
	m_subtitle_widget = 0;
#ifdef HAVE_EPG
	m_nownext_timer->stop();
#endif
	g_useUserSettings = false;
};



void eServiceApp::fillSubservices()
{
	m_subservice_vec.clear();
	m_subserviceref_vec.clear();

	if (isM3U8Url(m_ref.path))
	{
		M3U8VariantsExplorer ve(m_ref.path, getHttpHeaders(m_ref.path));
		m_subservice_vec = ve.getStreams();
		if (m_subservice_vec.empty())
		{
			eDebug("eServiceApp::fillSubservices - failed to retrieve subservices");
		}
		else
		{
			// sort subservices from best quality to worst (internally sorted according to bitrate)
			sort(m_subservice_vec.rbegin(), m_subservice_vec.rend());

			std::vector<M3U8StreamInfo>::const_iterator it;
			// find title from parent, if parent has already bitrate
			// string set, we look for this bitrate and separate original
			// name from it.
			std::stringstream sstm;
			std::string original_title(m_ref.name);
			for (it = m_subservice_vec.begin(); it != m_subservice_vec.end(); it++)
			{
				sstm.str(std::string());
				sstm << it->bitrate;
				std::string bitrate_str = sstm.str();
				size_t bitrate_idx = m_ref.name.find(": " + bitrate_str);
				if (bitrate_idx != std::string::npos)
				{
					original_title = m_ref.name.substr(0, bitrate_idx);
					break;
				}
			}

			int i = 0;
			for (it = m_subservice_vec.begin(); it != m_subservice_vec.end(); it++, i++)
			{
				if (SUBSERVICES_INDEX_START + i > SUBSERVICES_INDEX_END)
				{
					eWarning("eServiceApp::fillSubservices - cannot add more then %d subservices!", SUBSERVICES_INDEX_END);
					break;
				}
				// we need to copy all flags from parent service, neccessary for EPG
				eServiceReference ref(m_ref);
				// set index so we know which service to select from master playlist
				ref.setUnsignedData(7, SUBSERVICES_INDEX_START + i);
				// set parentTransportStreamId, since InfoBarSubservicesSupport
				// checks this flag when creating subservices menu. If it's available
				// at least for one subservice then it will allow to add subservices 
				// to bouquet or favorites, see subserviceSelection.
				//
				// If it's not available it will only allow to quickzap subservices and it
				// will also remove name for subservice service, see playSubservice.
				eServiceReferenceDVB &dvb_ref = (eServiceReferenceDVB&)ref;
				if (dvb_ref.getTransportStreamID().get())
				{
					// If user wants EPG, i.e. fills serviceId, transportStreamId then
					// we have to set these as parentServiceId and parentTransportStreamID
					// since epgcache uses those to create EPG query
					dvb_ref.setParentServiceID(dvb_ref.getServiceID());
					dvb_ref.setParentTransportStreamID(dvb_ref.getTransportStreamID());
				}
				else
				{
					dvb_ref.setParentTransportStreamID(1);// some random value
				}
				sstm.str(std::string());
				sstm << original_title << ": " << it->bitrate << "b/s";
				if (!it->resolution.empty())
					sstm << " - " << it->resolution;
				ref.name = sstm.str();
				m_subserviceref_vec.push_back(ref);
			}
			eDebug("eServiceApp::fillSubservices - found %zd subservices", m_subservice_vec.size());
		}
	}
	else
	{
		eDebug("eServiceApp::fillSubservices - failed to retrieve subservices, not supported url");
	}
}

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

ssize_t eServiceApp::getTrackPosition(const SubtitleTrack &track)
{
	ssize_t track_pos = -1;
	std::vector<SubtitleTrack>::const_iterator it(m_subtitle_tracks.begin());
	for (size_t i = 0; it != m_subtitle_tracks.end(); it++,i++)
	{
		if (it->pid == track.pid
				&& it->type == track.type
				&& it->page_number == track.page_number
				&& it->magazine_number == track.magazine_number
				&& it->language_code == track.language_code)
		{
			track_pos = i;
			break;
		}
	}
	return track_pos;
}

void eServiceApp::addEmbeddedTrack(std::vector<struct SubtitleTrack> &subtitlelist, subtitleStream &s, int pid)
{
	m_subtitle_streams.push_back(s);
	struct SubtitleTrack track;
	track.type = 2;
	track.page_number = 1;
	track.magazine_number = 0;
	track.pid = pid;
	track.language_code = s.language_code;

	subtitlelist.push_back(track);
	m_subtitle_tracks.push_back(track);
}

void eServiceApp::addExternalTrack(std::vector<struct SubtitleTrack> &subtitlelist, int pid, std::string lang, std::string path)
{
	subtitleStream s;
	s.path = path;
	m_subtitle_streams.push_back(s);

	struct SubtitleTrack track;
	track.type = 2;
	track.page_number = 4;
	track.magazine_number = 0;
	track.pid = pid;
	track.language_code = lang;

	subtitlelist.push_back(track);
	m_subtitle_tracks.push_back(track);
}

bool eServiceApp::isEmbeddedTrack(const SubtitleTrack &track)
{
	return (track.type == 2 && track.page_number == 1);
}

bool eServiceApp::isExternalTrack(const SubtitleTrack &track)
{
	return (track.type == 2 && track.page_number == 4);
}

void eServiceApp::pullSubtitles()
{
	std::queue<subtitleMessage> pulled;
	player->getSubtitles(pulled);
	eDebug("eServiceApp::pullSubtitles - pulling %d subtitles", pulled.size());
	while (!pulled.empty())
	{
		subtitleMessage sub = pulled.front();
		m_embedded_subtitle_pages.insert(subtitle_pages_map_pair(sub.end_ms, sub));
		pulled.pop();
	}
	m_subtitle_sync_timer->start(1, true);
}

void eServiceApp::pushSubtitles()
{
	pts_t running_pts = 0;
	int32_t next_timer = 0, decoder_ms, start_ms, end_ms, diff_start_ms, diff_end_ms;
	subtitle_pages_map::const_iterator current;

	int delay = eConfigManager::getConfigIntValue("config.subtitles.pango_subtitles_delay");
	if (isExternalTrack(*m_selected_subtitle_track))
	{
		int subtitle_fps = eConfigManager::getConfigIntValue("config.subtitles.pango_subtitles_fps");
		if (subtitle_fps != m_prev_subtitle_fps)
		{
			m_prev_subtitle_fps = subtitle_fps;
			ssize_t track_pos = getTrackPosition(*m_selected_subtitle_track);
			const subtitleMap *submap = NULL;
			submap = m_subtitle_manager.load(m_subtitle_streams[track_pos].path, m_framerate, subtitle_fps);
			if (submap)
			{
				m_prev_subtitle_message = NULL;
				m_subtitle_pages = submap;
			}
		}
	}

	if (!m_subtitle_pages)
		return;

	if (getPlayPosition(running_pts) < 0)
	{
		m_decoder_time_valid_state = 0;
		next_timer = 50;
		goto exit;
	}
	if (m_decoder_time_valid_state < 3)
	{
		m_decoder_time_valid_state++;
		// this happens after we start seeking operation
		// decoder pts is not updated, we have to wait
		// for seek to finish.
		if (m_prev_decoder_time == running_pts)
		{
			m_decoder_time_valid_state = 0;
		}
		if (m_decoder_time_valid_state < 3)
		{
			// eDebug("eServiceApp::pushSubtitles - waiting for clock to stabilise: valid=%d, prev=%lld,current=%lld",
			//		m_decoder_time_valid_state, m_prev_decoder_time, decoder_ms);
			m_prev_decoder_time = running_pts;
			// we are updating play position every 100ms in extplayer
			// so to see any progress in decoder_ms we have to wait a little longer
			next_timer = 110;
			goto exit;
		}
		// eDebug("eServiceApp::pushSubtitles - push subtitles, clock stable");
	}
	decoder_ms = (running_pts - delay) / 90;

	for (current = m_subtitle_pages->lower_bound(decoder_ms); current != m_subtitle_pages->end(); current++)
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
		// don't show the same message twice
		if (m_prev_subtitle_message && m_prev_subtitle_message == &(current->second))
		{
			next_timer = 30;
			goto exit;
		}
		if (m_subtitle_widget && !m_paused)
		{
			//eDebug("eServiceApp::pushSubtitles - current sub actual, show!");
			m_prev_subtitle_message = &(current->second);
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

void eServiceApp::signalEventUpdatedInfo()
{
	eDebug("eServiceApp::signalEventUpdatedInfo");
    m_event(this, evUpdatedInfo);
}

void eServiceApp::urlResolved(int success)
{
	eDebug("eServiceApp::urlResolved: %s", success ? "success": "error");
	if (success)
	{
		m_ref.path = m_resolver->getUrl();
		eDebug("eServiceApp::urlResolved: %s", m_ref.path.c_str());
		start();
	}
	else
		stop();
}

void eServiceApp::gotExtPlayerMessage(int message)
{
	switch (message)
	{
		case PlayerMessage::start:
			eDebug("eServiceApp::gotExtPlayerMessage - start");
			m_event_updated_info_timer->start(1000, true);
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
			if (m_selected_subtitle_track && isEmbeddedTrack(*m_selected_subtitle_track))
				pullSubtitles();
			break;
		default:
			eDebug("eServiceApp::gotExtPlayerMessage - unhandled message");
			break;
	}
}


// __iPlayableService
#if SIGCXX_MAJOR_VERSION == 2
RESULT eServiceApp::connectEvent(const sigc::slot2< void, iPlayableService*, int >& event, ePtr< eConnection >& connection)
#else
RESULT eServiceApp::connectEvent(const Slot2< void, iPlayableService*, int >& event, ePtr< eConnection >& connection)
#endif
{
	connection = new eConnection((iPlayableService*)this, m_event.connect(event));
	return 0;
}

RESULT eServiceApp::start()
{
	if (!m_event_started)
	{
		m_event(this, evUpdatedEventInfo);
		m_event(this, evStart);
		m_event_started = true;
	}
	std::string path_str(m_ref.path);

	if (path_str.find(m_resolve_uri) == 0)
	{
		m_resolver = new ResolveUrl(m_ref.path.substr(m_resolve_uri.size()));
		CONNECT(m_resolver->urlResolved, eServiceApp::urlResolved);
		m_resolver->start();
		return 0;
	}
	HeaderMap headers = getHttpHeaders(m_ref.path);
	if (options->HLSExplorer && options->autoSelectStream)
	{
		if (!m_subservices_checked)
		{
			fillSubservices();
			m_event(this, evUpdatedEventInfo);
			m_subservices_checked = true;
		}
		size_t subservice_num = m_subservice_vec.size();
		if (subservice_num)
		{
			M3U8StreamInfo subservice = *(m_subservice_vec.begin());
			unsigned int subservice_flag = m_ref.getUnsignedData(7);
			bool bitrate_selection = (!subservice_flag || subservice_flag >= SUBSERVICES_BITRATEKB_START);
			if (bitrate_selection)
			{
				unsigned int bitrate = 0;
				if (subservice_flag)
					bitrate = (subservice_flag - SUBSERVICES_BITRATEKB_START);
				else
					bitrate = options->connectionSpeedInKb;
				// vector is sorted from best to lowest quality in fillSubservices
				std::vector<M3U8StreamInfo>::const_reverse_iterator it(m_subservice_vec.rbegin());
				while(it != m_subservice_vec.rend())
				{
					if (it->bitrate > bitrate * 1000L)
					{
						if (it != m_subservice_vec.rbegin())
							subservice = *(--it);
						else
							subservice = *(it);
						break;
					}
					it++;
				}
				eDebug("eServiceApp::start - subservice(%lub/s) selected according to connection speed (%lu)",
					subservice.bitrate, bitrate * 1000L);
			}
			else
			{
				unsigned int subservice_idx = subservice_flag - SUBSERVICES_INDEX_START;
				if (subservice_idx < subservice_num)
				{
					subservice = m_subservice_vec[subservice_idx];
				}
				else
				{
					eWarning("eServiceApp::start - subservice_idx(%u) >= subservice_num(%zu), assuming lowest quality",
						subservice_idx, subservice_num);
					subservice = *(m_subservice_vec.end() - 1);
				}
				eDebug("eServiceApp::start - subservice(%lub/s) selected according to index(%u)",
					subservice.bitrate, subservice_idx);
			}
			path_str = subservice.url;
			headers = subservice.headers;
		}
	}
	// don't pass fragment part to player
	player->start(Url(path_str).url(), headers);
	return 0;
}

RESULT eServiceApp::stop()
{
	eDebug("eServiceApp::stop");
	if (m_resolver) m_resolver->stop();
	player->stop();
	return 0;
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
	pts_t length;
	if (to < 0)
	{
		to = 0;
	}
	else if (getLength(length) < 0)
	{
		eWarning("eServiceApp::seekTo - cannot get length");
	}
	else if (length > 0 && to > length)
	{
		stop();
		return 0;
	}
	player->seekTo(int(to/90000));

	m_prev_decoder_time = -1;
	m_decoder_time_valid_state = 0;
	if (m_selected_subtitle_track != NULL)
	{
		m_subtitle_sync_timer->start(1, true);
	}
	return 0;
}

RESULT eServiceApp::seekRelative(int direction, pts_t to)
{
	eDebug("eServiceApp::seekRelative - position = %lld", direction*to);
	pts_t position;
	if (getPlayPosition(position) < 0)
	{
		eWarning("eServiceApp::seekRelative - cannot get play position");
		return -1;
	}
	return seekTo(position + (to * direction));
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
	/* just assume that seeking and fast/slow winding are possible */
	return 3;
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
	trackInfo.m_pid = track.id;
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
	m_subtitle_sync_timer->stop();
	m_prev_subtitle_message = NULL;
	m_subtitle_pages = NULL;
	m_selected_subtitle_track = NULL;

	m_decoder_time_valid_state = 0;
	m_prev_decoder_time = -1;

	ssize_t track_pos = getTrackPosition(track);
	if (track_pos == -1)
	{
		eWarning("eServiceApp::enableSubtitles - track is not in the map!");
		return -1;
	}
	if (isEmbeddedTrack(track))
	{
		eDebug("eServiceApp::enableSubtitles - track = %d (embedded)", track.pid);
		m_embedded_subtitle_pages.clear();
		m_subtitle_pages = &m_embedded_subtitle_pages;
		player->subtitleSelectTrack(track.pid);
	}
	else if (isExternalTrack(track))
	{
		eDebug("eServiceApp::enableSubtitles - track = %d (external)", track.pid);
		subtitleStream s = m_subtitle_streams[track_pos];
		m_subtitle_pages = m_subtitle_manager.load(s.path);
		if (m_subtitle_pages != NULL)
		{
			m_subtitle_sync_timer->start(1, true);
		}
		else
		{
			eWarning("eServiceApp::enableSubtitles - cannot load external subtitles");
			return -1;
		}
	}
	else
	{
		eWarning("eServiceApp::enableSubtitles - not supported track page_number %d", track.page_number);
		return -1;
	}
	m_selected_subtitle_track = &(m_subtitle_tracks[track_pos]);
	m_subtitle_widget = user;
	return 0;
}

RESULT eServiceApp::disableSubtitles()
{
	eDebug("eServiceApp::disableSubtitles");
	m_subtitle_sync_timer->stop();
	m_prev_subtitle_message = NULL;
	m_embedded_subtitle_pages.clear();
	m_subtitle_pages = NULL;
	m_selected_subtitle_track = NULL;
	if (m_subtitle_widget) m_subtitle_widget->destroy();
	m_subtitle_widget = 0;

	m_decoder_time_valid_state = 0;
	m_prev_decoder_time = -1;
	return 0;
}

RESULT eServiceApp::getCachedSubtitle(struct SubtitleTrack &track)
{
	if (!options->autoTurnOnSubtitles)
	{
		eDebug("eServiceApp::getCachedSubtitle - auto-turning disabled in config");
		return -1;
	}
	std::vector<struct SubtitleTrack> tracks;
	if (getSubtitleList(tracks) < 0 || tracks.empty())
	{
		eDebug("eServiceApp::getCachedSubtitle - no subtitles available");
		return -1;
	}
	// TODO consider language setting
	int ret = -1;
	std::vector<struct SubtitleTrack> embedded_tracks;
	std::vector<struct SubtitleTrack> external_tracks;
	std::remove_copy_if(tracks.begin(), tracks.end(), std::back_inserter(embedded_tracks), isExternalTrack);
	std::remove_copy_if(tracks.begin(), tracks.end(), std::back_inserter(external_tracks), isEmbeddedTrack);

	bool select_embedded = (options->preferEmbeddedSubtitles || external_tracks.empty()) && !embedded_tracks.empty();
	if (!select_embedded)
	{
		struct SubtitleTrack tmp_track = *external_tracks.begin();
		subtitleStream tmp_stream = m_subtitle_streams[getTrackPosition(tmp_track)];
		std::string video_base, subtitle_base, extension;
		splitExtension(m_ref.path, video_base, extension);
		splitExtension(tmp_stream.path, subtitle_base, extension);
		if (video_base == subtitle_base || external_tracks.size() == 1)
		{
			track = tmp_track;
			ret = 0;
		}
		else
		{
			select_embedded = !embedded_tracks.empty();
		}
	}
	if (select_embedded)
	{
		track = *embedded_tracks.begin();
		ret = 0;
	}

	if (ret == 0)
	{
		if (options->preferEmbeddedSubtitles && isEmbeddedTrack(track))
			eDebug("eServiceApp::getCachedSubtitle - selected preferred embedded track");
		else if (options->preferEmbeddedSubtitles && !isEmbeddedTrack(track))
			eDebug("eServiceApp::getCachedSubtitle - selected embedded track");
		else if (!options->preferEmbeddedSubtitles && isExternalTrack(track))
			eDebug("eServiceApp::getCachedSubtitle - selected preferred external track");
		else if (!options->preferEmbeddedSubtitles && !isExternalTrack(track))
			eDebug("eServiceApp::getCachedSubtitle - selected external track");
	}
	else
	{
		eDebug("eServiceApp::getCachedSubtitle - no track selected, more than one external track found, name doesn't correspond to video file");
	}
	return ret;
}

RESULT eServiceApp::getSubtitleList(std::vector<struct SubtitleTrack> &subtitlelist)
{
	m_subtitle_tracks.clear();
	m_subtitle_streams.clear();
	int embedded_track_num = player->subtitleGetNumberOfTracks(500);
	eDebug("eServiceApp::getSubtitleList - found embedded tracks (%d)", embedded_track_num);
	int pid = 0;
	for (; pid < embedded_track_num; pid++)
	{
		subtitleStream s;
		if (player->subtitleGetTrackInfo(s, pid) == 0)
		{
			addEmbeddedTrack(subtitlelist, s, pid);
		}
	}
	std::string basename, extension;
	splitExtension(m_ref.path, basename, extension);
	std::string subtitle_path(basename + ".srt");

	std::string dirname, filename;
	splitPath(subtitle_path, dirname, filename);
	// TODO 
	//
	// - try to find out language code from filename if possible
	// - apply some sort of sorting which would add more relevant subtitles to beginning
	// of the list
	//
	// - probably whole thing should be moved to manager
	if (!access(subtitle_path.c_str(), F_OK))
	{
		addExternalTrack(subtitlelist, pid++, filename, subtitle_path);
	}
	std::vector<std::string> directories, files;
	if (listDir(dirname, &files, &directories) == 0)
	{
		std::vector<std::string>::const_iterator it;
		if ((std::find(directories.begin(), directories.end(), "Subs")) != directories.end())
		{
			std::vector<std::string> subsdir_files;
			if (listDir(dirname + "/Subs", &subsdir_files, NULL) == 0)
			{
				for (it = subsdir_files.begin(); it != subsdir_files.end(); it++)
				{
					splitExtension(*it, basename, extension);
					if (extension == ".srt")
					{
						addExternalTrack(subtitlelist, pid++, basename, dirname + "/Subs/" + *it);
					}
				}
			}
		}
		for (it = files.begin(); it != files.end(); it++)
		{
			splitExtension(*it, basename, extension);
			std::string path = dirname + "/" + *it;
			if (extension == ".srt" && subtitle_path != path)
			{
				addExternalTrack(subtitlelist, pid++, basename, path);
			}
		}
	}
	eDebug("eServiceApp::getSubtitleList - found external tracks (%d)", pid - embedded_track_num);
	return 0;
}

// __iSubservices
int eServiceApp::getNumberOfSubservices()
{
	std::string path_str(m_ref.path);
	if (options->HLSExplorer && path_str.find(m_resolve_uri) && !m_subservices_checked)
	{
		fillSubservices();
		m_subservices_checked = true;
	}
	eDebug("eServiceApp::getNumberOfSubservices - %zu", m_subserviceref_vec.size());
	return m_subserviceref_vec.size();
}

RESULT eServiceApp::getSubservice(eServiceReference &subservice, unsigned int n)
{
	eDebug("eServiceApp::getSubservice - %d", n);
	subservice = m_subserviceref_vec[n];
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
		return resNA;
	case sVideoType:
	{
		videoStream v;
		if (!player->videoGetTrackInfo(v,0))
		{
			// map exteplayer to stream type
			if (v.description == "V_MPEG2") return 0;
			else if (v.description == "V_MPEG4/ISO/AVC") return 1;
			else if (v.description.find("V_MPEG4") != std::string::npos) return 4;
			else if (v.description == "V_MPEG1") return 6;
			else if (v.description == "V_MPEGH/ISO/HEVC") return 7;
			else if (v.description == "V_VP8") return 8;
			else if (v.description == "V_VP9") return 9;

			// map gstplayer to stream type (mpeg might be mpeg1 or mpeg2, but can't tell from description ony)
			if (v.description == "video/mpeg" || v.description == "video/x-3ivx" || v.description == "video/x-msmpeg") return 4;
			else if (v.description == "video/x-h263") return 2;
			else if (v.description == "video/x-h264") return 1;
			else if (v.description == "video/x-h265") return 7;
			else if (v.description == "video/x-xvid") return 10;
			else if (v.description == "video/x-wmv") return 3;
			else if (v.description == "video/x-vp6" || v.description == "video/x-vp6-flash") return 18;
			else if (v.description == "video/x-vp8") return 8;
			else if (v.description == "video/x-vp9") return 9;
			else if (v.description == "video/x-flash-video") return 21;
		}
		return resNA;
	}
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
		extensions.push_back("mp3");
		extensions.push_back("wav");
		extensions.push_back("wave");
		extensions.push_back("ogg");
		extensions.push_back("flac");
		extensions.push_back("m4a");
		extensions.push_back("mp2");
		extensions.push_back("m2a");
		extensions.push_back("wma");
		extensions.push_back("ac3");
		extensions.push_back("mka");
		extensions.push_back("aac");
		extensions.push_back("ape");
		extensions.push_back("alac");
		extensions.push_back("mpg");
		extensions.push_back("vob");
		extensions.push_back("m4v");
		extensions.push_back("mkv");
		extensions.push_back("avi");
		extensions.push_back("divx");
		extensions.push_back("dat");
		extensions.push_back("flv");
		extensions.push_back("mp4");
		extensions.push_back("mov");
		extensions.push_back("wmv");
		extensions.push_back("asf");
		extensions.push_back("3gp");
		extensions.push_back("3g2");
		extensions.push_back("mpeg");
		extensions.push_back("mpe");
		extensions.push_back("rm");
		extensions.push_back("rmvb");
		extensions.push_back("ogm");
		extensions.push_back("ogv");
		extensions.push_back("webm");
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


eAutoInitPtr<eServiceFactoryApp> init_eServiceFactoryApp(eAutoInitNumbers::service+3, "eServiceFactoryApp");


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
			options = g_GstPlayerOptionsServiceGst;
			eDebug("[gstplayer_set_setting] setting servicegstplayer options");
			break;
		case OPTIONS_SERVICEMP3:
			options = g_GstPlayerOptionsServiceMP3;
			eDebug("[gstplayer_set_setting] setting servicemp3 options");
			break;
		case OPTIONS_USER:
			options = g_GstPlayerOptionsUser;
			eDebug("[gstplayer_set_setting] setting user options");
			break;
		default:
			eWarning("[gstplayer_set_setting] option '%d' is not known, cannot be set!", settingId);
			ret = false;
			break;
	}
	if (options != NULL)
	{
		options->GetSettingMap()[GST_VIDEO_SINK].setValue(videoSink);
		options->GetSettingMap()[GST_AUDIO_SINK].setValue(audioSink);
		options->GetSettingMap()[GST_SUBTITLE_ENABLED].setValue(subtitlesEnable);
		options->GetSettingMap()[GST_BUFFER_SIZE].setValue(bufferSize);
		options->GetSettingMap()[GST_BUFFER_DURATION].setValue(bufferDuration);
	}
	return Py_BuildValue("b", ret);
}

static PyObject *
exteplayer3_set_setting(PyObject *self, PyObject *args)
{
	bool ret = true;

	int settingId;
	bool aacSwDecoding;
	bool ac3SwDecoding;
	bool eac3SwDecoding;
	bool dtsSwDecoding;
	bool mp3SwDecoding;
	bool wmaSwDecoding;
	bool downmix;
	bool lpcmInjection;
	int rtmpProtocol;

	if (!PyArg_ParseTuple(args, "ibbbbbbbbi",
				&settingId,
				&aacSwDecoding,
				&dtsSwDecoding,
				&wmaSwDecoding,
				&lpcmInjection,
				&downmix,
				&ac3SwDecoding,
				&eac3SwDecoding,
				&mp3SwDecoding,
				&rtmpProtocol))
		return NULL;

	ExtEplayer3Options *options = NULL;
	switch (settingId)
	{
		case OPTIONS_SERVICEEXTEPLAYER3:
			options = g_ExtEplayer3OptionsServiceExt3;
			eDebug("[exteplayer3_set_setting] setting serviceextplayer3 options");
			break;
		case OPTIONS_SERVICEMP3:
			options = g_ExtEplayer3OptionsServiceMP3;
			eDebug("[exteplayer3_set_setting] setting servicemp3 options");
			break;
		case OPTIONS_USER:
			options = g_ExtEplayer3OptionsUser;
			eDebug("[exteplayer3_set_setting] setting user options");
			break;
		default:
			eWarning("[exteplayer3_set_setting] option '%d' is not known, cannot be set!", settingId);
			ret = false;
			break;
	}
	if (options != NULL)
	{
		options->GetSettingMap()[EXT3_SW_DECODING_AAC].setValue(aacSwDecoding);
		options->GetSettingMap()[EXT3_SW_DECODING_AC3].setValue(ac3SwDecoding);
		options->GetSettingMap()[EXT3_SW_DECODING_EAC3].setValue(eac3SwDecoding);
		options->GetSettingMap()[EXT3_SW_DECODING_DTS].setValue(dtsSwDecoding);
		options->GetSettingMap()[EXT3_SW_DECODING_WMA].setValue(wmaSwDecoding);
		options->GetSettingMap()[EXT3_SW_DECODING_MP3].setValue(mp3SwDecoding);
		options->GetSettingMap()[EXT3_LPCM_INJECTION].setValue(lpcmInjection);
		options->GetSettingMap()[EXT3_RTMP_PROTOCOL].setValue(rtmpProtocol);
		options->GetSettingMap()[EXT3_DOWNMIX].setValue(downmix);
	}
	return Py_BuildValue("b", ret);
}

static PyObject *
serviceapp_set_setting(PyObject *self, PyObject *args)
{
	bool ret = true;

	bool autoTurnOnSubtitles;
	int settingId;
	bool HLSExplorer;
	bool autoSelectStream;
	int32_t connectionSpeedInKb;

	if (!PyArg_ParseTuple(args, "ibbIb", &settingId, &HLSExplorer, &autoSelectStream, &connectionSpeedInKb, &autoTurnOnSubtitles))
		return NULL;
	
	eServiceAppOptions *options = NULL;
	switch (settingId)
	{
		case OPTIONS_SERVICEEXTEPLAYER3:
			options = g_ServiceAppOptionsServiceExt3;
			eDebug("[serviceapp_set_setting] setting serviceexteplayer3 options");
			break;
		case OPTIONS_SERVICEGSTPLAYER:
			options = g_ServiceAppOptionsServiceGst;
			eDebug("[serviceapp_set_setting] setting servicegstplayer options");
			break;
		case OPTIONS_SERVICEMP3:
			options = g_ServiceAppOptionsServiceMP3;
			eDebug("[serviceapp_set_setting] setting servicemp3 options");
			break;
		case OPTIONS_USER:
			options = g_ServiceAppOptionsUser;
			eDebug("[serviceapp_set_setting] setting user options");
			break;
		default:
			eWarning("[serviceapp_set_setting] option '%d' is not known, cannot be set!", settingId);
			ret = false;
			break;
	}
	if (options != NULL)
	{
		options->autoTurnOnSubtitles = autoTurnOnSubtitles;
		options->HLSExplorer = HLSExplorer;
		options->autoSelectStream = autoSelectStream;
		options->connectionSpeedInKb = connectionSpeedInKb;
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
	 " setting_id - (0 - servicemp3, 1 - servicegst, 2 - serviceextep3, 3 - user)\n"
	 " videoSink - (dvbvideosink, dvbvideosinkexp, ...)\n"
	 " audioSink - (dvbaudiosink, dvbaudiosinkexp, ...)\n"
	 " subtitleEnable - (True, False)\n"
	 " bufferSize - in kilobytes\n"
	 " bufferDuration - in seconds\n"
	},
	{"exteplayer3_set_setting", exteplayer3_set_setting, METH_VARARGS,
	 "set exteplayer3 settings (setting_id, aacSwDecoding, dtsSwDecoding, wmaSwDecoding, lpcmInjection, downmix, ac3SwDecoding, eac3SwDecoding, mp3SwDecoding, rtmpProtocol)\n\n"
	 " setting_id - (0 - servicemp3, 1 - servicegst, 2 - serviceextep3, 3 - user)\n"
	 " aacSwDecoding - (True, False)\n"
	 " dtsSwDecoding - (True, False)\n"
	 " wmaSwDecoding - (True, False)\n"
	 " lpcmInjection - (True, False)\n"
	 " downmix - (True, False)\n"
	 " ac3SwDecoding - (True, False)\n"
	 " eac3SwDecoding - (True, False)\n"
	 " mp3SwDecoding - (True, False)\n"
	 " rtmpProtocol - (0|1|2)\n"
	},
	{"serviceapp_set_setting", serviceapp_set_setting, METH_VARARGS,
	 "set serviceapp settings (setting_id, HLSExplorer, autoSelectStream, connectionSpeedInKb, autoTurnOnSubtitles\n\n"
	 " setting_id - (0 - servicemp3, 1 - servicegst, 2 - serviceextep3, 3 - user)\n"
	 " HLSExplorer - defines if HLS explorer will be used to retrieve streams from HLS master playlist (True, False))\n"
	 " autoSelectStream - if there are more streams available, it defines if stream will be auto-selected according to connectionSpeedInKb (True, False)\n"
	 " connectionSpeedInKb - defines bitrate in kilobits/s according to which will be selected stream from playlist <0, max(int32_t)>\n"
	 " autoTurnOnSubtitles - auto turn on subtitles if available (True, False)\n"
	},
	 {NULL,NULL,0,NULL}
};

PyMODINIT_FUNC
initserviceapp(void)
{
	Py_InitModule("serviceapp", serviceappMethods);
	g_GstPlayerOptionsServiceMP3 = new GstPlayerOptions();
	g_GstPlayerOptionsServiceGst = new GstPlayerOptions();
	g_GstPlayerOptionsUser = new GstPlayerOptions();

	g_ExtEplayer3OptionsServiceMP3 = new ExtEplayer3Options();
	g_ExtEplayer3OptionsServiceExt3 = new ExtEplayer3Options();
	g_ExtEplayer3OptionsUser = new ExtEplayer3Options();

	g_ServiceAppOptionsServiceMP3 = new eServiceAppOptions();
	g_ServiceAppOptionsServiceExt3 = new eServiceAppOptions();
	g_ServiceAppOptionsServiceGst = new eServiceAppOptions();
	g_ServiceAppOptionsUser = new eServiceAppOptions();

	SSL_load_error_strings();
	SSL_library_init();
}
