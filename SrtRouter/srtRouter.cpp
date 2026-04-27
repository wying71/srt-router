#include "srtRouter.h"
#include "logger.h"
#include "json/json.h"
#include <functional>
#include <stdio.h>
#include <vector>
#include <sstream>
#include <string>
#include <set>
#include <iomanip>
#include <chrono>
#include "util.h"


SrtRouter::SrtRouter(SrtRouterConfigurePtr config, const std::string& configFilePath)
    : mWebServer(nullptr), mConfig(config), mConfigFilePath(configFilePath) {
    mWebServer = WebServerCreate();
    if (mWebServer) {
        mWebServer->setHttpResponseHeader("Server", "SrtRouter/1.1");
        mWebServer->setHttpResponseHeader("Content-Type",
            "application/json; charset=utf-8");
        mWebServer->setHttpResponseHeader("Cache-Control", "no-cache");

        mWebServer->addHttpRequestRoutingHandler(
            std::bind(&SrtRouter::restApiKeepAlive, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3),
            "/api/health", "GET", nullptr);
        Logger::logPrint(LogInfo, "SrtRouter WebServer: Added handler for /api/health GET");

        mWebServer->addHttpRequestRoutingHandler(
            std::bind(&SrtRouter::restApiPathsList, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3),
            "/api/streams", "GET", nullptr);
        Logger::logPrint(LogInfo, "SrtRouter WebServer: Added handler for /api/streams GET");


        mWebServer->addHttpRequestRoutingHandler(
            std::bind(&SrtRouter::restApiStatus, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3),
            "/api/status", "GET", nullptr);
        Logger::logPrint(LogInfo, "SrtRouter WebServer: Added handler for /api/status GET");

        mWebServer->addHttpRequestRoutingHandler(
            std::bind(&SrtRouter::restApiPathStatus, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3),
            "/api/streams/:streamid/status", "GET", nullptr);
        Logger::logPrint(LogInfo, "SrtRouter WebServer: Added handler for /api/streams/:streamid/status GET");

        mWebServer->addHttpRequestRoutingHandler(
            std::bind(&SrtRouter::restApiGetConfiguration, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3),
            "/api/config", "GET", nullptr);
        Logger::logPrint(LogInfo, "SrtRouter WebServer: Added handler for /api/config GET");

        mWebServer->addHttpRequestRoutingHandler(
            std::bind(&SrtRouter::restApiPostConfiguration, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3),
            "/api/config", "POST", nullptr);
        Logger::logPrint(LogInfo, "SrtRouter WebServer: Added handler for /api/config POST");

    }
}

SrtRouter::~SrtRouter() { stop(); }

int SrtRouter::startApiServer(const std::string& address, int port,
    int threads) {
    if (!mWebServer) {
        Logger::logPrint(LogError, "WebServer not initialized");
        return -1;
    }

    mWebServer->setAddress(address.c_str());
    int ret = mWebServer->start(port, threads);

    if (ret == 0) {
        Logger::logPrint(LogInfo, "SrtRouter WebServer started at {}:{}", address,
            port);
    }
    else {
        Logger::logPrint(LogError,
            "Failed to start SrtRouter WebServer at {}:{}, ret={}",
            address, port, ret);
    }

    return ret;
}

int SrtRouter::startSrtServer(const std::string& address, int port) {
    int maxConnections = mConfig ? mConfig->get().maxStreamsLimit : 100;
    StdKeyValue params;
    const std::string localaddr = address + ":" + std::to_string(port);
    params["localaddr"] = localaddr;
    params["maxconnections"] = std::to_string(maxConnections);

    mSrtSocket.create("multiplexlistener", params);

    mSrtSocket.setOnConnectedHandler(
        std::bind(&SrtRouter::onSrtConnected, this, std::placeholders::_1,
            std::placeholders::_2, std::placeholders::_3));

    mSrtSocket.setOnDisconnectedHandler(
        std::bind(&SrtRouter::onSrtDisconnected, this, std::placeholders::_1));

    mSrtSocket.setOnReadHandler(std::bind(
        &SrtRouter::onSrtRead, this, std::placeholders::_1, std::placeholders::_2,
        std::placeholders::_3, std::placeholders::_4));

    mSrtSocket.setOnStreamIDHandler(
        std::bind(&SrtRouter::onSrtStreamID, this, std::placeholders::_1,
            std::placeholders::_2, std::placeholders::_3));

    if (mSrtSocket.open(false)) {
        Logger::logPrint(LogInfo, "SRT multiplex listener opened at {}",
            localaddr);
        return 0;
    }
    else {
        Logger::logPrint(LogError, "SRT multiplex listener open failed on {}",
            localaddr);
        return -1;
    }
}

