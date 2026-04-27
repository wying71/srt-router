#include "SrtSocket.h"
#include <string>
#include <thread>
#include <chrono>
#include <list>
#include <set>
#include <vector>
#include <atomic>
#include <string.h>
#include "srt/udt.h"
#include "stringutils.h"

#ifndef _WIN32
#define _stricmp    strcasecmp
#endif

#define LOG_NAME        "Srt socket"

#define SRT_BUFFER_SIZE 1*1024*1024
static bool YES         = true;
static bool NO          = false;
static int  TIMEOUT     = 100;
static std::atomic<bool> gIsFirstRun(true);
static char		g_listenerLocaladdr[265] = {0};

#define SRT_SET_OPTION(sock, opt, key, len)         \
    ret = srt_setsockflag(sock, opt, key, len);     \
    if(ret == SRT_ERROR)                            \
    {                                               \
        sl_log(SL_LEVEL::SL_ERROR, LOG_NAME,        \
            "srt_setsockflag(0x%x) error, %s\n",    \
            sock, srt_getlasterror_str());          \
        break;                                      \
    }


#define SRT_CLOSE(ss)                               \
    if(ss != SRT_INVALID_SOCK)                      \
    {                                               \
        srt_close(ss);                              \
        ss = SRT_INVALID_SOCK;                      \
    }


static inline int limitRange(int v, int a, int b)
{
    return ((v < a) ? a : ((v > b) ? b : v));
}

static bool get_sockaddr(const char* host, int port, struct sockaddr_in& sa)
{
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = sa.sin_family;

    struct addrinfo* presult = nullptr;
    int ret = getaddrinfo(host, nullptr, &hints, &presult);
    if((ret == 0) && (presult != nullptr) && (presult->ai_addr != nullptr) && (presult->ai_addrlen >= sizeof(sockaddr_in)))
    {
        sa.sin_addr.s_addr = ((struct sockaddr_in *)presult->ai_addr)->sin_addr.s_addr;
    }
    else
    {
        ret = -1;
        sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "get sock address failed(%s)", host);
    }
    freeaddrinfo(presult);
    return (ret == 0);
}
/*
static bool get_sockaddr_v6(const char* host, int port, struct sockaddr_in6& sa)
{
    memset(&sa, 0, sizeof(sa));
    sa.sin6_family = AF_INET6;
    sa.sin6_port = htons(port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = sa.sin6_family;

    struct addrinfo* presult = nullptr;
    int ret = getaddrinfo(host, nullptr, &hints, &presult);
    if((ret == 0) && (presult != nullptr) && (presult->ai_addr != nullptr) && (presult->ai_addrlen >= sizeof(sockaddr_in6)))
    {
        memcpy(sa.sin6_addr.s6_addr, ((struct sockaddr_in6 *)presult->ai_addr)->sin6_addr.s6_addr, sizeof(sa.sin6_addr));
    }
    else
    {
        ret = -1;
        sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "get sock address failed(%s)", host);
    }
    freeaddrinfo(presult);
    return (ret == 0);
}
*/
// "host:port"
static bool parse_hostport(const std::string& hostport, std::string& host, int& port)
{
    host = "0.0.0.0";
    port = 0;
    if(!hostport.empty())
    {
        int pos = (int)hostport.find(':');
        if(pos < 0)
        {
            host = hostport;
        }
        else
        {
            host = hostport.substr(0, pos);
            port = atoi(hostport.substr(pos + 1).c_str());
        }
        if(host.empty())
        {
            host = "0.0.0.0";
        }
    }
    return true;
}

static bool hostport2sockaddr(const std::string& hostport, struct sockaddr_in& sa)
{
    std::string host;
    int port = 0;
    if(!parse_hostport(hostport, host, port))
    {
        return false;
    }
    return get_sockaddr(host.c_str(), port, sa);
}

static int64_t now_clock_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - 
        std::chrono::time_point<std::chrono::high_resolution_clock>()).count();
}

