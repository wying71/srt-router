#ifndef WEBSERVERBEASTIMPL_H
#define WEBSERVERBEASTIMPL_H

#include "../include/webserverinterface.h"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <string>
#include <map>
#include <set>
#include <list>
#include <vector>
#include <thread>
#include <atomic>
#include <tuple>
// #include <shared_mutex>

class WebServerBeastImpl;
// class WebSocketSession
class WebSocketSession : public IWebServer::WebSocket, public std::enable_shared_from_this<WebSocketSession>                         
{
public:
    explicit WebSocketSession(WebServerBeastImpl *pWebServer, boost::asio::ip::tcp::socket&& socket);
    explicit WebSocketSession(WebServerBeastImpl *pWebServer, boost::asio::ip::tcp::socket&& socket,
                              std::shared_ptr<boost::asio::ssl::context> ssl_context);
    virtual ~WebSocketSession();

public:
    template<class Body, class Allocator>
    void run(boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>> req);

public:
    std::string     getTarget() override;
    IWebServer::Uri getUri() override;
    int64_t         sendData(const void* data, int size, const char* channel = "") override;
    int64_t         bufferedBytes() override;
    void            close() override;
    bool            isClosed() const override;

private:
    void on_handshake(boost::beast::error_code ec); // SSL

    void on_accept(boost::beast::error_code ec);
    void do_read();
    void on_read(boost::beast::error_code ec, std::size_t bytes_transferred);
    void do_close(bool isManualClose = false);

private:
    WebServerBeastImpl* mWebServer = nullptr;
    std::shared_ptr<boost::beast::websocket::stream<boost::beast::tcp_stream>>                           mNosWsStream;
    std::shared_ptr<boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>> mSslWsStream;
    boost::beast::flat_buffer mBuffer;
    std::string mTarget;
    IWebServer::Uri mUri;
    std::atomic<bool> mIsClosed;
};

// class HttpSession
class HttpSession : public IWebServer::HttpRequest, public std::enable_shared_from_this<HttpSession>
{
public:
    HttpSession(WebServerBeastImpl *pWebServer, boost::asio::ip::tcp::socket&& socket);
    HttpSession(WebServerBeastImpl *pWebServer, boost::asio::ip::tcp::socket&& socket, 
                std::shared_ptr<boost::asio::ssl::context> ssl_context);
    virtual ~HttpSession();

public:
    void run();

public:
    int                 getVersion() override;
    std::string         getMethod() override;
    std_keyvalue        getHeaders() override;
    std::string         getTarget() override;
    IWebServer::Uri     getUri() override;
    const std::string&  getBody() override;
    int64_t             responseData(const std::string &body, const std_keyvalue* headers = NULL, int statusCode = 200) override;
    int64_t             responseFile(const char* filePath, const std_keyvalue* headers = NULL, int statusCode = 200) override;
    int64_t             responseHeader(int64_t contentLength, const std_keyvalue* headers = NULL, int statusCode = 200) override;
    int64_t             responseContent(const void* content, int64_t contentSize) override;
    void                close() override;
    bool                isClosed() const override;

private:
    void on_run(); // SSL
    void on_handshake(boost::beast::error_code ec); // SSL

    void do_read();
    void on_read(boost::beast::error_code ec, std::size_t bytes_transferred);
    void do_close(bool isManualClose = false);

private:
    WebServerBeastImpl* mWebServer = nullptr;
    std::shared_ptr<boost::beast::tcp_stream>                           mNosStream;
    std::shared_ptr<boost::beast::ssl_stream<boost::beast::tcp_stream>> mSslStream;
    boost::beast::flat_buffer m_buffer;
    boost::optional<boost::beast::http::request_parser<boost::beast::http::string_body>> mParser;
    boost::beast::http::message<true, boost::beast::http::string_body, boost::beast::http::fields> mRequest;
    IWebServer::Uri mUri;
    std_keyvalue mHeaders;
    std::atomic<bool> mIsClosed;
public:
    static std::atomic<int64_t> gCount;
};

