#ifndef __LIB_MY_CONSOLE_H__
#define __LIB_MY_CONSOLE_H__

#include <string>
#include <lib/base/ebase.h>
#include <lib/python/connections.h>
#include <queue>
#include <vector>

struct queue_data
{
    queue_data( char *data, int len )
        :data(data), len(len), dataSent(0)
    {
    }
    char *data;
    int len;
    int dataSent;
};

#if SIGCXX_MAJOR_VERSION >= 2
class eConsoleContainer: public sigc::trackable, public iObject
#else
class eConsoleContainer: public Object, public iObject
#endif
{
    DECLARE_REF(eConsoleContainer);
    int fd[3];
    int filefd[3];
    int pid;
    int killstate;
    std::string m_cwd;
    std::queue<struct queue_data> outbuf;
    ePtr<eSocketNotifier> in, out, err;
    std::vector<char> buffer;
    void readyRead(int what);
    void readyErrRead(int what);
    void readyWrite(int what);
    void closePipes();
public:
    eConsoleContainer();
    ~eConsoleContainer();
    int setCWD( const char *path );
    int execute( eMainloop * context, const char *str );
    int execute( eMainloop * context, const char *cmdline, const char *const argv[] );
    int getPID() { return pid; }
    void kill();
    void sendCtrlC();
    void sendEOF();
    void write( const char *data, int len );
    void setFileFD(int num, int fd) { if (num >= 0 && num <= 2) filefd[num] = fd; }
    bool running() { return (fd[0]!=-1) && (fd[1]!=-1) && (fd[2]!=-1); }
    PSignal1<void, const char*> dataAvail;
    PSignal1<void, const char*> stdoutAvail;
    PSignal1<void, const char*> stderrAvail;
    PSignal1<void,int> dataSent;
    PSignal1<void,int> appClosed;
};

#endif // __LIB_MY_CONSOLE_H__