static void msleep(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

static bool msleep_break(std::atomic<bool>& isStart, int ms)
{
    const static int P = 50;
    int count = ms/P;
    const int remain = ms%P;
    while(count > 0)
    {
        msleep(P);
        if(!isStart)
        {
            return false;
        }
        count --;
    }
    if(remain > 0)
    {
        msleep(remain);
    }
    return true;
}

static bool srt_is_disconnected(SRT_SOCKSTATUS state)
{
    if((state == SRTS_BROKEN) ||
       (state == SRTS_CLOSING) ||
       (state == SRTS_CLOSED) ||
       (state == SRTS_NONEXIST))
    {
        return true;
    }
    return false;
}

static int srt_connent_wait(SRTSOCKET ss, int epoll_id, int conn_timeout, std::function<bool()> isBreak)
{
    int ret = SRT_ERROR;
    int64_t begin_ms = now_clock_ms();
    while(!isBreak())
    {
        int       rlen = 1;
        SRTSOCKET rready = SRT_INVALID_SOCK;
        int       wlen = 1;
        SRTSOCKET wready = SRT_INVALID_SOCK;
        srt_epoll_wait(epoll_id, &rready, &rlen, &wready, &wlen, TIMEOUT, 0, 0, 0, 0);
        SRT_SOCKSTATUS state = srt_getsockstate(ss);
        if(state == SRTS_CONNECTED)
        {
            ret = SRT_SUCCESS;
            break;
        }
        if(srt_is_disconnected(state))
        {
            break;
        }
        int64_t delta = (now_clock_ms() - begin_ms);
        if((delta < 0) || (delta > conn_timeout))
        {
            break;
        }
    }
    return ret;
}

static int srt_connent_break(SRTSOCKET ss, const struct sockaddr* name, int namelen,
                             std::function<bool()> isBreak)
{
    int ret = SRT_ERROR;
    int32_t conn_timeout = 3000; // ms
    bool oldRcvSyn = true;
    {
        int val_len = 0;
        srt_getsockflag(ss, SRTO_CONNTIMEO, &conn_timeout, &val_len);
        bool isYes = false;
        val_len = 0;
        srt_getsockflag(ss, SRTO_RENDEZVOUS, &isYes, &val_len);
        if(isYes)
        {
            conn_timeout *= 10;
        }
        val_len = 0;
        srt_getsockopt(ss, 0, SRTO_RCVSYN, &oldRcvSyn, &val_len);
        srt_setsockopt(ss, 0, SRTO_RCVSYN, &NO, sizeof(NO)); // for async connect
    }
    int epoll_id = srt_epoll_create();
    if(epoll_id >= 0)
    {
        const int epoll_events = SRT_EPOLL_OUT | SRT_EPOLL_ERR;
        srt_epoll_add_usock(epoll_id, ss, &epoll_events);
        ret = srt_connect(ss, name, namelen);
        if(ret != SRT_ERROR)
        {
            ret = srt_connent_wait(ss, epoll_id, conn_timeout, isBreak);
        }
        srt_epoll_remove_usock(epoll_id, ss);
        srt_epoll_release(epoll_id);
    }
    srt_setsockopt(ss, 0, SRTO_RCVSYN, &oldRcvSyn, sizeof(oldRcvSyn));

    return ret;
}

static int srt_connect_group_break(SRTSOCKET ss, SRT_SOCKGROUPCONFIG name[], int arraysize,
                                   std::function<bool()> isBreak)
{
    int ret = SRT_ERROR;
    int32_t conn_timeout = 3000; // ms
    bool oldRcvSyn = true;
    {
        int val_len = 0;
        srt_getsockflag(ss, SRTO_CONNTIMEO, &conn_timeout, &val_len);
        val_len = 0;
        srt_getsockopt(ss, 0, SRTO_RCVSYN, &oldRcvSyn, &val_len);
        srt_setsockopt(ss, 0, SRTO_RCVSYN, &NO, sizeof(NO)); // for async connect
    }
    int epoll_id = srt_epoll_create();
    if(epoll_id >= 0)
    {
        const int epoll_events = SRT_EPOLL_OUT | SRT_EPOLL_ERR;
        srt_epoll_add_usock(epoll_id, ss, &epoll_events);
        ret = srt_connect_group(ss, name, arraysize);
        if(ret != SRT_ERROR)
        {
            ret = srt_connent_wait(ss, epoll_id, conn_timeout, isBreak);
        }
    }
    srt_setsockopt(ss, 0, SRTO_RCVSYN, &oldRcvSyn, sizeof(oldRcvSyn));
    return ret;
}

// class SrtBase
class SrtBase
{
public:
    SrtBase()
    {
        if(gIsFirstRun)
        {
            gIsFirstRun = false;
            srt_setloglevel(7);
            srt_setloghandler(nullptr, &onSrtLogCallback);
        }
        srt_msgctrl_init(&mMsgRecvCtrl);
    }
    virtual ~SrtBase()
    {
    }

public:
    virtual const char* mode() const
    {
        return mMode.c_str();
    }
    virtual void setParams(const StdKeyValue& params)
    {
        mParams = params;
    }
    virtual void setOnReadHandler(OnSrtReadHandler handler)
    {
        mReadHandler = handler;
    }
    virtual void setOnConnectedHandler(OnSrtConnectedHandler handler)
    {
        mConnectedHandler = handler;
    }
    virtual void setOnDisconnectedHandler(OnSrtDisconnectedHandler handler)
    {
        mDisconnectedHandler = handler;
    }
    virtual void setOnStreamIDHandler(OnSrtStreamIDHandler handler)
    {
        mStreamIDHandler = handler;
    }
    virtual bool open(bool isSync) = 0;
    virtual int  write(const char* data, int size, SRT_MSGCTRL* mctrl = nullptr) = 0;
    virtual SRT_SOCKSTATUS state() = 0;
    virtual int  bstats(SRT_TRACEBSTATS* perf, int clear) = 0;

    virtual int  write(SRTSOCKET ss, const char* data, int size, SRT_MSGCTRL* mctrl = nullptr)
    {
        return srt_sendmsg2(ss, (const char*)data, size, mctrl);
    }

    virtual SRT_SOCKSTATUS state(SRTSOCKET ss)
    {
        return srt_getsockstate(ss);
    }

    virtual int bstats(SRTSOCKET ss, SRT_TRACEBSTATS* perf, int clear = 0)
    {
        return srt_bstats(ss, perf, clear);
    }

    virtual void close() = 0;
	std::string getParam(const std::string& key, const std::string& defaultValue = "")
	{
		std::string value = defaultValue;
		if(mParams.find(key) != mParams.end())
		{
			value = mParams[key];
		}
		return value;
	}

protected:
    virtual bool onSetOptionsBefore(SRTSOCKET ss)
    {
        int ret = 0;
        bool isOk = false;
        do
        {
            bool isClient = ((_stricmp(mMode.c_str(), "caller") == 0) ||
                             (_stricmp(mMode.c_str(), "bondingcaller") == 0));

            SRT_TRANSTYPE transtype = SRTT_LIVE;
            std::string strTransType = getParam("transtype", "live");
            if(_stricmp(strTransType.c_str(), "file") == 0)
            {
                transtype = SRTT_FILE;
            }
            if(isClient || (_stricmp(mMode.c_str(), "rendezvous") == 0))
            {
                int conntimeo = atoi(getParam("conntimeo", "0").c_str());
                if(conntimeo > 0)
                {
                    SRT_SET_OPTION(ss, SRTO_CONNTIMEO, &conntimeo, sizeof(conntimeo));
                }
            }
            if(isClient)
            {
                SRT_SET_OPTION(ss, SRTO_SENDER, &YES, sizeof(YES));
            }
            SRT_SET_OPTION(ss, SRTO_TRANSTYPE, &transtype, sizeof(transtype));
            SRT_SET_OPTION(ss, SRTO_RCVSYN, &YES, sizeof(YES));
            std::string streamId = getParam("streamid");
            if(!streamId.empty())
            {
                SRT_SET_OPTION(ss, SRTO_STREAMID, streamId.c_str(), (int)streamId.size());
            }
            std::string password = getParam("password");
            int keyLength = atoi(getParam("keylength", "0").c_str());
            if((keyLength > 0) && (!password.empty()))
            {
                SRT_SET_OPTION(ss, SRTO_PBKEYLEN, &keyLength, sizeof(keyLength));
                SRT_SET_OPTION(ss, SRTO_PASSPHRASE, password.c_str(), (int)password.size());
            }
            int payloadsize = atoi(getParam("payloadsize", std::to_string(SRT_LIVE_DEF_PLSIZE)).c_str());
            if(payloadsize > 0)
            {
                SRT_SET_OPTION(ss, SRTO_PAYLOADSIZE, &payloadsize, sizeof(payloadsize));
            }
            int rcvlatency = atoi(getParam("rcvlatency", "0").c_str());
            if(rcvlatency > 0)
            {
                SRT_SET_OPTION(ss, SRTO_RCVLATENCY, &rcvlatency, sizeof(rcvlatency));
            }
            int peerlatency = atoi(getParam("peerlatency", "0").c_str());
            if(peerlatency > 0)
            {
                SRT_SET_OPTION(ss, SRTO_PEERLATENCY, &peerlatency, sizeof(peerlatency));
            }
            int sndbuf = atoi(getParam("sndbuf", "0").c_str());
            if(sndbuf > 0)
            {
                SRT_SET_OPTION(ss, SRTO_SNDBUF, &sndbuf, sizeof(sndbuf));
            }
            int rcvbuf = atoi(getParam("rcvbuf", "0").c_str());
            if(rcvbuf > 0)
            {
                SRT_SET_OPTION(ss, SRTO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
            }
            
            mIOType = getParam("iotype", "unknown");
            mMaxConnections = atoi(getParam("maxconnections", "-1").c_str());

            isOk = true;
        }while(0);
        return isOk;
    }

    virtual bool onEpollRecv(int epollid, SRTSOCKET ss, std::vector<char>& buffer)
    {
        SRTSOCKET srt_rfd = SRT_INVALID_SOCK;
        int srt_rfd_len = 1;
        int n = 0;
        n = srt_epoll_wait(epollid, &srt_rfd, &srt_rfd_len, nullptr, nullptr, TIMEOUT,
                            nullptr, nullptr, nullptr, nullptr);
        if(n <= 0)
        {
            return true;
        }
        if(srt_rfd != ss)
        {
            return true;
        }
        SRT_SOCKSTATUS state = srt_getsockstate(ss);
        if(srt_is_disconnected(state))
        {
            if(mDisconnectedHandler != nullptr)
            {
                mDisconnectedHandler(ss);
            }
            //sl_log(SL_LEVEL::SL_WARNING, LOG_NAME, "disconnected");
            return false;
        }
        srt_msgctrl_init(&mMsgRecvCtrl);
        if(!mGroupRecvData.empty())
        {
            mMsgRecvCtrl.grpdata      = mGroupRecvData.data();
            mMsgRecvCtrl.grpdata_size = mGroupRecvData.size();
        }
        int ret = srt_recvmsg2(ss, buffer.data(), (int)buffer.size(), &mMsgRecvCtrl);
        if(ret > 0)
        {
            if(mReadHandler)
            {
                mReadHandler(ss, buffer.data(), ret, &mMsgRecvCtrl);
            }
            return true;
        }
        else if(ret == SRT_ERROR)
        {
            int srterr = srt_getlasterror(nullptr);
            if((srterr == SRT_EASYNCSND) || 
               (srterr == SRT_EASYNCRCV) || 
               (srterr == SRT_ETIMEOUT) ||
               (srterr == SRT_ECONGEST))
            {
                return true;
            }
        }
        sl_log(SL_LEVEL::SL_WARNING, LOG_NAME, "srt_epoll_wait failed, %s", srt_getlasterror_str());
        return false;
    }

    virtual bool onEpollRecv(int epollid, std::vector<SRTSOCKET>& socks, std::vector<char>& buffer)
    {
        std::vector<SRTSOCKET> sss = std::move(socks);
        int srt_rfd_len = (int)(sss.size());
        int n = 0;
        n = srt_epoll_wait(epollid, sss.data(), &srt_rfd_len, nullptr, nullptr, TIMEOUT,
                            nullptr, nullptr, nullptr, nullptr);
        if(n <= 0)
        {
            return true;
        }
        for(int i = 0; i < n; i++)
        {
            SRTSOCKET ss = sss[i];
            SRT_SOCKSTATUS state = srt_getsockstate(ss);
            if(srt_is_disconnected(state))
            {
                socks.push_back(ss);
                if(mDisconnectedHandler != nullptr)
                {
                    mDisconnectedHandler(ss);
                }
            }
            else
            {
                srt_msgctrl_init(&mMsgRecvCtrl);
                if(!mGroupRecvData.empty())
                {
                    mMsgRecvCtrl.grpdata = mGroupRecvData.data();
                    mMsgRecvCtrl.grpdata_size = mGroupRecvData.size();
                }
                int ret = srt_recvmsg2(ss, buffer.data(), (int)buffer.size(), &mMsgRecvCtrl);
                if(ret > 0)
                {
                    if(mReadHandler)
                    {
                        mReadHandler(ss, buffer.data(), ret, &mMsgRecvCtrl);
                    }
                }
                else if(ret == SRT_ERROR)
                {
                    int srterr = srt_getlasterror(nullptr);
                    if((srterr == SRT_EASYNCSND) ||
                        (srterr == SRT_EASYNCRCV) ||
                        (srterr == SRT_ETIMEOUT) ||
                        (srterr == SRT_ECONGEST))
                    {
                    }
                }
            }
        }
        return true;
    }

protected:
    std::string                     mMode;
    OnSrtReadHandler                mReadHandler = nullptr;
    OnSrtConnectedHandler           mConnectedHandler = nullptr;
    OnSrtDisconnectedHandler        mDisconnectedHandler = nullptr;
    OnSrtStreamIDHandler            mStreamIDHandler = nullptr;
    StdKeyValue                     mParams;
    SRT_MSGCTRL                     mMsgRecvCtrl;
    std::vector<SRT_SOCKGROUPDATA>  mGroupRecvData;
    std::string                     mIOType; // "input" | "output" | "both" | "unknown"
    int                             mMaxConnections = -1; // unlimited      
};

// class SrtCaller
class SrtCaller : public SrtBase
{
public:
    SrtCaller() : mIsStart(false)
    {
        mMode = "caller";
    }
    virtual ~SrtCaller()
    {
        close();
    }

public:
    bool open(bool isSync) override
    {
        mIsStart = true;
        if(isSync && (!open_internal()))
        {
            return false;
        }
        mIsFirstOpen = true;
        mBuffer.resize(SRT_BUFFER_SIZE);
        mThread = std::thread([&]()
        {
            while(mIsStart)
            {
                mMutex.lock_shared();
                if(mSrtSocket == SRT_INVALID_SOCK)
                {
                    mMutex.unlock_shared();
                    if(mIsFirstOpen || msleep_break(mIsStart, 2000))
                    {
                        mIsFirstOpen = false;
                        open_internal();
                    }
                    continue;
                }
                mMutex.unlock_shared();
                while(mIsStart)
                {
                    if(!onEpollRecv(mEpollId, mSrtSocket, mBuffer))
                    {
                        break;
                    }
                }
                mMutex.lock();
                close_internal();
                mMutex.unlock();
            }
        });
        return true;
    }
    bool open_internal()
    {
        bool isOk = false;
        int ret = 0;
        do
        {
            mMutex.lock();
            mSrtSocket = srt_create_socket();
            mMutex.unlock();
            if(mSrtSocket == SRT_INVALID_SOCK)
            {
                ret = SRT_ERROR;
                break;
            }
            if(!onSetOptionsBefore(mSrtSocket))
            {
                ret = SRT_ERROR;
                break;
            }
            if(!onOpen(mSrtSocket))
            {
                ret = SRT_ERROR;
                break;
            }
            std::string hostport = getParam("localaddr");
            if(!hostport.empty())
            {
                struct sockaddr_in sa;
                if(!hostport2sockaddr(hostport, sa))
                {
                    ret = SRT_ERROR;
                    break;
                }
                ret = srt_bind(mSrtSocket, (const sockaddr*)&sa, sizeof(sa));
                if(ret == SRT_ERROR)
                {
                    sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "srt socket bind(%s) failed, %s", 
                           hostport.c_str(), srt_getlasterror_str());
                    break;
                }
            }
            std::string remoteaddr = getParam("remoteaddr");
            struct sockaddr_in sa;
            if(!hostport2sockaddr(remoteaddr, sa))
            {
                ret = SRT_ERROR;
                break;
            }

            ret = srt_connent_break(mSrtSocket, (const sockaddr*)&sa, sizeof(sa), 
                                    [&](){ return !mIsStart; }
            );

            if(ret == SRT_ERROR)
            {
				int reject_reason = srt_getrejectreason((SRTSOCKET)mSrtSocket);
				if(SRT_REJ_BADSECRET == reject_reason )
				{
					sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "connection was rejected, wrong password, srt://%s", remoteaddr.c_str());
				}
				else if(SRT_REJ_UNSECURE == reject_reason)
				{
					sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "connection was rejected, password required or unexpected, srt://%s", remoteaddr.c_str());
				}
				else if(SRT_REJ_ROGUE == reject_reason || SRT_REJ_PEER == reject_reason)
				{
					sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "connection was rejected, maybe stream id does not match, srt://%s", remoteaddr.c_str());
				}
				else
				{
					//sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "srt socket connnect failed, %s.", srt_getlasterror_str());
				}
                break;
            }
            if(mConnectedHandler)
            {
                char peerStreamId[514];
                memset(peerStreamId, 0, sizeof(peerStreamId));
                int streamIdLen = sizeof(peerStreamId) - 1;
                srt_getsockopt(mSrtSocket, 0, SRTO_STREAMID, &peerStreamId, &streamIdLen);
                ret = mConnectedHandler(mSrtSocket, peerStreamId, streamIdLen);
                if(ret == SRT_ERROR)
                {
                    break;
                }
            }
            SRT_SET_OPTION(mSrtSocket, SRTO_SNDSYN, &NO, sizeof(NO));
            SRT_SET_OPTION(mSrtSocket, SRTO_RCVSYN, &NO, sizeof(NO));
            mEpollId = srt_epoll_create();
            if(mEpollId < 0)
            {
                ret = SRT_ERROR;
                break;
            }
            const int events = SRT_EPOLL_IN | SRT_EPOLL_ERR;
            ret = srt_epoll_add_usock(mEpollId, mSrtSocket, &events);
            if(ret == SRT_ERROR)
            {
                break;
            }
			std::string strPassphrase;
			int keyLength = atoi(getParam("keylength", "0").c_str());
			if((keyLength > 0) && (!getParam("password").empty()))
			{
				if (keyLength == 16)
					strPassphrase = " with passphrase (AES-128)";
				else if(keyLength == 24)
					strPassphrase = " with passphrase (AES-192)";
				else if(keyLength == 32)
					strPassphrase = " with passphrase (AES-256)";
			}
			sl_log(SL_LEVEL::SL_WARNING, LOG_NAME, "connected successfully%s, srt://%s\n", strPassphrase.c_str(), remoteaddr.c_str());

            isOk = true;
        } while(0);
        if(!isOk)
        {
            mMutex.lock();
            close_internal();
            mMutex.unlock();
        }
        return isOk;
    }

    int write(const char* data, int size, SRT_MSGCTRL* mctrl = nullptr) override
    {
        int ret = -1;
        std::shared_lock<std::shared_mutex> locker(mMutex);
        if(mSrtSocket != SRT_INVALID_SOCK)
        {
            ret = srt_sendmsg2(mSrtSocket, (const char*)data, size, mctrl);
        }
        return ret;
    }

    SRT_SOCKSTATUS state() override
    {
        SRT_SOCKSTATUS ret = SRTS_NONEXIST;
        std::shared_lock<std::shared_mutex> locker(mMutex);
        if(mSrtSocket != SRT_INVALID_SOCK)
        {
            ret = srt_getsockstate(mSrtSocket);
        }
        return ret;
    }
    
    int bstats(SRT_TRACEBSTATS* perf, int clear) override
    {
        int ret = -1;
        std::shared_lock<std::shared_mutex> locker(mMutex);
        if(mSrtSocket != SRT_INVALID_SOCK)
        {
            ret = srt_bstats(mSrtSocket, perf, clear);
        }
        return ret;
    }

    void close_internal()
    {
        if(mEpollId >= 0)
        {
            if(mSrtSocket != SRT_INVALID_SOCK)
            {
                srt_epoll_remove_usock(mEpollId, mSrtSocket);
            }
            srt_epoll_release(mEpollId);
            mEpollId = -1;
        }
        SRT_CLOSE(mSrtSocket);
    }
    
    void close() override
    {
        mIsStart = false;
        if(mThread.joinable())
        {
            mThread.join();
        }
        mMutex.lock();
        close_internal();
        mMutex.unlock();
        mBuffer.clear();
    }

