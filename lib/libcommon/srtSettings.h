#pragma once
//#include "typedefine.h"
#include <vector>
#include <string>
#include <tuple>
#include <sstream>

using namespace std;

#ifndef _WIN32
#ifndef stricmp
#define stricmp    strcasecmp
#endif
#endif

struct SrtSettings
{
	bool enabled = false;
    // "caller" | "listener" | "rendezvous" | "bondingcaller" | "bondinglistener"
	std::string connectionMode = "listener";
	int latencyMs = 0;
	std::string streamId;
	std::string password;
	int keyLength = 0;
    std::string localAddress; // x.x.x.x:port
    // <url, localInterface, localPort>, "bondingcaller" | "bondinglistener"
    // <"srt://x.x.x.x:port", "x.x.x.x", port>
    std::string groupType; // "broadcast"(default) | "backup"
    std::vector<std::tuple<std::string, std::string, int>> bondLinks;
    int maxConnections = -1; // "maxconnections"

    static std::string formatStreamId(const std::string& streamId)
    {
        if (streamId.substr(0, 4) == "#!::")
        {
            std::string path, mode;
            std::string content = streamId.substr(4);
            std::vector<std::string> kvs;

            std::stringstream ss(content);
            std::string item;
            while (std::getline(ss, item, ','))
            {
                kvs.push_back(item);
            }

            for (const auto& kv : kvs)
            {
                size_t eq_pos = kv.find('=');
                if (eq_pos == std::string::npos || eq_pos == 0 || eq_pos == kv.length() - 1)
                {
                    return streamId;
                }
                std::string key = kv.substr(0, eq_pos);
                std::string value = kv.substr(eq_pos + 1);

                if (key == "r") path = value;
                else if (key == "m") mode = value;
            }

            std::string result;
            if (!path.empty()) result += path;
            if (!mode.empty()) result += "[" + mode + "]";
            return result.empty() ? streamId : result;
        }
        return streamId;
    }

    std::string formatStreamId() const
    {
        return formatStreamId(streamId);
    }
};

static inline bool parseSrtAddress(const std::string& host, int port, const SrtSettings& srtSetting,
                                   std::string& localaddr, std::string& remoteaddr)
{
    if((stricmp(srtSetting.connectionMode.c_str(), "caller") == 0) ||
       (stricmp(srtSetting.connectionMode.c_str(), "rendezvous") == 0))
    {
        localaddr = srtSetting.localAddress;
        remoteaddr = host + ":" + std::to_string(port);
    }
    else if(stricmp(srtSetting.connectionMode.c_str(), "listener") == 0)
    {
        localaddr = host + ":" + std::to_string(port);
    }
    else if((stricmp(srtSetting.connectionMode.c_str(), "bondingcaller") == 0) ||
            (stricmp(srtSetting.connectionMode.c_str(), "bondinglistener") == 0))
    {
        if(srtSetting.bondLinks.empty())
        {
            if(stricmp(srtSetting.connectionMode.c_str(), "bondinglistener") == 0)
            {
                localaddr = host + ":" + std::to_string(port);
            }
            else
            {
                localaddr = srtSetting.localAddress;
                remoteaddr = host + ":" + std::to_string(port);
            }
        }
        else
        {
            for(const auto& bondLink : srtSetting.bondLinks)
            {
                if(!remoteaddr.empty())
                {
                    remoteaddr += ";";
                }
                std::string bondUrl = std::get<0>(bondLink);
#if 1
				int indexHost = bondUrl.find("://");
				string strHost = bondUrl;
				if(indexHost > 0)
				{
					int indexPort = bondUrl.find(":", indexHost + 3);
					if(indexPort > 0)
					{
						strHost = bondUrl.substr(indexHost + 3, indexPort - indexHost - 3);
						int port = atoi(bondUrl.substr(indexPort + 1).c_str());
						remoteaddr += (strHost + ":" + std::to_string(port));
					}
				}
#else

                Uri uri(bondUrl);
                remoteaddr += (uri.getHost() + ":" + std::to_string(uri.getPort()));
#endif
                if(!localaddr.empty())
                {
                    localaddr += ";";
                }
                std::string bondInterface = std::get<1>(bondLink);
                int bondPort = std::get<2>(bondLink);
                localaddr += (bondInterface + ":" + std::to_string(bondPort));
            }
        }
    }
    else
    {
        return false;
    }
    return true;
}