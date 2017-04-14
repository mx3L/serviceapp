#ifndef __extplayer_h
#define __extplayer_h

#include <lib/base/ebase.h>
#include <lib/base/message.h>
#include <lib/base/thread.h>
#include <lib/python/connections.h>


#include "cJSON/cJSON.h"
#include "myconsole.h"
#include "subtitles/subtitles.h"

#ifndef eLog
 #define eLog(lvl,...)
#endif

enum
{
	STD_OUTPUT,
	STD_ERROR,
};

class PlayerApp: public sigc::trackable
{
	ePtr<eConsoleContainer> console;
	std::string jsonstr;
	unsigned int parseOutput;
	unsigned int truncated;
	void stdoutAvail(const char *data);
	void stderrAvail(const char *data);
	void appClosed(int retval);
	void handleOutput(const std::string& data);
	void handleJsonStr(const std::string& data);
	void handleAppClosed(int retval);
protected:
	virtual std::vector<std::string> buildCommand() = 0;
	virtual void handleJsonOutput(cJSON *json) = 0;
	virtual void handleProcessStopped(int retval) = 0;
	int processStart(eMainloop *context);
	int processSend(const std::string& data);
	void processKill();
	bool processRunning();
public:
	PlayerApp(int parseOutput=STD_ERROR):
		parseOutput(parseOutput),
		truncated(0){}
	~PlayerApp(){}
};


struct PlayerMessage
{
	enum
	{
		start,
		stop,
		pause,
		resume,
		error,
		videoSizeChanged,
		videoProgressiveChanged,
		videoFramerateChanged,
		subtitleAvailable,
	};
};


struct audioStream
{
	int id;
	std::string language_code; /* iso-639, if available. */
	std::string description; /* clear text codec description */
	audioStream(): id(-1){};
};


struct videoStream
{
	int id;
	std::string language_code; /* iso-639, if available. */
	std::string description; /* clear text codec description */
	int width;
	int height;
	int framerate;
	int progressive;
	videoStream(): id(-1), width(-1), height(-1), framerate(-1), progressive(-1){};
};


struct errorMessage
{
	int code;
	std::string message;
	errorMessage():code(-1){}
};


class iPlayerSend
{
public:
	virtual int sendStop() { return -1;}
	virtual int sendForceStop() { return -1;}
	virtual int sendPause(){ return -1;}
	virtual int sendResume(){ return -1;}
	virtual int sendUpdateLength(){ return -1;}
	virtual int sendUpdatePosition(){ return -1;}
	virtual int sendUpdateAudioTracksList(){ return -1;}
	virtual int sendUpdateAudioTrackCurrent(){ return -1;}
	virtual int sendAudioSelectTrack(int trackId){ return -1;}
	virtual int sendUpdateSubtitleTracksList(){ return -1;}
	virtual int sendUpdateSubtitleTrackCurrent(){ return -1;}
	virtual int sendSubtitleSelectTrack(int trackId){ return -1;}
	virtual int sendSeekTo(int seconds){ return -1;}
	virtual int sendSeekRelative(int seconds){ return -1;}
};


class iPlayerCallback
{
public:
	virtual void recvStarted(int status){};
	virtual void recvStopped(int status){};
	virtual void recvPaused(int status){};
	virtual void recvResumed(int status){};
	virtual void recvLength(int status, int mseconds){};
	virtual void recvPosition(int status, int mseconds){};
	virtual void recvSeekTo(int status, int seconds){};
	virtual void recvSeekRelative(int status, int seconds){};
	virtual void recvAudioTracksList(int status, std::vector<audioStream>&){};
	virtual void recvAudioTrackCurrent(int status, audioStream&){}; 
	virtual void recvAudioTrackSelected(int status, int trackId){};
	virtual void recvSubtitleTracksList(int status, std::vector<subtitleStream>&){};
	virtual void recvSubtitleTrackCurrent(int status, subtitleStream&){}; 
	virtual void recvSubtitleTrackSelected(int status, int trackId){};
	virtual void recvSubtitleMessage(subtitleMessage&){};
	virtual void recvVideoTrackCurrent(int status, videoStream&){};
	virtual void recvErrorMessage(errorMessage&){};
};