protected:
    virtual bool onOpen(SRTSOCKET ss)
    {
        return true;
    }

private:
    SRTSOCKET mSrtSocket = SRT_INVALID_SOCK;
    int mEpollId = -1;
    std::atomic<bool> mIsStart = false;
    std::atomic<bool> mIsFirstOpen = true;
    std::thread mThread;
    std::vector<char> mBuffer;
    std::shared_mutex mMutex;
};

// class SrtListener
class SrtListener : public SrtBase
{
public:
    SrtListener() : mIsStart(false)
    {
        mMode = "listener";
    }
    virtual ~SrtListener()
    {
        close();
    }

	int static on_listen_callback(void* opaq, SRTSOCKET ns, int hsversion, const struct sockaddr* peeraddr, const char* peerStreamId)
	{
		SrtListener* listener = (SrtListener*)opaq;
		if(listener)
		{
			memcpy(g_listenerLocaladdr, listener->getParam("localaddr").c_str(), listener->getParam("localaddr").length());
			srt_setloghandler((void*)g_listenerLocaladdr, &onSrtLogCallback);

			std::string streamId = listener->getParam("streamid");
			// Any stream can be connected if the configured "stream id" is empty.
			if(!streamId.empty())
			{
				if(listener->mStreamIDHandler)
				{
					// If the callback returns false, the connection is rejected.
					if(!listener->mStreamIDHandler(ns, peerStreamId, strlen(peerStreamId)))
					{
						sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "stream id verification failed.");
						srt_setrejectreason(ns, SRT_REJ_PEER);//caller cannot parse the reject reason and will be converted to SRT_REJ_ROGUE on srt v1.5
						return -1;
					}
				}
				else
				{
					if(strcmp(streamId.c_str(), peerStreamId) != 0)
					{
						sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "stream id verification failed, the caller's stream id [%s], srt://%s", peerStreamId, listener->getParam("localaddr").c_str());
						srt_setrejectreason(ns, SRT_REJ_PEER);//caller cannot parse the reject reason and will be converted to SRT_REJ_ROGUE on srt v1.5
						return -1;
					}
				}
			}
		}
		return 0;
	}

    bool open(bool isSync) override
    {
        bool isOk = false;
        int ret = 0;
        do
        {
            std::string hostports = getParam("localaddr");
            auto aryHostports = stringSplit<std::string>(hostports, ";", false, true, false);
            bool isOneOk = false;
            SRTSOCKET srtSocket = SRT_INVALID_SOCK;
            for(const auto& hostport : aryHostports)
            {
                isOneOk = false;
                struct sockaddr_in sa;
                if(!hostport2sockaddr(hostport, sa))
                {
                    ret = SRT_ERROR;
                    break;
                }
                srtSocket = srt_create_socket();
                if(srtSocket == SRT_INVALID_SOCK)
                {
                    ret = SRT_ERROR;
                    break;
                }
                if(!onSetOptionsBefore(srtSocket))
                {
                    ret = SRT_ERROR;
                    break;
                }
                if(!onOpen(srtSocket))
                {
                    break;
                }
                ret = srt_bind(srtSocket, (const sockaddr*)&sa, sizeof(sa));
                if(ret == SRT_ERROR)
                {
                    sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "srt socket bind(%s) failed, %s",
                           hostport.c_str(), srt_getlasterror_str());
                    break;
                }
                ret = srt_listen(srtSocket, 16);
                if(ret == SRT_ERROR)
                {
                    sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "srt socket listen(%s) failed, %s",
                           hostport.c_str(), srt_getlasterror_str());
                    break;
                }
                isOneOk = true;

				srt_listen_callback(srtSocket, SrtListener::on_listen_callback, this);


                mSrtSockets.push_back(srtSocket);
                srtSocket = SRT_INVALID_SOCK;
            }
            if(!isOneOk)
            {
                SRT_CLOSE(srtSocket);
                break ;
            }
            mEpollId = srt_epoll_create();
            if(mEpollId < 0)
            {
                ret = SRT_ERROR;
                break;
            }
            mIsOutputType = (_stricmp(mIOType.c_str(), "output") == 0);
            mBuffer.resize(SRT_BUFFER_SIZE);
            mIsStart = true;
            mThreadAccept = std::thread([&]()
            {
                while(mIsStart)
                {
                    SRTSOCKET ss = srt_accept_bond(mSrtSockets.data(), (int)mSrtSockets.size(), TIMEOUT);
                    if(ss == SRT_INVALID_SOCK)
                    {
                        msleep(5);
                        continue;
                    }
                    mMutexClient.lock();
                    SRTSOCKET ssClient = mSrtClientSocket;
                    mMutexClient.unlock();
                    if(mIsOutputType)
                    {
                        if(onConnected(ss))
                        {
                            mMutexClient.lock();
                            mSrtClients.insert(ss);
                            mMutexClient.unlock();
                            ss = SRT_INVALID_SOCK;
                            //if(ss != SRT_INVALID_SOCK)
                            //{
                            //    sl_log(SL_LEVEL::SL_WARNING, LOG_NAME, "too many connections, disconnect the new connection");
                            //}
                        }
                    }
                    else if(ssClient != SRT_INVALID_SOCK)
                    {
                        //sl_log(SL_LEVEL::SL_WARNING, LOG_NAME, "srt client socket is already connected, disconnect the new connection");
                    }
                    else
                    {
                        if(onConnected(ss))
                        {
                            const int events = SRT_EPOLL_IN | SRT_EPOLL_ERR;
                            ret = srt_epoll_add_usock(mEpollId, ss, &events);
                            if(ret != SRT_ERROR)
                            {
                                mMutexClient.lock();
                                mSrtClientSocket = ss;
                                ss = SRT_INVALID_SOCK;
                                mMutexClient.unlock();
                            }
                        }
                    }
                    SRT_CLOSE(ss);
                }
            });
            mThreadClient = std::thread([&]()
            {
                while(mIsStart)
                {
                    mMutexClient.lock_shared();
                    if(mSrtClientSocket == SRT_INVALID_SOCK)
                    {
                        mMutexClient.unlock_shared();
                        msleep(100);
                        continue;
                    }
                    mMutexClient.unlock_shared();
                    while(mIsStart)
                    {
                        if(!onEpollRecv(mEpollId, mSrtClientSocket, mBuffer))
                        {
                            break;
                        }
                    }
                    mMutexClient.lock();
                    srt_epoll_remove_usock(mEpollId, mSrtClientSocket);
                    SRT_CLOSE(mSrtClientSocket);
                    mMutexClient.unlock();
                }
            });
            isOk = true;
        } while(0);
        if(!isOk)
        {
            close();
        }
        return isOk;
    }

    int write(const char* data, int size, SRT_MSGCTRL* mctrl = nullptr) override
    {
        int ret = -1;
        if(mIsOutputType)
        {
            std::list<SRTSOCKET> closedSRTs;
            mMutexClient.lock_shared();
            for(auto& srtSocket : mSrtClients)
            {
                ret = srt_sendmsg2(srtSocket, (const char*)data, size, mctrl);
                if(ret == SRT_ERROR)
                {
                    SRT_SOCKSTATUS state = srt_getsockstate(srtSocket);
                    if(srt_is_disconnected(state))
                    {
                        closedSRTs.push_back(srtSocket);
                    }
                }
            }
            mMutexClient.unlock_shared();

            if(!closedSRTs.empty())
            {
                for(auto& srtSocket : closedSRTs)
                {
                    mMutexClient.lock();
                    mSrtClients.erase(srtSocket);
                    mMutexClient.unlock();
                    if(mDisconnectedHandler != nullptr)
                    {
                        mDisconnectedHandler(srtSocket);
                    }
					//sl_log(SL_LEVEL::SL_NOTICE, LOG_NAME, "disconnected");
                    SRT_CLOSE(srtSocket);
                }
                closedSRTs.clear();
            }
        }
        else
        {
            std::shared_lock<std::shared_mutex> locker(mMutexClient);
            if(mSrtClientSocket != SRT_INVALID_SOCK)
            {
                ret = srt_sendmsg2(mSrtClientSocket, (const char*)data, size, mctrl);
            }
        }
        return ret;
    }

    SRT_SOCKSTATUS state() override
    {
        SRT_SOCKSTATUS ret = SRTS_NONEXIST;
        std::shared_lock<std::shared_mutex> locker(mMutexClient);
        if(mIsOutputType)
        {
            if(!mSrtClients.empty())
            {
                ret = srt_getsockstate(*mSrtClients.begin());
            }
        }
        else
        {
            if(mSrtClientSocket != SRT_INVALID_SOCK)
            {
                ret = srt_getsockstate(mSrtClientSocket);
            }
        }
        return ret;
    }
    
    int bstats(SRT_TRACEBSTATS* perf, int clear) override
    {
        int ret = -1;
        std::shared_lock<std::shared_mutex> locker(mMutexClient);
        if(mIsOutputType)
        {
            if(!mSrtClients.empty())
            {
                ret = srt_bstats(*mSrtClients.begin(), perf, clear);
            }
        }
        else
        {
            if(mSrtClientSocket != SRT_INVALID_SOCK)
            {
                ret = srt_bstats(mSrtClientSocket, perf, clear);
            }
        }
        return ret;
    }

    void close() override
    {
        mIsStart = false;
        if(mThreadAccept.joinable())
        {
            mThreadAccept.join();
        }
        if(mThreadClient.joinable())
        {
            mThreadClient.join();
        }
        while(!mSrtSockets.empty())
        {
            auto srtSocket = mSrtSockets.front();
            mSrtSockets.erase(mSrtSockets.begin());
            SRT_CLOSE(srtSocket);
        }
        mMutexClient.lock();
        SRT_CLOSE(mSrtClientSocket);
        while(!mSrtClients.empty())
        {
            auto iter = mSrtClients.begin();
            auto srtSocket = *iter;
            mSrtClients.erase(iter);
            SRT_CLOSE(srtSocket);
        }
        mMutexClient.unlock();
        if(mEpollId >= 0)
        {
            srt_epoll_release(mEpollId);
            mEpollId = -1;
        }
        mBuffer.clear();
    }

