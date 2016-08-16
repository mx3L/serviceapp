#ifndef __serviceapp_h
#define __serviceapp_h

#include <limits>
#include <lib/service/iservice.h>
#include <lib/base/ebase.h>
#include <lib/base/message.h>
#include <lib/base/thread.h>
#include <lib/dvb/subtitle.h>

#include "common.h"
#include "extplayer.h"
#include "m3u8.h"

struct eServiceAppOptions
{
	bool HLSExplorer;
	bool autoSelectStream;
	unsigned int connectionSpeedInKb;
	eServiceAppOptions():
		HLSExplorer(true), 
		autoSelectStream(true),
		connectionSpeedInKb(std::numeric_limits<unsigned int>::max())
	{};
};

class eServiceApp: public Object, public iPlayableService, public iPauseableService, public iSeekableService, 
	public iAudioChannelSelection, public iAudioTrackSelection,  public iSubtitleOutput, public iSubserviceList, public iServiceInformation
{
	DECLARE_REF(eServiceApp);

	eServiceReference m_ref;

	std::vector<eServiceReference> m_subserviceref_vec;
	std::vector<M3U8StreamInfo> m_subservice_vec;
	bool m_subservices_checked;
	void fillSubservices();

	Signal2<void,iPlayableService*,int> m_event;
	eServiceAppOptions *options;
	PlayerBackend *player;
	BasePlayer *extplayer;
	std::string cmd;

	bool m_paused;
	int m_framerate, m_width, m_height, m_progressive;

	struct subtitle_page_t
	{
		uint32_t start_ms;
		uint32_t end_ms;
		std::string text;

		subtitle_page_t(uint32_t start_ms_in, uint32_t end_ms_in, const std::string& text_in)
			: start_ms(start_ms_in), end_ms(end_ms_in), text(text_in)
		{
		}
	};

	typedef std::map<uint32_t, subtitle_page_t> subtitle_pages_map_t;
	typedef std::pair<uint32_t, subtitle_page_t> subtitle_pages_map_pair_t;
	subtitle_pages_map_t m_subtitle_pages;
	ePtr<eTimer> m_subtitle_sync_timer;
	iSubtitleUser *m_subtitle_widget;
	void pullSubtitles();
	void pushSubtitles();

#ifdef HAVE_EPG
	ePtr<eTimer> m_nownext_timer;
	ePtr<eServiceEvent> m_event_now, m_event_next;
	void updateEpgCacheNowNext();
#endif
	HeaderMap getHeaders(const std::string& url);

	void gotExtPlayerMessage(int message);

public:
	eServiceApp(eServiceReference ref);
	~eServiceApp();

	// iPlayableService
	RESULT connectEvent(const Slot2<void,iPlayableService*,int> &event, ePtr<eConnection> &connection);
	RESULT start();
	RESULT stop();
	RESULT setTarget(int target);
	RESULT seek(ePtr<iSeekableService> &ptr){ ptr=this; return 0;};
	RESULT pause(ePtr<iPauseableService> &ptr){ ptr=this; return 0;};
	RESULT audioTracks(ePtr<iAudioTrackSelection> &ptr) { ptr=this; return 0;};
	RESULT audioChannel(ePtr<iAudioChannelSelection> &ptr) { ptr=this; return 0;};
	RESULT info(ePtr<iServiceInformation> &ptr) { ptr=this; return 0;};
	RESULT subServices(ePtr<iSubserviceList> &ptr){ ptr=this; return 0;};
	RESULT frontendInfo(ePtr<iFrontendInformation> &ptr){ ptr=0; return -1;};
	RESULT timeshift(ePtr<iTimeshiftService> &ptr){ ptr=0; return -1;};
	RESULT cueSheet(ePtr<iCueSheet> &ptr){ ptr=0; return -1;};
	RESULT subtitle(ePtr<iSubtitleOutput> &ptr){ ptr=this; return 0;};
	RESULT audioDelay(ePtr<iAudioDelay> &ptr){ ptr=0; return -1;};
	RESULT rdsDecoder(ePtr<iRdsDecoder> &ptr){ ptr=0; return -1;};
	RESULT stream(ePtr<iStreamableService> &ptr){ ptr=0; return -1;};
	RESULT streamed(ePtr<iStreamedService> &ptr){ ptr=0; return -1;};
	RESULT keys(ePtr<iServiceKeys> &ptr){ ptr=0; return -1;};

	// iPausableService
	RESULT pause();
	RESULT unpause();
	RESULT setSlowMotion(int ratio);
	RESULT setFastForward(int ratio);

	// iSeekableService
	RESULT getLength(pts_t &SWIG_OUTPUT);
	RESULT seekTo(pts_t to);
	RESULT seekRelative(int direction, pts_t to);
	RESULT getPlayPosition(pts_t &SWIG_OUTPUT);
	RESULT setTrickmode(int trick);
	RESULT isCurrentlySeekable();

	// iAudioTrackSelection
	int getNumberOfTracks();
	RESULT selectTrack(unsigned int i);
	RESULT getTrackInfo(struct iAudioTrackInfo &, unsigned int n);
	int getCurrentTrack();

	// iAudioChannelSelection
	int getCurrentChannel();
	RESULT selectChannel(int i);

	// iSubtitleOutput
	RESULT enableSubtitles(iSubtitleUser *user, SubtitleTrack &track);
	RESULT disableSubtitles();
	RESULT getCachedSubtitle(SubtitleTrack &track);
	RESULT getSubtitleList(std::vector<SubtitleTrack> &subtitlelist);

	// iSubserviceList
	int getNumberOfSubservices();
	RESULT getSubservice(eServiceReference &subservice, unsigned int n);

	// iServiceInformation
	RESULT getName(std::string &name);
#ifdef HAVE_EPG
	RESULT getEvent(ePtr<eServiceEvent> &evt, int nownext);
#endif
	int getInfo(int w);
	std::string getInfoString(int w);
};

