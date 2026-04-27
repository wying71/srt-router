#ifndef SRTROUTER_H
#define SRTROUTER_H

#include "libwebserver.h"
#include "SrtSocket.h"
#include "configure.h"

#include <memory>
#include <list>
#include <mutex>
#include <map>
#include <atomic>
#include <ctime>
#include <sstream>
#include <string>
#include <cstdint>

enum class StreamIDMode {
    None,
    Request,
    Publish
};

struct StreamId
{
    std::string path;
    std::string query;
    std::string user;
    std::string pass;
    StreamIDMode mode;
    StreamId() : path(""), query(""), user(""), pass(""), mode(StreamIDMode::None) {}
    void reset() {
        path = "";
        query = "";
        user = "";
        pass = "";
        mode = StreamIDMode::None;
    }
    std::string toString() const {
        std::stringstream ss;

        ss << "path=\"" << path << "\"";
        if (!query.empty())
            ss << ", " << "query=\"" << query << "\"";
        if (!user.empty())
            ss << ", " << "user=\"" << user << "\"";
        if (!pass.empty())
            ss << ", " << "pass=\"" << pass << "\"";
        ss << ", " << "mode=" << modeToString(mode);

        return ss.str();
    }

    static std::string modeToString(StreamIDMode m) {
        switch (m) {
        case StreamIDMode::None: return "None";
        case StreamIDMode::Request: return "Request";
        case StreamIDMode::Publish: return "Publish";
        default: return "Unknown";
        }
    }
};

struct SocketContext
{
    uint64_t connectTime;
    bool attached;
    uint64_t attachedTime;
    SRTSOCKET publishSocket;
    uint64_t recvBytes;
    uint64_t sentBytes;
    int64_t sendTotalUs;
    int64_t sendMaxUs;
    int64_t sendCount;
    int64_t sendZeroCount;
    int64_t sendErrorCount;
    StreamId streamId;

    SocketContext()
        : connectTime(0LL)
        , attached(false)
        , attachedTime(0LL)
        , publishSocket(-1)
        , recvBytes(0LL)
        , sentBytes(0LL)
        , sendTotalUs(0LL)
        , sendMaxUs(0LL)
        , sendCount(0LL)
        , sendZeroCount(0LL)
        , sendErrorCount(0LL)
        , streamId() {}

    void reset() {
        connectTime = 0LL;
        attached = false;
        attachedTime = 0LL;
        publishSocket = -1;
        recvBytes = 0LL;
        sentBytes = 0LL;
        sendTotalUs = 0LL;
        sendMaxUs = 0LL;
        sendCount = 0LL;
        sendZeroCount = 0LL;
        sendErrorCount = 0LL;
        streamId.reset();
    }

    void resetSendStat() {
        // reset per-window stats; sendErrorCount is cumulative and intentionally not reset here
        sendTotalUs = 0LL;
        sendMaxUs = 0LL;
        sendCount = 0LL;
        sendZeroCount = 0LL;
    }
};

struct SendTarget {
    SRTSOCKET sock;
    std::string streamIdStr;
};

class SrtRouter
{
public:
    SrtRouter(SrtRouterConfigurePtr config = nullptr, const std::string& configFilePath = "srtrouterconfig.json");
    ~SrtRouter();

    int startApiServer(const std::string& address, int port, int threads = 128);
    int startSrtServer(const std::string& address, int port);
    void stop();

private:
    bool restApiKeepAlive(std::shared_ptr<IWebServer::HttpRequest> http,
                          const std_keyvalue& params,
                          void* userData);
    bool restApiPathsList(std::shared_ptr<IWebServer::HttpRequest> http,
                          const std_keyvalue& params,
                          void* userData);
    bool restApiStatus(std::shared_ptr<IWebServer::HttpRequest> http,
                       const std_keyvalue& params,
                       void* userData);
    bool restApiPathStatus(std::shared_ptr<IWebServer::HttpRequest> http,
                           const std_keyvalue& params,
                           void* userData);
    bool restApiGetConfiguration(std::shared_ptr<IWebServer::HttpRequest> http,
                                 const std_keyvalue& params,
                                 void* userData);
    bool restApiPostConfiguration(std::shared_ptr<IWebServer::HttpRequest> http,
                                  const std_keyvalue& params,
                                  void* userData);

    bool checkNeedRestart(const SrtRouterConfigure& newConfig, Json::Value& reasons);

    int onSrtConnected(SRTSOCKET ss, const char* streamId, int length);
    int onSrtDisconnected(SRTSOCKET ss);
    void onSrtRead(SRTSOCKET ss, const char* data, int size, SRT_MSGCTRL* mctrl);
    void srtSendMsg(const SendTarget& target, const char* data, int size, SRT_MSGCTRL* mctrl);
    bool onSrtStreamID(SRTSOCKET ns, const char* streamIdStr, int length);

private:
    bool checkAndUpdateStatusReportTimer();
    void logSocketStatus(SRTSOCKET sock, const SocketContext& ctx);
    void logSendStats();

    static constexpr int64_t kSendSlowThresholdUs = 1000; // 1ms
    static constexpr int64_t kMinStatusReportIntervalSeconds = 10;

private:
    IWebServer* mWebServer;

    std::list<std::shared_ptr<IWebServer::HttpRequest>> httpRequests;
    std::recursive_mutex mutexRequests;

    SrtSocket mSrtSocket;
    std::recursive_mutex mMutexSocket;
    std::map<SRTSOCKET, SocketContext> mMapSocketContext;
    SrtRouterConfigurePtr mConfig;
    std::string mConfigFilePath;
};

#endif // SRTROUTER_H
