#include <sstream>

#include <lib/base/eerror.h>
#include "gstplayer.h"

const std::string GST_DOWNLOAD_BUFFER_PATH = "download_buffer_path";
const std::string GST_RING_BUFFER_MAXSIZE  = "ring_buffer_maxsize";
const std::string GST_BUFFER_SIZE          = "buffer_size";
const std::string GST_BUFFER_DURATION      = "buffer_duration";
const std::string GST_VIDEO_SINK           = "video_sink";
const std::string GST_AUDIO_SINK           = "audio_sink";
const std::string GST_AUDIO_TRACK_IDX      = "audio_id";
const std::string GST_SUBTITLE_ENABLED     = "subtitles_enabled";

GstPlayerOptions::GstPlayerOptions()
{
	settingMap[GST_DOWNLOAD_BUFFER_PATH] = SettingEntry("-p", "string");
	settingMap[GST_RING_BUFFER_MAXSIZE]  = SettingEntry("-r", "int");
	settingMap[GST_BUFFER_SIZE]          = SettingEntry("-s", 8*1024, "int");
	settingMap[GST_BUFFER_DURATION]      = SettingEntry("-d", 0, "int");
	settingMap[GST_VIDEO_SINK]           = SettingEntry("-v", "string");
	settingMap[GST_AUDIO_SINK]           = SettingEntry("-a", "string");
	settingMap[GST_AUDIO_TRACK_IDX]      = SettingEntry("-i", "int");
	settingMap[GST_SUBTITLE_ENABLED]     = SettingEntry("-e", true, "bool");
}

SettingMap &GstPlayerOptions::GetSettingMap()
{
	return settingMap;
}

int GstPlayerOptions::update(const std::string &key, const std::string &val)
{
	int ret = 0;
	if (settingMap.find(key) != settingMap.end())
	{
		SettingEntry &entry = settingMap[key];
		if (entry.getType() == "bool")
		{
			if (val == "1")
				entry.setValue(1);
			else if (val == "0")
				entry.setValue(0);
			else
			{
				eWarning("GstPlayerOptions::update - invalid value '%s' for '%s' setting, allowed values are 0|1", key.c_str(), val.c_str());
				ret = -2;
			}
		}
		else if (entry.getType() == "int")
		{
			char *endptr = NULL;
			int intval = -1;
			intval = strtol(val.c_str(), &endptr , 10);
			if (!*endptr && intval >= 0)
			{
				entry.setValue(intval);
			}
			else
			{
				eWarning("GstPlayerOptions::update - invalid value '%s' for '%s' setting, allowed values are >= 0", val.c_str(), key.c_str());
				ret = -2;
			}
		}
		else if (entry.getType() == "string")
		{
			if (val.empty())
			{
				eWarning("GstPlayerOptions::update - empty string for '%s' setting", key.c_str());
				ret = -2;
			}
			else
			{
				entry.setValue(val);
			}
		}
	}
	else
	{
		eWarning("GstPlayerOptions::update - not recognized setting '%s'", key.c_str());
		ret = -1;
	}
	return ret;
}

void GstPlayerOptions::print() const
{
	for (SettingIter it(settingMap.begin()); it != settingMap.end(); it++)
	{
		std::stringstream ss;
		ss << " %-30s = %s";
		if (it->first == GST_BUFFER_SIZE)
		{
			ss << "KB";
		}
		else if (it->first == GST_BUFFER_DURATION)
		{
			ss << "s";
		}
		eDebug(ss.str().c_str(), it->first.c_str(), it->second.toString().c_str());
	}
}

GstPlayer::GstPlayer(GstPlayerOptions& options): PlayerApp(STD_ERROR)
{
	mPlayerOptions = options;
	eDebug("GstPlayer::GstPlayer initializing with options:");
	mPlayerOptions.print();
}

std::vector<std::string> GstPlayer::buildCommand()
{
	std::vector<std::string> args;
	args.push_back("gstplayer_gst-1.0");
	args.push_back(mPath);
	for (std::map<std::string,std::string>::const_iterator i(mHeaders.begin()); i!=mHeaders.end(); i++)
	{
		args.push_back("-H");
		args.push_back(i->first + "=" + i->second);
	}
	for (SettingIter i(mPlayerOptions.GetSettingMap().begin()); i != mPlayerOptions.GetSettingMap().end(); i++)
	{
		if (!i->second.isSet())
		{
			continue;
		}
		if (i->second.getType() == "bool" && i->second.getValueInt())
		{
			args.push_back(i->second.getAppArg());
		}
		if (i->second.getType() == "int" || i->second.getType() == "string")
		{
			std::stringstream ss;
			ss << i->second.getAppArg();
			ss << " ";
			ss << i->second.getValue();
			args.push_back(ss.str());
		}
	}

	return args;
}
int GstPlayer::start(eMainloop* context)
{
	return processStart(context);
}

