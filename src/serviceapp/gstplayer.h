#ifndef __gstplayer_h
#define __gstplayer_h
#include <lib/base/eerror.h>
#include "extplayer.h"


struct GstPlayerOptions
{
	int bufferSize; // in kB
	int bufferDuration; // in ms
	std::string videoSink; // custom sink name
	std::string audioSink; // custom sink name
	bool subtitleEnabled;
	GstPlayerOptions(): bufferSize(8*1024), bufferDuration(0), subtitleEnabled(true){};
};

class GstPlayer: public PlayerApp, public BasePlayer
{
	GstPlayerOptions mPlayerOptions;
	void handleJsonOutput(cJSON* json);
	void handleProcessStopped(int retval);
	std::string buildCommand();
public:
	GstPlayer(GstPlayerOptions& options): PlayerApp(STD_ERROR) 
	{
		eDebug("GstPlayer");
		mPlayerOptions = options;
	}
	~GstPlayer()
	{
		eDebug("~GstPlayer");
	}
	int start(eMainloop *context);
	int sendStop();
	int sendForceStop();
	int sendPause();
	int sendResume();
	int sendUpdateLength();
	int sendUpdatePosition();
	int sendUpdateAudioTracksList();
	int sendUpdateAudioTrackCurrent();
	int sendAudioSelectTrack(int trackId);
	int sendUpdateSubtitleTracksList();
	int sendUpdateSubtitleTrackCurrent();
	int sendSubtitleSelectTrack(int trackId);
	int sendSeekTo(int seconds);
	int sendSeekRelative(int seconds);
};
#endif