int parseStreamId(const std::string& raw, StreamId& streamId) {
    if (raw.substr(0, 4) == "#!::") {
        std::string content = raw.substr(4);
        std::vector<std::string> kvs;

        std::stringstream ss(content);
        std::string item;
        while (std::getline(ss, item, ',')) {
            kvs.push_back(item);
        }

        for (const auto& kv : kvs) {
            size_t eq_pos = kv.find('=');
            if (eq_pos == std::string::npos || eq_pos == 0 ||
                eq_pos == kv.length() - 1) {
                return -1;
            }

            std::string key = kv.substr(0, eq_pos);
            std::string value = kv.substr(eq_pos + 1);

            if (key == "u") {
                streamId.user = value;
            }
            else if (key == "r") {
                streamId.path = value;
            }
            else if (key == "h") {
            }
            else if (key == "s") {
                streamId.pass = value;
            }
            else if (key == "t") {
            }
            else if (key == "m") {
                if (value == "request") {
                    streamId.mode = StreamIDMode::Request;
                }
                else if (value == "publish") {
                    streamId.mode = StreamIDMode::Publish;
                }
                else {
                    streamId.mode = StreamIDMode::None;
                    return -1;
                }
            }
        }
    }
    return 0;
}

bool SrtRouter::onSrtStreamID(SRTSOCKET ns, const char* streamIdStr, int length) {
    StreamId streamId;
    if (parseStreamId(streamIdStr, streamId) < 0) {
        return true;
    }

    if (mConfig) {
        const auto& cfg = mConfig->get();
        const SrtEncryption* matched = nullptr;
        const SrtEncryption* wildcard = nullptr;
        for (const auto& enc : cfg.srt.encryptions) {
            if (enc.path == streamId.path) {
                matched = &enc;
                break;
            }
            if (enc.path == "*")
                wildcard = &enc;
        }
        const SrtEncryption* enc = matched ? matched : wildcard;
        if (enc) {
            const std::string& passphrase = (streamId.mode == StreamIDMode::Publish)
                ? enc->publishPassphrase
                : enc->requestPassphrase;
            if (!passphrase.empty()) {
                int keylen = enc->pbkeylen();
                if (srt_setsockflag(ns, SRTO_PASSPHRASE, passphrase.c_str(), (int)passphrase.size()) == SRT_ERROR) {
                    Logger::logPrint(LogError, "onSrtStreamID: set SRTO_PASSPHRASE failed for 0x{:08X}: {}", ns, srt_getlasterror_str());
                } else if (srt_setsockflag(ns, SRTO_PBKEYLEN, &keylen, sizeof(keylen)) == SRT_ERROR) {
                    Logger::logPrint(LogError, "onSrtStreamID: set SRTO_PBKEYLEN failed for 0x{:08X}: {}", ns, srt_getlasterror_str());
                } else {
                    Logger::logPrint(LogInfo, "onSrtStreamID: set passphrase for 0x{:08X}, stream: {}", ns, streamId.toString());
                }
            }
        }
    }

    return true;
}

int SrtRouter::onSrtConnected(SRTSOCKET ss, const char* streamIdStr, int length) {
    StreamId streamId;
    int ret = parseStreamId(streamIdStr, streamId);
    if (ret < 0)
    {
        Logger::logPrint(LogError, "Srt connection refused, streamId should start with #!::, 0x{:08X}, streamId:[{}]", ss, streamIdStr);
        return ret;
    }

    {
        std::lock_guard<std::recursive_mutex> lock(mMutexSocket);
        
        if (streamId.mode == StreamIDMode::Publish) {
            for (const auto& sc : mMapSocketContext) {
                if (sc.second.streamId.mode == StreamIDMode::Publish &&
                    sc.second.streamId.path == streamId.path) {
                    Logger::logPrint(LogError, "Srt publish rejected, streamId already exists: 0x{:08X}, {}", ss, streamId.toString());
                    return -1;
                }
            }
        }

        SocketContext ctx;
        ctx.streamId = streamId;
        ctx.connectTime = Util::Date::now();
        mMapSocketContext[ss] = ctx;
    }

    Logger::logPrint(LogInfo, "Srt connected: 0x{:08X}, {}", ss, streamId.toString());

    return ret;
}

