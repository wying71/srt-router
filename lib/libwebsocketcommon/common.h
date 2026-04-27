#ifndef LIBWEBSOCKETCOMMON_H
#define LIBWEBSOCKETCOMMON_H
#include <stdint.h>
#include <string.h>
#include <vector>

#define WSS_HEADER_FLAG     4
#define WSS_HEADER_CHANNEL  8
#define WSS_HEADER_LENGTH   (WSS_HEADER_FLAG+WSS_HEADER_CHANNEL)

#define WSS_DATA_MAX_LENGTH 8*1024*1024

#ifndef _WIN32
#define _strdup     strdup
#define  stricmp    strcasecmp
#define _stricmp    strcasecmp
#define _strnicmp   strncasecmp
#define _wcsicmp    wcscasecmp
#define strncpy_s   strncpy
#endif

// @WSC
static const char WSS_FLAG[WSS_HEADER_FLAG] = { '@', 'W', 'S', 'C' };

#define WSS_VERSION         "2.0.2020.0403"

template<typename TYPE>
int packInteger(uint8_t *p, int size, TYPE v)
{
    const int len = sizeof(TYPE);
    if(size < len)
    {
        return 0;
    }
    for(int i = 0; i < len; i ++)
    {
        const int sb = (len - i - 1) * 8;
        p[i] = (v >> sb) & 0xFF;
    }
    return len;
}

template<typename TYPE>
TYPE unpackInteger(const uint8_t *p, int size)
{
    const int len = sizeof(TYPE);
    if(size < len)
    {
        return 0;
    }
    TYPE v = 0;
    for(int i = 0; i < len; i ++)
    {
        const int sb = (len - i - 1) * 8;
        TYPE b = p[i];
        v |= (b << sb);
    }
    return v;
}

struct DataProtocol
{
    char           flag[WSS_HEADER_FLAG]         = { 0 };
    char           channel[WSS_HEADER_CHANNEL+1] = { 0 };
    int            size = 0;
    const uint8_t* data = NULL;  // only pointer

    int pack(uint8_t *packet, int len) const
    {
        if(len < (int)(WSS_HEADER_LENGTH + size))
        {
            return 0;
        }
        memcpy(packet, WSS_FLAG, WSS_HEADER_FLAG);
        memcpy(packet + WSS_HEADER_FLAG, this->channel, WSS_HEADER_CHANNEL);
        if(this->size > 0)
        {
            memcpy(packet + WSS_HEADER_LENGTH, this->data, this->size);
        }
        return (WSS_HEADER_LENGTH + this->size);
    }

    int unpack(const uint8_t *packet, int len)
    {
        if(len >= WSS_HEADER_LENGTH)
        {
            memcpy(this->flag, packet, WSS_HEADER_FLAG);
            if(memcmp(this->flag, WSS_FLAG, WSS_HEADER_FLAG) == 0)
            {
                memcpy(this->channel, packet + WSS_HEADER_FLAG, WSS_HEADER_CHANNEL);
                this->channel[WSS_HEADER_CHANNEL] = '\0';
                this->size = len - WSS_HEADER_LENGTH;
                this->data = packet + WSS_HEADER_LENGTH;
                return (WSS_HEADER_LENGTH + this->size);
            }
        }
        this->size = len;
        this->data = packet;
        return len;
    }
};

int     wss_packetData(const void* data, int size, const char* channel, 
                       std::vector<uint8_t> &vecData, 
                       const void **outData, int *outSize);

void    wss_sleepMS(int64_t ms);
int64_t wss_nowClockUS();

#endif // LIBWEBSOCKETCOMMON_H