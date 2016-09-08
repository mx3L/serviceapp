#include <sstream>

#include <lib/base/eerror.h>
#include "exteplayer3.h"


std::string ExtEplayer3::buildCommand()
{
	// TODO add all options
	std::string cmd("exteplayer3");
	bool addquote = false;
	cmd += " \"" + mPath + "\"";
	if (!mHeaders.empty())
	{
		std::map<std::string,std::string>::const_iterator i(mHeaders.find("User-Agent"));
		if (i != mHeaders.end())
		{
			cmd += " -u \"" + i->second + "\"";
			if (mHeaders.size() > 1)
			{
				cmd += " -h \"";
				addquote = true;
			}
		}
		else
		{
			cmd += " -h \"";
			addquote = true;
		}
	}
	for (std::map<std::string,std::string>::const_iterator i(mHeaders.begin()); i != mHeaders.end(); i++)
	{
		if (i->first.compare("User-Agent") == 0)
			continue;
		cmd +=  i->first + ":" + i->second + "\r\n";
	}
	if (addquote)
		cmd += "\"";
	if (mPlayerOptions.aacSwDecoding)
		cmd += " -a ";
	if (mPlayerOptions.dtsSwDecoding)
		cmd += " -d ";
	if (mPlayerOptions.wmaSwDecoding)
		cmd += " -w ";
	if (mPlayerOptions.lpcmInjection)
		cmd += " -l ";
	if (mPlayerOptions.downmix)
		cmd += " -s ";
	return cmd;
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