protected:
    virtual bool onOpen(SRTSOCKET ss)
    {
        return true;
    }

    virtual bool onConnected(SRTSOCKET ss)
    {
        bool isOk = false;
        int ret = 0;
        do
        {
            mMutexClient.lock_shared();
            if((mMaxConnections > 0) && ((int)mSrtClients.size() >= mMaxConnections))
            {
                mMutexClient.unlock_shared();
                break;
            }
            mMutexClient.unlock_shared();

            char peerStreamId[514];
            memset(peerStreamId, 0, sizeof(peerStreamId));
            int streamIdLen = sizeof(peerStreamId)-1;
            ret = srt_getsockopt(ss, 0, SRTO_STREAMID, &peerStreamId, &streamIdLen);
            if(ret == SRT_ERROR)
            {
                sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "srt_getsockopt(0x%x) error, %s",
                       ss, srt_getlasterror_str());
                ret = 0;
            }
#if 0
            std::string streamId = getParam("streamid");
            // Any stream can be connected if the configured "stream id" is empty.
            if(!streamId.empty())
            {
                if(mStreamIDHandler)
                {
                    // If the callback returns false, the connection is rejected.
                    if(!mStreamIDHandler(ss, peerStreamId, streamIdLen))
                    {
                        sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "The stream id verification failed.\n");
                        ret = SRT_ERROR;
                        break;
                    }
                }
                else
                {
                    // The stream id must match.
                    if(strcmp(streamId.c_str(), peerStreamId) != 0)
                    {
                        sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "The stream id does not match! The caller's stream id is [%s]\n", peerStreamId);
                        ret = SRT_ERROR;
                        break;
                    }
                }
            }
