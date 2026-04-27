#ifndef WEBSERVERINTERFACE_H
#define WEBSERVERINTERFACE_H
#include <stdint.h>
#include <string>
#include <functional>
#include <map>
#include <unordered_map>
#include <set>
#include <memory>

using std_keyvalue = std::unordered_map<std::string, std::string>;
/*
    IWebServer* pWebServer = WebServerCreate();

    pWebServer->setHttpResponseHeader("Content-Type", "application/json; charset=utf-8");
    pWebServer->setHttpResponseHeader("Cache-Control", "no-cache");

    pWebServer->setHttpRequestOpenHandler([](std::shared_ptr<IWebServer::HttpRequest> http)
    {
        http->responseData("{ \"code\": 200 }");
    });

    class MyHandler
    {
    public:
        MyHandler(IWebServer* pWebServer)
        {
            pWebServer->addHttpRequestRoutingHandler(
                std::bind(&MyHandler::onHandler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), "/path", "*");
        }
        bool onHandler(std::shared_ptr<IWebServer::HttpRequest> http, const std_keyvalue &params, void* userData)
        {
            printf("HTTP Routing: %s /path\n", http->getMethod().c_str());
            http->responseData("{ \"routing\": \"/path\" }");
            return true;
        }
    };
    MyHandler handler(pWebServer);

    int port = 8001;
    int ret = pWebServer->start(port, 8);
    
    // ...............

    pWebServer->stop();
    WebServerRelease(pWebServer);
*/

// class IWebServer
class IWebServer
{
public:
    IWebServer() { }
    virtual ~IWebServer() { }

public:
    struct Uri
    {
        std::string  scheme;
        std::string  username;
        std::string  password;
        std::string  host;
        std::string  port;
        std::string  path;
        std::string  query;
        std_keyvalue params;
        std::string  fragment;
    };
    enum class Handler_Type { HTTP, WEBSOCKET };

    class AbstractWebServerHandler
    {
    public:
        AbstractWebServerHandler() {}
        virtual ~AbstractWebServerHandler() { }

    public:
        virtual std::string     getTarget() = 0;
        virtual IWebServer::Uri getUri() = 0;
        virtual void            close() = 0;
        virtual bool            isClosed() const = 0;
        virtual Handler_Type    getType() const = 0;
    };

    // class WebSocket
    class WebSocket : public AbstractWebServerHandler
    {
    public:
        WebSocket() { }
        virtual ~WebSocket() { }
    public:
        virtual int64_t         sendData(const void* data, int size, const char* channel = "") = 0;
        virtual int64_t         bufferedBytes() = 0;
        virtual Handler_Type    getType() const { return Handler_Type::WEBSOCKET; }
    };

    // class HttpRequest
    class HttpRequest : public AbstractWebServerHandler
    {
    public:
        HttpRequest() { }
        virtual ~HttpRequest() { }
    public:
        // major = version / 10, minor = version % 10;
        virtual int                 getVersion() = 0;
        virtual std::string         getMethod() = 0;
        virtual std_keyvalue        getHeaders() = 0;
        virtual const std::string&  getBody() = 0;
        virtual Handler_Type        getType() const { return Handler_Type::HTTP; }

        virtual int64_t             responseData(const std::string &body, const std_keyvalue* headers = NULL, int statusCode = 200) = 0;
        virtual int64_t             responseFile(const char* filePath, const std_keyvalue* headers = NULL, int statusCode = 200) = 0;
        // chunked if contentLength is 0
        virtual int64_t             responseHeader(int64_t contentLength, const std_keyvalue* headers = NULL, int statusCode = 200) = 0;
        virtual int64_t             responseContent(const void* content, int64_t contentSize) = 0;
    };

    enum class LogLevel
    {
        None    = 0,
        Debug   = 1, 
        Info    = 2, 
        Warning = 3, 
        Error   = 4
    };

public:
    using WebSocketOpenHandler      = std::function<void(std::shared_ptr<IWebServer::WebSocket> ws)>;
    using WebSocketDataHandler      = std::function<void(std::shared_ptr<IWebServer::WebSocket> ws, const void* data, int size, const char* channel)>;
    using WebSocketCloseHandler     = std::function<void(std::shared_ptr<IWebServer::WebSocket> ws)>;
    using HttpRequestOpenHandler    = std::function<void(std::shared_ptr<IWebServer::HttpRequest> http)>;
    using HttpRequestCloseHandler   = std::function<void(std::shared_ptr<IWebServer::HttpRequest> http)>;
    using HttpRequestRoutingHandler = std::function<bool(std::shared_ptr<IWebServer::HttpRequest> http, 
                                                         const std_keyvalue &params, void* userData)>;
    using LogOutputHandler          = std::function<void(IWebServer::LogLevel level, const char* msg)>;

public:
    static const char*     version();
    static IWebServer::Uri parseUri(const char *uri, bool decode = true);
    static std::string     buildUri(const IWebServer::Uri &uri);

public:
    virtual void setCertificate(const char* certFile, const char* keyFile) = 0;
    virtual void setWebSocketOpenHandler(WebSocketOpenHandler handler) = 0;
    virtual void setWebSocketDataHandler(WebSocketDataHandler handler) = 0;
    virtual void setWebSocketCloseHandler(WebSocketCloseHandler handler) = 0;
    virtual void setHttpRequestOpenHandler(HttpRequestOpenHandler handler) = 0;
    virtual void setHttpRequestCloseHandler(HttpRequestCloseHandler handler) = 0;
    // method: GET|POST|PUT|PATCH|DELETE|*
    virtual void addHttpRequestRoutingHandler(HttpRequestRoutingHandler handler, 
                                              const char* path = "/", const char* method = "*",
                                              void *userData = nullptr) = 0;
    virtual void setHttpResponseHeader(const char* key, const char* value) = 0;
    virtual void removeHttpResponseHeader(const char* key) = 0;
    virtual void setLogOutputHandler(LogOutputHandler handler) = 0;
    // IPv4+IPv6: "::", IPv4: "0.0.0.0"
    virtual void setAddress(const char* address = "::") = 0;
    virtual int  start(int port, int threads = 4) = 0;
    virtual void stop() = 0;
};
typedef std::shared_ptr<IWebServer::HttpRequest> LibWebHttp;
typedef std::shared_ptr<IWebServer::WebSocket> LibWebSocket;
typedef std::shared_ptr<IWebServer::AbstractWebServerHandler> WebServerHandler;

#endif // WEBSERVERINTERFACE_H