int GstPlayer::sendStop(){ return processSend(std::string("q\n"));}
int GstPlayer::sendForceStop(){ processKill(); return 0;}
int GstPlayer::sendPause(){ return processSend(std::string("p\n"));}
int GstPlayer::sendResume(){ return processSend(std::string("c\n"));}
int GstPlayer::sendUpdateLength(){ return processSend(std::string("l\n"));}
int GstPlayer::sendUpdatePosition(){ return processSend(std::string("j\n"));}
int GstPlayer::sendUpdateAudioTracksList(){ return processSend(std::string("al\n"));}
int GstPlayer::sendUpdateAudioTrackCurrent(){ return processSend(std::string("ac\n"));}

int GstPlayer::sendAudioSelectTrack(int trackId)
{
	std::stringstream sstm;
	sstm << "a" << trackId << std::endl;
	return processSend(sstm.str());
}

int GstPlayer::sendUpdateSubtitleTracksList(){ return processSend(std::string("sl\n"));}
int GstPlayer::sendUpdateSubtitleTrackCurrent(){ return processSend(std::string("sc\n"));}

int GstPlayer::sendSubtitleSelectTrack(int trackId)
{
	std::stringstream sstm;
	sstm << "s" << trackId << std::endl;
	return processSend(sstm.str());
}

int GstPlayer::sendSeekTo(int seconds)
{
	std::stringstream sstm;
	sstm << "gc" << seconds << std::endl;
	return processSend(sstm.str());
}

int GstPlayer::sendSeekRelative(int seconds)
{
	std::stringstream sstm;
	sstm << "kc" << seconds << std::endl;
	return processSend(sstm.str());
}

void GstPlayer::handleProcessStopped(int retval)
{
	recvStopped(0);
}

