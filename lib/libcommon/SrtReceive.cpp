/* FOR COMPILE
#include "SrtReceive.h"

class SrtCallback
{
public:
    SrtCallback() { }
    virtual ~SrtCallback() { }
public:
	void OnData(char* pData, int lens, int64_t dataTick = -1)
    {
        return ;
    }
};

void testSrt()
{
    SrtCallback cb;
    CSrtReceive<SrtCallback> sr(&cb);
    sr.Start("::1", 4444, "", 5555, "");
    while(1)
    {
        Sleep(100);
        SRT_TRACEBSTATS perf;
        sr.GetStats(&perf, true);
    }
    sr.Stop();
}
*/