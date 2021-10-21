#include <sstream>

#include <lib/base/eerror.h>
#include "exteplayer3.h"

const std::string  EXT3_SW_DECODING_AAC            = "aac_swdec";
const std::string  EXT3_SW_DECODING_AC3            = "ac3_swdec";
const std::string  EXT3_SW_DECODING_EAC3           = "eac3_swdec";
const std::string  EXT3_SW_DECODING_DTS            = "dts_swdec";
const std::string  EXT3_SW_DECODING_MP3            = "mp3_swdec";
const std::string  EXT3_SW_DECODING_WMA            = "wma_swdec";
const std::string  EXT3_DOWNMIX                    = "downmix";
const std::string  EXT3_LPCM_INJECTION             = "lpcm_injection";
const std::string  EXT3_NO_PCM_RESAMPLING          = "no_pcm_resampling";
const std::string  EXT3_FLV2MPEG4_CONVERTER        = "flv2mpeg4";
const std::string  EXT3_PLAYBACK_PROGRESSIVE       = "progressive";
const std::string  EXT3_PLAYBACK_INIFITY_LOOP      = "loop";
const std::string  EXT3_PLAYBACK_LIVETS            = "live_ts";
const std::string  EXT3_PLAYBACK_AUDIO_TRACK_ID    = "audio_id";
const std::string  EXT3_PLAYBACK_SUBTITLE_TRACK_ID = "subtitle_id";
const std::string  EXT3_PLAYBACK_AUDIO_URI         = "audio_uri";
const std::string  EXT3_PLAYBACK_DASH_VIDEO_ID     = "dash_video_id";
const std::string  EXT3_PLAYBACK_DASH_AUDIO_ID     = "dash_audio_id";
const std::string  EXT3_PLAYBACK_MPEGTS_PROGRAM    = "mpegts_program_id";
const std::string  EXT3_RTMP_PROTOCOL              = "rtmpproto";
const std::string  EXT3_NICE_VALUE                 = "nice";
const std::string  EXT3_FFMPEG_SETTING_STRING      = "ffmpeg_option";

SettingMap &ExtEplayer3Options::GetSettingMap()
{
	return settingMap;
}

ExtEplayer3Options::ExtEplayer3Options()
{
	settingMap[EXT3_SW_DECODING_AAC]            = SettingEntry ("-a", "int");
	settingMap[EXT3_SW_DECODING_EAC3]           = SettingEntry ("-e", "bool");
	settingMap[EXT3_SW_DECODING_AC3]            = SettingEntry ("-3", "bool");
	settingMap[EXT3_SW_DECODING_DTS]            = SettingEntry ("-d", "bool");
	settingMap[EXT3_SW_DECODING_MP3]            = SettingEntry ("-m", "bool");
	settingMap[EXT3_SW_DECODING_WMA]            = SettingEntry ("-w", "bool");
	settingMap[EXT3_LPCM_INJECTION]             = SettingEntry ("-l", "bool");
	settingMap[EXT3_DOWNMIX]                    = SettingEntry ("-s", "bool");
	settingMap[EXT3_NO_PCM_RESAMPLING]          = SettingEntry ("-r", "bool");
	settingMap[EXT3_FLV2MPEG4_CONVERTER]        = SettingEntry ("-4", "bool");
	settingMap[EXT3_PLAYBACK_INIFITY_LOOP]      = SettingEntry ("-i", "bool");
	settingMap[EXT3_PLAYBACK_LIVETS]            = SettingEntry ("-v", "bool");
	settingMap[EXT3_RTMP_PROTOCOL]              = SettingEntry ("-n", "int");
	settingMap[EXT3_PLAYBACK_PROGRESSIVE]       = SettingEntry ("-o", "bool");
	settingMap[EXT3_NICE_VALUE]                 = SettingEntry ("-p", "int");
	settingMap[EXT3_PLAYBACK_MPEGTS_PROGRAM]    = SettingEntry ("-P", "int");
	settingMap[EXT3_PLAYBACK_AUDIO_TRACK_ID]    = SettingEntry ("-t", "int");
	settingMap[EXT3_PLAYBACK_SUBTITLE_TRACK_ID] = SettingEntry ("-9", "int");
	settingMap[EXT3_PLAYBACK_AUDIO_URI]         = SettingEntry ("-x", "string");
	settingMap[EXT3_PLAYBACK_DASH_VIDEO_ID]     = SettingEntry ("-0", "int");
	settingMap[EXT3_PLAYBACK_DASH_AUDIO_ID]     = SettingEntry ("-1", "int");
	settingMap[EXT3_FFMPEG_SETTING_STRING]      = SettingEntry ("-f", "string");
}

