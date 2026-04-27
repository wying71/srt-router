#ifndef SRTRECEIVE_H
#define SRTRECEIVE_H

#include "ace/OS.h"
#include "ace/Date_Time.h"
#include "srtSettings.h"
#include "SrtSocket.h"
#include <memory>
#include "alignedmemory.h"
#include <atomic>
#include <mutex>
#include <list>
#include <thread>
#include <vector>
#include <unordered_map>
#include "json/json.h"
#include <string>

using namespace std;

template<class T>
class CSrtReceive
{
public:
	CSrtReceive(T* pReceive);
	virtual ~CSrtReceive(){};

public:
    int                  Start(const string& host, int port, const string& localInterface, int localPort, const string& options);
    bool                 Stop();
    int                  GetStats(SRT_TRACEBSTATS* perf, int clear);
	string				 getRemoteAddr();
	void				 setOnDisconnectedHandler(OnSrtDisconnectedHandler handler);

private:
	T*                   mReceiver        = nullptr;
	bool                 mRunning         = false;    
	std::shared_ptr<SrtSocket> m_srtSocket = nullptr;
	std::mutex           mBufferMutex;
	std::thread          mRecvThread;
	std::thread          mDemuxThread;
	std::string			 mRemoteAddr;
	map<string, int64_t> mRemoteAddrList;
	std::mutex           mRemoteAddrMutex;
	SrtSettings			 mSrtSettings;
	OnSrtDisconnectedHandler mSrtDisconnectedHandler = nullptr;
	std::list<std::pair<int64_t, std::pair<char*, int>>> mPackets;
};

template<class T>
CSrtReceive<T>::CSrtReceive(T* pReceive)
: mReceiver(pReceive)
{
}