int SrtRouter::onSrtDisconnected(SRTSOCKET ss) {
    Logger::logPrint(LogInfo, "Srt disconnected: 0x{:08X}", ss);

    std::lock_guard<std::recursive_mutex> lock(mMutexSocket);
    mMapSocketContext.erase(ss);

    return 0;
}

void SrtRouter::srtSendMsg(const SendTarget& target, const char* data, int size, SRT_MSGCTRL* mctrl)
{
    auto t0 = std::chrono::steady_clock::now();
    int ret = srt_sendmsg2(target.sock, data, size, mctrl);
    auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();

    SRT_SOCKSTATUS state = SRTS_NONEXIST;
    if (ret == SRT_ERROR) {
        state = srt_getsockstate(target.sock);
    }

    {
        std::lock_guard<std::recursive_mutex> lock(mMutexSocket);
        auto sit = mMapSocketContext.find(target.sock);
        if (sit != mMapSocketContext.end()) {
            auto& ctx = sit->second;
            ctx.sendTotalUs += elapsedUs;
            ctx.sendCount++;
            ctx.sendMaxUs = std::max(ctx.sendMaxUs, elapsedUs); 

            if (ret > 0) {
                ctx.sentBytes += ret;
            }
            else if (ret == 0) {
                ctx.sendZeroCount++;
            }
            else { // ret == SRT_ERROR
                ctx.sendErrorCount++;
            }
        }
    } // lock released before logging

    if (ret == SRT_ERROR) {
        Logger::logPrint(LogInfo, "Srt send error: 0x{:08X}, stream: {}, state: {}",
            target.sock, target.streamIdStr, (int)state);
    }
    else if (ret == 0) {
        Logger::logPrint(LogWarn, "srt_sendmsg2 zero send (socket not ready): "
            "0x{:08X}, stream: {}, size: {}",
            target.sock, target.streamIdStr, size);
    }

    if (elapsedUs >= kSendSlowThresholdUs) {
        Logger::logPrint(LogWarn, "srt_sendmsg2 slow: 0x{:08X}, stream: {}, size: {}, elapsed: {}us",
            target.sock, target.streamIdStr, size, elapsedUs);
    }
}

void SrtRouter::onSrtRead(SRTSOCKET ss, const char* data, int size, SRT_MSGCTRL* mctrl)
{
    const bool shouldPrint = checkAndUpdateStatusReportTimer();

    std::vector<SendTarget> targets;
    //std::vector<std::pair<SRTSOCKET, SocketContext>> contextSnapshot;
    {
        std::lock_guard<std::recursive_mutex> lock(mMutexSocket);

        auto it = mMapSocketContext.find(ss);
        if (it == mMapSocketContext.end())
            return;

        it->second.recvBytes += size;
        const StreamId& streamId = it->second.streamId;

        for (const auto& sc : mMapSocketContext)
        {
            auto sock = sc.first;
            auto ctx = sc.second;
            if (ctx.streamId.mode == StreamIDMode::Request &&
                ctx.streamId.path == streamId.path)
            {
                targets.push_back({ sock, ctx.streamId.toString() });
            }

            if (shouldPrint)
                logSocketStatus(sock, ctx);
        }
    }

    for (const auto& target : targets)
        srtSendMsg(target, data, size, mctrl);

    if (shouldPrint)
        logSendStats();
}

bool SrtRouter::checkAndUpdateStatusReportTimer()
{
    static auto s_lastReportTime = std::chrono::steady_clock::now();
    const auto kMinInterval = std::chrono::seconds(kMinStatusReportIntervalSeconds);

    auto now = std::chrono::steady_clock::now();
    bool shouldPrint = (now - s_lastReportTime) >= kMinInterval;

    if (!shouldPrint)
        return false;

    s_lastReportTime = now;
    Logger::logPrint(LogInfo, "Srt socket context count: {}", mMapSocketContext.size());
    return true;
}

