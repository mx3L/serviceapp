#ifndef __gstplayer_h
#define __gstplayer_h
#include <cstdlib>
#include <lib/base/eerror.h>
#include "extplayer.h"
#include "common.h"

extern const std::string GST_DOWNLOAD_BUFFER_PATH;
extern const std::string GST_RING_BUFFER_MAXSIZE;
extern const std::string GST_BUFFER_SIZE;
extern const std::string GST_BUFFER_DURATION;
extern const std::string GST_VIDEO_SINK;
extern const std::string GST_AUDIO_SINK;
extern const std::string GST_AUDIO_TRACK_IDX;
extern const std::string GST_SUBTITLE_ENABLED;

class GstPlayerOptions : public IOption
{
public:
	GstPlayerOptions();
	SettingMap &GetSettingMap();

	int update(const std::string &, const std::string &);
	void print() const;
private:
	SettingMap settingMap;
};

class GstPlayer: public PlayerApp, public BasePlayer
{
	GstPlayerOptions mPlayerOptions;
	void handleJsonOutput(cJSON* json);
	void handleProcessStopped(int retval);
	std::vector<std::string> buildCommand();
public:
	GstPlayer(GstPlayerOptions& options);
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