class BasePlayer: public iPlayerSend, public iPlayerCallback
{
	iPlayerCallback *pCallback;

protected:
	std::string mPath;
	std::map<std::string, std::string> mHeaders;
	
	void recvStarted(int status){pCallback->recvStarted(status);};
	void recvStopped(int status){pCallback->recvStopped(status);};
	void recvPaused(int status){pCallback->recvPaused(status);};
	void recvResumed(int status){pCallback->recvResumed(status);};
	void recvLength(int status, int mseconds){pCallback->recvLength(status, mseconds);};
	void recvPosition(int status, int mseconds){pCallback->recvPosition(status, mseconds);};
	void recvAudioTracksList(int status, std::vector<audioStream>& streams){pCallback->recvAudioTracksList(status, streams);};
	void recvAudioTrackCurrent(int status, audioStream& stream){pCallback->recvAudioTrackCurrent(status, stream);};
	void recvAudioTrackSelected(int status, int trackId){pCallback->recvAudioTrackSelected(status, trackId);};
	void recvSubtitleMessage(subtitleMessage& sub){pCallback->recvSubtitleMessage(sub);};
	void recvSubtitleTracksList(int status, std::vector<subtitleStream>& streams){pCallback->recvSubtitleTracksList(status, streams);};
	void recvSubtitleTrackCurrent(int status, subtitleStream& stream){pCallback->recvSubtitleTrackCurrent(status, stream);};
	void recvSubtitleTrackSelected(int status, int trackId){pCallback->recvSubtitleTrackSelected(status, trackId);};
	void recvVideoTrackCurrent(int status, videoStream& stream){pCallback->recvVideoTrackCurrent(status, stream);};
	void recvSeekTo(int status, int seconds){pCallback->recvSeekTo(status, seconds);};
	void recvSeekRelative(int status, int seconds){pCallback->recvSeekRelative(status, seconds);};
	void recvErrorMessage(errorMessage& message){pCallback->recvErrorMessage(message);};
public:
	virtual ~BasePlayer(){}

	void setCallback(iPlayerCallback *cb){pCallback = cb;}
	void setPath(const std::string& path){mPath = path;}
	void setHttpHeaders(const std::map<std::string, std::string>& headers){mHeaders = headers;}

	virtual int start(eMainloop *context) = 0;

};


class PlayerBackend: public sigc::trackable, public eThread, public eMainloop, public iPlayerCallback
{
	struct Message
	{
		int type;
		int dataInt;
		enum
		{
			start,
			tStart,
			stop,
			tStop,
			tKill,
			pause,
			tPause,
			resume,
			tResume,
			seekTo,
			tSeekTo,
			seekRelative,
			tSeekRelative,
			audioSelect,
			tAudioSelect,
			audioList,
			tAudioList,
			subtitleSelect,
			tSubtitleSelect,
			subtitleList,
			tSubtitleList,
			tGetLength,
			tGetPosition,
			videoSizeChanged,
			videoFramerateChanged,
			videoProgressiveChanged,
			subtitleAvailable,
			error,
		};
		Message(int type)
			:type(type), dataInt(0) {}
		Message(int type, int dataInt)
			:type(type), dataInt(dataInt) {}
	};

	int mPositionInMs, mLengthInMs;
	bool playbackStarted;
	bool mThreadRunning;

	BasePlayer *pPlayer;

	audioStream *pCurrentAudio;
	videoStream *pCurrentVideo;
	subtitleStream *pCurrentSubtitle;
	errorMessage *pErrorMessage;

	std::vector<audioStream> mAudioStreams;
	std::vector<subtitleStream> mSubtitleStreams;
	std::queue<subtitleMessage> mSubtitles;

	eFixedMessagePump<Message> mMessageMain, mMessageThread;
	ePtr<eTimer> mTimer;
	unsigned int mTimerDelay;

	eSingleLock mSubLock;
	
	pthread_mutex_t mWaitMutex;
	pthread_cond_t mWaitCond;
	bool mWaitForUpdate;

	pthread_mutex_t mWaitForStopMutex;
	pthread_cond_t mWaitForStopCond;
	bool mWaitForStop;

	void gotMessage(const Message &message);
	void _updatePosition();

	void sendMessage(const Message& m, int timeout=0);
	void recvMessage();
	
	// eThread
	void thread();
	void thread_finished();
	
