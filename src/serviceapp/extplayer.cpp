#include <time.h>
#include <extplayer.h>
#include <cJSON/cJSON.h>
#include <error.h>

void PlayerApp::handleJsonStr(const std::string& data)
{
	eLog(5, "PlayerApp::handleJsonStr: %s", data.c_str());
	cJSON *json = cJSON_Parse(data.c_str());
	if (!json)
	{
		eDebug("Error before: [%s]", cJSON_GetErrorPtr());
		return;
	}
	handleJsonOutput(json);
	cJSON_Delete(json);
}
void PlayerApp::handleOutput(const std::string& mydata)
{
	//FIXME
	std::size_t pos = 0;
	std::size_t startpos = 0;

	while((pos = mydata.find('\n', pos)) != std::string::npos)
	{
		if (truncated)
		{
			if (mydata[pos-1] != '}')
			{
				jsonstr = "";
				truncated = 0;
				return;
			}
			handleJsonStr(jsonstr + mydata.substr(startpos, pos - startpos));
			jsonstr = "";
			truncated = 0;
		}
		else
		{
			if (mydata[0] != '{')
			{
				jsonstr = "";
				truncated = 0;
				return;
			}
			handleJsonStr(mydata.substr(startpos, pos - startpos));
			jsonstr = "";
			truncated = 0;
		}
		pos+=1;
		startpos = pos;
	}
	if (startpos != std::string::npos && startpos != mydata.length())
	{
		//std::cout << "remains: " << mydata.length()-startpos;
		if (mydata[mydata.length()-1] == '}')
		{
			handleJsonStr(mydata.substr(startpos, mydata.length() - pos));
			truncated = 0;
		}
		else
		{
			truncated = 1;
			jsonstr = mydata.substr(startpos);
		}
	}
}

void PlayerApp::stderrAvail(const char *data)
{
	std::string mydata(data);
	eLog(5, "PlayerApp::stderrAvail: %s", mydata.c_str());
	if (parseOutput == STD_ERROR)
	{
		handleOutput(mydata);
	}
}

void PlayerApp::stdoutAvail(const char *data)
{
	std::string mydata(data);
	eLog(5, "PlayerApp::stdoutAvail: %s", mydata.c_str());
	if (parseOutput == STD_OUTPUT)
	{
		handleOutput(mydata);
	}
}

void PlayerApp::appClosed(int retval)
{
	handleProcessStopped(retval);
}

int PlayerApp::processStart(eMainloop *context)
{
	console = new eConsoleContainer();
	CONNECT(console->appClosed, PlayerApp::appClosed);
	CONNECT(console->stdoutAvail, PlayerApp::stdoutAvail);
	CONNECT(console->stderrAvail, PlayerApp::stderrAvail);
	const std::vector<std::string> args = buildCommand();
	eDebugNoNewLine("PlayerApp::processStart: ");
	char **cargs = (char **) malloc(sizeof(char *) * args.size()+1);
	for (size_t i=0; i <= args.size(); i++)
	{
		// execvp needs args array terminated with NULL
		if (i == args.size())
		{
			cargs[i] = NULL;
			eDebugNoNewLine("\n");
		}
		else
		{
			cargs[i] = strdup(args[i].c_str());
			if (i != 0 && cargs[i][0] != '-')
				eDebugNoNewLine("\"%s\" ", cargs[i]);
			else
				eDebugNoNewLine("%s ", cargs[i]);
		}
	}
	int ret = console->execute(context, cargs[0], cargs);
	for (size_t i=0; i < args.size(); i++)
		free(cargs[i]);
	free(cargs);
	return ret;
}

int PlayerApp::processSend(const std::string& data)
{
	if (console && console->running())
	{
		eLog(5, "sending command \"%s\" ", data.c_str());
		console->write(data.c_str(), data.length());
		return 0;
	}
	return -1;
}

void PlayerApp::processKill()
{
	if (console && console->running())
	{
		console->sendCtrlC();
	}
}

bool PlayerApp::processRunning()
{
	return console ? console->running() : false;
}


