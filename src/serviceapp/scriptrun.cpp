#include "scriptrun.h"
#include "common.h"

void scriptrun::stdoutAvail(const char *data)
{
    //eDebug("outData: %s", data);
    m_stdout.append(data);
}

void scriptrun::stderrAvail(const char *data)
{
    //eDebug("errData: %s", data);
    m_stderr.append(data);
}

void scriptrun::appClosed(int retval)
{
    //eDebug("scriptrun::appClosed: %d", retval);
    scriptEnded(retval);
}

scriptrun::scriptrun(const std::string &scriptPath, const std::vector<std::string> &params)
{
    m_scriptpath = scriptPath;
    m_params = params;
}

scriptrun::~scriptrun()
{
    stop();
}

void scriptrun::run(eMainloop *context)
{
    m_console = new eConsoleContainer();
    CONNECT(m_console->appClosed, scriptrun::appClosed);
    CONNECT(m_console->stdoutAvail, scriptrun::stdoutAvail);
    CONNECT(m_console->stderrAvail, scriptrun::stderrAvail);

    std::vector<std::string> args;
    args.push_back(m_scriptpath);
    for (size_t i = 0;  i < m_params.size(); i++)
        args.push_back(m_params[i]);

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
    m_console->execute(context, cargs[0], cargs);
}

void scriptrun::stop()
{
    if (m_console && m_console->running())
        m_console->sendCtrlC();
}

ResolveUrl::ResolveUrl(const std::string &url):

    m_url(url),
    m_success(0),
    mStopped(false),
    mThreadRunning(false),
    mMessageMain(eApp, 1),
    mMessageThread(this, 1)
{
    eDebug("ResolveUrl::ResolveUrl %s", url.c_str());
    CONNECT(mMessageThread.recv_msg, ResolveUrl::gotMessage);
    CONNECT(mMessageMain.recv_msg, ResolveUrl::gotMessage);
    pthread_mutex_init(&mWaitForStopMutex, NULL);
    pthread_cond_init(&mWaitForStopCond, NULL);
}

ResolveUrl::~ResolveUrl()
{
    stop();
    pthread_mutex_destroy(&mWaitForStopMutex);
    pthread_cond_destroy(&mWaitForStopCond);
    delete m_scriptrun;
}

void ResolveUrl::start()
{
    std::vector<std::string> params;
    std::string delimiter = "|";
    size_t last = 0; size_t next = 0;
    while ((next = m_url.find(delimiter, last)) != std::string::npos)
    {
        params.push_back(m_url.substr(last, next-last));
        last = next + 1;
    }
    params.push_back(m_url.substr(last));

    m_scriptrun = new scriptrun("/etc/enigma2/script", params);

    // start player only when mainloop in player thread has started
    mMessageThread.send(Message(Message::tStart));
    run();
}

void ResolveUrl::stop()
{
    mStopped = true;
    if (mThreadRunning)
    {
        mMessageThread.send(Message(Message::tStop));
    }
    kill();
}

std::string ResolveUrl::getUrl()
{
    std::string url = m_scriptrun->getStdOut();
    url = url.substr(0, url.size() - 1);
    return url;
}

void ResolveUrl::scriptEnded(int retval)
{
    pthread_mutex_lock(&mWaitForStopMutex);
    if (mWaitForStop)
    {
        mWaitForStop = false;
        pthread_cond_signal(&mWaitForStopCond);
    }
    pthread_mutex_unlock(&mWaitForStopMutex);
    quit(0);
    if (mStopped)
        m_success = false;
    else
    {
        m_success = !retval;
        if (m_success)
            m_success = !getUrl().empty();
    }
    mMessageMain.send(Message(Message::stop));
}

void ResolveUrl::thread()
{
    mThreadRunning = true;
    hasStarted();
    runLoop();
}

void ResolveUrl::thread_finished()
{
    //eDebug("ResolveUrl::thread_finished");
    mThreadRunning = false;
}

void ResolveUrl::gotMessage(const ResolveUrl::Message &message)
{
    switch (message.type)
    {
    case Message::tStart:
        //eDebug("ResolveUrl::gotMessage - tStart");
        CONNECT(m_scriptrun->scriptEnded, ResolveUrl::scriptEnded);
        m_scriptrun->run(this);
        break;
    case Message::tStop:
        eDebug("ResolveUrl::gotMessage - tStop");
        m_scriptrun->stop();
        break;
    case Message::stop:
        eDebug("ResolveUrl::gotMessage - stop");
        urlResolved(m_success);
        break;
    default:
        break;
    }
}
