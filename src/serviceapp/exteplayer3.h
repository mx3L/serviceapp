#ifndef __exteplayer3_h
#define __exteplayer3_h
#include <lib/base/eerror.h>
#include "extplayer.h"
#include "common.h"
#include <map>

extern const std::string EXT3_SW_DECODING_AAC;
extern const std::string EXT3_SW_DECODING_AC3;
extern const std::string EXT3_SW_DECODING_EAC3;
extern const std::string EXT3_SW_DECODING_DTS;
extern const std::string EXT3_SW_DECODING_MP3;
extern const std::string EXT3_SW_DECODING_WMA;
extern const std::string EXT3_DOWNMIX;
extern const std::string EXT3_LPCM_INJECTION;
extern const std::string EXT3_NO_PCM_RESAMPLING;
extern const std::string EXT3_FLV2MPEG4_CONVERTER;
extern const std::string EXT3_PLAYBACK_PROGRESSIVE;
extern const std::string EXT3_PLAYBACK_INIFITY_LOOP;
extern const std::string EXT3_PLAYBACK_LIVETS;
extern const std::string EXT3_PLAYBACK_AUDIO_TRACK_ID;
extern const std::string EXT3_PLAYBACK_SUBTITLE_TRACK_ID;
extern const std::string EXT3_PLAYBACK_AUDIO_URI;
extern const std::string EXT3_PLAYBACK_DASH_VIDEO_ID;
extern const std::string EXT3_PLAYBACK_DASH_AUDIO_ID;
extern const std::string EXT3_PLAYBACK_MPEGTS_PROGRAM;
extern const std::string EXT3_RTMP_PROTOCOL;
extern const std::string EXT3_NICE_VALUE;
extern const std::string EXT3_FFMPEG_SETTING_STRING;

struct ExtEplayer3Options : public IOption
{
	SettingMap settingMap;
	SettingMap &GetSettingMap();

	ExtEplayer3Options();
	void print() const;
	int update(const std::string&, const std::string&);
};

class ExtEplayer3: public PlayerApp, public BasePlayer
{
	ExtEplayer3Options mPlayerOptions;
	void handleProcessStopped(int retval);
	void handleJsonOutput(cJSON* json);
	std::vector<std::string> buildCommand();
	public:
	ExtEplayer3(ExtEplayer3Options& options);
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
