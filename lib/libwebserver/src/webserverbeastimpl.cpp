#include "webserverbeastimpl.h"
#include "libwebsocketcommon/common.h"
#include <list>
#include <vector>
#include <atomic>
#include <chrono>
#include <network/uri.hpp>
#include "stringutils.h"

#ifdef _WIN32
#define w_stricmp        _stricmp
#define w_strnicmp       _strnicmp
#else
#define w_stricmp        strcasecmp
#define w_strnicmp       strncasecmp
#endif

#define WEBSERVER_VERSION         "1.3.2021.0408"

static IWebServer::Uri toUri(const std::string &url, bool decode)
{
    auto decodeIf = [](const std::string &str, bool decode)
    {
        return decode ? network::detail::decode(str) : str; 
    };

    bool isPathOnly = false;
    auto str = url;
    if(url.substr(0, 1) == "/")
    {
        str = std::string("http://127.0.0.1") + url;
        isPathOnly = true;
    }
    IWebServer::Uri sUri;
    std::error_code ec;
    network::uri uri(str, ec);
    if(ec)
    {
        return sUri;
    }
    if(!isPathOnly)
    {
        auto user_info = uri.user_info().to_string();
        auto pos = user_info.find(":");
        sUri.scheme = uri.scheme().to_string();
        if(pos != std::string::npos)
        {
            sUri.username = decodeIf(user_info.substr(0, pos), decode);
            sUri.password = decodeIf(user_info.substr(pos + 1), decode);
        }
        else
        {
            sUri.username = decodeIf(user_info, decode);
        }
        sUri.host = decodeIf(uri.host().to_string(), decode);
        sUri.port = uri.port().to_string();
    }
    sUri.path      = decodeIf(uri.path().to_string(), decode);
    sUri.query     = decodeIf(uri.query().to_string(), decode);
    auto iter = uri.query_begin();
    while(iter != uri.query_end())
    {
        auto key   = decodeIf(iter->first.to_string(), decode);
        auto value = decodeIf(iter->second.to_string(), decode);
        sUri.params[key] = value; 
        iter ++;
    }
    sUri.fragment = decodeIf(uri.fragment().to_string(), decode);
    return sUri;
}

static std::string toUri(const IWebServer::Uri &uri)
{
    network::uri_builder builder;
    if(!uri.scheme.empty())
    {
        builder.scheme(uri.scheme);
    }
    if((!uri.username.empty()) || (!uri.password.empty()))
    {
        builder.user_info(uri.username + ":" + uri.password);
    }
    if(!uri.host.empty())
    {
        builder.host(uri.host);
    }
    if(!uri.port.empty())
    {
        builder.port(uri.port);
    }
    if(!uri.path.empty())
    {
        builder.path(uri.path);
    }
    if(!uri.query.empty())
    {
        builder.append_query(uri.query);
    }
    for(const auto &kv : uri.params)
    {
        builder.append_query_key_value_pair(kv.first, kv.second);
    }
    if(!uri.fragment.empty())
    {
        builder.fragment(uri.fragment);
    }
    return builder.uri().string();
}

// class WebSocketSession
WebSocketSession::WebSocketSession(WebServerBeastImpl* pWebServer, boost::asio::ip::tcp::socket&& socket)
    : mWebServer(pWebServer), mIsClosed(false)
{
    mNosWsStream = std::make_shared<boost::beast::websocket::stream<boost::beast::tcp_stream>>(std::move(socket));
}

WebSocketSession::WebSocketSession(WebServerBeastImpl* pWebServer, boost::asio::ip::tcp::socket&& socket,
                                   std::shared_ptr<boost::asio::ssl::context> ssl_context)
    : mWebServer(pWebServer), mIsClosed(false)
{
    mSslWsStream = std::make_shared<boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>>(std::move(socket), *ssl_context);
}

WebSocketSession::~WebSocketSession()
{
}

template<class Body, class Allocator>
void WebSocketSession::run(boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>> req)
{
    mWebServer->addWebSocketSession(shared_from_this());

    mTarget = req.target().to_string();
    mUri = toUri(mTarget, true);

    if(mNosWsStream)
    {
        mNosWsStream->set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::server));
        mNosWsStream->set_option(boost::beast::websocket::stream_base::decorator(
            [](boost::beast::websocket::response_type& res)
        {
            res.set(boost::beast::http::field::server, "Endeavor Streaming/1.1");
        })
        );
        mNosWsStream->auto_fragment(true);
        mNosWsStream->async_accept(req, boost::beast::bind_front_handler(&WebSocketSession::on_accept, shared_from_this()));
    }
    else
    {
        mSslWsStream->next_layer().async_handshake(boost::asio::ssl::stream_base::server,
                                                   boost::beast::bind_front_handler(&WebSocketSession::on_handshake, shared_from_this()));
    }
}