void GstPlayer::handleJsonOutput(cJSON *json)
{
	if (!json->child)
	{
		return;
	}
	const char *key = json->child->string;
	cJSON* value = cJSON_GetObjectItem(json, key);

	if (!strcmp(key, "PLAYBACK_PLAY"))
	{
		if (!cJSON_GetObjectItem(value, "sts")->valueint)
		{
			recvStarted(0);
		}
	}
	else if (!strcmp(key, "PLAYBACK_INFO"))
	{
//		int isPlaying = cJSON_GetObjectItem(value, "isPlaying")->valueint;
//		int isPaused = cJSON_GetObjectItem(value, "isPaused")->valueint;
//		int isForwarding = cJSON_GetObjectItem(value, "isForwarding")->valueint;
//		int isSeeking = cJSON_GetObjectItem(value, "isSeeking")->valueint;
//		int isCreatingPhase = cJSON_GetObjectItem(value, "isCreatingPhase")->valueint;
//		float backWard = cJSON_GetObjectItem(value, "BackWard")->valuedouble;
//		int slowMotion = cJSON_GetObjectItem(value, "SlowMotion")->valueint;
//		int speed = cJSON_GetObjectItem(value, "Speed")->valueint;
//		int avSync = cJSON_GetObjectItem(value, "AVSync")->valueint;
//		int isAudio = cJSON_GetObjectItem(value, "isAudio")->valueint;
//		int isVideo = cJSON_GetObjectItem(value, "isVideo")->valueint;
//		int isSubtitle = cJSON_GetObjectItem(value, "isSubtitle")->valueint;
	}
	else if (!strcmp(key, "v_c"))
	{
		videoStream v;
		v.id = cJSON_GetObjectItem(value, "id")->valueint;
		v.description = cJSON_GetObjectItem(value, "e")->valuestring;
		v.language_code = cJSON_GetObjectItem(value, "n")->valuestring;
		v.width = cJSON_GetObjectItem(value, "w")->valueint;
		v.height = cJSON_GetObjectItem(value, "h")->valueint;
		v.framerate = cJSON_GetObjectItem(value, "f")->valueint;
		
		// this would crash if somebody was using older version
		// of gstplayer where progressive was not passed
		cJSON *progressive = cJSON_GetObjectItem(value, "p");
		if (progressive != NULL)
		{
			v.progressive = progressive->valueint;
		}
		recvVideoTrackCurrent(0, v);
	}
	else if (!strcmp(key, "a_s"))
	{
		if (!cJSON_GetObjectItem(value, "sts")->valueint)
		{
			int s = cJSON_GetObjectItem(value, "id")->valueint;
			recvAudioTrackSelected(0, s);
			return;
		}
		recvAudioTrackSelected(1, -1);
	}
	else if (!strcmp(key, "a_c"))
	{
		audioStream a;
		a.id = cJSON_GetObjectItem(value, "id")->valueint;
		a.description = cJSON_GetObjectItem(value, "e")->valuestring;
		a.language_code = cJSON_GetObjectItem(value, "n")->valuestring;
		recvAudioTrackCurrent(0, a);
	}
	else if (!strcmp(key, "a_l"))
	{
		std::vector<audioStream> streams;
		for (int i=0; i<cJSON_GetArraySize(value); i++)
		{
			cJSON *subitem=cJSON_GetArrayItem(value,i);
			audioStream a;
			a.id = cJSON_GetObjectItem(subitem, "id")->valueint; 
			a.description = cJSON_GetObjectItem(subitem, "e")->valuestring;
			a.language_code = cJSON_GetObjectItem(subitem, "n")->valuestring;
			streams.push_back(a);
		}
		recvAudioTracksList(0, streams);
	}
	else if (!strcmp(key, "s_s"))
	{
		if (!cJSON_GetObjectItem(value, "sts")->valueint)
		{
			int s = cJSON_GetObjectItem(value, "id")->valueint;
			recvSubtitleTrackSelected(0, s);
			return;
		}
		recvSubtitleTrackSelected(1, -1);
	}
	else if (!strcmp(key, "s_c"))
	{
		subtitleStream s;
		s.id = cJSON_GetObjectItem(value, "id")->valueint;
		s.description = cJSON_GetObjectItem(value, "e")->valuestring;
		s.language_code = cJSON_GetObjectItem(value, "n")->valuestring;
		recvSubtitleTrackCurrent(0, s);
	}
	else if (!strcmp(key, "s_l"))
	{
		std::vector<subtitleStream> streams;
		for (int i=0; i<cJSON_GetArraySize(value); i++)
		{
			cJSON *subitem=cJSON_GetArrayItem(value,i);
			subtitleStream s;
			s.id = cJSON_GetObjectItem(subitem, "id")->valueint; 
			s.description = cJSON_GetObjectItem(subitem, "e")->valuestring;
			s.language_code = cJSON_GetObjectItem(subitem, "n")->valuestring;
			streams.push_back(s);
		}
		recvSubtitleTracksList(0, streams);
	}
	else if (!strcmp(key, "PLAYBACK_SUBTITLE"))
	{
		subtitleMessage s;
		s.start_ms = cJSON_GetObjectItem(value, "start")->valueint;
		s.duration_ms = cJSON_GetObjectItem(value, "duration")->valueint;
		s.end_ms = s.start_ms + s.duration_ms;
		s.text = cJSON_GetObjectItem(value, "text")->valuestring;
		recvSubtitleMessage(s);
	}
	else if (!strcmp(key, "PLAYBACK_LENGTH"))
	{
		if (!cJSON_GetObjectItem(value, "sts")->valueint)
		{
			float l = cJSON_GetObjectItem(value, "length")->valuedouble;
			recvLength(0, l * 1000);
		}
	}
	else if (!strcmp(key, "J"))
	{
		int positionInMs = cJSON_GetObjectItem(value, "ms")->valueint;
		recvPosition(0, positionInMs);
	}
	else if (!strcmp(key, "GST_ERROR"))
	{
		errorMessage e;
		e.message = cJSON_GetObjectItem(value, "msg")->valuestring;
		//e.code = cJSON_GetObjectItem(value, "code")->valueint;
		recvErrorMessage(e);
	}
	else if (!strcmp(key, "GST_MISSING_PLUGIN"))
	{
		errorMessage e;
		e.message = "GStreamer plugin ";
		e.message += cJSON_GetObjectItem(value, "msg")->valuestring;
		e.message += " is not available!";
		recvErrorMessage(e);
	}
	else if (!strcmp(key, "PLAYBACK_STOP"))
	{
		if (!cJSON_GetObjectItem(value, "sts")->valueint)
		{
			//recvStopped(0);
			return;
		}
		//recvStopped(1);
	}
	else if (!strcmp(key, "PLAYBACK_CONTINUE"))
	{
		if (!cJSON_GetObjectItem(value, "sts")->valueint)
		{
			recvResumed(0);
			return;
		}
		recvResumed(1);
	}
	else if (!strcmp(key, "PLAYBACK_PAUSE"))
	{
		if (!cJSON_GetObjectItem(value, "sts")->valueint)
		{
			recvPaused(0);
			return;
		}
		recvPaused(1);
	}
	else if (!strcmp(key, "PLAYBACK_FASTFORWARD"))
	{
		if (!cJSON_GetObjectItem(value, "sts")->valueint)
		{
		}
	}
	else if (!strcmp(key, "PLAYBACK_SEEK_ABS"))
	{
		if (!cJSON_GetObjectItem(value, "sts")->valueint)
		{
			//FIXME
			recvSeekTo(0, 0);
			return;
		}
		recvSeekTo(1, 0);
	}
	else if (!strcmp(key, "PLAYBACK_SEEK"))
	{
		if (!cJSON_GetObjectItem(value, "sts")->valueint)
		{
			recvSeekRelative(0, 0);
			return;
		}
		recvSeekRelative(1, 0);
	}
	else
	{
		eDebug("GstPlayer::handleJsonOutput - unhandled key \"%s\"", key);
	}
}