// class Listener
class Listener : public std::enable_shared_from_this<Listener>
{
public:
    Listener(WebServerBeastImpl *pWebServer);
    virtual ~Listener();

public:
    void setCertificate(const char* certFile, const char* keyFile);
    int  start(uint16_t port, const char* bindAddress = "0.0.0.0", int threads = 4);
    void stop();

private:
    void do_accept();
    void on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket);

private:
    WebServerBeastImpl* mWebServer = nullptr;
    std::shared_ptr<boost::asio::io_context> mIoContext;
    std::shared_ptr<boost::asio::ip::tcp::acceptor> mAcceptor;
    std::shared_ptr<boost::asio::ssl::context> mSSLContext;
    std::string mCertFile;
    std::string mKeyFile;
    std::vector<std::thread> mThreads;
};

// class WebServerBeastImpl
struct websocket_client;
class WebServerBeastImpl : public IWebServer
{
public:
    WebServerBeastImpl();
    virtual ~WebServerBeastImpl();

public:
    void setCertificate(const char* certFile, const char* keyFile) override;
    void setWebSocketOpenHandler(WebSocketOpenHandler handler) override;
    void setWebSocketDataHandler(WebSocketDataHandler handler) override;
    void setWebSocketCloseHandler(WebSocketCloseHandler handler) override;
    void setHttpRequestOpenHandler(HttpRequestOpenHandler handler) override;
    void setHttpRequestCloseHandler(HttpRequestCloseHandler handler) override;
    void addHttpRequestRoutingHandler(HttpRequestRoutingHandler handler,
                                      const char* path = "/", const char* method = "*",
                                      void* userData = nullptr) override;
    void setHttpResponseHeader(const char* key, const char* value) override;
    void removeHttpResponseHeader(const char* key) override;
    void setLogOutputHandler(LogOutputHandler handler) override;
    void setAddress(const char* address = "0.0.0.0") override;
    int  start(int port, int threads = 4) override;
    void stop() override;

public:
    bool handleHttpRequestRouting(std::shared_ptr<IWebServer::HttpRequest> http);
    void addWebSocketSession(std::shared_ptr<IWebServer::WebSocket> ws);
    void removeWebSocketSession(std::shared_ptr<IWebServer::WebSocket> ws);
    void addHttpRequestSession(std::shared_ptr<IWebServer::HttpRequest> http);
    void removeHttpRequestSession(std::shared_ptr<IWebServer::HttpRequest> http);
    template<class Response>
    void setHttpResponseHeaders(Response &res);
    void logOutput(IWebServer::LogLevel level, const char* format, ...);

public:
    std::shared_ptr<Listener>   mListener;
    std::string                 mCertFile;
    std::string                 mKeyFile;
    std::string                 mAddress;
    WebSocketOpenHandler        mWebSocketOpenHandler;
    WebSocketDataHandler        mWebSocketDataHandler;
    WebSocketCloseHandler       mWebSocketCloseHandler;
    HttpRequestOpenHandler      mHttpRequestOpenHandler;
    HttpRequestCloseHandler     mHttpRequestCloseHandler;
    LogOutputHandler            mLogOutputHandler;
    // std::tuple<path, method, function>
    std::list<std::tuple<std::string, std::string, HttpRequestRoutingHandler, void*>> mRoutingHandlers;
    boost::shared_mutex mMutexHandlers;
    boost::shared_mutex mMutexSessions;
    std::set<std::shared_ptr<IWebServer::WebSocket>>   mWebSocketSessions;
    std::set<std::shared_ptr<IWebServer::HttpRequest>> mHttpRequestSessions;
    std_keyvalue    mHttpResponseHeaders;
    bool mIsStop = true;
    char* mLogBuffer = nullptr;
    int   mLogSize   = 4096;
    std::recursive_mutex mLogMutex;
};

#endif // WEBSERVERBEASTIMPL_H