#endif
            SRT_SET_OPTION(ss, SRTO_RCVSYN, &NO, sizeof(NO));
            if(mConnectedHandler)
            {
                char peerStreamId[514];
                memset(peerStreamId, 0, sizeof(peerStreamId));
                int streamIdLen = sizeof(peerStreamId) - 1;
                srt_getsockopt(ss, 0, SRTO_STREAMID, &peerStreamId, &streamIdLen);

                ret = mConnectedHandler(ss, peerStreamId, streamIdLen);
                if(ret == SRT_ERROR)
                {
                    break;
                }
            }
			std::string strPassphrase;
			int keyLength = atoi(getParam("keylength", "0").c_str());
			if((keyLength > 0) && (!getParam("password").empty()))
			{
				if(keyLength == 16)
					strPassphrase = " with passphrase (AES-128)";
				else if(keyLength == 24)
					strPassphrase = " with passphrase (AES-192)";
				else if(keyLength == 32)
					strPassphrase = " with passphrase (AES-256)";
			}
			sl_log(SL_LEVEL::SL_WARNING, LOG_NAME, "connected successfully%s, srt://%s\n", strPassphrase.c_str(), getParam("localaddr").c_str());
            isOk = true;
        } while(0);
        return isOk;
    }

private:
    std::vector<SRTSOCKET> mSrtSockets;
    SRTSOCKET mSrtClientSocket = SRT_INVALID_SOCK;
    int mEpollId = -1;
    std::vector<char> mBuffer;
    std::atomic<bool> mIsStart;
    std::thread mThreadAccept;
    std::thread mThreadClient;
    std::shared_mutex mMutexClient;
    std::set<SRTSOCKET> mSrtClients; // output mode: support multiple connections
    bool mIsOutputType = false;
};

class SrtRendezvous : public SrtCaller
{
public:
    SrtRendezvous()
    {
        mMode = "rendezvous";
    }
    virtual ~SrtRendezvous()
    {
    }

protected:
    virtual bool onOpen(SRTSOCKET ss)
    {
        bool isOk = false;
        int ret = 0;
        do
        {
            SRT_SET_OPTION(ss, SRTO_RENDEZVOUS, &YES, sizeof(YES));
            isOk = true;
        }while(0);
        return isOk;
    }
};

class SrtBondingCaller : public SrtBase
{
public:
    SrtBondingCaller() : mIsStart(false)
    {
        mMode = "bondingcaller";
        srt_msgctrl_init(&mMsgSendCtrl);
    }
    virtual ~SrtBondingCaller()
    {
    }

    bool open(bool isSync) override
    {
        mIsStart = true;
        if(isSync && (!open_internal()))
        {
            return false;
        }
        mIsFirstOpen = true;
        mBuffer.resize(SRT_BUFFER_SIZE);
        mThread = std::thread([&]()
        {
            while(mIsStart)
            {
                mMutex.lock_shared();
                if(mSrtGroup == SRT_INVALID_SOCK)
                {
                    mMutex.unlock_shared();
                    if(mIsFirstOpen || msleep_break(mIsStart, 2000))
                    {
                        mIsFirstOpen = false;
                        open_internal();
                    }
                    continue;
                }
                mMutex.unlock_shared();
                while(mIsStart)
                {
                    if(!onEpollRecv(mEpollId, mSrtGroup, mBuffer))
                    {
                        break;
                    }
                }
                mMutex.lock();
                close_internal();
                mMutex.unlock();
            }
        });
        return true;
    }

