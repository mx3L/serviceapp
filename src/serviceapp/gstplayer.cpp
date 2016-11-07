#include <sstream>

#include <lib/base/eerror.h>
#include "gstplayer.h"


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
	if (!mPlayerOptions.videoSink.empty())
	{
		args.push_back("-v");
		args.push_back(mPlayerOptions.videoSink);
	}
	if (!mPlayerOptions.audioSink.empty())
	{
		args.push_back("-a");
		args.push_back(mPlayerOptions.audioSink);
	}
	if (mPlayerOptions.subtitleEnabled)
	{
		args.push_back("-e");
	}
	if (mPlayerOptions.bufferDuration >= 0)
	{
		std::stringstream sstm;
		sstm << mPlayerOptions.bufferDuration; 
		args.push_back("-d");
		args.push_back(sstm.str());
	}
	if (mPlayerOptions.bufferSize >= 0)
	{
		std::stringstream sstm;
		sstm << mPlayerOptions.bufferSize; 
		args.push_back("-s");
		args.push_back(sstm.str());
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