void WebSocketSession::on_handshake(boost::beast::error_code ec) // SSL
{
    if(ec)
    {
        mWebServer->logOutput(IWebServer::LogLevel::Error, "WebSocketSession - handshake error: %s.\n", ec.message().c_str());
        do_close();
        return;
    }
    mSslWsStream->set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::server));
    mSslWsStream->set_option(boost::beast::websocket::stream_base::decorator(
        [](boost::beast::websocket::response_type& res)
    {
        res.set(boost::beast::http::field::server, "Endeavor Streaming/1.1");
    })
    );
    mSslWsStream->auto_fragment(true);
    mSslWsStream->async_accept(boost::beast::bind_front_handler(&WebSocketSession::on_accept, shared_from_this()));
}

void WebSocketSession::on_accept(boost::beast::error_code ec)
{
    if(ec)
    {
        do_close();
        return;
    }
    if(mWebServer->mWebSocketOpenHandler)
    {
        mWebServer->mWebSocketOpenHandler(shared_from_this());
    }
    do_read();
}

void WebSocketSession::do_read()
{
    if(mNosWsStream)
    {
        mNosWsStream->async_read(mBuffer, boost::beast::bind_front_handler(&WebSocketSession::on_read, shared_from_this()));
    }
    else
    {
        mSslWsStream->async_read(mBuffer, boost::beast::bind_front_handler(&WebSocketSession::on_read, shared_from_this()));
    }
}

void WebSocketSession::on_read(boost::beast::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);
    if((ec == boost::beast::websocket::error::closed) || ec)
    {
        do_close();
        return;
    }
    auto bufferSize = mBuffer.size();
    if(bufferSize > 0)
    {
        if(mWebServer->mWebSocketDataHandler)
        {
            DataProtocol dp;
            bufferSize = dp.unpack((const uint8_t *)mBuffer.data().data(), bufferSize);
            if(bufferSize > 0)
            {
                mWebServer->mWebSocketDataHandler(shared_from_this(), dp.data, dp.size, dp.channel);
            }
            else
            {
                mWebServer->logOutput(IWebServer::LogLevel::Error, "WebSocketSession - Unpack data error.\n");
                bufferSize = mBuffer.size();
            }
        }
        mBuffer.consume(bufferSize);
    }
    do_read();
}

void WebSocketSession::do_close(bool isManualClose)
{
    if(mIsClosed)
    {
        return ;
    }
    if(!isManualClose)
    {
        if(mWebServer->mWebSocketCloseHandler)
        {
            mWebServer->mWebSocketCloseHandler(shared_from_this());
        }
        mWebServer->removeWebSocketSession(shared_from_this());
    }
    boost::beast::error_code ec;
    if(mNosWsStream)
    {
        mNosWsStream->close(boost::beast::websocket::close_code::normal, ec);
    }
    else
    {
        mSslWsStream->close(boost::beast::websocket::close_code::normal, ec);
    }
    mIsClosed = true;
}

std::string WebSocketSession::getTarget()
{
    return mTarget;
}

IWebServer::Uri WebSocketSession::getUri()
{
    return mUri;
}

int64_t WebSocketSession::sendData(const void* data, int size, const char* channel)
{
    if(mIsClosed)
    {
        return -1;
    }
    std::vector<uint8_t> vecData;
    const void* outData = nullptr;
    int outSize = 0;
    if(wss_packetData(data, size, channel, vecData, &outData, &outSize) <= 0)
    {
        return -1;
    }
    boost::beast::flat_buffer buffer;
    if(mNosWsStream)
    {
        mNosWsStream->binary(true);
    }
    else
    {
        mSslWsStream->binary(true);
    }
    auto b = buffer.prepare(outSize);
    memcpy(b.data(), outData, outSize);
    buffer.commit(outSize);
    int64_t ret = 0;
    try
    {
        if(mNosWsStream)
        {
            ret = mNosWsStream->write(buffer.data());
        }
        else
        {
            ret = mSslWsStream->write(buffer.data());
        }
    }
    catch(const std::exception &e)
    {
        mWebServer->logOutput(IWebServer::LogLevel::Error, "WebSocketSession - sendData: %s.\n", e.what());
        do_close();
        ret = -1;
    }
    return ret;
}