class eServiceFactoryApp;

class eStaticServiceAppInfo: public iStaticServiceInformation
{
	DECLARE_REF(eStaticServiceAppInfo);
	friend class eServiceFactoryApp;
	eStaticServiceAppInfo(){};
public:
	RESULT getName(const eServiceReference &ref, std::string &name);
	int getLength(const eServiceReference &ref);
	int getInfo(const eServiceReference &ref, int w);
	int isPlayable(const eServiceReference &ref, const eServiceReference &ignore, bool simulate) { return 1; }
	long long getFileSize(const eServiceReference &ref);
	RESULT getEvent(const eServiceReference &ref, ePtr<eServiceEvent> &ptr, time_t start_time);
};

class eServiceFactoryApp: public iServiceHandler
{
	DECLARE_REF(eServiceFactoryApp);
	ePtr<eStaticServiceAppInfo> m_service_info;
public:
	eServiceFactoryApp();
	virtual ~eServiceFactoryApp();
	enum { 
		idServiceMP3 = 4097,
		idServiceGstPlayer = 5001,
		idServiceExtEplayer3 = 5002
	};

	// iServiceHandler
	RESULT play(const eServiceReference &ref, ePtr<iPlayableService> &ptr){ptr = new eServiceApp(ref); return 0;};
	RESULT record(const eServiceReference &, ePtr<iRecordableService> &ptr){ptr=0;return -1;};
	RESULT list(const eServiceReference &, ePtr<iListableService> &ptr){ptr=0;return -1;};
	RESULT info(const eServiceReference &, ePtr<iStaticServiceInformation> &ptr){ptr=m_service_info; return 0;};
	RESULT offlineOperations(const eServiceReference &, ePtr<iServiceOfflineOperations> &ptr){ptr=0;return -1;};
};
#endif