template<class T>
int CSrtReceive<T>::Start(const string& host, int port, const string& localInterface, int localPort, const string& options)
{
	Json::Reader reader;
	Json::Value srtOptions;
	reader.parse(options.c_str(), srtOptions);
    if(srtOptions.isMember("connectionMode"))
	    mSrtSettings.connectionMode = srtOptions["connectionMode"].asString();
    else
        mSrtSettings.connectionMode = "listener";

    // "caller" | "listener" | "rendezvous" | "bondingcaller" | "bondinglistener"
    if (mSrtSettings.connectionMode != "caller"
        && mSrtSettings.connectionMode != "listener"
        && mSrtSettings.connectionMode != "rendezvous"
        && mSrtSettings.connectionMode != "bondingcaller"
        && mSrtSettings.connectionMode != "bondinglistener")
        mSrtSettings.connectionMode = "listener";

    if (mSrtSettings.connectionMode == "listener" && localPort == 0)
        localPort = port;

    if(srtOptions.isMember("latencyMs"))
	    mSrtSettings.latencyMs = srtOptions["latencyMs"].asInt();
   
    if(mSrtSettings.latencyMs <= 0)
        mSrtSettings.latencyMs = 500;

    if(srtOptions.isMember("streamId"))
	    mSrtSettings.streamId = srtOptions["streamId"].asString();

    if(srtOptions.isMember("encryption"))
	    mSrtSettings.keyLength = srtOptions["encryption"].asInt();

    if(srtOptions.isMember("password"))
	    mSrtSettings.password = srtOptions["password"].asString();

    if(srtOptions.isMember("groupType"))
	    mSrtSettings.groupType = srtOptions["groupType"].asString();

	mSrtSettings.localAddress = localInterface + ":" + std::to_string(localPort);
	mSrtSettings.bondLinks.clear();
	if (srtOptions.isMember("bondLinks"))
	{
		const Json::Value& srtBondLinks = srtOptions["bondLinks"];
		for (Json::ArrayIndex index = 0; index < srtBondLinks.size(); index++)
		{
			const auto& srtBondLink = srtBondLinks[index];
			std::string bondUrl = srtBondLink.get("url", "").asString();
			std::string bondInterface = srtBondLink.get("localInterface", "").asString();
			int bondPort = srtBondLink.get("localPort", 0).asInt();
			mSrtSettings.bondLinks.push_back(std::make_tuple(bondUrl, bondInterface, bondPort));
		}
	}
    mRunning = true;
    m_srtSocket = std::make_shared<SrtSocket>();
    StdKeyValue params;
    params["rcvlatency"]  = std::to_string(mSrtSettings.latencyMs);
    params["peerlatency"] = std::to_string(mSrtSettings.latencyMs);
    params["streamid"]    = mSrtSettings.streamId;
    params["password"]    = mSrtSettings.password;
    params["keylength"]   = std::to_string(mSrtSettings.keyLength);
    params["grouptype"]   = mSrtSettings.groupType;
    params["conntimeo"]   = "10000";
    params["iotype"]      = "input"; // set IO type: "input"
    std::string localaddr;
    std::string remoteaddr;
    if(!parseSrtAddress(host, port, mSrtSettings, localaddr, remoteaddr))
    {
        return -1;
    }
    params["localaddr"] = localaddr;
    params["remoteaddr"] = remoteaddr;

	mRemoteAddr = "null";
	if(mSrtSettings.connectionMode == "bondingcaller")
	{
		auto srt_splitstring = [](const string & input, const string & delimiter, bool includeEmpties)
		{
			vector<string> strary;
			if(delimiter.length() <= 0 || input.length() <= 0)
				return strary;

			int start = 0;
			int end = 0;
			while((end = input.find(delimiter, start)) >= 0)
			{
				if((start != end) || includeEmpties)
					strary.push_back(input.substr(start, end - start));
				start = end + delimiter.size();
			}
			if((start != (int)(input.size())) || includeEmpties)
				strary.push_back(input.substr(start));

			return strary;
		};

		vector<string> keyvals = srt_splitstring(remoteaddr, ";", false);
		for(auto& kv : keyvals)
		{
			vector<string> keyval = srt_splitstring(kv, ":", false);
			if(keyval.size() == 2)
			{
				if(!mRemoteAddr.empty())
				{
					mRemoteAddr += ";";
				}
				mRemoteAddr += keyval[0];
			}
		}
	}
	else if(mSrtSettings.connectionMode == "caller" || mSrtSettings.connectionMode == "rendezvous")
	{
		mRemoteAddr = host;
	}

    if(!m_srtSocket->create(mSrtSettings.connectionMode.c_str(), params))
    {
        return -1;
    }
	m_srtSocket->setOnConnectedHandler([&](SRTSOCKET ss, const char* streamId, int length) -> int
	{		
		if(mSrtSettings.connectionMode == "listener")
		{
			struct sockaddr_in peer_addr;
			int addr_len = sizeof(peer_addr);

			if(srt_getpeername(ss, (struct sockaddr*)&peer_addr, &addr_len) != SRT_ERROR)
			{
				char ip[INET_ADDRSTRLEN] = { 0 };
				ACE_OS::inet_ntop(AF_INET, &peer_addr.sin_addr, ip, INET_ADDRSTRLEN);
				mRemoteAddr = ip;
			}
		}
		return 0;
	});

	m_srtSocket->setOnDisconnectedHandler(mSrtDisconnectedHandler);

    m_srtSocket->setOnReadHandler([&](SRTSOCKET ss, const char* data, int size, SRT_MSGCTRL* mctrl) -> void
    {
        char* newData = (char*)mem_malloc(size);
		if (newData == nullptr)
			return;
		memcpy(newData, data, size);
        std::lock_guard<std::mutex> guard(mBufferMutex);
        mPackets.push_back(make_pair(0, make_pair(newData, size)));

		if(mctrl != nullptr && mctrl->grpdata != nullptr && mSrtSettings.connectionMode == "bondinglistener")
		{
			char strIp[INET_ADDRSTRLEN] = { 'n','u','l','l'};

			ACE_Time_Value tv = ACE_OS::gettimeofday();
			int64_t curTick = tv.sec();
			curTick *= 1000;
			curTick += (tv.usec() / 1000);
			for(int i = 0; i < mctrl->grpdata_size; i++)
			{
				struct sockaddr_in peer_addr;
				int addr_len = sizeof(peer_addr);
				if(srt_getpeername(mctrl->grpdata[i].id, (struct sockaddr*)&peer_addr, &addr_len) != SRT_ERROR)
				{					
					ACE_OS::inet_ntop(AF_INET, &peer_addr.sin_addr, strIp, INET_ADDRSTRLEN);
					mRemoteAddrList[strIp] = curTick;
				}
			}
				
			if(mRemoteAddrList.size() > 1)
			{
				vector<string> strList;
				for(auto& ip : mRemoteAddrList)
				{
					if(curTick - ip.second < 2000)
					{
						strList.push_back(ip.first);
					}
					else
					{
						mRemoteAddrList.erase(ip.first);
						break;
					}
				}
				if(strList.size() == mRemoteAddrList.size())
				{
					std::lock_guard<std::mutex> guard(mRemoteAddrMutex);

					mRemoteAddr="";
					for(auto& it : strList)
					{
						if(mRemoteAddr.length() > 0)
						{
							mRemoteAddr.append(";");
						}
						mRemoteAddr.append(it);
					}
				}
			}
			else
			{
				std::lock_guard<std::mutex> guard(mRemoteAddrMutex);
				mRemoteAddr = strIp;
			}
		}
    });

    if(!m_srtSocket->open(false))
    {
        return -1;
    }

    mDemuxThread = std::thread([&]()
    {
        while(mRunning)
        {
            bool empty = false;
            {
                std::lock_guard<std::mutex> guard(mBufferMutex);
                empty = mPackets.empty();
            }
            if(empty)
            {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            pair<int64_t, std::pair<char*, int>> item;
            {
                std::lock_guard<std::mutex> guard(mBufferMutex);
                item = mPackets.front();
                mPackets.pop_front();
            }
            mReceiver->OnData((char*)item.second.first, item.second.second, item.first);
			mem_free((char*)item.second.first);
        }

        return 0;
    });

    return mRunning ? 0 : -1;
}

template<class T>
bool CSrtReceive<T>::Stop()
{
    mRunning = false;
    if(mRecvThread.joinable())
    {
        mRecvThread.join();
    }
    if(mDemuxThread.joinable())
    {
        mDemuxThread.join();
    }
    if(m_srtSocket != nullptr)
    {
        m_srtSocket->close();
        m_srtSocket = nullptr;
    }
	for(auto& data : mPackets)
	{
		if(data.second.first)
			mem_free((char*)data.second.first);
	}
    mPackets.clear();
	mRemoteAddr="";
    return true;
}

template<class T>
int CSrtReceive<T>::GetStats(SRT_TRACEBSTATS* perf, int clear)
{
	if (m_srtSocket != nullptr)
		return m_srtSocket->bstats(perf, clear);
	return -1;
}

template<class T>
inline string CSrtReceive<T>::getRemoteAddr()
{
	std::lock_guard<std::mutex> guard(mRemoteAddrMutex); 
	return mRemoteAddr;
}

template<class T>
inline void CSrtReceive<T>::setOnDisconnectedHandler(OnSrtDisconnectedHandler handler)
{
	mSrtDisconnectedHandler = handler;
}

#endif // SRTRECEIVE_H