int64_t WebSocketSession::bufferedBytes()
{
    if(mNosWsStream)
    {
        return mNosWsStream->write_buffer_bytes();
    }
    else
    {
        return mSslWsStream->write_buffer_bytes();
    }
}

void WebSocketSession::close()
{
    do_close(true);
}

bool WebSocketSession::isClosed() const
{
    return mIsClosed;
}

// class HttpSession
std::atomic<int64_t> HttpSession::gCount(0);
HttpSession::HttpSession(WebServerBeastImpl* pWebServer, boost::asio::ip::tcp::socket&& socket)
    : mWebServer(pWebServer), mIsClosed(false)
{
    mNosStream = std::make_shared<boost::beast::tcp_stream>(std::move(socket));
    gCount ++;
}

HttpSession::HttpSession(WebServerBeastImpl* pWebServer, boost::asio::ip::tcp::socket&& socket,
                         std::shared_ptr<boost::asio::ssl::context> ssl_context)
    : mWebServer(pWebServer), mIsClosed(false)
{
    mSslStream = std::make_shared<boost::beast::ssl_stream<boost::beast::tcp_stream>>(std::move(socket), *ssl_context);
    gCount ++;
}

HttpSession::~HttpSession()
{
    gCount --;
}

void HttpSession::run()
{
    mWebServer->addHttpRequestSession(shared_from_this());
    if(mNosStream)
    {
        boost::asio::dispatch(mNosStream->get_executor(),
                              boost::beast::bind_front_handler(&HttpSession::do_read, shared_from_this()));
    }
    else
    {
        boost::asio::dispatch(mSslStream->get_executor(),
                              boost::beast::bind_front_handler(&HttpSession::on_run, shared_from_this()));
    }
}

int HttpSession::getVersion()
{
    return mRequest.version();
}

std::string HttpSession::getMethod()
{
    return mRequest.method_string().to_string();
}

std_keyvalue HttpSession::getHeaders()
{
    return mHeaders;
}

std::string HttpSession::getTarget()
{
    return mRequest.target().to_string();
}

IWebServer::Uri HttpSession::getUri()
{
    return mUri;
}

const std::string& HttpSession::getBody()
{
    return mRequest.body();
}

int64_t HttpSession::responseData(const std::string &body, const std_keyvalue* headers, int statusCode)
{
    if(mIsClosed)
    {
        return -1;
    }
    boost::beast::http::response<boost::beast::http::string_body> res{ boost::beast::http::status(statusCode), mRequest.version() };
    res.set(boost::beast::http::field::server, "Endeavor Streaming/1.1");
    res.set(boost::beast::http::field::content_type, "text/html;charset=utf-8");
    mWebServer->setHttpResponseHeaders(res);
    if(headers != nullptr)
    {
        for(const auto& header : *headers)
        {
            res.set(header.first, header.second);
        }
    }
    res.body() = body;
    res.prepare_payload();
    int64_t ret = 0;
    try
    {
        mWebServer->logOutput(IWebServer::LogLevel::Info, "HttpSession - responseData: %s %s %d.\n",
                              this->getMethod().c_str(), this->getTarget().c_str(), statusCode);
        if(mNosStream)
        {
            ret = boost::beast::http::write(*mNosStream, res);
        }
        else
        {
            ret = boost::beast::http::write(*mSslStream, res);
        }
    }
    catch(const std::exception &e)
    {
        mWebServer->logOutput(IWebServer::LogLevel::Error, "HttpSession - responseData: %s.\n", e.what());
        ret = -1;
    }
    return ret;
}