void WaitThread::thread()
{
	hasStarted();
	pthread_mutex_lock(&mutex);
	if (!waitForUpdate)
	{
		eDebug("WaitThread - not waiting");
		goto unlock;
	}
	struct timespec ts;
	if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
	{
		perror("WaitThread - cannot get clock:");
		goto unlock;
	}
	ts.tv_sec += timeout / 1000;
	ts.tv_nsec += (timeout % 1000) * 1000000;
	eDebug("WaitThread - waiting for %ldms", timeout);
	if (pthread_cond_timedwait(&cond, &mutex, &ts) == ETIMEDOUT)
	{
		eDebug("WaitThread - timed out");
		waitForUpdate = false;
		timedOut = true;
		goto unlock;
	}
	eDebug("WaitThread - in time\n");
unlock:
	pthread_mutex_unlock(&mutex);
}


void PlayerBackend::sendMessage(const Message& m, int timeout)
{
	if (timeout > 0)
	{
		mWaitForUpdate = true;
		WaitThread t(mWaitMutex, mWaitCond, mWaitForUpdate, timeout);
		t.run();
		mMessageThread.send(m);
		t.kill();
	}
	else
	{
		mWaitForUpdate = false;
		mMessageThread.send(m);
	}
}

void PlayerBackend::recvMessage()
{
	pthread_mutex_lock(&mWaitMutex);
	if (mWaitForUpdate)
	{
		mWaitForUpdate = false;
		pthread_cond_signal(&mWaitCond);
	}
	pthread_mutex_unlock(&mWaitMutex);
}

void PlayerBackend::_updatePosition()
{
	pPlayer->sendUpdatePosition();
}

int PlayerBackend::start(const std::string& path, const std::map<std::string,std::string>& headers)
{
	pPlayer->setPath(path);
	pPlayer->setHttpHeaders(headers);
	// start player only when mainloop in player thread has started
	mMessageThread.send(Message(Message::tStart));
	run();
	return 0;
}
int PlayerBackend::stop()
{
	if (mThreadRunning)
	{
		// wait 10 seconds for normal exit if timed out then kill process
		mWaitForStop = true;
		WaitThread t(mWaitForStopMutex, mWaitForStopCond, mWaitForStop, 10000);
		t.run();
		mMessageThread.send(Message(Message::tStop));
		t.kill();
		if (t.isTimedOut())
		{
			mMessageThread.send(Message(Message::tKill));
		}
	}
	kill();
	return 0;
}

int PlayerBackend::pause()
{
	if (!playbackStarted)
		return -1;
	mMessageThread.send(Message(Message::tPause));
	return 0;
}

int PlayerBackend::resume()
{
	if (!playbackStarted)
		return -1;
	mMessageThread.send(Message(Message::tResume));
	return 0;
}

int PlayerBackend::seekTo(int seconds)
{
	if (!playbackStarted)
		return -1;
	mMessageThread.send(Message(Message::tSeekTo, seconds));
	return 0;
}

int PlayerBackend::seekRelative(int seconds)
{
	if (!playbackStarted)
		return -1;
	mMessageThread.send(Message(Message::tSeekRelative, seconds));
	return 0;
}

int PlayerBackend::getPlayPosition(int& mseconds)
{
	if (!playbackStarted)
	{
		return -1;
	}
	if (!mPositionInMs) 
	{
		//mMessageThread.send(Message(Message::tGetPosition));
		return -2;
	}
	mseconds = mPositionInMs;
	return 0;
}

int PlayerBackend::getLength(int& mseconds)
{
	if (!playbackStarted)
	{
		return -1;
	}
	if (!mLengthInMs) 
	{
		mMessageThread.send(Message(Message::tGetLength));
		return -2;
	}
	mseconds = mLengthInMs;
	return 0;
}

int PlayerBackend::getErrorMessage(errorMessage& error)
{
	if (!playbackStarted || !pErrorMessage)
	{
		return -1;
	}
	error = *pErrorMessage;
	return 0;
}

int PlayerBackend::audioGetNumberOfTracks(int timeout)
{
	if (!mWaitForUpdate)
	{
		Message m (Message::tAudioList);
		sendMessage(m, timeout);
	}
	return mAudioStreams.size();
}

