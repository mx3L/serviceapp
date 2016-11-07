#ifndef __exteplayer3_h
#define __exteplayer3_h
#include <lib/base/eerror.h>
#include "extplayer.h"

struct ExtEplayer3Options
{
	bool aacSwDecoding;
	bool dtsSwDecoding;
	bool wmaSwDecoding;
	bool lpcmInjection;
	bool downmix;
	ExtEplayer3Options():
		aacSwDecoding(false),
		dtsSwDecoding(false),
		wmaSwDecoding(false),
		lpcmInjection(false),
		downmix(false) {};
};

class ExtEplayer3: public PlayerApp, public BasePlayer
{
	ExtEplayer3Options mPlayerOptions;
	void handleProcessStopped(int retval);
	void handleJsonOutput(cJSON* json);
	std::vector<std::string> buildCommand();
public:
	ExtEplayer3(ExtEplayer3Options& options): PlayerApp(STD_ERROR) 
	{
		mPlayerOptions = options;
	}
	~ExtEplayer3()
	{
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