int64_t HttpSession::responseFile(const char* filePath, const std_keyvalue* headers, int statusCode)
{
    if(mIsClosed)
    {
        return -1;
    }
    boost::beast::error_code ec;
    boost::beast::http::file_body::value_type body;
    body.open(filePath, boost::beast::file_mode::scan, ec);

    if(ec == boost::beast::errc::no_such_file_or_directory)
    {
        auto const not_found = [&]()
        {
            boost::beast::http::response<boost::beast::http::string_body> res { boost::beast::http::status::not_found, mRequest.version() };
            res.set(boost::beast::http::field::server, "Endeavor Streaming/1.1");
            res.set(boost::beast::http::field::content_type, "text/html");
            res.body() = "Not found.";
            res.prepare_payload();
            return res;
        };
        int64_t ret = 0;
        try
        {
            mWebServer->logOutput(IWebServer::LogLevel::Info, "HttpSession - responseFile: %s %d.\n",
                                  filePath, (int)boost::beast::http::status::not_found);
            if(mNosStream)
            {
                ret = boost::beast::http::write(*mNosStream, not_found());
            }
            else
            {
                ret = boost::beast::http::write(*mSslStream, not_found());
            }
        }
        catch(const std::exception &e)
        {
            printf("HttpSession::responseFile: %s\n", e.what());
            do_close();
            ret = -1;
        }
        return ret;
    }
    if(ec)
    {
        auto const server_error = [&]()
        {
            boost::beast::http::response<boost::beast::http::string_body> res { boost::beast::http::status::internal_server_error, mRequest.version() };
            res.set(boost::beast::http::field::server, "Endeavor Streaming/1.1");
            res.set(boost::beast::http::field::content_type, "text/html");
            res.body() = "Server error.";
            res.prepare_payload();
            return res;
        };
        int64_t ret = 0;
        try
        {
            mWebServer->logOutput(IWebServer::LogLevel::Info, "HttpSession - responseFile: %s %d.\n",
                                  filePath, (int)boost::beast::http::status::internal_server_error);
            if(mNosStream)
            {
                ret = boost::beast::http::write(*mNosStream, server_error());
            }
            else
            {
                ret = boost::beast::http::write(*mSslStream, server_error());
            }
        }
        catch(const std::exception &e)
        {
            printf("HttpSession::responseFile: %s\n", e.what());
            do_close();
            ret = -1;
        }
        return ret;
    }
    auto const bodySize = body.size();

    boost::beast::http::response<boost::beast::http::file_body> res{
        std::piecewise_construct, std::make_tuple(std::move(body)), 
        std::make_tuple(boost::beast::http::status::ok, 11) };
    res.set(boost::beast::http::field::server, "Endeavor Streaming/1.1");
    res.set(boost::beast::http::field::content_type, "text/html;charset=utf-8");
    res.content_length(bodySize);
    mWebServer->setHttpResponseHeaders(res);
    if(headers != nullptr)
    {
        for(const auto& header : *headers)
        {
            res.set(header.first, header.second);
        }
    }
    int64_t ret = 0;
    try
    {
        mWebServer->logOutput(IWebServer::LogLevel::Info, "HttpSession - responseFile: %s %d.\n",
                              filePath, (int)boost::beast::http::status::ok);
        if(mNosStream)
        {
            ret = boost::beast::http::write(*mNosStream, res);
        }
        else
        {
            ret = boost::beast::http::write(*mSslStream, res);
        }
    }
    catch(const std::exception& e)
    {
        mWebServer->logOutput(IWebServer::LogLevel::Error, "HttpSession - responseFile: %s.\n", e.what());
        do_close();
        ret = -1;
    }
    return ret;
}

int64_t HttpSession::responseHeader(int64_t contentLength, const std_keyvalue* headers, int statusCode)
{
    if(mIsClosed)
    {
        return -1;
    }
    boost::beast::http::response<boost::beast::http::empty_body> res{ boost::beast::http::status(statusCode), mRequest.version() };
    res.set(boost::beast::http::field::server, "Endeavor Streaming/1.1");
    res.set(boost::beast::http::field::content_type, "text/html;charset=utf-8");
    mWebServer->setHttpResponseHeaders(res);
    if(headers != nullptr)
    {
        for(const auto& header : *headers)
        {
            res.set(header.first, header.second);
        }
    }
    if(contentLength == 0)
    {
        res.chunked(true);
    }
    else if(contentLength > 0)
    {
        res.content_length(contentLength);
    }
    int64_t ret = 0;
    try
    {
        mWebServer->logOutput(IWebServer::LogLevel::Info, "HttpSession - responseHeader: %s %lld %d.\n",
                              this->getTarget().c_str(), contentLength, statusCode);
        boost::beast::http::response_serializer<boost::beast::http::empty_body> sr{ res };
        if(mNosStream)
        {
            ret = boost::beast::http::write_header(*mNosStream, sr);
        }
        else
        {
            ret = boost::beast::http::write_header(*mSslStream, sr);
        }
    }
    catch(const std::exception &e)
    {
        mWebServer->logOutput(IWebServer::LogLevel::Error, "HttpSession - responseHeader: %s.\n", e.what());
        do_close();
        ret = -1;
    }
    return ret;
}

