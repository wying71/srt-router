#ifndef SRTWRAPPER_H
#define SRTWRAPPER_H
#include <stdlib.h>
#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include "srt/srt.h"
#include "simplelogging.h"

using StdKeyValue               = std::map<std::string, std::string>;
using OnSrtConnectedHandler     = std::function<int(SRTSOCKET ss, const char* streamId, int length)>;
using OnSrtDisconnectedHandler  = std::function<int(SRTSOCKET ss)>;
using OnSrtReadHandler          = std::function<void(SRTSOCKET ss, const char* data, int size, SRT_MSGCTRL* mctrl)>;
using OnSrtStreamIDHandler      = std::function<bool(SRTSOCKET ss, const char* streamId, int length)>;

void onSrtLogCallback(void* opaque, int level, const char* file, int line, const char* area, const char* message);

// class SrtSocket
class SrtBase;
class SrtSocket
{
public:
    SrtSocket();
    virtual ~SrtSocket();

public:
    // mode: "caller" | "listener" | "rendezvous" | "bondingcaller" | "bondinglistener" | "multiplexlistener"
    // --------------------------------------------------------------------------------
    // params: map<string, string>
    // transtype:   string, "live"(default) | "file", 
    // conntimeo:   int,    3000ms(default)
    // rcvlatency:  int,    120 ms(default)
    // peerlatency: int,    0 ms(default)
    // sndbuf:      int,    8192*(1500-28)(default)
    // rcvbuf:      int,    8192*(1500-28)(default)
    // streamid:    string
    // localaddr:   string, host:port | host:port;host:port;host:port
    // remoteaddr:  string, host:port;host:port;host:port
    // weight:      int(0~32767), 0 | 0;0;0, "bondingcaller" and "backup"
    // password:    string
    // keylength:   int
    // payloadsize: int,    7*188=1316(default)
    // grouptype:   string, "broadcast"(default) | "backup"
    // iotype:      string, "input" | "output" | "unknown"
    // maxconnections: int, -1(unlimited), "listener" | "bondinglistener" | "multiplexlistener" mode

    bool            create(const char* mode, const StdKeyValue& params = StdKeyValue());
    const char*     mode() const;
    void            setOnReadHandler(OnSrtReadHandler handler);
    void            setOnConnectedHandler(OnSrtConnectedHandler handler);
    void            setOnDisconnectedHandler(OnSrtDisconnectedHandler handler);
    void            setOnStreamIDHandler(OnSrtStreamIDHandler handler);
    bool            open(bool isSync);
    bool            isConnected(int timeout_ms = 5000, std::function<bool()> isBreak = nullptr);
    int             write(const char* data, int size, SRT_MSGCTRL* mctrl = nullptr);
    SRT_SOCKSTATUS  state();
    int             bstats(SRT_TRACEBSTATS* perf, int clear = 0);

    int             write(SRTSOCKET ss, const char* data, int size, SRT_MSGCTRL* mctrl = nullptr);
    SRT_SOCKSTATUS  state(SRTSOCKET ss);
    int             bstats(SRTSOCKET ss, SRT_TRACEBSTATS* perf, int clear = 0);

    void            close();

private:
    std::shared_ptr<SrtBase> mSrtBase;
    mutable std::shared_mutex mMutex;
};

#endif // SRTWRAPPER_H