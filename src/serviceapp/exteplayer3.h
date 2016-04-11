#ifndef __exteplayer3_h
#define __exteplayer3_h
#include <lib/base/eerror.h>
#include "extplayer.h"

class ExtEplayer3: public PlayerApp, public BasePlayer
{
	void handleProcessStopped(int retval);
	void handleJsonOutput(cJSON* json);
	std::string buildCommand();
public:
	ExtEplayer3(): PlayerApp(STD_ERROR) 
	{
		eDebug("ExtEplayer3");
	}
	~ExtEplayer3()
	{
		eDebug("~ExtEplayer3");
	}
	int start(eMainloop *context);
	
	int sendStop();
	int sendPause();
	int sendResume();
	int sendUpdateLength();
	int sendUpdatePosition();
	int sendUpdateAudioTracksList();
	int sendUpdateAudioTrackCurrent();
	int sendAudioSelectTrack(int trackId);
	int sendSeekTo(int seconds);
	int sendSeekRelative(int seconds);
};
#endif