int64_t HttpSession::responseContent(const void* content, int64_t contentSize)
{
    if(mIsClosed)
    {
        return -1;
    }
    int64_t ret = 0;
    try
    {
        mWebServer->logOutput(IWebServer::LogLevel::Info, "HttpSession - responseHeader: %s %lld.\n",
                              this->getTarget().c_str(), contentSize);
        if(mNosStream)
        {
            ret = boost::beast::net::write(*mNosStream, boost::beast::net::const_buffer(content, contentSize));
        }
        else
        {
            ret = boost::beast::net::write(*mSslStream, boost::beast::net::const_buffer(content, contentSize));
        }
    }
    catch(const std::exception &e)
    {
        mWebServer->logOutput(IWebServer::LogLevel::Error, "HttpSession - responseContent: %s.\n", e.what());
        do_close();
        ret = -1;
    }
    return ret;
}

void HttpSession::close()
{
    do_close(true);
}

bool HttpSession::isClosed() const
{
    return mIsClosed;
}

void HttpSession::on_run() // SSL
{
    // Set the timeout.
    boost::beast::get_lowest_layer(*mSslStream).expires_after(std::chrono::seconds(30));
    // Perform the SSL handshake
    mSslStream->async_handshake(boost::asio::ssl::stream_base::server,
        boost::beast::bind_front_handler(&HttpSession::on_handshake, shared_from_this()));
}

void HttpSession::on_handshake(boost::beast::error_code ec) // SSL
{
    if(ec)
    {
        mWebServer->logOutput(IWebServer::LogLevel::Error, "HttpSession - handshake error: %s.\n", ec.message().c_str());
        do_close();
        return ;
    }
    do_read();
}

void HttpSession::do_read()
{
    mParser.emplace();
    mParser->body_limit(10 * 1024 * 1024);
    if(mNosStream)
    {
        boost::beast::http::async_read(*mNosStream, m_buffer, *mParser, boost::beast::bind_front_handler(&HttpSession::on_read, shared_from_this()));
    }
    else
    {
        boost::beast::get_lowest_layer(*mSslStream).expires_after(std::chrono::seconds(30));
        boost::beast::http::async_read(*mSslStream, m_buffer, *mParser, boost::beast::bind_front_handler(&HttpSession::on_read, shared_from_this()));
    }
}

void HttpSession::on_read(boost::beast::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);
    if(ec == boost::beast::http::error::end_of_stream)
    {
        do_close();
        return;
    }
    if(ec)
    {
        do_close();
        return;
    }
    if(boost::beast::websocket::is_upgrade(mParser->get()))
    {
        if(mNosStream)
        {
            std::make_shared<WebSocketSession>(mWebServer, mNosStream->release_socket())->run(mParser->release());
        }
        else
        {
            std::make_shared<WebSocketSession>(mWebServer, boost::beast::get_lowest_layer(*mSslStream).release_socket())->run(mParser->release());
        }
        mWebServer->removeHttpRequestSession(shared_from_this());
        return;
    }
    if(!mParser->is_done())
    {
        do_read();
        return;
    }

    mRequest = mParser->release();
    mUri = toUri(mRequest.target().to_string(), true);
    for(const auto& header : mRequest)
    {
        auto key   = header.name_string().to_string();
        auto value = header.value().to_string();
        mHeaders[key] = value;
    }
    std::string strAddr;
    int port = 0;
    if(mNosStream)
    {
        auto endpoint = mNosStream->socket().remote_endpoint(ec);
        strAddr = endpoint.address().to_string(ec);
        port = endpoint.port();
    }
    else
    {
        auto endpoint = boost::beast::get_lowest_layer(*mSslStream).socket().remote_endpoint(ec);
        strAddr = endpoint.address().to_string(ec);
        port = endpoint.port();
    }
    mWebServer->logOutput(IWebServer::LogLevel::Debug, "HttpSession - Request(%s@%d): %s %s.\n",
                          strAddr.c_str(), port, 
                          this->getMethod().c_str(), this->getTarget().c_str());
    if(!mWebServer->handleHttpRequestRouting(shared_from_this()))
    {
        mWebServer->logOutput(IWebServer::LogLevel::Info, "HttpSession - Unhandled routing: %s %s.\n",
                              this->getMethod().c_str(), this->getTarget().c_str());
        if(mWebServer->mHttpRequestOpenHandler)
        {
            mWebServer->mHttpRequestOpenHandler(shared_from_this());
        }
    }
    do_read();
}

