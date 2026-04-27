#include "../include/libwebserver.h"
#include "webserverbeastimpl.h"
#include <stdio.h>
#include <string>
#include <chrono>
#include <future>
#include <thread>

IWebServer* WebServerCreate()
{
    return (new WebServerBeastImpl());
}

void WebServerRelease(IWebServer* pWebServer)
{
    if(pWebServer != NULL)
    {
        delete pWebServer;
    }
}