void SrtRouter::logSocketStatus(SRTSOCKET sock, const SocketContext& ctx)
{
    if (ctx.streamId.mode == StreamIDMode::Request)
    {
        Logger::logPrint(LogInfo, "Srt sent: 0x{:08X}, stream: {}, bytes: {}",
            sock, ctx.streamId.toString(), ctx.sentBytes);
    }
    else if (ctx.streamId.mode == StreamIDMode::Publish)
    {
        Logger::logPrint(LogInfo, "Srt recv: 0x{:08X}, stream: {}, bytes: {}",
            sock, ctx.streamId.toString(), ctx.recvBytes);
    }
}

void SrtRouter::logSendStats()
{
    std::lock_guard<std::recursive_mutex> lock(mMutexSocket);

    for (auto& sc : mMapSocketContext)
    {
        auto sock = sc.first;
        auto ctx = sc.second;
        if (sc.second.streamId.mode != StreamIDMode::Request || ctx.sendCount == 0)
            continue;

        Logger::logPrint(LogInfo,
            "srt_sendmsg2 stat: 0x{:08X}, stream: {}, count: {}, avg: {}us, max: {}us, zero: {}, errors: {}",
            sock,
            ctx.streamId.toString(),
            ctx.sendCount,
            ctx.sendTotalUs / ctx.sendCount,
            ctx.sendMaxUs,
            ctx.sendZeroCount,
            ctx.sendErrorCount);

        ctx.resetSendStat();
    }
}

void SrtRouter::stop() {
    if (mWebServer) {
        mWebServer->stop();
        Logger::logPrint(LogInfo, "Http requests size: {}", httpRequests.size());

        httpRequests.clear();
        WebServerRelease(mWebServer);
        mWebServer = nullptr;
    }

    if (mSrtSocket.state() == SRTS_NONEXIST) {
        Logger::logPrint(LogInfo, "SRT socket NONEXIST.");
        return;
    }
    {
        std::lock_guard<std::recursive_mutex> lock(mMutexSocket);
        mMapSocketContext.clear();
        Logger::logPrint(LogInfo, "Socket Context Map clear.");
    }
    {
        std::lock_guard<std::recursive_mutex> lock(mMutexSocket);
        mSrtSocket.setOnConnectedHandler(nullptr);
        mSrtSocket.setOnDisconnectedHandler(nullptr);
        mSrtSocket.setOnReadHandler(nullptr);
        mSrtSocket.setOnStreamIDHandler(nullptr);
        Logger::logPrint(LogInfo, "Ready to close SRT socket.");
    }
    mSrtSocket.close();

    //Logger::logPrint(LogInfo, "Waiting 3 seconds for SRT socket to close.");
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    Logger::logPrint(LogInfo, "SRT socket closed.");

    Logger::logPrint(LogInfo, "SrtRouter closed successfully. Exiting gracefully.");
}

bool SrtRouter::restApiKeepAlive(std::shared_ptr<IWebServer::HttpRequest> http,
    const std_keyvalue& params, void* userData) {
    Logger::logPrint(LogInfo, "HTTP Request: {} /api/heartbeat",
        http->getMethod());

    Json::Value response;
    response["status"] = "alive";
    response["code"] = 200;

    http->responseData(response.toStyledString());

    return true;
}

bool SrtRouter::restApiPathsList(std::shared_ptr<IWebServer::HttpRequest> http,
    const std_keyvalue& params, void* userData) {
    Logger::logPrint(LogInfo, "HTTP Request: {} /api/paths/list",
        http->getMethod());

    Json::Value response;
    Json::Value paths(Json::arrayValue);

    {
        std::lock_guard<std::recursive_mutex> lock(mMutexSocket);

        std::set<std::string> uniquePaths;
        for (const auto& sc : mMapSocketContext) {
            std::string pathKey = sc.second.streamId.path + "|" +
                StreamId::modeToString(sc.second.streamId.mode);
            uniquePaths.insert(pathKey);
        }

        for (const auto& pathKey : uniquePaths) {
            size_t pos = pathKey.find('|');
            if (pos != std::string::npos) {
                Json::Value item;
                item["path"] = pathKey.substr(0, pos);
                item["mode"] = pathKey.substr(pos + 1);
                paths.append(item);
            }
        }
    }

    response["code"] = 200;
    response["paths"] = paths;

    http->responseData(response.toStyledString());

    return true;
}