void HttpSession::do_close(bool isManualClose)
{
    if(mIsClosed)
    {
        return ;
    }
    if(!isManualClose)
    {
        if(mWebServer->mHttpRequestCloseHandler)
        {
            mWebServer->mHttpRequestCloseHandler(shared_from_this());
        }
        mWebServer->removeHttpRequestSession(shared_from_this());
    }
    boost::beast::error_code ec;
    if(mNosStream)
    {
        mNosStream->socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
    }
    else
    {
        boost::beast::get_lowest_layer(*mSslStream).socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
    }
    mIsClosed = true;
}

// class Listener
Listener::Listener(WebServerBeastImpl* pWebServer)
    : mWebServer(pWebServer)
{
}

Listener::~Listener()
{
}

void Listener::setCertificate(const char* certFile, const char* keyFile)
{
    mCertFile = (certFile != nullptr) ? std::string(certFile) : "";
    mKeyFile = (keyFile != nullptr) ? std::string(keyFile) : "";
}

int Listener::start(uint16_t port, const char* bindAddress, int threads)
{
    mWebServer->logOutput(IWebServer::LogLevel::Debug, "Listener - start: %s@%d, threads: %d.\n", 
                          bindAddress, (int)port, threads);

    auto address = boost::asio::ip::make_address(bindAddress);
    auto endpoint = boost::asio::ip::tcp::endpoint{ address, port };
    mIoContext = std::make_shared<boost::asio::io_context>(threads);
    mAcceptor = std::make_shared<boost::asio::ip::tcp::acceptor>(boost::asio::make_strand(*mIoContext));
    if((!mCertFile.empty()) && (!mKeyFile.empty()))
    {
        mSSLContext = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);
        mSSLContext->set_options(
            boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::no_sslv3 |
            boost::asio::ssl::context::single_dh_use);
        boost::system::error_code ec1;
        boost::system::error_code ec2;
        mSSLContext->use_certificate_file(mCertFile, boost::asio::ssl::context::file_format::pem, ec1);
        mSSLContext->use_private_key_file(mKeyFile, boost::asio::ssl::context::file_format::pem, ec2);
        if(ec1 || ec2)
        {
            mSSLContext = nullptr;
        }
    }
    boost::system::error_code ec;
    mAcceptor->open(endpoint.protocol(), ec);
    if(ec)
    {
        return -1;
    }
#ifndef _WIN32
    mAcceptor->set_option(boost::asio::socket_base::reuse_address(true), ec);
    if(ec)
    {
        return -2;
    }
#endif
    mAcceptor->bind(endpoint, ec);
    if(ec)
    {
        return -3;
    }
    mAcceptor->listen(boost::asio::socket_base::max_listen_connections, ec);
    if(ec)
    {
        return -4;
    }
    boost::asio::dispatch(mAcceptor->get_executor(), boost::beast::bind_front_handler(&Listener::do_accept, shared_from_this()));
    mThreads.reserve(threads);
    for(auto i = 0; i < threads; i ++)
    {
        mThreads.emplace_back([&]
        {
            mIoContext->run();
        });
    }
    return 0;
}

void Listener::stop()
{
    if(mAcceptor)
    {
        mAcceptor->close();
    }
    if(mIoContext)
    {
        mIoContext->stop();
    }
    while(!mThreads.empty())
    {
        auto iter = mThreads.begin();
        if(iter->joinable())
        {
            iter->join();
        }
        mThreads.erase(iter);
    }
    mWebServer->logOutput(IWebServer::LogLevel::Debug, "Listener - stop.\n");
}

void Listener::do_accept()
{
    mAcceptor->async_accept(boost::asio::make_strand(*mIoContext), boost::beast::bind_front_handler(&Listener::on_accept, shared_from_this()));
}

void Listener::on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket)
{
    if(ec)
    {
        return ;
    }
    mWebServer->mMutexSessions.lock_shared();
    if(mWebServer->mIsStop)
    {
        mWebServer->mMutexSessions.unlock_shared();
        return;
    }
    mWebServer->mMutexSessions.unlock_shared();
    if(!mSSLContext)
    {
        std::make_shared<HttpSession>(mWebServer, std::move(socket))->run();
    }
    else
    {
        std::make_shared<HttpSession>(mWebServer, std::move(socket), mSSLContext)->run();
    }
    do_accept();
}