int ExtEplayer3Options::update(const std::string &key, const std::string &val)
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
				eWarning("ExtEplayer3Options::update - invalid value '%s' for '%s' setting, allowed values are 0|1", key.c_str(), val.c_str());
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
				if (key == EXT3_SW_DECODING_AAC || key == EXT3_RTMP_PROTOCOL)
				{
					if (intval < 3)
					{
						entry.setValue(intval);
					}
					else
					{
						eWarning("ExtEplayer3Options::update - invalid value '%s' for '%s' setting, allowed values <0,2>", val.c_str(), key.c_str());
						ret = -2;
					}
				}
				else
				{
					entry.setValue(intval);
				}
			}
			else
			{
				eWarning("ExtEplayer3Options::update - invalid value '%s' for '%s' setting, allowed values are >= 0", val.c_str(), key.c_str());
				ret = -2;
			}
		}
		else if (entry.getType() == "string")
		{
		}
	}
	else
	{
		eWarning("ExtEplayer3Options::update - not recognized setting '%s'", key.c_str());
		ret = -1;
	}
	return ret;
}

void ExtEplayer3Options::print() const
{
	for (SettingIter it(settingMap.begin()); it != settingMap.end(); it++)
	{
		eDebug(" %-30s = %s", it->first.c_str(), it->second.toString().c_str());
	}
}

ExtEplayer3::ExtEplayer3(ExtEplayer3Options& options): PlayerApp(STD_ERROR)
{
	mPlayerOptions = options;
	eDebug("ExtEplayer3::ExtEplayer3 initializing with options:");
	mPlayerOptions.print();
}

std::vector<std::string> ExtEplayer3::buildCommand()
{
	// TODO add all options
	std::vector<std::string> args;
	args.push_back("exteplayer3");
	size_t pos = mPath.find("&suburi=");
	if (pos != std::string::npos)
	{
		args.push_back(mPath.substr(0, pos));
		args.push_back("-x");
		args.push_back(mPath.substr(pos + 8));
	}
	else
	{
		args.push_back(mPath);
	}
	std::map<std::string,std::string>::const_iterator i(mHeaders.find("User-Agent"));
	if (i != mHeaders.end())
	{
		args.push_back("-u");
		args.push_back(i->second);
	}
	std::string headersStr;
	for (std::map<std::string,std::string>::const_iterator i(mHeaders.begin()); i != mHeaders.end(); i++)
	{
		if (i->first.compare("User-Agent") == 0)
			continue;
		headersStr +=  i->first + ":" + i->second + "\r\n";
	}
	if (!headersStr.empty())
	{
		args.push_back("-h");
		args.push_back(headersStr);
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
int ExtEplayer3::start(eMainloop *context)
{
	return processStart(context);
}


int ExtEplayer3::sendStop(){ return processSend(std::string("q\n"));}
int ExtEplayer3::sendForceStop(){ processKill(); return 0;}
int ExtEplayer3::sendPause(){ return processSend(std::string("p\n"));}
int ExtEplayer3::sendResume(){ return processSend(std::string("c\n"));}
int ExtEplayer3::sendUpdateLength(){ return processSend(std::string("l\n"));}
int ExtEplayer3::sendUpdatePosition(){ return processSend(std::string("j\n"));}
int ExtEplayer3::sendUpdateAudioTracksList(){ return processSend(std::string("al\n"));}
int ExtEplayer3::sendUpdateAudioTrackCurrent(){ return processSend(std::string("ac\n"));}

int ExtEplayer3::sendAudioSelectTrack(int trackId)
{
	std::stringstream sstm;
	sstm << "a" << trackId << std::endl;
	return processSend(sstm.str());
}

int ExtEplayer3::sendUpdateSubtitleTracksList(){ return processSend(std::string("sl\n"));}
int ExtEplayer3::sendUpdateSubtitleTrackCurrent(){ return processSend(std::string("sc\n"));}

int ExtEplayer3::sendSubtitleSelectTrack(int trackId)
{
	std::stringstream sstm;
	sstm << "s" << trackId << std::endl;
	return processSend(sstm.str());
}

int ExtEplayer3::sendSeekTo(int seconds)
{
	std::stringstream sstm;
	sstm << "gc" << seconds << std::endl;
	return processSend(sstm.str());
}

int ExtEplayer3::sendSeekRelative(int seconds)
{
	std::stringstream sstm;
	sstm << "kc" << seconds << std::endl;
	return processSend(sstm.str());
}

void ExtEplayer3::handleProcessStopped(int retval)
{
	recvStopped(0);
}

void ExtEplayer3::handleJsonOutput(cJSON *json)
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
			recvStarted(0);
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
		recvAudioTrackSelected(1, 0);
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
	else if (!strcmp(key, "s_a"))
	{
		subtitleMessage s;
		s.start_ms = cJSON_GetObjectItem(value, "s")->valueint;
		s.end_ms = cJSON_GetObjectItem(value, "e")->valueint;
		s.duration_ms = s.end_ms - s.start_ms;
		s.text = cJSON_GetObjectItem(value, "t")->valuestring;
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
		if (cJSON_GetObjectItem(value, "sts")->valueint)
		{
		}
	}
	else if (!strcmp(key, "PLAYBACK_SEEK_ABS"))
	{
		if (!cJSON_GetObjectItem(value, "sts")->valueint)
		{
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
		eDebug("ExtEPlayer3::handleJsonOutput - unhandled key \"%s\"", key);
	}
}
