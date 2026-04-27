#include "common.h"
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <Windows.h>
#include <timeapi.h>
#pragma comment(lib, "Winmm.lib")
#endif

void wss_sleepMS(int64_t ms)
{
#ifdef _WIN32
    timeBeginPeriod(1);
#endif
    const std::chrono::steady_clock::time_point beginTime = std::chrono::steady_clock::now();
    std::this_thread::sleep_until(beginTime + std::chrono::milliseconds(ms));
}

int64_t wss_nowClockUS()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - 
        std::chrono::time_point<std::chrono::high_resolution_clock>()).count();
}

int wss_packetData(const void* data, int size, const char* channel, std::vector<uint8_t>& vecData,
                   const void** outData, int* outSize)
{
    if((data == NULL) || (size < 1) || (size > WSS_DATA_MAX_LENGTH))
    {
        return -1;
    }
    int cLen = (channel != NULL) ? ((int)strlen(channel)) : 0;
    if(cLen > 0)
    {
        if(cLen > WSS_HEADER_CHANNEL)
        {
            cLen = WSS_HEADER_CHANNEL;
        }
        vecData.resize(WSS_HEADER_LENGTH + size);
        uint8_t* packet = vecData.data();
        DataProtocol dp;
        memcpy(dp.channel, channel, cLen);
        dp.size = size;
        dp.data = (const uint8_t*)data;
        dp.pack(packet, (int)vecData.size());
        *outData = (void *)vecData.data();
        *outSize = (int)vecData.size();
    }
    else
    {
        *outData = (void *)data;
        *outSize = (int)size;
    }
    return (*outSize);
}