bool SrtRouter::restApiStatus(std::shared_ptr<IWebServer::HttpRequest> http,
    const std_keyvalue& params, void* userData) {
    Logger::logPrint(LogInfo, "HTTP Request: {} /api/status",
        http->getMethod());

    Json::Value response;
    int totalSockets = mMapSocketContext.size();
    int publishSockets = 0;
    int readSockets = 0;

    {
        std::lock_guard<std::recursive_mutex> lock(mMutexSocket);

        for (const auto& sc : mMapSocketContext) {
            if (sc.second.streamId.mode == StreamIDMode::Publish) {
                publishSockets++;
            }
            else if (sc.second.streamId.mode == StreamIDMode::Request) {
                readSockets++;
            }
        }
    }

    response["code"] = 200;
    response["totalSockets"] = totalSockets;
    response["publishSockets"] = publishSockets;
    response["requestSockets"] = readSockets;

    http->responseData(response.toStyledString());

    return true;
}

std::string timestampMsToDateTimeString(uint64_t timestamp_ms, const std::string& format = "%Y-%m-%d %H:%M:%S") {
    std::time_t time_sec = static_cast<std::time_t>(timestamp_ms / 1000);

    std::tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &time_sec);
#else
    localtime_r(&time_sec, &tm_struct);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_struct, format.c_str());
    return oss.str();
}

bool SrtRouter::restApiPathStatus(std::shared_ptr<IWebServer::HttpRequest> http,
    const std_keyvalue& params, void* userData) {
    Logger::logPrint(LogInfo, "HTTP Request: {} /api/paths/:streamid/status",
        http->getMethod());

    Json::Value response;

    auto it = params.find("streamid");
    if (it == params.end()) {
        response["code"] = 400;
        response["error"] = "Missing streamid parameter";
        http->responseData(response.toStyledString());
        return true;
    }

    std::string streamId = it->second;

    auto socketContextToJson = [](SRTSOCKET socket, const SocketContext& ctx) -> Json::Value {
        Json::Value obj;
        obj["connectTime"] = timestampMsToDateTimeString(ctx.connectTime); //Json::Value::UInt64(ctx.connectTime); 
        //obj["attached"] = ctx.attached;
        //obj["attachedTime"] = Json::Value::UInt64(ctx.attachedTime);
        obj["recvBytes"] = Json::Value::UInt64(ctx.recvBytes);
        obj["sentBytes"] = Json::Value::UInt64(ctx.sentBytes);

        obj["query"] = ctx.streamId.query;
        obj["user"] = ctx.streamId.user;
        obj["pass"] = ctx.streamId.pass;

        return obj;
    };

    Json::Value publish = Json::Value::null;
    Json::Value readers(Json::arrayValue);

    {
        std::lock_guard<std::recursive_mutex> lock(mMutexSocket);

        for (const auto& sc : mMapSocketContext) {
            if (sc.second.streamId.path == streamId) {
                if (sc.second.streamId.mode == StreamIDMode::Publish) {
                    publish = socketContextToJson(sc.first, sc.second);
                }
                else if (sc.second.streamId.mode == StreamIDMode::Request) {
                    readers.append(socketContextToJson(sc.first, sc.second));
                }
            }
        }
    }

    response["code"] = 200;
    response["streamId"] = streamId;
    response["publish"] = publish;
    response["request"] = readers;

    http->responseData(response.toStyledString());

    return true;
}

bool SrtRouter::restApiGetConfiguration(std::shared_ptr<IWebServer::HttpRequest> http,
    const std_keyvalue& params, void* userData) {
    Logger::logPrint(LogInfo, "HTTP Request: {} /api/config", http->getMethod());

    Json::Value response;
    if (mConfig) {
        response = mConfig->get().toJson();
        response["code"] = 200;
    } else {
        response["code"] = 500;
        response["error"] = "Configuration not loaded";
    }

    http->responseData(response.toStyledString());
    return true;
}