// class IWebServer
const char* IWebServer::version()
{
    return WEBSERVER_VERSION;
}

IWebServer::Uri IWebServer::parseUri(const char *uri, bool decode)
{
    return toUri((uri != nullptr) ? uri : "", decode);
}

std::string IWebServer::buildUri(const IWebServer::Uri &uri)
{
    return toUri(uri);
}

// class WebServerBeastImpl
WebServerBeastImpl::WebServerBeastImpl()
    : mIsStop(true)

{
    mAddress = "::";
    mLogBuffer = new char[mLogSize];
}

WebServerBeastImpl::~WebServerBeastImpl()
{
    if(mListener)
    {
        mListener = nullptr;
    }
    logOutput(IWebServer::LogLevel::Debug, "WebServerBeast - HttpSession object counter: %lld.\n", (int64_t)(HttpSession::gCount));
    delete []mLogBuffer;
}

void WebServerBeastImpl::setCertificate(const char* certFile, const char* keyFile)
{
    mCertFile = (certFile != nullptr) ? std::string(certFile) : "";
    mKeyFile = (keyFile != nullptr) ? std::string(keyFile) : "";
}

void WebServerBeastImpl::setWebSocketOpenHandler(WebSocketOpenHandler handler)
{
    mWebSocketOpenHandler = handler;
}

void WebServerBeastImpl::setWebSocketDataHandler(WebSocketDataHandler handler)
{
    mWebSocketDataHandler = handler;
}

void WebServerBeastImpl::setWebSocketCloseHandler(WebSocketCloseHandler handler)
{
    mWebSocketCloseHandler = handler;
}

void WebServerBeastImpl::setHttpRequestOpenHandler(HttpRequestOpenHandler handler)
{
    mHttpRequestOpenHandler = handler;
}

void WebServerBeastImpl::setHttpRequestCloseHandler(HttpRequestCloseHandler handler)
{
    mHttpRequestCloseHandler = handler;
}

void WebServerBeastImpl::addHttpRequestRoutingHandler(HttpRequestRoutingHandler handler,
                                                      const char *path, const char *method,
                                                      void* userData)
{
    boost::unique_lock<boost::shared_mutex> locker(mMutexHandlers);
    mRoutingHandlers.push_back(std::make_tuple(path, method, handler, userData));
}

void WebServerBeastImpl::setHttpResponseHeader(const char* key, const char* value)
{
    if((key == nullptr) || (strlen(key) == 0))
    {
        return;
    }
    auto strKey = std::string(key);
    mHttpResponseHeaders[strKey] = value;
}

void WebServerBeastImpl::removeHttpResponseHeader(const char* key)
{
    if((key == nullptr) || (strlen(key) == 0))
    {
        return;
    }
    auto strKey = std::string(key);
    if(mHttpResponseHeaders.find(strKey) != mHttpResponseHeaders.end())
    {
        mHttpResponseHeaders.erase(strKey);
    }
}

void WebServerBeastImpl::setLogOutputHandler(LogOutputHandler handler)
{
    std::lock_guard<std::recursive_mutex> locker(mLogMutex);
    mLogOutputHandler = handler;
}

void WebServerBeastImpl::setAddress(const char* address)
{
    mAddress = (address != nullptr) ? address : "::";
}

int WebServerBeastImpl::start(int port, int threads)
{
    logOutput(IWebServer::LogLevel::Debug, "Version: %s.\n", this->version());
    mIsStop = false;
    mListener = std::make_shared<Listener>(this);
    mListener->setCertificate(mCertFile.c_str(), mKeyFile.c_str());
    return mListener->start(port, mAddress.c_str(), threads);
}

void WebServerBeastImpl::stop()
{
    mMutexSessions.lock();
    mIsStop = true;
    mMutexSessions.unlock();

    if(mListener)
    {
        mListener->stop();
    }

    mMutexSessions.lock();
    while(!mWebSocketSessions.empty())
    {
        auto iter = mWebSocketSessions.begin();
        (*iter)->close();
        mWebSocketSessions.erase(iter);
    }
    while(!mHttpRequestSessions.empty())
    {
        auto iter = mHttpRequestSessions.begin();
        (*iter)->close();
        mHttpRequestSessions.erase(iter);
    }
    mWebSocketSessions.clear();
    mHttpRequestSessions.clear();
    mMutexSessions.unlock();

    mMutexHandlers.lock();
    mRoutingHandlers.clear();
    mMutexHandlers.unlock();
}

