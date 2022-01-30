#ifndef SCRIPTRUN_H
#define SCRIPTRUN_H

#include <lib/base/ebase.h>
#include <lib/base/message.h>
#include <lib/python/connections.h>
#include <lib/base/thread.h>

#include "extplayer.h"
#include "myconsole.h"

#if SIGCXX_MAJOR_VERSION >= 2
class scriptrun: public sigc::trackable
#else
class scriptrun: public Object
#endif
{
    std::vector<std::string> m_params;
    std::string m_scriptpath;
    std::string m_stdout;
    std::string m_stderr;
    ePtr<eConsoleContainer> m_console;

    void stdoutAvail(const char *data);
    void stderrAvail(const char *data);
    void appClosed(int retval);
public:
    scriptrun(const std::string &scriptPath,
              const std::vector<std::string> &params);
    ~scriptrun();
    void run(eMainloop *context);
    void stop();

    std::string getStdOut(){return m_stdout;}
    std::string getStdErr(){return m_stderr;}

    PSignal1<void, int> scriptEnded;
};


#if SIGCXX_MAJOR_VERSION >= 2
class ResolveUrl: public sigc::trackable, public eThread, public eMainloop
#else
class ResolveUrl: public Object, public eThread, public eMainloop
#endif
{
    struct Message
    {
        int type;
        enum
        {
            start,
            tStart,
            stop,
            tStop,
        };
        Message(int type)
            :type(type) {}
    };
    scriptrun *m_scriptrun;
    std::string m_url;
    int m_success;

    bool mThreadRunning;
    bool mStopped;

    eFixedMessagePump<Message> mMessageMain, mMessageThread;
    pthread_mutex_t mWaitForStopMutex;
    pthread_cond_t mWaitForStopCond;
    bool mWaitForStop;


    // eThread
    void thread();
    void thread_finished();

public:
    ResolveUrl(const std::string &url);
    ~ResolveUrl();
    void gotMessage(const Message &message);

    void start();
    void stop();
    std::string getUrl();

    void scriptEnded(int retval);
#if SIGCXX_MAJOR_VERSION == 3
    sigc::signal<void(int)> urlResolved;
#elif SIGCXX_MAJOR_VERSION == 2
    sigc::signal1<void,int> urlResolved;
#else
    Signal1<void, int> urlResolved;
#endif
};

#endif // SCRIPTRUN_H