bool SrtRouter::restApiPostConfiguration(std::shared_ptr<IWebServer::HttpRequest> http,
    const std_keyvalue& params, void* userData) {
    Logger::logPrint(LogInfo, "HTTP Request: {} /api/config", http->getMethod());

    Json::Value response;
    std::string body = http->getBody();

    SrtRouterConfigure newConfig;
    if (!newConfig.loadFromString(body)) {
        response["code"] = 400;
        response["error"] = "Invalid configuration JSON";
        http->responseData(response.toStyledString());
        return true;
    }

    Json::Value restartReasons(Json::arrayValue);
    bool needRestart = checkNeedRestart(newConfig, restartReasons);

    if (!newConfig.saveToFile(mConfigFilePath)) {
        response["code"] = 500;
        response["error"] = "Failed to save configuration to disk";
        http->responseData(response.toStyledString());
        return true;
    }

    if (mConfig) {
        mConfig->set(newConfig);
    }

    Logger::logPrint(LogInfo, "Configuration updated, needRestart={}", needRestart);

    response["code"] = 200;
    response["needRestart"] = needRestart;
    response["restartReasons"] = restartReasons;
    response["message"] = needRestart ? "Configuration saved, restart required"
                                      : "Configuration saved and applied";
    http->responseData(response.toStyledString());

    return true;
}

bool SrtRouter::checkNeedRestart(const SrtRouterConfigure& newConfig, Json::Value& reasons) {
    bool needRestart = false;

    auto add = [&](const std::string& reason) {
        needRestart = true;
        reasons.append(reason);
    };

    if (!mConfig) {
        add("no current configuration loaded");
        return needRestart;
    }

    SrtRouterConfigure curConfig = mConfig->get();

    if (newConfig.srt.enable != curConfig.srt.enable)
        add("srt.enable changed");
    if (newConfig.srt.listenPort != curConfig.srt.listenPort)
        add("srt.listenPort changed");

    if (newConfig.api.enable != curConfig.api.enable)
        add("api.enable changed");
    if (newConfig.api.listenPort != curConfig.api.listenPort)
        add("api.listenPort changed");

    if (newConfig.maxStreamsLimit != curConfig.maxStreamsLimit)
        add("maxStreamsLimit changed");

    if (newConfig.logging.logFilePath != curConfig.logging.logFilePath)
        add("logging.logFilePath changed");
    if (newConfig.logging.logLevel != curConfig.logging.logLevel)
        add("logging.logLevel changed");
    if (newConfig.logging.maxLogSize != curConfig.logging.maxLogSize)
        add("logging.maxLogSize changed");
    if (newConfig.logging.logKeepDays != curConfig.logging.logKeepDays)
        add("logging.logKeepDays changed");

    std::set<std::string> activePaths;
    {
        std::lock_guard<std::recursive_mutex> lock(mMutexSocket);
        for (const auto& sc : mMapSocketContext) {
            activePaths.insert(sc.second.streamId.path);
        }
    }

    auto encCredentialsChanged = [](const SrtEncryption& a, const SrtEncryption& b) {
        return a.publishPassphrase != b.publishPassphrase ||
               a.requestPassphrase != b.requestPassphrase ||
               a.encryptionType    != b.encryptionType;
    };

    std::map<std::string, const SrtEncryption*> oldEncMap, newEncMap;
    for (const auto& enc : curConfig.srt.encryptions)
        oldEncMap[enc.path] = &enc;
    for (const auto& enc : newConfig.srt.encryptions)
        newEncMap[enc.path] = &enc;

    // Check wildcard "*" change — affects all active paths not explicitly listed
    {
        bool oldHasWild = oldEncMap.count("*") > 0;
        bool newHasWild = newEncMap.count("*") > 0;
        if (oldHasWild != newHasWild) {
            if (!activePaths.empty())
                add("srt.encryptions: wildcard \"*\" entry " + std::string(newHasWild ? "added" : "removed"));
        } else if (oldHasWild && newHasWild) {
            if (encCredentialsChanged(*oldEncMap["*"], *newEncMap["*"]))
                if (!activePaths.empty())
                    add("srt.encryptions: wildcard \"*\" credentials changed");
        }
    }

    for (const auto& path : activePaths) {
        if (path == "*") continue;
        bool inOld = oldEncMap.count(path) > 0;
        bool inNew = newEncMap.count(path) > 0;
        if (inOld && !inNew)
            add("srt.encryptions: active path removed: " + path);
        else if (inOld && inNew) {
            if (encCredentialsChanged(*oldEncMap[path], *newEncMap[path]))
                add("srt.encryptions: active path credentials changed: " + path);
        }
    }

    return needRestart;
}