	// iPlayerCallback
	void recvStarted(int status);
	void recvStopped(int status);
	void recvPaused(int status);
	void recvResumed(int status);
	void recvLength(int status, int mseconds){ if (!status) mLengthInMs = mseconds; }
	void recvPosition(int status, int mseconds){ if (!status) mPositionInMs = mseconds; }
	void recvAudioTracksList(int status, std::vector<audioStream>& streams);
	void recvAudioTrackCurrent(int status, audioStream& stream);
	void recvAudioTrackSelected(int status, int trackId);
	void recvSubtitleTracksList(int status, std::vector<subtitleStream>& streams);
	void recvSubtitleTrackCurrent(int status, subtitleStream& stream);
	void recvSubtitleTrackSelected(int status, int trackId);
	void recvVideoTrackCurrent(int status, videoStream& stream);
	void recvSeekTo(int status, int seconds){eDebug("PlayerBackend::recvSeekTo %ds", seconds);}
	void recvSeekRelative(int status, int seconds){eDebug("PlayerBackend::recvSeekRelative %ds", seconds);}
	void recvErrorMessage(errorMessage& message){pErrorMessage = new errorMessage(message);};
	void recvSubtitleMessage(subtitleMessage& sub);

public:
	PlayerBackend(BasePlayer* extplayer):
		mPositionInMs(0),
		mLengthInMs(0),
		playbackStarted(false),
		mThreadRunning(false),
		pPlayer(extplayer),
		pCurrentAudio(NULL),
		pCurrentVideo(NULL),
		pCurrentSubtitle(NULL),
		pErrorMessage(NULL),
		mMessageMain(eApp, 1),
		mMessageThread(this, 1),
		mTimerDelay(100) // updated play position timer
	{
		pPlayer->setCallback(this);
		CONNECT(mMessageThread.recv_msg, PlayerBackend::gotMessage);
		CONNECT(mMessageMain.recv_msg, PlayerBackend::gotMessage);
		pthread_mutex_init(&mWaitMutex, NULL);
		pthread_cond_init(&mWaitCond, NULL);
		pthread_mutex_init(&mWaitForStopMutex, NULL);
		pthread_cond_init(&mWaitForStopCond, NULL);
	}
	~PlayerBackend()
	{
		if (pErrorMessage != NULL)
			delete pErrorMessage;
		if (pCurrentVideo != NULL)
			delete pCurrentVideo;
		if (pCurrentAudio != NULL)
			delete pCurrentAudio;
		if (pCurrentSubtitle != NULL)
			delete pCurrentSubtitle;
		stop();
		pthread_mutex_destroy(&mWaitMutex);
		pthread_cond_destroy(&mWaitCond);
		pthread_mutex_destroy(&mWaitForStopMutex);
		pthread_cond_destroy(&mWaitForStopCond);
	}
	int start(const std::string& path, const std::map<std::string,std::string>& headers);
	int stop();
	int pause();
	int resume();
	int seekTo(int seconds);
	int seekRelative(int seconds);
	int getLength(int& mseconds);
	int getPlayPosition(int& mseconds);
	int getErrorMessage(errorMessage& error);
	int getSubtitles(std::queue<subtitleMessage>&);
	int audioGetNumberOfTracks(int timeout=0);
	int audioSelectTrack(int trackId);
	int audioGetTrackInfo(audioStream& trackInfo, int trackId);
	int audioGetCurrentTrackNum();
	int subtitleGetNumberOfTracks(int timeout=0);
	int subtitleSelectTrack(int trackId);
	int subtitleGetTrackInfo(subtitleStream& trackInfo, int trackId);
	int subtitleGetCurrentTrackNum();
	int videoGetTrackInfo(videoStream& trackInfo, int trackId);

	PSignal1<void,int> gotPlayerMessage;
};

class WaitThread: public eThread
{
	bool& waitForUpdate;
	pthread_mutex_t& mutex;
	pthread_cond_t& cond;
	long timeout;
	void thread();
	bool timedOut;
public:
	WaitThread(pthread_mutex_t& mutex, pthread_cond_t& cond, bool& waitForUpdate, long timeout):
		waitForUpdate(waitForUpdate),
		mutex(mutex),
		cond(cond),
		timeout(timeout),
		timedOut(false)
	{
	}
	bool isTimedOut(){return timedOut;}
};

#endif
