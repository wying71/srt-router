#include "configure.h"
#include "util.h"
#include <fstream>
#include <sstream>
#include <regex>

Json::Value SrtEncryption::toJson() const {
    Json::Value json;
    json["path"] = path;
    json["publishPassphrase"] = publishPassphrase;
    json["requestPassphrase"] = requestPassphrase;
    json["encryptionType"] = encryptionType;
    return json;
}

void SrtEncryption::fromJson(const Json::Value& json) {
    path = json.get("path", "").asString();
    publishPassphrase = json.get("publishPassphrase", "").asString();
    requestPassphrase = json.get("requestPassphrase", "").asString();
    encryptionType = json.get("encryptionType", "aes-128").asString();
}

Json::Value Srt::toJson() const {
    Json::Value json;
    json["enable"] = enable;
    json["listenPort"] = listenPort;
    Json::Value arr(Json::arrayValue);
    for (const auto& e : encryptions) {
        arr.append(e.toJson());
    }
    json["encryptions"] = arr;
    return json;
}

void Srt::fromJson(const Json::Value& json) {
    enable = json.get("enable", true).asBool();
    listenPort = json.get("listenPort", 8890).asInt();
    encryptions.clear();
    const Json::Value& arr = json["encryptions"];
    if (arr.isArray()) {
        for (const auto& e : arr) {
            SrtEncryption enc;
            enc.fromJson(e);
            encryptions.push_back(enc);
        }
    }
}

Json::Value Api::toJson() const {
    Json::Value json;
    json["enable"] = enable;
    json["listenPort"] = listenPort;
    return json;
}

void Api::fromJson(const Json::Value& json) {
    enable = json.get("enable", true).asBool();
    listenPort = json.get("listenPort", 3000).asInt();
}

Json::Value User::toJson() const {
    Json::Value json;
    json["username"] = username;
    json["password"] = password;
    return json;
}

void User::fromJson(const Json::Value& json) {
    username = json.get("username", "").asString();
    password = json.get("password", "").asString();
}

Json::Value ExternalAuth::toJson() const {
    Json::Value json;
    json["enable"] = enable;
    json["authHttpAddress"] = authHttpAddress;
    return json;
}

void ExternalAuth::fromJson(const Json::Value& json) {
    enable = json.get("enable", false).asBool();
    authHttpAddress = json.get("authHttpAddress", "").asString();
}

Json::Value Authentication::toJson() const {
    Json::Value json;
    Json::Value usersArray(Json::arrayValue);
    for (const auto& user : internalUsers) {
        usersArray.append(user.toJson());
    }
    json["internalUsers"] = usersArray;
    json["externalAuth"] = externalAuth.toJson();
    return json;
}

void Authentication::fromJson(const Json::Value& json) {
    internalUsers.clear();
    const Json::Value& usersArray = json["internalUsers"];
    if (usersArray.isArray()) {
        for (const auto& userJson : usersArray) {
            User user;
            user.fromJson(userJson);
            internalUsers.push_back(user);
        }
    }
    externalAuth.fromJson(json["externalAuth"]);
}

Json::Value Logging::toJson() const {
    Json::Value json;
    json["logLevel"] = logLevel;
    json["logFilePath"] = logFilePath;
    json["maxLogSize"] = static_cast<Json::Value::Int64>(maxLogSize);
    json["logKeepDays"] = logKeepDays;
    return json;
}

void Logging::fromJson(const Json::Value& json) {
    logLevel = json.get("logLevel", "info").asString();
    logFilePath = json.get("logFilePath", "logs/srtRouter.log").asString();
    maxLogSize = static_cast<size_t>(json.get("maxLogSize", 10485760).asInt64());
    logKeepDays = json.get("logKeepDays", 30).asInt();
}

Json::Value MetricsPost::toJson() const {
    Json::Value json;
    json["StreamMetricsEnabled"] = streamMetricsEnabled;
    json["postUrl"] = postUrl;
    json["errorReportingEnabled"] = errorReportingEnabled;
    return json;
}

void MetricsPost::fromJson(const Json::Value& json) {
    streamMetricsEnabled = json.get("StreamMetricsEnabled", true).asBool();
    postUrl = json.get("postUrl", "").asString();
    errorReportingEnabled = json.get("errorReportingEnabled", true).asBool();
}

bool SrtRouterConfigure::loadFromFile(const std::string& filePath) {
    std::string configPath = resolveConfigPath(filePath);

    std::ifstream file(configPath);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    std::string content = buffer.str();
    return loadFromString(content);
}

bool SrtRouterConfigure::loadFromString(const std::string& jsonStr) {
    std::string cleanedJson = removeJsonComments(jsonStr);

    Json::Reader reader;
    Json::Value root;

    if (!reader.parse(cleanedJson, root)) {
        return false;
    }

    return loadFromJson(root);
}

bool SrtRouterConfigure::loadFromJson(const Json::Value& json) {
    try {
        srt.fromJson(json["srt"]);
        api.fromJson(json["api"]);
        authentication.fromJson(json["authentication"]);
        logging.fromJson(json["logging"]);
        metricsPost.fromJson(json["metricsPost"]);
        maxStreamsLimit = json.get("maxStreamsLimit", 100).asInt();
        return true;
    } catch (...) {
        return false;
    }
}

std::string SrtRouterConfigure::toJsonString(bool formatted) const {
    Json::Value json = toJson();

    if (formatted) {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "    ";
        return Json::writeString(builder, json);
    } else {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        return Json::writeString(builder, json);
    }
}

Json::Value SrtRouterConfigure::toJson() const {
    Json::Value json;
    json["srt"] = srt.toJson();
    json["api"] = api.toJson();
    json["authentication"] = authentication.toJson();
    json["logging"] = logging.toJson();
    json["metricsPost"] = metricsPost.toJson();
    json["maxStreamsLimit"] = maxStreamsLimit;
    return json;
}

bool SrtRouterConfigure::saveToFile(const std::string& filePath) const {
    std::ofstream file(resolveConfigPath(filePath));
    if (!file.is_open()) {
        return false;
    }

    std::string jsonStr = toJsonString(true);
    file << jsonStr;
    file.close();
    return true;
}

std::string SrtRouterConfigure::resolveConfigPath(const std::string& filePath) {
    if (Util::isAbsolutePath(filePath)) {
        return filePath;
    }
#ifdef _WIN32
    const std::string path_separator = "\\";
#else
    const std::string path_separator = "/";
#endif
    return Util::getExecutableDirectory() + path_separator + filePath;
}

std::string SrtRouterConfigure::removeJsonComments(const std::string& jsonStr) {
    std::string result;
    result.reserve(jsonStr.size());

    bool inString = false;
    bool escaped = false;

    for (size_t i = 0; i < jsonStr.size(); ++i) {
        char c = jsonStr[i];

        if (escaped) {
            result += c;
            escaped = false;
            continue;
        }

        if (c == '"') {
            inString = !inString;
            result += c;
            continue;
        }

        if (inString) {
            if (c == '\\') {
                escaped = true;
            }
            result += c;
            continue;
        }

        if (c == '/' && i + 1 < jsonStr.size() && jsonStr[i + 1] == '/') {
            while (i < jsonStr.size() && jsonStr[i] != '\n') {
                ++i;
            }
            if (i < jsonStr.size()) {
                result += '\n';
            }
            continue;
        }

        result += c;
    }

    return result;
}