    bool open_internal()
    {
        bool isOk = false;
        int ret = 0;
        do
        {
            SRT_GROUP_TYPE type = SRT_GTYPE_BROADCAST;
            std::string strType = getParam("grouptype", "");
            if(_stricmp(strType.c_str(), "backup") == 0)
            {
                type = SRT_GTYPE_BACKUP;
            }
            mMutex.lock();
            mSrtGroup = srt_create_group(type);
            mMutex.unlock();
            if(mSrtGroup == SRT_INVALID_SOCK)
            {
                ret = SRT_ERROR;
                break;
            }
            if(!onSetOptionsBefore(mSrtGroup))
            {
                ret = SRT_ERROR;
                break;
            }
            std::string strLocalHostPorts = getParam("localaddr", "");
            auto aryLocalHostPorts = stringSplit<std::string>(strLocalHostPorts, ";", false, true, false);
            const int localAddrCount = (int)aryLocalHostPorts.size();

            std::string strRemoteHostPorts = getParam("remoteaddr", "");
            auto aryRemoteHostPorts = stringSplit<std::string>(strRemoteHostPorts, ";", false, true, false);
            const int remoteAddrCount = (int)aryRemoteHostPorts.size();
            if(remoteAddrCount <= 0)
            {
                ret = SRT_ERROR;
                break;
            }
            std::string strWeights = getParam("weight", "");
            auto aryWeights = stringSplit<std::string>(strWeights, ";", false, true, false);
            const int weightCount = (int)aryWeights.size();
            std::vector<SRT_SOCKGROUPCONFIG> groupConfigs;
            groupConfigs.resize(remoteAddrCount);
            for(int i = 0; i < remoteAddrCount; i ++)
            {
                std::string remoteHostPort = aryRemoteHostPorts[i];
                struct sockaddr_in sa_remote;
                if(!hostport2sockaddr(remoteHostPort, sa_remote))
                {
                    ret = SRT_ERROR;
                    break;
                }
                struct sockaddr_in sa_local;
                memset(&sa_local, 0, sizeof(sa_local));
                if(i < localAddrCount)
                {
                    std::string localHostPort = aryLocalHostPorts[i];
                    if(!hostport2sockaddr(localHostPort, sa_local))
                    {
                        ret = SRT_ERROR;
                        break;
                    }
                }
                groupConfigs[i] = srt_prepare_endpoint((sa_local.sin_family == AF_UNSPEC) ?
                                                       nullptr : (struct sockaddr*)&sa_local,
                                                       (struct sockaddr*)&sa_remote, sizeof(sa_remote));
                if(type == SRT_GTYPE_BACKUP)
                {
                    if(i == 0)
                    {
                        groupConfigs[i].weight = 1; // default 1
                    }
                    if(i < weightCount)
                    {
                        std::string strWeight = aryWeights[i];
                        int nWeight = atoi(strWeight.c_str());
                        groupConfigs[i].weight = limitRange(nWeight, 0, 32767);
                    }
                }
            }
            if(ret == SRT_ERROR)
            {
                break;
            }
            ret = srt_connect_group_break(mSrtGroup, groupConfigs.data(), remoteAddrCount,
                                          [&](){ return !mIsStart; }
            );
            if(ret == SRT_ERROR)
            {
                sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "srt socket connnect to group failed, %s",
                       srt_getlasterror_str());
                break;
            }
            mGroupRecvData.resize(remoteAddrCount);
            mGroupSendData.resize(remoteAddrCount);
            int countConnected = 0;
            for(int loop = 0; loop < 30; loop ++)
            {
                countConnected = 0;
                size_t groupCount = mGroupSendData.size();
                ret = srt_group_data(mSrtGroup, mGroupSendData.data(), &groupCount);
                if((ret == SRT_ERROR) || (ret <= 0))
                {
                    break ;
                }
                for(size_t g = 0; g < groupCount; g ++)
                {
                    if(mGroupSendData[g].sockstate == SRTS_CONNECTED)
                    {
                        countConnected ++;
                        break ;
                    }
                }
                if(countConnected >= remoteAddrCount)
                {
                    break ;
                }
                msleep(100);
            }
            if(countConnected <= 0)
            {
                sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "srt socket connnect to group failed, all connections failed");
                break;
            }
            if(mConnectedHandler)
            {
                char peerStreamId[514];
                memset(peerStreamId, 0, sizeof(peerStreamId));
                int streamIdLen = sizeof(peerStreamId) - 1;
                srt_getsockopt(mSrtGroup, 0, SRTO_STREAMID, &peerStreamId, &streamIdLen);

                ret = mConnectedHandler(mSrtGroup, peerStreamId, streamIdLen);
                if(ret == SRT_ERROR)
                {
                    break;
                }
            }
            SRT_SET_OPTION(mSrtGroup, SRTO_SNDSYN, &NO, sizeof(NO));
            SRT_SET_OPTION(mSrtGroup, SRTO_RCVSYN, &NO, sizeof(NO));
            mEpollId = srt_epoll_create();
            if(mEpollId < 0)
            {
                ret = SRT_ERROR;
                break;
            }
            const int events = SRT_EPOLL_IN | SRT_EPOLL_ERR;
            ret = srt_epoll_add_usock(mEpollId, mSrtGroup, &events);
            if(ret == SRT_ERROR)
            {
                break;
            }
            isOk = true;
        } while(0);
        if(!isOk)
        {
            mMutex.lock();
            close_internal();
            mMutex.unlock();
        }
        return isOk;
    }

    int write(const char* data, int size, SRT_MSGCTRL* mctrl = nullptr) override
    {
        int ret = -1;
        std::shared_lock<std::shared_mutex> locker(mMutex);
        if(mSrtGroup != SRT_INVALID_SOCK)
        {
            if(mctrl == nullptr)
            {
                srt_msgctrl_init(&mMsgSendCtrl);
                mMsgSendCtrl.grpdata = mGroupSendData.data();
                mMsgSendCtrl.grpdata_size = mGroupSendData.size();
                mctrl = &mMsgSendCtrl;
            }
            else if(mctrl->grpdata == nullptr)
            {
                mMsgSendCtrl.grpdata = mGroupSendData.data();
                mMsgSendCtrl.grpdata_size = mGroupSendData.size();
            }
            ret = srt_sendmsg2(mSrtGroup, (const char*)data, size, mctrl);
        }
        return ret;
    }

    SRT_SOCKSTATUS state() override
    {
        SRT_SOCKSTATUS ret = SRTS_NONEXIST;
        std::shared_lock<std::shared_mutex> locker(mMutex);
        if(mSrtGroup != SRT_INVALID_SOCK)
        {
            ret = srt_getsockstate(mSrtGroup);
        }
        return ret;
    }
    
    int bstats(SRT_TRACEBSTATS* perf, int clear) override
    {
        int ret = -1;
        std::shared_lock<std::shared_mutex> locker(mMutex);
        if(mSrtGroup != SRT_INVALID_SOCK)
        {
            ret = srt_bstats(mSrtGroup, perf, clear);
        }
        return ret;
    }

    void close_internal()
    {
        if(mEpollId >= 0)
        {
            if(mSrtGroup != SRT_INVALID_SOCK)
            {
                srt_epoll_remove_usock(mEpollId, mSrtGroup);
            }
            srt_epoll_release(mEpollId);
            mEpollId = -1;
        }
        SRT_CLOSE(mSrtGroup);
    }
    
    void close() override
    {
        mIsStart = false;
        if(mThread.joinable())
        {
            mThread.join();
        }
        mMutex.lock();
        close_internal();
        mMutex.unlock();
        mBuffer.clear();
    }

private:
    SRTSOCKET mSrtGroup = SRT_INVALID_SOCK;
    SRT_MSGCTRL mMsgSendCtrl;
    std::vector<SRT_SOCKGROUPDATA> mGroupSendData;
    int mEpollId = -1;
    std::atomic<bool> mIsStart = false;
    std::atomic<bool> mIsFirstOpen = true;
    std::thread mThread;
    std::vector<char> mBuffer;
    std::shared_mutex mMutex;
};

// class SrtBondingListener
class SrtBondingListener : public SrtListener
{
public:
    SrtBondingListener()
    {
        mMode = "bondinglistener";
    }
    virtual ~SrtBondingListener()
    {
    }

protected:
    bool onOpen(SRTSOCKET ss) override
    {
        bool isOk = false;
        int ret = 0;
        do
        {
            int yes = 1;
            SRT_SET_OPTION(ss, SRTO_GROUPCONNECT, &yes, sizeof(yes));
            mGroupRecvData.resize(32);
            isOk = true;
        } while(0);
        return isOk;
    }
};

// class SrtMultiplexListener
class SrtMultiplexListener : public SrtBase
{
public:
    SrtMultiplexListener() : mIsStart(false)
    {
        mMode = "multiplexlistener";
    }

    static int on_listen_callback(void* opaq, SRTSOCKET ns, int hsversion,
                                  const struct sockaddr* peeraddr, const char* peerStreamId)
    {
        SrtMultiplexListener* self = (SrtMultiplexListener*)opaq;
        if (self && self->mStreamIDHandler && peerStreamId)
        {
            sl_log(SL_LEVEL::SL_NOTICE, LOG_NAME, "on_listen_callback: ns=%d hsversion=%d streamId=[%s]\r\n",
                   ns, hsversion, peerStreamId ? peerStreamId : "");
            if (!self->mStreamIDHandler(ns, peerStreamId, (int)strlen(peerStreamId)))
            {
                srt_setrejectreason(ns, SRT_REJ_PEER);
                return -1;
            }
        }
        return 0;
    }
    virtual ~SrtMultiplexListener()
    {
        close();
    }

