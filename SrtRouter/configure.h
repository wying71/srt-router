#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include "json/json.h"

struct SrtEncryption {
    std::string path;
    std::string publishPassphrase;
    std::string requestPassphrase;
    std::string encryptionType = "aes-128"; // "aes-128" | "aes-192" | "aes-256"

    int pbkeylen() const {
        if (encryptionType == "aes-192") return 24;
        if (encryptionType == "aes-256") return 32;
        return 16; // aes-128 default
    }

    Json::Value toJson() const;
    void fromJson(const Json::Value& json);
};

struct Srt {
    bool enable = true;
    int listenPort = 8890;
    std::vector<SrtEncryption> encryptions;

    Json::Value toJson() const;
    void fromJson(const Json::Value& json);
};

struct Api {
    bool enable = true;
    int listenPort = 3000;

    Json::Value toJson() const;
    void fromJson(const Json::Value& json);
};

struct User {
    std::string username;
    std::string password;

    Json::Value toJson() const;
    void fromJson(const Json::Value& json);
};

struct ExternalAuth {
    bool enable = false;
    std::string authHttpAddress = "";

    Json::Value toJson() const;
    void fromJson(const Json::Value& json);
};

struct Authentication {
    std::vector<User> internalUsers;
    ExternalAuth externalAuth;

    Json::Value toJson() const;
    void fromJson(const Json::Value& json);
};

struct Logging {
    std::string logLevel = "info";
    std::string logFilePath = "logs/srtRouter.log";
    size_t maxLogSize = 10485760;  // 10MB
    int logKeepDays = 30;

    Json::Value toJson() const;
    void fromJson(const Json::Value& json);
};

struct MetricsPost {
    bool streamMetricsEnabled = true;
    std::string postUrl = "";
    bool errorReportingEnabled = true;

    Json::Value toJson() const;
    void fromJson(const Json::Value& json);
};

class SrtRouterConfigure {

public:
    SrtRouterConfigure() = default;
    ~SrtRouterConfigure() = default;

    SrtRouterConfigure(const SrtRouterConfigure&) = default;
    SrtRouterConfigure& operator=(const SrtRouterConfigure&) = default;

    SrtRouterConfigure(SrtRouterConfigure&&) = default;
    SrtRouterConfigure& operator=(SrtRouterConfigure&&) = default;

    Srt srt;
    Api api;
    Authentication authentication;
    Logging logging;
    MetricsPost metricsPost;
    int maxStreamsLimit = 100;

    bool loadFromFile(const std::string& filePath);

    bool loadFromString(const std::string& jsonStr);

    bool loadFromJson(const Json::Value& json);

    std::string toJsonString(bool formatted = true) const;

    Json::Value toJson() const;

    bool saveToFile(const std::string& filePath) const;

private:
    static std::string removeJsonComments(const std::string& jsonStr);
    static std::string resolveConfigPath(const std::string& filePath);
};

class SharedConfig {
public:
    explicit SharedConfig(const SrtRouterConfigure& cfg) : mData(cfg) {}

    SrtRouterConfigure get() const {
        std::lock_guard<std::mutex> lock(mMutex);
        return mData;
    }

    void set(const SrtRouterConfigure& cfg) {
        std::lock_guard<std::mutex> lock(mMutex);
        mData = cfg;
    }

private:
    mutable std::mutex mMutex;
    SrtRouterConfigure mData;
};

using SrtRouterConfigurePtr = std::shared_ptr<SharedConfig>;