bool WebServerBeastImpl::handleHttpRequestRouting(std::shared_ptr<IWebServer::HttpRequest> http)
{
    const auto uri = http->getUri();
    const auto method = http->getMethod();
    auto paths = stringSplit<std::string>(uri.path, "/", true, false, false);
    // std::pair<path, param>
    auto mapParams = [](const std::string &path, std::vector<std::pair<std::string, std::string>> &kvs) -> void
    {
        auto vPaths = stringSplit<std::string>(path, "/", true, false, false);
        for(const auto &v : vPaths)
        {
            if(v.substr(0, 1) == ":")
            {
                kvs.push_back(std::make_pair("", v.substr(1)));
            }
            else
            {
                kvs.push_back(std::make_pair(v, ""));
            }
        }
    };

    auto handleRouting = [&](const std::tuple<std::string, std::string, HttpRequestRoutingHandler, void*>& handler) -> bool
    {
        const auto& vPath     = std::get<0>(handler);
        const auto& vMethod   = std::get<1>(handler);
        const auto& vHandler  = std::get<2>(handler);
        const auto  vUserData = std::get<3>(handler);

        // check method
        if((w_stricmp(vMethod.c_str(), "*") != 0) && (w_stricmp(method.c_str(), vMethod.c_str()) != 0))
        {
            return false;
        }
        // convert path to array<path, param>
        std::vector<std::pair<std::string, std::string>> kvs;
        mapParams(vPath, kvs);
        const auto kvSize = kvs.size();
        if(paths.size() != kvSize)
        {
            return false;
        }
        std_keyvalue params;
        for(size_t i = 0; i < kvSize; i ++)
        {
            const auto &p1 = paths[i];
            const auto &p2 = kvs[i];
            if(p2.second.empty())
            {
                if(w_stricmp(p1.c_str(), p2.first.c_str()) != 0)
                {
                    return false;
                }
            }
            else
            {
                params[p2.second] = p1;
            }
        }
        return vHandler(http, params, vUserData);
    };

    boost::shared_lock<boost::shared_mutex> locker(mMutexHandlers);
    for(const auto& handler : mRoutingHandlers)
    {
        if(handleRouting(handler))
        {
            return true;
        }
    }

    return false;
}

void WebServerBeastImpl::addWebSocketSession(std::shared_ptr<IWebServer::WebSocket> ws)
{
    mMutexSessions.lock();
    if(mWebSocketSessions.find(ws) == mWebSocketSessions.end())
    {
        mWebSocketSessions.insert(ws);
    }
    mMutexSessions.unlock();
}

void WebServerBeastImpl::removeWebSocketSession(std::shared_ptr<IWebServer::WebSocket> ws)
{
    mMutexSessions.lock();
    if(mWebSocketSessions.find(ws) != mWebSocketSessions.end())
    {
        mWebSocketSessions.erase(ws);
    }
    mMutexSessions.unlock();
}

void WebServerBeastImpl::addHttpRequestSession(std::shared_ptr<IWebServer::HttpRequest> http)
{
    mMutexSessions.lock();
    if(mHttpRequestSessions.find(http) == mHttpRequestSessions.end())
    {
        mHttpRequestSessions.insert(http);
    }
    mMutexSessions.unlock();
}

void WebServerBeastImpl::removeHttpRequestSession(std::shared_ptr<IWebServer::HttpRequest> http)
{
    mMutexSessions.lock();
    if(mHttpRequestSessions.find(http) != mHttpRequestSessions.end())
    {
        mHttpRequestSessions.erase(http);
    }
    mMutexSessions.unlock();
}

template<class Response>
void WebServerBeastImpl::setHttpResponseHeaders(Response &res)
{
    for(const auto& header : mHttpResponseHeaders)
    {
        res.set(header.first, header.second);
    }
}

void WebServerBeastImpl::logOutput(IWebServer::LogLevel level, const char* format, ...)
{
    std::lock_guard<std::recursive_mutex> locker(mLogMutex);
    if(mLogOutputHandler)
    {
        memset(mLogBuffer, 0, mLogSize);
        va_list ap;
        va_start(ap, format);
        vsnprintf(mLogBuffer, (mLogSize - 1), format, ap);
        va_end(ap);
        size_t len = strlen(mLogBuffer);
        if((len <= (mLogSize - 1)) && (mLogBuffer[len - 1] != '\n'))
        {
            mLogBuffer[len] = '\n';
        }
        mLogOutputHandler(level, mLogBuffer);
    }
}