    bool open(bool isSync) override
    {
        bool isOk = false;
        int ret = 0;
        do
        {
            std::string hostports = getParam("localaddr");
            auto aryHostports = stringSplit<std::string>(hostports, ";", false, true, false);
            bool isOneOk = false;
            SRTSOCKET srtSocket = SRT_INVALID_SOCK;
            for(const auto& hostport : aryHostports)
            {
                isOneOk = false;
                struct sockaddr_in sa;
                if(!hostport2sockaddr(hostport, sa))
                {
                    ret = SRT_ERROR;
                    break;
                }
                srtSocket = srt_create_socket();
                if(srtSocket == SRT_INVALID_SOCK)
                {
                    ret = SRT_ERROR;
                    break;
                }
                if(!onSetOptionsBefore(srtSocket))
                {
                    ret = SRT_ERROR;
                    break;
                }
                if(!onOpen(srtSocket))
                {
                    break;
                }
                ret = srt_bind(srtSocket, (const sockaddr*)&sa, sizeof(sa));
                if(ret == SRT_ERROR)
                {
                    sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "srt socket bind(%s) failed, %s",
                           hostport.c_str(), srt_getlasterror_str());
                    break;
                }
                srt_listen_callback(srtSocket, SrtMultiplexListener::on_listen_callback, this);
                ret = srt_listen(srtSocket, 16);
                if(ret == SRT_ERROR)
                {
                    sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "srt socket listen(%s) failed, %s",
                           hostport.c_str(), srt_getlasterror_str());
                    break;
                }
                isOneOk = true;

                mSrtSockets.push_back(srtSocket);
                srtSocket = SRT_INVALID_SOCK;
            }
            if(!isOneOk)
            {
                SRT_CLOSE(srtSocket);
                break ;
            }
            mEpollId = srt_epoll_create();
            if(mEpollId < 0)
            {
                ret = SRT_ERROR;
                break;
            }
            mBuffer.resize(SRT_BUFFER_SIZE);
            mIsStart = true;
            mThreadAccept = std::thread([&]()
            {
                while(mIsStart)
                {
                    //SRTSOCKET ss = srt_accept_bond(mSrtSockets.data(), (int)mSrtSockets.size(), TIMEOUT);
                    struct sockaddr_storage their_addr;
                    int addr_size = sizeof their_addr;
                    SRTSOCKET* srtSocket = mSrtSockets.data();
                    SRT_SET_OPTION(*srtSocket, SRTO_RCVSYN, &NO, sizeof(NO));
                    SRTSOCKET ss = srt_accept(*srtSocket, (struct sockaddr*)&their_addr, &addr_size);

                    if(ss == SRT_INVALID_SOCK)
                    {
                        msleep(5);
                        continue;
                    }
                    if(onConnected(ss))
                    {
                        const int events = SRT_EPOLL_IN | SRT_EPOLL_ERR;
                        ret = srt_epoll_add_usock(mEpollId, ss, &events);
                        if(ret != SRT_ERROR)
                        {
                            mMutexClient.lock();
                            mSrtClients.insert(ss);
                            mMutexClient.unlock();
                            ss = SRT_INVALID_SOCK;
                        }
                    }
                    SRT_CLOSE(ss);
                }
            });
            mThreadClient = std::thread([&]()
            {
                while(mIsStart)
                {
                    mMutexClient.lock_shared();
                    if(mSrtClients.empty())
                    {
                        mMutexClient.unlock_shared();
                        msleep(50);
                        continue;
                    }

                    std::vector<SRTSOCKET> sss;
                    sss.reserve(mSrtClients.size());
                    for(auto& srtSocket : mSrtClients)
                    {
                        sss.push_back(srtSocket);
                    }
                    mMutexClient.unlock_shared();

                    if(!onEpollRecv(mEpollId, sss, mBuffer))
                    {
                        break;
                    }
                    if(!sss.empty())
                    {
                        for(auto& srtSocket : sss)
                        {
                            srt_epoll_remove_usock(mEpollId, srtSocket);
                            mMutexClient.lock();
                            mSrtClients.erase(srtSocket);
                            mMutexClient.unlock();
                            SRT_CLOSE(srtSocket);
                        }
                    }
                }
            });
            isOk = true;
        } while(0);
        if(!isOk)
        {
            close();
        }
        return isOk;
    }

    int write(const char* data, int size, SRT_MSGCTRL* mctrl = nullptr) override
    {
        int ret = -1;

        std::list<SRTSOCKET> closedSRTs;
        mMutexClient.lock_shared();
        for(auto& srtSocket : mSrtClients)
        {
            ret = srt_sendmsg2(srtSocket, (const char*)data, size, mctrl);
            if(ret == SRT_ERROR)
            {
                SRT_SOCKSTATUS state = srt_getsockstate(srtSocket);
                if(srt_is_disconnected(state))
                {
                    closedSRTs.push_back(srtSocket);
                }
            }
        }
        mMutexClient.unlock_shared();

        if(!closedSRTs.empty())
        {
            for(auto& srtSocket : closedSRTs)
            {
                srt_epoll_remove_usock(mEpollId, srtSocket);
                mMutexClient.lock();
                mSrtClients.erase(srtSocket);
                mMutexClient.unlock();
                if(mDisconnectedHandler != nullptr)
                {
                    mDisconnectedHandler(srtSocket);
                }
                SRT_CLOSE(srtSocket);
            }
            closedSRTs.clear();
        }

        return ret;
    }

    SRT_SOCKSTATUS state() override
    {
        SRT_SOCKSTATUS ret = SRTS_NONEXIST;
        std::shared_lock<std::shared_mutex> locker(mMutexClient);
        if(!mSrtClients.empty())
        {
            ret = srt_getsockstate(*mSrtClients.begin());
        }
        return ret;
    }
    
    int bstats(SRT_TRACEBSTATS* perf, int clear) override
    {
        int ret = -1;
        std::shared_lock<std::shared_mutex> locker(mMutexClient);
        if(!mSrtClients.empty())
        {
            ret = srt_bstats(*mSrtClients.begin(), perf, clear);
        }
        return ret;
    }

    void close() override
    {
        mIsStart = false;
        if(mThreadAccept.joinable())
        {
            mThreadAccept.join();
        }
        if(mThreadClient.joinable())
        {
            mThreadClient.join();
        }
        while(!mSrtSockets.empty())
        {
            auto srtSocket = mSrtSockets.front();
            mSrtSockets.erase(mSrtSockets.begin());
            SRT_CLOSE(srtSocket);
        }
        mMutexClient.lock();
        while(!mSrtClients.empty())
        {
            auto iter = mSrtClients.begin();
            auto srtSocket = *iter;
            mSrtClients.erase(iter);
            SRT_CLOSE(srtSocket);
        }
        mMutexClient.unlock();
        if(mEpollId >= 0)
        {
            srt_epoll_release(mEpollId);
            mEpollId = -1;
        }
        mBuffer.clear();
    }

protected:
    virtual bool onOpen(SRTSOCKET ss)
    {
        return true;
    }

    virtual bool onConnected(SRTSOCKET ss)
    {
        bool isOk = false;
        int ret = 0;
        do
        {
            char peerStreamId[514];
            memset(peerStreamId, 0, sizeof(peerStreamId));
            int streamIdLen = sizeof(peerStreamId)-1;
            ret = srt_getsockopt(ss, 0, SRTO_STREAMID, &peerStreamId, &streamIdLen);
            if(ret == SRT_ERROR)
            {
                sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "srt_getsockopt(0x%x) error, %s",
                       ss, srt_getlasterror_str());
                ret = 0;
            }

            mMutexClient.lock_shared();
            if ((mMaxConnections > 0) && ((int)mSrtClients.size() >= mMaxConnections))
            {
                mMutexClient.unlock_shared();
                sl_log(SL_LEVEL::SL_WARNING, LOG_NAME, "Connection reach max limit: %d, refused stream %s %s\n", mMaxConnections, getParam("remoteaddr").c_str(), peerStreamId);
                break;
            }
            mMutexClient.unlock_shared();

            SRT_SET_OPTION(ss, SRTO_RCVSYN, &NO, sizeof(NO));
            if(mConnectedHandler)
            {
                ret = mConnectedHandler(ss, peerStreamId, streamIdLen);
                if(ret == SRT_ERROR)
                {
                    sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "connect failed, srt://%s, streamId=[%s]\n", getParam("localaddr").c_str(), peerStreamId);
                    break;
                }
            }
            std::string strPassphrase;
            int keyLength = atoi(getParam("keylength", "0").c_str());
            if((keyLength > 0) && (!getParam("password").empty()))
            {
                if(keyLength == 16)
                    strPassphrase = " with passphrase (AES-128)";
                else if(keyLength == 24)
                    strPassphrase = " with passphrase (AES-192)";
                else if(keyLength == 32)
                    strPassphrase = " with passphrase (AES-256)";
            }
            sl_log(SL_LEVEL::SL_NOTICE, LOG_NAME, "connected successfully%s, srt://%s, streamId=[%s]\n", strPassphrase.c_str(), getParam("localaddr").c_str(), peerStreamId);
            isOk = true;
        } while(0);
        return isOk;
    }