int PlayerBackend::audioGetCurrentTrackNum()
{
	int trackNum = 0, j=0;
	int trackId = pCurrentAudio ? pCurrentAudio->id : 0;
	for (std::vector<audioStream>::const_iterator i(mAudioStreams.begin()); i!=mAudioStreams.end(); i++, j++)
	{
		if (trackId == i->id)
		{
			trackNum = j;
			break;
		}
	}
	return trackNum;
}

int PlayerBackend::audioSelectTrack(int trackNum)
{
	if (trackNum >= 0 && trackNum < (int) mAudioStreams.size())
	{
		mMessageThread.send(Message(Message::tAudioSelect, mAudioStreams[trackNum].id));
		return 0;
	}
	return -1;
}

int PlayerBackend::audioGetTrackInfo(audioStream& trackInfo, int trackNum)
{
	if (trackNum >= 0 && trackNum < (int) mAudioStreams.size())
	{
		trackInfo = mAudioStreams[trackNum];
		return 0;
	}
	return -1;
}

int PlayerBackend::subtitleGetNumberOfTracks(int timeout)
{
	if (!mWaitForUpdate)
	{
		Message m(Message::tSubtitleList);
		sendMessage(m, timeout);
	}
	return mSubtitleStreams.size();
}

int PlayerBackend::subtitleGetCurrentTrackNum()
{
	int trackNum = 0, j=0;
	int trackId = pCurrentSubtitle ? pCurrentSubtitle->id : 0;
	for (std::vector<subtitleStream>::const_iterator i(mSubtitleStreams.begin()); i!=mSubtitleStreams.end(); i++, j++)
	{
		if (trackId == i->id)
		{
			trackNum = j;
			break;
		}
	}
	return trackNum;
}

int PlayerBackend::subtitleSelectTrack(int trackNum)
{
	if (trackNum >= 0 && trackNum < (int) mSubtitleStreams.size())
	{
		mMessageThread.send(Message(Message::tSubtitleSelect, mSubtitleStreams[trackNum].id));
		return 0;
	}
	return -1;
}

int PlayerBackend::subtitleGetTrackInfo(subtitleStream& trackInfo, int trackNum)
{
	if (trackNum >= 0 && trackNum < (int) mSubtitleStreams.size())
	{
		trackInfo = mSubtitleStreams[trackNum];
		return 0;
	}
	return -1;
}

int PlayerBackend::videoGetTrackInfo(videoStream& trackInfo, int trackNum)
{
	if (pCurrentVideo == NULL)
		return -1;
	trackInfo = *pCurrentVideo;
	return 0;
}