private:
    std::vector<SRTSOCKET> mSrtSockets;
    int mEpollId = -1;
    std::vector<char> mBuffer;
    std::atomic<bool> mIsStart;
    std::thread mThreadAccept;
    std::thread mThreadClient;
    std::shared_mutex mMutexClient;
    std::set<SRTSOCKET> mSrtClients; // receive mode: support multiple connections
};

// class SrtWrapper
SrtSocket::SrtSocket()
{
    srt_startup();
}

SrtSocket::~SrtSocket()
{
    close();
    srt_cleanup();
}

bool SrtSocket::create(const char* mode, const StdKeyValue& params)
{
    std::lock_guard<std::shared_mutex> locker(mMutex);
    if(_stricmp(mode, "caller") == 0)
    {
        mSrtBase = std::make_shared<SrtCaller>();
    }
    else if(_stricmp(mode, "listener") == 0)
    {
        mSrtBase = std::make_shared<SrtListener>();
    }
    else if(_stricmp(mode, "rendezvous") == 0)
    {
        mSrtBase = std::make_shared<SrtRendezvous>();
    }
    else if(_stricmp(mode, "bondingcaller") == 0)
    {
        mSrtBase = std::make_shared<SrtBondingCaller>();
    }
    else if(_stricmp(mode, "bondinglistener") == 0)
    {
        mSrtBase = std::make_shared<SrtBondingListener>();
    }
    else if(_stricmp(mode, "multiplexlistener") == 0)
    {
        mSrtBase = std::make_shared<SrtMultiplexListener>();
    }
    if(mSrtBase == nullptr)
    {
        return false;
    }
    mSrtBase->setParams(params);
    return true;
}

const char* SrtSocket::mode() const
{
    std::shared_lock<std::shared_mutex> locker(mMutex);
    if(mSrtBase != nullptr)
    {
        return mSrtBase->mode();
    }
    return "";
}

void SrtSocket::setOnReadHandler(OnSrtReadHandler handler)
{
    std::shared_lock<std::shared_mutex> locker(mMutex);
    if(mSrtBase != nullptr)
    {
        mSrtBase->setOnReadHandler(handler);
    }
}

void SrtSocket::setOnConnectedHandler(OnSrtConnectedHandler handler)
{
    std::shared_lock<std::shared_mutex> locker(mMutex);
    if(mSrtBase != nullptr)
    {
        mSrtBase->setOnConnectedHandler(handler);
    }
}

void SrtSocket::setOnDisconnectedHandler(OnSrtDisconnectedHandler handler)
{
    std::shared_lock<std::shared_mutex> locker(mMutex);
    if(mSrtBase != nullptr)
    {
        mSrtBase->setOnDisconnectedHandler(handler);
    }
}

void SrtSocket::setOnStreamIDHandler(OnSrtStreamIDHandler handler)
{
    std::shared_lock<std::shared_mutex> locker(mMutex);
    if(mSrtBase != nullptr)
    {
        mSrtBase->setOnStreamIDHandler(handler);
    }
}

bool SrtSocket::open(bool isSync)
{
    std::shared_lock<std::shared_mutex> locker(mMutex);
    if(mSrtBase == nullptr)
    {
        return false;
    }
    return mSrtBase->open(isSync);
}

bool SrtSocket::isConnected(int timeout_ms, std::function<bool()> isBreak)
{
    SRT_SOCKSTATUS state = SRTS_NONEXIST;
    int64_t begin_ms = now_clock_ms();
    while(true)
    {
        state = this->state();
        if(state == SRTS_CONNECTED)
        {
            break;
        }
        if(isBreak && isBreak())
        {
            break;
        }
        int64_t delta = (now_clock_ms() - begin_ms);
        if((delta < 0) || (delta > timeout_ms))
        {
            break;
        }
        msleep(10);
    }
    return (state == SRTS_CONNECTED);
}

int SrtSocket::write(const char* data, int size, SRT_MSGCTRL* mctrl)
{
    std::shared_lock<std::shared_mutex> locker(mMutex);
    if(mSrtBase == nullptr)
    {
        return -1;
    }
    return mSrtBase->write(data, size, mctrl);
}

SRT_SOCKSTATUS SrtSocket::state()
{
    std::shared_lock<std::shared_mutex> locker(mMutex);
    if(mSrtBase == nullptr)
    {
        return SRTS_NONEXIST;
    }
    return mSrtBase->state();
}

int SrtSocket::bstats(SRT_TRACEBSTATS* perf, int clear)
{
    std::shared_lock<std::shared_mutex> locker(mMutex);
    if(mSrtBase == nullptr)
    {
        return -1;
    }
    return mSrtBase->bstats(perf, clear);
}

int SrtSocket::write(SRTSOCKET ss, const char* data, int size, SRT_MSGCTRL* mctrl)
{
    std::shared_lock<std::shared_mutex> locker(mMutex);
    if(mSrtBase == nullptr)
    {
        return -1;
    }
    return mSrtBase->write(ss, data, size, mctrl);
}

SRT_SOCKSTATUS SrtSocket::state(SRTSOCKET ss)
{
    std::shared_lock<std::shared_mutex> locker(mMutex);
    if(mSrtBase == nullptr)
    {
        return SRTS_NONEXIST;
    }
    return mSrtBase->state(ss);
}

int SrtSocket::bstats(SRTSOCKET ss, SRT_TRACEBSTATS* perf, int clear)
{
    std::shared_lock<std::shared_mutex> locker(mMutex);
    if(mSrtBase == nullptr)
    {
        return -1;
    }
    return mSrtBase->bstats(ss, perf, clear);
}

void SrtSocket::close()
{
    {
        std::shared_lock<std::shared_mutex> locker(mMutex);
        if(mSrtBase != nullptr)
        {
            mSrtBase->close();
        }
    }
    {
        std::lock_guard<std::shared_mutex> locker(mMutex);
        mSrtBase = nullptr;
    }
}

void onSrtLogCallback(void* opaque, int level,
							 const char* file, int line,
							 const char* area, const char* message)
{
#if 1 //SRT Cor has a large number of logs [ ERROR: srt::CEPoll::wait ... or WARNING: srt::CUDT::processData....] when connection was broken or stream id does not match, os SRT Core log has blocked for this version
	if(sl_getloglevel() == SL_DEBUG)
	{
		sl_log((SL_LEVEL)level, "Srt core", "%s %s", area, message);
	}
	else
	{
		sl_log(SL_MAKE_ERROR_STRING, "Srt core", "%s %s", area, message);
	}
#endif

	//parser listener passphrase error
	std::string strMsg = message;
	std::string strError;
	if(strMsg.find("Incorrect passphrase") != std::string::npos && strMsg.find("rsp(REJECT)") != std::string::npos)
	{
		strError = "passphrase verification failed, wrong password";
	}
	else if(strMsg.find("Password required or unexpected") != std::string::npos && strMsg.find("rsp(REJECT)") != std::string::npos)
	{
		strError = "passphrase verification failed, password required or unexpected";
	}
	if(strError.length() > 0)
	{
		if(opaque != nullptr)
		{
			std::string localaddr = (char*)opaque;
			sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "%s, srt://%s\n", strError.c_str(), localaddr.c_str());
		}
		else
		{
			sl_log(SL_LEVEL::SL_ERROR, LOG_NAME, "%s\n", strError.c_str());
		}
	}
}