void PlayerBackend::gotMessage(const PlayerBackend::Message& message)
{
	switch (message.type)
	{
		case Message::tStart:
			eDebug("PlayerBackend::gotMessage - tStart");
			if (pPlayer->start(this) < 0)
			{
				quit(0);
				mMessageMain.send(Message(Message::stop));
			}
			else
			{
				mTimer = eTimer::create(this);
				CONNECT(mTimer->timeout, PlayerBackend::_updatePosition);
			}
			break;
		case Message::tStop:
			eDebug("PlayerBackend::gotMessage - tStop");
			mTimer->stop();
			pPlayer->sendStop();
			break;
		case Message::tKill:
			eDebug("PlayerBackend::gotMessage - tKill");
			pPlayer->sendForceStop();
			break;
		case Message::tPause:
			eDebug("PlayerBackend::gotMessage - tPause");
			pPlayer->sendPause();
			break;
		case Message::tResume:
			eDebug("PlayerBackend::gotMessage - tUnpause");
			pPlayer->sendResume();
			break;
		case Message::tSeekTo:
			eDebug("PlayerBackend::gotMessage - tSeekTo");
			pPlayer->sendSeekTo(message.dataInt);
			break;
		case Message::tSeekRelative:
			eDebug("PlayerBackend::gotMessage - tSeekRelative");
			pPlayer->sendSeekRelative(message.dataInt);
			break;
		case Message::tAudioSelect:
			eDebug("PlayerBackend::gotMessage - tAudioSelect");
			pPlayer->sendAudioSelectTrack(message.dataInt);
			break;
		case Message::tAudioList:
			eDebug("PlayerBackend::gotMessage - tAudioList");
			pPlayer->sendUpdateAudioTracksList();
			break;
		case Message::tSubtitleSelect:
			eDebug("PlayerBackend::gotMessage - tSubtitleSelect");
			pPlayer->sendSubtitleSelectTrack(message.dataInt);
			break;
		case Message::tSubtitleList:
			eDebug("PlayerBackend::gotMessage - tSubtitleList");
			pPlayer->sendUpdateSubtitleTracksList();
			break;
		case Message::tGetLength:
			eDebug("PlayerBackend::gotMessage - tGetLength");
			pPlayer->sendUpdateLength();
			break;
		case Message::start:
			eDebug("PlayerBackend::gotMessage - start");
			gotPlayerMessage(PlayerMessage::start);
			break;
		case Message::stop:
			eDebug("PlayerBackend::gotMessage - stop");
			gotPlayerMessage(PlayerMessage::stop);
			break;
		case Message::pause:
			eDebug("PlayerBackend::gotMessage - pause");
			gotPlayerMessage(PlayerMessage::pause);
			break;
		case Message::resume:
			eDebug("PlayerBackend::gotMessage - resume");
			gotPlayerMessage(PlayerMessage::resume);
			break;
		case Message::videoSizeChanged:
			eDebug("PlayerBackend::gotMessage - videoSizeChanged");
			gotPlayerMessage(PlayerMessage::videoSizeChanged);
			break;
		case Message::videoFramerateChanged:
			eDebug("PlayerBackend::gotMessage - videoFramerateChanged");
			gotPlayerMessage(PlayerMessage::videoFramerateChanged);
			break;
		case Message::videoProgressiveChanged:
			eDebug("PlayerBackend::gotMessage - videoProgressiveChanged");
			gotPlayerMessage(PlayerMessage::videoProgressiveChanged);
			break;
		case Message::audioSelect:
			eDebug("PlayerBackend::gotMessage - audioSelect");
			break;
		case Message::audioList:
			eDebug("PlayerBackend::gotMessage - audioList");
			break;
		case Message::error:
			eDebug("PlayerBackend::gotMessage - error");
			gotPlayerMessage(PlayerMessage::error);
			break;
		case Message::subtitleAvailable:
			eDebug("PlayerBackend::gotMessage - subtitleAvailable");
			gotPlayerMessage(PlayerMessage::subtitleAvailable);
			break;
		default:
			eDebug("PlayerBackend::gotMessage - unhandled message");
			break;
	}
}

void PlayerBackend::thread()
{
	mThreadRunning = true;
	hasStarted();
	runLoop();
}

void PlayerBackend::thread_finished()
{
	eDebug("PlayerBackend::thread_finished");
	mThreadRunning = false;
}

void PlayerBackend::recvStarted(int status)
{
	eDebug("PlayerBackend::recvStart - status = %d", status);
	if (playbackStarted || status)
		return;
	playbackStarted = true;
	mTimer->start(mTimerDelay, false);
	mMessageMain.send(Message(Message::start));
}

void PlayerBackend::recvStopped(int retval)
{
	pthread_mutex_lock(&mWaitForStopMutex);
	if (mWaitForStop)
	{
		mWaitForStop = false;
		pthread_cond_signal(&mWaitForStopCond);
	}
	pthread_mutex_unlock(&mWaitForStopMutex);
	eDebug("PlayerBackend::recvStopped - retval = %d", retval);
	quit(0);
	mMessageMain.send(Message(Message::stop));
}

void PlayerBackend::recvPaused(int status)
{
	eDebug("PlayerBackend::recvPause - status = %d", status);
	if (!status)
	{
		mTimer->stop();
		mMessageMain.send(Message(Message::pause));
	}
}

void PlayerBackend::recvResumed(int status)
{
	eDebug("PlayerBackend::recvResume - status = %d", status);
	if (!status)
	{
		mTimer->start(mTimerDelay, false);
		mMessageMain.send(Message(Message::resume));
	}
}

void PlayerBackend::recvAudioTracksList(int status, std::vector<audioStream>& streams)
{
	if(!status) 
		mAudioStreams = streams;
	recvMessage();
}

void PlayerBackend::recvAudioTrackCurrent(int status, audioStream& stream)
{ 
	eDebug("PlayerBackend::recvAudioTrackCurrent - status = %d", status);
	if(!status)
	{
		if (pCurrentAudio != NULL)
		{
			delete pCurrentAudio;
			pCurrentAudio = NULL;
		}
		pCurrentAudio = new audioStream(stream);
	}
} 

void PlayerBackend::recvAudioTrackSelected(int status, int trackId)
{
	eDebug("PlayerBackend::recvAudioTrackSelected - status = %d, trackId = %d", status, trackId);
	if (!status)
	{
		for (std::vector<audioStream>::const_iterator i(mAudioStreams.begin()); i!=mAudioStreams.end(); i++)
		{
			if (trackId == i->id)
			{
				if (pCurrentAudio != NULL)
				{
					delete pCurrentAudio;
					pCurrentAudio = NULL;
				}
				pCurrentAudio = new audioStream(*i);
				break;
			}
		}
	}
}

void PlayerBackend::recvSubtitleTracksList(int status, std::vector<subtitleStream>& streams)
{ 
	if(!status) 
		mSubtitleStreams = streams;
	recvMessage();
}

void PlayerBackend::recvSubtitleTrackCurrent(int status, subtitleStream& stream)
{ 
	eDebug("PlayerBackend::recvSubtitleTrackCurrent - status = %d", status);
	if(!status)
	{
		if (pCurrentSubtitle != NULL)
		{
			delete pCurrentSubtitle;
			pCurrentSubtitle = NULL;
		}
		pCurrentSubtitle = new subtitleStream(stream);
	}
} 

void PlayerBackend::recvSubtitleTrackSelected(int status, int trackId)
{
	eDebug("PlayerBackend::recvSubtitleTrackSelected - status = %d, trackId = %d", status, trackId);
	if (!status)
	{
		for (std::vector<subtitleStream>::const_iterator i(mSubtitleStreams.begin()); i!=mSubtitleStreams.end(); i++)
		{
			if (trackId == i->id)
			{
				if (pCurrentSubtitle != NULL)
				{
					delete pCurrentSubtitle;
					pCurrentSubtitle = NULL;
				}
				pCurrentSubtitle = new subtitleStream(*i);
				break;
			}
		}
	}
}

void PlayerBackend::recvVideoTrackCurrent(int status, videoStream& stream)
{
	eDebug("PlayerBackend::recvVideoTrackCurrent - status = %d", status);
	if (!status)
	{
		videoStream prev;
		if (pCurrentVideo != NULL)
		{
			prev = *pCurrentVideo;
			delete pCurrentVideo;
			pCurrentVideo = NULL;
		}
		pCurrentVideo = new videoStream(stream);
		if (stream.progressive >= 0 && prev.progressive != stream.progressive)
			mMessageMain.send(Message(Message::videoProgressiveChanged));
		if (stream.framerate > 0 && prev.framerate != stream.framerate)
			mMessageMain.send(Message(Message::videoFramerateChanged));
		if ((stream.width > 0 && prev.width != stream.width) || (stream.height > 0 && prev.height != stream.height))
			mMessageMain.send(Message(Message::videoSizeChanged));
	}
}

void PlayerBackend::recvSubtitleMessage(subtitleMessage& sub)
{
	eSingleLocker s(mSubLock);
	mSubtitles.push(sub);
	mMessageMain.send(Message(Message::subtitleAvailable));
}

int PlayerBackend::getSubtitles(std::queue< subtitleMessage >& subtitles)
{
	eSingleLocker s(mSubLock);
	if (mSubtitles.empty())
	{
		return -1;
	}
	while (!mSubtitles.empty())
	{
		subtitles.push(mSubtitles.front());
		mSubtitles.pop();
	}
	return 0;
}
